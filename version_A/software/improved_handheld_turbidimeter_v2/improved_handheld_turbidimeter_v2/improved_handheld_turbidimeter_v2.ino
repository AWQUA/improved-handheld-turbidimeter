#include <avr/wdt.h>
#include <Statistic.h>
#include "U8glib.h"
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_NO_ACK|U8G_I2C_OPT_FAST);

#include <Wire.h>
//#include <SoftwareSerial.h>
#include <Adafruit_ADS1015.h>
#include <PinChangeInt.h>
//#include <EnableInterrupt.h>
#include <EEPROM.h>
#include <EEPROMAnything.h>

#define DEVICE_ID "00000002"
#define RTC_ADDR 0x6F
#define RTC_TS_BITS 7
#define TIME_REG 0x00
#define EEP0 0x50    //Address of 24LC256 eeprom chip
#define EEP1 0x51    //Address of 24LC256 eeprom chip

#define JSON_OUTPUT false
#define SERIAL_DEBUG true
#define CALIB_DEBUG false

#define MAX_READS_PER_BATCH 100
#define FLASH_SCREEN_TIME 2500
#define BAUD_RATE 19200

//#define DELAY__REFERENCE_SWITCH  10
//#define DELAY__BETWEEN_ADC_READS 5

//replace this block with EEPROM vars
#define STARTING_BRIGHTNESS 100
#define STARTING_PGA_GAIN 5

#define VREF_INTERNAL   1.1   //default: 1.1
#define VPIN A2
#define TPIN A1

#define VDIV_R1 3.3
#define VDIV_R2 1
#define VDIV_RATIO 4.3

#define LBUTTON 4
#define MBUTTON 5
#define RBUTTON 6

#define COMS_EN 10

Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
Statistic stat, clean_stat;

bool BT_on;
bool debug_set_rtc = false;

boolean bchange = false, lchange = false, mchange = false, rchange = false;
boolean b_cbuf0 = false, b_cbuf1 = true, b_cbuf2 = false, b_cbuf3 = false;
boolean b_cbuf2BIG = true;

unsigned long timer, last_action_ts, last_led_flash_ts;
int xplace = 0, yplace = 25;

int reads_per_batch = 50;
float avg, sd, clean_avg, clean_sd;

int serial_val, counter;
char message[8];
int gain_index;
int pwm_index;

float mv_adc_ratio;
float mv_per_adc[6] = {0.1875, 0.125, 0.0625, 0.03125, 0.015625, 0.0078125};
adsGain_t gain[6] = {GAIN_TWOTHIRDS, GAIN_ONE, GAIN_TWO, GAIN_FOUR, GAIN_EIGHT, GAIN_SIXTEEN};
adsGain_t ADC_PGA_GAIN;
int brightness;

int data[MAX_READS_PER_BATCH];
int adc0_target, adc3_target;
int upper_t, lower_t, upper_pwm, lower_pwm;
float prop_t;


struct config_t{
  int foo;           
  long machine_id;   
  unsigned long last_calibration_timestamp; 
  float y0, y1, y2, y3, a0, b0, c0, d0, a1, b1, c1, d1;
}
config;

void lif() {
  //lic++;
  bchange = true;
  lchange = true;
}
void mif() {
  //mic++;
  bchange = true;
  mchange = true;
}
void rif() {
  //ric++;
  bchange = true;
  rchange = true;
}

//SoftwareSerial BT(7,8); // RX, TX

char cbuf0[25], cbuf1[25], cbuf2[25], cbuf3[25];
String text;
float system_voltage;

void draw_setup_update(int code) {
  u8g.firstPage();
  do draw_setup(code);
  while (u8g.nextPage());
}

void draw_setup(int code){
  if(code == 0){
  u8g.setColorIndex(1);
  u8g.drawFilledEllipse(xplace,yplace,20,20);
  u8g.setColorIndex(0);
  u8g.drawFilledEllipse(xplace-22,yplace,5,5);
  u8g.drawFilledEllipse(xplace+22,yplace,5,5);
  u8g.drawFilledEllipse(xplace-11,yplace-18,5,5);
  u8g.drawFilledEllipse(xplace-11,yplace+18,5,5);
  u8g.drawFilledEllipse(xplace+11,yplace-18,5,5);
  u8g.drawFilledEllipse(xplace+11,yplace+18,5,5);
  u8g.drawFilledEllipse(xplace,yplace,12,12);
  u8g.setColorIndex(1);
  u8g.drawFilledEllipse(xplace,yplace+4,5,5);
  //u8g.drawTriangle(xplace-5,yplace+4, xplace,yplace-3, xplace+5,yplace+4);  //not available
  u8g.drawLine(xplace-6,yplace+4, xplace,yplace-5);
  u8g.drawLine(xplace,yplace-5, xplace+6,yplace+4);
  u8g.drawLine(xplace-4,yplace+4, xplace,yplace-3);
  u8g.drawLine(xplace,yplace-3, xplace+4,yplace+4);
  u8g.drawStr( 0, 59, cbuf3);
  }
  else if(code == 1){
    u8g.drawStr( 0, 59, cbuf3);
  }
}

