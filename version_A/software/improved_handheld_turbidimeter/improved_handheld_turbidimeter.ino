#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <Statistic.h>
//Fix maxstats so that we analyse and add in readoperation and takereading 
//For this version the adc reads from 2 and 3. so be sure to attach photo diodes there 
#define NUM_READINGS 10
#define BUTTON1 2
#define SENSOR1 7
#define SENSOR2 12
#define LED1 8
#define LED2 9
#define VERB_OFF false
#define VERB_ON true
#define WASTE true
#define NO_WASTE false
#define LED_ON true
#define LED_OFF false
#define LOWER_BOUND 10000
#define UPPER_BOUND 32000

Adafruit_ADS1115 ads;
int gain1, gain2;
float scale1, scale2;
float hist1[NUM_READINGS];
float hist2[NUM_READINGS];
//int histbins[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
float realval1 = 0.0, realval2 = 0.0;

boolean button1_press = false;
Statistic realStatOne;
Statistic realStatTwo;
void trigger_button1() {
  button1_press = true;
}

void setup(void) {
  pinMode(BUTTON1, INPUT_PULLUP);
  attachInterrupt(0, trigger_button1, FALLING);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(SENSOR1, OUTPUT);
  pinMode(SENSOR2, OUTPUT);
  Serial.begin(9600);
  Serial.println(F("NOTE: is it safe to use GAIN_TWOTHIRDS with an input voltage = 5V?"));
  Serial.println(F("NOTE: why does the voltage dip for one sensor when the other is plugged in? Current insufficient?"));
  // Be careful never to exceed VDD +0.3V max, or to exceed the upper and lower limits if you adjust the input range!
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 0.1875mV (default)
     ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.0078125mV
  ads.begin();
  delay(20);
  gain1 = ads.getGain();
  gain2 = gain1;
  scale1 = 1.0;
  scale2 = 1.0;
  realStatOne.clear();
  realStatTwo.clear();
}

void loop (void) {
  if (button1_press) {
    Serial.println("Warmup...");
    read_operation(LED_ON, VERB_OFF, WASTE);
    delay(100);
    Serial.println("Reading...");
    read_operation(LED_ON, VERB_OFF, NO_WASTE);
    delay(100);
    read_operation(LED_OFF, VERB_OFF, NO_WASTE);
    button1_press = false;
    Serial.println("Done!");
  }
  delay(200);
}

