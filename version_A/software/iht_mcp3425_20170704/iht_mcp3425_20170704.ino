#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include "U8glib.h"
#include <Statistic.h>
#include  <MCP342X.h>

#define DISPLAY_LINE0_HEIGHT 12
#define DISPLAY_LINE1_HEIGHT 27
#define DISPLAY_LINE2_HEIGHT 42
#define DISPLAY_LINE3_HEIGHT 59
#define DISPLAY_LINE_LENGTH 25
#define FLASH_SCREEN_TIME 3000

MCP342X myADC;
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_NO_ACK|U8G_I2C_OPT_FAST);

boolean b_cbuf0 = false, b_cbuf1 = true, b_cbuf2 = false, b_cbuf3 = false;
boolean b_cbuf2BIG = true;
boolean dynamic_led = true;

int xplace = 0, yplace = 25;
char cbuf0[25], cbuf1[25], cbuf2[25], cbuf3[25];
String text;
float system_voltage;

adsGain_t ADC_PGA_GAIN;
int16_t pd_adc, led_adc;
int serial_val, counter;
bool prescaler;
char message[8];
byte reg;

int dummy_adc_target = 1000, dummy_adc_increment = 1;

static int16_t  result;

void setup() {
  TWBR = 12;  // 400 kHz (maximum)
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
  //strcpy(cbuf3, "CAL   READ  HELP");
  draw_setup_update(1);
  delay(500);
  strcpy(cbuf0, "");
  strcpy(cbuf1, "");
  strcpy(cbuf2, "");
  strcpy(cbuf3, "");
  
  Serial.begin(9600);
  pinMode(9, OUTPUT);
  TCCR1A = _BV(COM1A1);
  //*8-bit*/TCCR1A |= _BV(WGM10);
  //*9-bit*/TCCR1A |= _BV(WGM11);
  /*10-bit*/TCCR1A |= _BV(WGM10) | _BV(WGM11);
  TCCR1B = _BV(CS10); //no pre-scaler
  OCR1A = 0; //initial level

  serial_val = 0;
  prescaler = false;
  counter = 0;
  memset(message, 0, sizeof(message));

  /*
  ADC_PGA_GAIN = GAIN_TWO;
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
  ads.setGain(ADC_PGA_GAIN);
  ads.begin();
  Serial.print("ADC gain is ");
  Serial.println(ADC_PGA_GAIN);*/

  Serial.println("Testing device connection...");
  Serial.println(myADC.testConnection() ? "MCP342X connection successful" : "MCP342X connection failed");
    
  myADC.configure( MCP342X_MODE_CONTINUOUS |
                   MCP342X_CHANNEL_1 |
                   MCP342X_SIZE_16BIT |
                   MCP342X_GAIN_1X
                 );

  Serial.print("ADC configure check: ");
  Serial.println(myADC.getConfigRegShdw(), HEX);
  
}



void loop() {
  
  String s = F("pwm: ");
  s += OCR1A;
  s.toCharArray(cbuf0, 25);
  b_cbuf0 = true;
  //Serial.println(s);

  myADC.startConversion();
  myADC.getResult(&result);
  
  float t = float(result);
  if(t > 32767) t -= 65535;
  t = t / 32767 * 2048 / 10;
  Serial.print(result);
  Serial.print(",");
  Serial.print(t);

  bool bdraw = false;
  pd_adc = ads.readADC_SingleEnded(3);  delay(5);
  
    
  
  //increment input
  //we know the scale for the input and output
  //measure (have - want)
  int dif = (pd_adc - dummy_adc_target);
  float pdif = float(pd_adc) / float(dummy_adc_target);
  //determine increment direction and size
  if(!dynamic_led || (pdif > 0.95 && pdif < 1.05) || OCR1A == 0 || OCR1A == 1023) bdraw = true;
  //else if(
  if(bdraw){
    s = F("adc: ");
    s += pd_adc;
    s.toCharArray(cbuf1, 25);
    b_cbuf1 = true;
  }
  
  
  if(dif < 0 && OCR1A < 1023){
    //int val = OCR1A + dif/32 + 1;
    //if(val > 1023) val = 1023;
    //OCR1A = val;
    if(dynamic_led) ++OCR1A;
    if(bdraw) s = F("pwm up...");
  }
  else if(dif > 0 && OCR1A > 0){
    //int val = OCR1A - dif/32 - 1;
    //if(val < 0) val = 0;
    //OCR1A = val;
    if(dynamic_led) --OCR1A;
    if(bdraw) s = F("pwm down...");
  }
  else{
    if(bdraw) s = F("pwm stay...");
  }
  //Serial.println("\noutput: ");
  //Serial.println(pd_adc);
  //Serial.println(dummy_adc_target);
  //Serial.println(pdif);
  
  if(bdraw) {
    s.toCharArray(cbuf3, 25);
    b_cbuf3 = true;
    if(bdraw) draw_loop_update();
    delay(10);
  }
  SerialCheck();
  delay(10);
}

void SerialCheck(){
  while (Serial.available() > 0) {
    char new_byte = Serial.read();
    message[counter++] = new_byte;
    if (new_byte == -1) continue;  // if no characters are in the buffer read() returns -1
    else if (new_byte == 'p')
    {
      prescaler = true;
    }
    else if (new_byte == '\n')
    {
      //Serial.print("message: ");      Serial.println(message);
      if (prescaler && message[0] == 'p') {
        bool c0 = true, c1 = true, c2 = true;
        Serial.println(F("adjusting TIMER1 prescaler bits..."));// + message[1] + message[2] + message[3]);
        if(message[1] == '0'){c0 = false;}
        if(message[2] == '0'){c1 = false;}
        if(message[3] == '0'){c2 = false;}
        TCCR1B = (c0 << CS10) | (c1 << CS11) | (c2 << CS12);
        reg = TCCR1B;
        Serial.print(F("CS20="));Serial.print(bitRead(reg,0));
        Serial.print(F(", CS21="));Serial.print(bitRead(reg,1));
        Serial.print(F(", CS22="));Serial.println(bitRead(reg,2));
      }
      else if(message[0] == 'u'){
        Serial.print("auto-brightness ");
        if(dynamic_led) Serial.println("OFF");
        else Serial.println("ON");
        dynamic_led = !dynamic_led;
      }
      else if(message[0] == 't'){
        Serial.print(F("Setting target to: "));
        Serial.println(serial_val % 32767);
        dummy_adc_target = serial_val % 32767;
      }
      else {
        OCR1A = serial_val;
        Serial.print(F("Setting duty cycle to: "));
        Serial.println(serial_val % 1023 );
      }
      counter = 0;
      serial_val = 0;
      prescaler = false;
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

//--------------------------------
//--------------------------------
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
    }else{u8g.drawStr( 0, 59, cbuf2);}
  }
  if (b_cbuf3) u8g.drawStr( 0, 59, cbuf3);
}