void draw_loop_update() {
  u8g.firstPage();
  do draw_loop();
  while (u8g.nextPage());
}

void draw_loop() {// graphic commands to redraw the complete screen should be placed here
  //Estimate: letters are 12px tall and 10px wide
  //Keep a line offset of >= 15px to account for this
  if (b_cbuf0) u8g.drawStr( 0, 12, cbuf0);
  if (b_cbuf1) u8g.drawStr( 0, 27, cbuf1);
  if (b_cbuf2){
    if(b_cbuf2BIG){
      //u8g.setFont(u8g_font_unifont); 
      u8g.drawStr( 0, 42, cbuf2);
    }else{u8g.drawStr( 0, 42, cbuf2);}
  }
  if (b_cbuf3) u8g.drawStr( 0, 59, cbuf3);
}

void setup(void) 
{
  //---pin setups----
  analogReference(INTERNAL);
  pinMode(9, OUTPUT);
  TCCR1A = _BV(COM1A1);  //*8-bit*/TCCR1A |= _BV(WGM10);  //*9-bit*/TCCR1A |= _BV(WGM11);
  /*10-bit*/TCCR1A |= _BV(WGM10) | _BV(WGM11);
  TCCR1B = _BV(CS10); //no pre-scaler
  //OCR1A = ; //initial level
  set_brightness(STARTING_BRIGHTNESS);
  light_off();
  Serial.begin(BAUD_RATE);

  //---serial and i2c setups-----
  Serial.println("Hello!");
  set_adc_gain(STARTING_PGA_GAIN);
  ads.begin();
  ads.readADC_SingleEnded(0);
  ads.readADC_SingleEnded(1);
  ads.readADC_SingleEnded(2);
  ads.readADC_SingleEnded(3);
  analogRead(A0);
  delay(20);
  bt_setup();

  //---button setups-----
  pinMode(LBUTTON, INPUT_PULLUP);
  pinMode(MBUTTON, INPUT_PULLUP);
  pinMode(RBUTTON, INPUT_PULLUP);
  PCintPort::attachInterrupt(LBUTTON, lif, RISING);
  PCintPort::attachInterrupt(MBUTTON, mif, RISING);
  PCintPort::attachInterrupt(RBUTTON, rif, RISING);
  //enableInterrupt(LBUTTON, lif, RISING);
  //enableInterrupt(MBUTTON, mif, RISING);
  //enableInterrupt(RBUTTON, rif, RISING);
  b_cbuf0 = true;
  b_cbuf1 = true;
  b_cbuf2 = true;
  b_cbuf3 = true;

  //---OLED setup----
  u8g.setRot180(); // flip screen, if required
  u8g.setFont(u8g_font_8x13B);      //see list at https://code.google.com/p/u8glib/wiki/fontsize
  if      ( u8g.getMode() == U8G_MODE_R3G3B2 )    {u8g.setColorIndex(255);}// white
  else if ( u8g.getMode() == U8G_MODE_GRAY2BIT )  {u8g.setColorIndex(3);}  // max intensity
  else if ( u8g.getMode() == U8G_MODE_BW )        {u8g.setColorIndex(1);}  // pixel on
  else if ( u8g.getMode() == U8G_MODE_HICOLOR )   {u8g.setHiColorByRGB(255, 255, 255);}

  strcpy(cbuf3, "    Welcome!");
  xplace = 64;
  yplace = 25;
  draw_setup_update(0);
  delay(FLASH_SCREEN_TIME);
  strcpy(cbuf3, "BT    READ  INFO");
  draw_setup_update(1);
  delay(100);

  //---EEPROM setup----
  if(CALIB_DEBUG){
    config.foo = 255; //EEPROMAnything seems to need the struct to start with a integer in [0,255]
    config.machine_id = 11111111; //example
    config.last_calibration_timestamp = 1462835961;  //example
  //PLEASE NOTE -- the coefficients below are merely reasonable examples. 
  //Your device WILL require a calibration to determine the proper coefficient values.
    config.y0 = 0;
    config.a0 = -0.0039024;
    config.b0 = 0.274395;
    config.c0 = -5.593;
    config.d0 = 35.99;
    config.a1 = 0.00000000049184;
    config.b1 = -0.0000023599;
    config.c1 = 0.091663;
    config.d1 = -25.99;
    config.y1 = 0;
    config.y2 = 5540;
    EEPROM_writeAnything(0, config);
  }
  else{ EEPROM_readAnything(0, config); }
    
  if(SERIAL_DEBUG){
      Serial.println(F("Left button for calibration, middle button to take a reading, right button for help menu."));
  }

//----rtc stuff------------------------
 // mcp_read_timestamp(0);
  delay(10);
  if(debug_set_rtc) mcp_write_date(25, 49, 16, 6, 21, 7, 17);
  delay(10);
 // mcp_read_timestamp(0);
  delay(10);

  //----last_things---
  analogReference(INTERNAL);
  delay(10);

  for(int i = 0; i < MAX_READS_PER_BATCH; i++){
    data[i] = 0.0;
  }
  
}