void read_operation(boolean leds_on, boolean verbosity, boolean waste) {
  if (leds_on) {
    digitalWrite(LED1, HIGH);
    digitalWrite(LED2, HIGH);
  }
  delay(2);
  digitalWrite(SENSOR1, HIGH);
  digitalWrite(SENSOR2, LOW);
  delay(10);
  for (int i = 0; i < 5; i++) takeReading(verbosity, 1,&gain1,&scale1);
  for (int i = 0; i < NUM_READINGS; i++) {
    hist1[i] = takeReading(verbosity, 1, &gain1, &scale1);
    realStatOne.add(hist1[i]); 
  }

  digitalWrite(SENSOR1, LOW);
  digitalWrite(SENSOR2, HIGH);
  delay(10);
  for (int i = 0; i < 5; i++) takeReading(verbosity, 2,&gain2,&scale2);
  for (int i = 0; i < NUM_READINGS; i++) {
    hist2[i] = takeReading(verbosity, 2, &gain2, &scale2);
    realStatTwo.add(hist2[i]);
  }
  
  if(!waste){
    float sd1 = realStatOne.pop_stdev();
    float av1 = realStatOne.average();
    float sd2 = realStatTwo.pop_stdev();
    float av2 = realStatTwo.average();
    Serial.print(F("LED: "));
    if (leds_on) Serial.println("true");
    else Serial.println("false");
    Serial.print(F("Current GAIN1: "));
    Serial.println(gain1);
    Serial.print(F("Current GAIN2: "));
    Serial.println(gain2);
    //Serial.print(F("After "));
    //Serial.print(num_read);
    //Serial.println(F(" reads here at the stats: "));
    Serial.println("Summary Data for sensor 1");
    Serial.print(F("Min: "));
    Serial.println(realStatOne.minimum());
    Serial.print(F("Max: "));
    Serial.println(realStatOne.maximum());
    Serial.print(F("Average: "));
    Serial.println(av1);
    Serial.print(F("Standard Dev: "));
    Serial.println(sd1);
    Serial.print(F("Variance: "));
    Serial.println(realStatOne.variance());
    Serial.print(F("Confidence Interval1: ("));
    Serial.print(av1 - 1.96 * sd1);
    Serial.print(F(","));
    Serial.print(av1 + 1.96 * sd1);
    Serial.println(F(")"));
    Serial.println("Summary Data for sensor 2");
    Serial.print(F("Min: "));
    Serial.println(realStatTwo.minimum());
    Serial.print(F("Max: "));
    Serial.println(realStatTwo.maximum());
    Serial.print(F("Average: "));
    Serial.println(av2);
    Serial.print(F("Standard Dev: "));
    Serial.println(sd2);
    Serial.print(F("Variance: "));
    Serial.println(realStatTwo.variance());
    Serial.print(F("Confidence Interval2: ("));
    Serial.print(av2 - 1.96 * sd2);
    Serial.print(F(","));
    Serial.print(av2 + 1.96 * sd2);
    Serial.println(F(")"));
  
    float nrsum1 = 0.0, zsum1 = 0.0, z1 = 0.0, zcut = 1.5;
    float nrsum2 = 0.0, zsum2 = 0.0, z2 = 0.0;
  
    int reject_count1 = 0;
    int reject_count2 = 0;
    for (int i = 0; i < NUM_READINGS; i++) {
      z1 = (hist1[i] - av1) / sd1;
      z2 = (hist2[i] - av2) / sd2;
      zsum1 += z1;
      zsum2 +=z2;
      Serial.print(i);
      Serial.print(F(", "));
      Serial.print(hist1[i]);
      Serial.print(F(", "));
      Serial.print(av1);
      Serial.print(F(", "));
      Serial.print(sd1);
      Serial.print(F(", "));
      Serial.print(z1);
      Serial.print(F(", "));
      Serial.print(hist2[i]);
      Serial.print(F(", "));
      Serial.print(av2);
      Serial.print(F(", "));
      Serial.print(sd2);
      Serial.print(F(", "));
      Serial.println(z2);
      if (z1 >= zcut || z1 <= -zcut) {
        ++reject_count1;
      } else {
        nrsum1 += hist1[i];
      }
      if (z2 >= zcut || z2 <= -zcut) {
        ++reject_count2;
      } else {
        nrsum2 += hist2[i];
      }
    }
    Serial.print(F("Bias 1: "));
    Serial.println(zsum1 * sd1);
    Serial.print(F("Adjusted reading 1: "));
    Serial.println(nrsum1 / (NUM_READINGS - reject_count1));
    Serial.print(F("Bias 2: "));
    Serial.println(zsum2 * sd2);
    Serial.print(F("Adjusted reading 2: "));
    Serial.println(nrsum2 / (NUM_READINGS - reject_count2));
  }
  realStatOne.clear();
  realStatTwo.clear();
  delay(10);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  delay(2);
}
float takeReading(boolean verbosity, int adc_channel, int *gain, float *scale) {
  int rd, adc;
  float realval;
  //rd = analogRead(A0);
  adc = ads.readADC_SingleEnded(adc_channel);
  delay(2);  
  realval = adc / *scale;
  if (verbosity) {
    Serial.print(analogRead(A0));
    Serial.print('\t');
    Serial.print(*gain);
    Serial.print('\t');
    Serial.print(adc);
    Serial.print('\t');
    Serial.println(realval);
  }
  delay(2);
 // if (!waste) realValStats.add(realval);

  if (adc > UPPER_BOUND && *gain != GAIN_TWOTHIRDS) {
    // Serial.println("UPPER BOUND REACHED! Adjusting gain downwards");
    // Serial.println(gain);
    switch (*gain) {
      case GAIN_ONE:
        ads.setGain(GAIN_TWOTHIRDS);
        *gain = GAIN_TWOTHIRDS;
        *scale = 0.6667;
        break;
      case GAIN_TWO:
        ads.setGain(GAIN_ONE);
        *gain = GAIN_ONE;
        *scale = 1.0;
        break;
      case GAIN_FOUR:
        ads.setGain(GAIN_TWO);
        *gain = GAIN_TWO;
        *scale = 2.0;
        break;
      case GAIN_EIGHT:
        ads.setGain(GAIN_FOUR);
        *gain = GAIN_FOUR;
        *scale = 4.0;
        break;
      case GAIN_SIXTEEN:
        ads.setGain(GAIN_EIGHT);
        *gain = GAIN_EIGHT;
        *scale = 8.0;
        break;
    }
    delay(50);
    adc = ads.readADC_SingleEnded(adc_channel);
    delay(2);
    adc = ads.readADC_SingleEnded(adc_channel);
    delay(2);
  }
  else if (adc < LOWER_BOUND && * gain != GAIN_SIXTEEN) { //adjust gain to a higher mode
    // Serial.println("LOWER BOUND REACHED! Adjusting gain upwards");
    // Serial.println(gain);
    switch (*gain) {
      case GAIN_TWOTHIRDS:
        ads.setGain(GAIN_ONE);
        *gain = GAIN_ONE;
        *scale = 1.0;
        break;
      case GAIN_ONE:
        ads.setGain(GAIN_TWO);
        *gain = GAIN_TWO;
        *scale = 2.0;
        break;
      case GAIN_TWO:
        ads.setGain(GAIN_FOUR);
        *gain = GAIN_FOUR;
        *scale = 4.0;
        break;
      case GAIN_FOUR:
        ads.setGain(GAIN_EIGHT);
        *gain = GAIN_EIGHT;
        *scale = 8.0;
        break;
      case GAIN_EIGHT:
        ads.setGain(GAIN_SIXTEEN);
        *gain = GAIN_SIXTEEN;
        *scale = 16.0;
        break;
    }
    delay(50);
    adc = ads.readADC_SingleEnded(adc_channel);
    delay(2);
    adc = ads.readADC_SingleEnded(adc_channel);
    delay(2);
  }
  return realval;
}