void software_reset(){
  MCUSR = 0; //clear all flags
  WDTCSR |= _BV(WDCE) | _BV(WDE); //Write logical one to WDCE and WDE. Keep old prescaler setting to prevent unintentional time-out
  WDTCSR = 0;  
  wdt_enable(WDTO_1S); while(1) {}
}
  
void loop(void) {
  /*for(int z = 0; z < 25; z++)
  {
    get_sensor_output(false);
    delay(1000);
  }
  software_reset();*/
  
  if (bchange) {
    if(lchange){
      strcpy(cbuf0, "Bluetooth");  
      if(BT_on){
        bt_off();
        strcpy(cbuf1, "OFF");  
      }else{
        bt_on();
        strcpy(cbuf1, "ON");  
      }
      strcpy(cbuf2, "");
      if(SERIAL_DEBUG){Serial.println(F("Bluetooth toggled"));}
    }
    else if(mchange){
      int warmup = 0;
      if(millis() - last_led_flash_ts > 10000){
        long lapse = millis() - last_led_flash_ts;
        warmup = 500;
        if(lapse > 120000){warmup = 5000;}
        else if(lapse > 60000){warmup = 4000;}
        else if(lapse > 45000){warmup = 3500;}
        else if(lapse > 30000){warmup = 2500;}
        else if(lapse > 15000){warmup = 1000;}
      }
      if(warmup > 0){
        if(SERIAL_DEBUG){Serial.println(F("Warmup....."));}
        strcpy(cbuf0, "Warmup.....");
        strcpy(cbuf1, "");
        strcpy(cbuf2, "");
        draw_loop_update();
        delay(5);
      }
      strcpy(cbuf0, "Reading.....");
      strcpy(cbuf1, "");
      strcpy(cbuf2, "");
      if(SERIAL_DEBUG){Serial.println(F("Reading....."));}
      draw_loop_update();
      get_sensor_output(true);
    }
    else if(rchange){
      strcpy(cbuf0, "");
      strcpy(cbuf1, "");
      strcpy(cbuf2, "");
      get_helper_data();
    }
    draw_loop_update();
    bchange = false;
    lchange = false;
    mchange = false;
    rchange = false;
    last_action_ts = millis();
    last_led_flash_ts = millis();
    if(SERIAL_DEBUG){Serial.println(F("Left button for calibration, middle button to take a reading, right button for help menu."));}
  }
  else{;}
  SerialCheck();
  delay(200);
}


void get_helper_data() {
  float t = readTemperature();
  text = "temp: ";
  text += t;
  text += "C";
  text.toCharArray(cbuf0, 25);
  mcp_read_timestamp(4);
  //text = "btn cnt:";  //text += lic;  //text += ",";  //text += mic;
  //text += ",";  //text += ric;  //text.toCharArray(cbuf2, 25);
  text = "";
}

/******************************TURBIDITY SENSOR***********************************/
String get_sensor_output(bool print_times) {
  float timer = millis();
  if(print_times) Serial.print(F("Timestamp: "));
  if(print_times) mcp_read_timestamp(3);
  if(print_times) Serial.println();
  String s0 = "Raw mV: ", s1 = "ADC: ", s2 = "ADC sd: ";
  //strcpy(cbuf0, "Raw mV:");
  //strcpy(cbuf1, "");
  //strcpy(cbuf2, "");
  int clean_cnt;
  float avg, std, clean_avg, clean_std;
  batch_reads(1, reads_per_batch, false, avg, std, clean_avg, clean_std, clean_cnt);
  Serial.print(clean_cnt);
  Serial.print(F(","));
  Serial.print(clean_avg);
  Serial.print(F(","));
  Serial.print(clean_std);
  Serial.print(F(","));
  float clean_avg_mv = convert_to_mv(clean_avg);
  float clean_std_mv = convert_to_mv(clean_std);
  Serial.print(clean_avg_mv,4);
  Serial.print(F(","));
  Serial.println(clean_std_mv,4);
  s0 += clean_avg_mv;
  s1 += clean_avg;
  s2 += clean_std;
  s0.toCharArray(cbuf0, 25);
  s1.toCharArray(cbuf1, 25);
  s2.toCharArray(cbuf2, 25);
  if(print_times) Serial.print(F("readings completed in "));
  if(print_times) Serial.print(millis() - timer);
  if(print_times) Serial.println(F(" milliseconds."));
  return "done";
}

void clean_stats(float prune_dev, float avg, int num_reads, int& clean_cnt ){
  clean_stat.clear();
  int cntr = 0;
  for(int i = 0; i < num_reads; i++)
  {
    if(data[i] < avg + prune_dev && data[i] > avg - prune_dev){
      clean_stat.add(data[i]);
      ++cntr;
    }
  }
  clean_cnt = cntr;
  //Serial.print("from: ");  Serial.print(num_reads);  Serial.print(" used: ");  Serial.print(cntr);
  //Serial.print(", clean avg: ");  Serial.print(clean_stat.average());  Serial.print(", clean std: ");  Serial.println(clean_stat.unbiased_stdev());
}

float batch_reads(int num_batches, int num_reps, bool verboz, float& avg, float& std, float& clean_avg, float& clean_std, int& clean_cnt) {
  for(int j = 0; j < num_batches; j++){
    if(verboz) Serial.println("reads:");
    long function_timer = millis();
    light_on();
    delay(500);
    stat.clear();
    clean_stat.clear();
    memset(data, 0, sizeof(data));
    
    for(int i = 0; i < num_reps; i++){
      float a = ads.readADC_SingleEnded(3);
      data[i] = a;
      delay(5);
      stat.add(a);
      if(verboz) Serial.println(a);
    }
    light_off();
    delay(1000);
    avg = stat.average();
    std = stat.unbiased_stdev();
    clean_stats(1 * std, avg, num_reps, clean_cnt);
    clean_avg = clean_stat.average();
    clean_std = clean_stat.unbiased_stdev();
  }
  return 0;
}

float take_readings(int index, boolean raw_only, int rdgs, float canary, boolean throwaway, boolean dark_counts, boolean b_led1, boolean b_led2, boolean b_led3) {
  
}
/**************************TEMPERATURE AND VOLTAGE****************************/
float divisionFactor_TSL230R(){
  float m = .0052;   //slope of sensor's linear response curve
  float vmin = 3.0;  //min operating v of sensor
  float vmax = 5.5;  //max operating v of sensor
  float v100 = 4.9;  //voltage | normalized response of TSL230r = 1.0
  float v = getVoltageLevel();
  //Serial.print(F("Sensor normalization factor: "));
  //Serial.println(1000 * (4.9 - v) * m);
  if(v < vmin || v > vmax){ return -1; }
  else{ return 1.0 - (4.9 - v) * m; } 
}
float getVoltageLevel() {
  float vpin = analogRead(VPIN); //drop the first reading
  delay(10);//DELAY__BETWEEN_ADC_READS);
  vpin = analogRead(VPIN);
  delay(10);
  //Serial.println(vpin);
  system_voltage = vpin / 1023 * VREF_INTERNAL * VDIV_RATIO;
  text = "voltage: ";
  text += system_voltage;
  text += "V";
  text.toCharArray(cbuf1, 25);
  text = "";
  return system_voltage;
}

float readTemperature() { //Assumes using an LM35
  float t0 = analogRead(TPIN);
  delay(10);//DELAY__BETWEEN_ADC_READS);
  t0 = analogRead(TPIN);
  t0 /= 10;
  float v = getVoltageLevel();
  if(SERIAL_DEBUG){
    Serial.print(F("temperature: "));
    Serial.print(t0);
    Serial.println(F("C"));
    Serial.print(F("voltage: "));
    Serial.println(v);
  }
  return t0;
}

//------------------------------------
//---------RTC---------------------
void mcp_write_date(int sec, int mint, int hr24, int dotw, int day, int mon, int yr)
{
  Wire.beginTransmission(RTC_ADDR);
  Wire.write((uint8_t)TIME_REG);
  Wire.write(dec2bcd(sec) | B10000000);
  Wire.write(dec2bcd(mint));
  Wire.write(dec2bcd(hr24)); //hr
  Wire.write(dec2bcd(dotw) | B00001000); //dotw
  Wire.write(dec2bcd(day)); //date
  Wire.write(dec2bcd(mon)); //month
  Wire.write(dec2bcd(yr)); //year
  
  Wire.write((byte) 0);   
  //Wire.write((uint8_t)0x00);                     //stops the oscillator (Bit 7, ST == 0)
  /*
   * Wire.write(dec2bcd(05));
  Wire.write(dec2bcd(04));                  //sets 24 hour format (Bit 6 == 0)
  Wire.endTransmission();

  Wire.beginTransmission(RTC_ADDR);
  Wire.write((uint8_t)TIME_REG);
  Wire.write(dec2bcd(30) | _BV(ST));    //set the seconds and start the oscillator (Bit 7, ST == 1)
  */
  Wire.endTransmission();
}

void mcp_read_timestamp(int mode)
{
  Wire.beginTransmission(RTC_ADDR);
  Wire.write((byte)0x00);
  if (Wire.endTransmission() != 0) {
    Serial.println("no luck");
    //return false;
  }
  else {
    //request 7 bytes (secs, min, hr, dow, date, mth, yr)
    Wire.requestFrom(RTC_ADDR, RTC_TS_BITS);
    byte rtc_out[RTC_TS_BITS];
    String s = "";
    for (int i = 0; i < RTC_TS_BITS; i++) {
      rtc_out[i] = Wire.read();
      if(mode == 0) printBits(rtc_out[i]);
      else 
      {
        if(i == 0) rtc_out[i] &= B01111111;
        else if(i == 3) rtc_out[i] &= B00000111;
        else if(i == 5) rtc_out[i] &= B00011111;
        rtc_out[i] = bcd2dec(rtc_out[i]);
      }
    }
    char delimiters[7] = "::_ //";
    for(int i = 0; i < RTC_TS_BITS; i++){
      //int j = bcd2dec(rtc_out[i]);
      if(mode == 2){
        Serial.print(rtc_out[i]);
        if(i < RTC_TS_BITS - 1) Serial.print(",");
      }
      else if(mode == 3){
        if(i != 3){
          Serial.print(rtc_out[i]);
          if(i < RTC_TS_BITS - 1) Serial.print(delimiters[i]);
        }
      }
    }
    if(mode == 4){
      for(int i = 0; i < RTC_TS_BITS; i++){  
        Serial.println(rtc_out[i]);
      }
      s += 2000 + rtc_out[6]; s += "/";
      s += rtc_out[5];        s += "/";
      s += rtc_out[4];        s += " ";
      s += rtc_out[2];        s += ":";
      s += rtc_out[1];        
      //s += ":";      s += rtc_out[0];
      s.toCharArray(cbuf2, 25);
    }
  }
  Wire.endTransmission();
}


uint8_t dec2bcd(uint8_t n) {
  return n + 6 * (n / 10);
}

uint8_t bcd2dec(uint8_t n) {
  return n - 6 * (n >> 4);
}

void printBits(byte myByte) {
  for (byte mask = 0x80; mask; mask >>= 1) {
    if (mask  & myByte)
      Serial.print('1');
    else
      Serial.print('0');
  }
  Serial.println();
}

/*int second = bcd2dec(Wire.read() & ~_BV(ST));
Serial.print(second);
Serial.print(",");
int minute = bcd2dec(Wire.read());
Serial.print(second);
Serial.print(",");
int hour = bcd2dec(Wire.read() & ~_BV(HR1224));    //assumes 24hr clock
Serial.println(hour);
/*tm.Wday = i2cRead() & ~(_BV(OSCON) | _BV(VBAT) | _BV(VBATEN));    //mask off OSCON, VBAT, VBATEN bits
tm.Day = bcd2dec(i2cRead());
tm.Month = bcd2dec(i2cRead() & ~_BV(LP));       //mask off the leap year bit
tm.Year = y2kYearToTm(bcd2dec(i2cRead()));*/


//------------------------------------
//---------EEPROM---------------------
void writeEEPROM(int deviceaddress, unsigned int eeaddress, byte data ) 
{
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddress >> 8));   // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.write(data);
  Wire.endTransmission();
 
  delay(5);
}
 
byte readEEPROM(int deviceaddress, unsigned int eeaddress ) 
{
  byte rdata = 0xFF;
 
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddress >> 8));   // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.endTransmission();
 
  Wire.requestFrom(deviceaddress,1);
 
  if (Wire.available()) rdata = Wire.read();
 
  return rdata;
}


void SerialCheck(){
  while (Serial.available() > 0) {
    char new_byte = Serial.read();
    message[counter++] = new_byte;
    if (new_byte == -1) continue;  // if no characters are in the buffer read() returns -1
    else if (new_byte == '\n')
    {
      if(message[0] == 'g'){
        Serial.print("gain: ");
        Serial.println(ads.getGain());
        if(message[1] == '0'){ ADC_PGA_GAIN = GAIN_TWOTHIRDS;  gain_index = 0;}
        else if(message[1] == '1'){ ADC_PGA_GAIN = GAIN_ONE;  gain_index = 1;}
        else if(message[1] == '2'){ ADC_PGA_GAIN = GAIN_TWO;  gain_index = 2;}
        else if(message[1] == '3'){ ADC_PGA_GAIN = GAIN_FOUR;  gain_index = 3;}
        else if(message[1] == '4'){ ADC_PGA_GAIN = GAIN_EIGHT;  gain_index = 4;}
        else if(message[1] == '5'){ ADC_PGA_GAIN = GAIN_SIXTEEN;  gain_index = 5;}
        set_adc_gain(gain_index);
        Serial.print(F("ADC PGA gain set to: "));
        Serial.println(gain_index);
      }
      else if(message[0] == 'f'){
        Serial.println(F("freezing..."));
        Serial.println();
        OCR1A = 0;
        delay(10000);
      }
      else if(message[0] == 't'){
        Serial.print(F("Setting target to: "));
        Serial.println(serial_val % 32767);
        adc0_target = serial_val % 32767;
      }
      else if(message[0] == 'T'){
        Serial.print(F("Setting target to: "));
        Serial.println(serial_val % 32767);
        adc3_target = serial_val % 32767;
      }
      else if(message[0] == 'l'){
        pwm_index = serial_val % 1023 ;
        set_brightness(pwm_index);
        Serial.print(F("Setting duty cycle to: "));
        Serial.println(pwm_index);
      }
      else if(message[0] == 'r'){
        //float avg, sd;
        //batch_reads(6, 30, false, avg, sd);
        get_sensor_output(true);
      }
      else if(message[0] == 'B'){
        bt_on();
      }
      else if(message[0] == 'b'){
        bt_off();
      }
      counter = 0;
      serial_val = 0;
      memset(message, 0, sizeof(message));
      break;   // exit the while(1), we're done receiving
    }
    else if (new_byte > 47 && new_byte < 58)
    {
      serial_val *= 10;  // shift left 1 decimal place
      serial_val += (new_byte - 48); // convert ASCII to integer, add, and shift left 1 decimal place
    }
  }
}

void set_brightness(int b){  brightness = b; }
void light_on(){  OCR1A = brightness; }
void light_off(){  OCR1A = 0; }

void set_adc_gain(int g)
{
  gain_index = g;
  mv_adc_ratio = mv_per_adc[gain_index];
  ads.setGain(gain[gain_index]);  
}

float convert_to_mv(float adc){
  if(adc > 32767) adc -= 65535;
  return adc * mv_adc_ratio;
}

void bt_on(){
  Serial.println(F("BT on"));
  BT_on = true;
  digitalWrite(COMS_EN, HIGH);
}

void bt_off(){
  Serial.println(F("BT off"));
  BT_on = false;
  digitalWrite(COMS_EN, LOW);
}

void bt_setup(){
  pinMode(COMS_EN, OUTPUT);
  //BT.begin(9600);
  bt_off();  
}
