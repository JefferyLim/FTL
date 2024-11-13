#include <ADC.h>

const int readPin = A0;
ADC *adc = new ADC();

void setup() {

  pinMode(readPin, INPUT);
  Serial.begin(9600);

  adc->setAveraging(1); // set number of averages
  adc->setResolution(8); // set bits of resolution
  adc->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // change the conversion speed
  adc->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED); // change the sampling speed
}

int value;

void loop() {

  uint32_t start = millis();

  for (uint16_t i = 0; i < 10000; i++) {
    value = adc->analogRead(readPin);
  }

  uint32_t end = millis();

  Serial.print(end - start);
  Serial.println(" MicroSeconds for 10 readings");
  // result 58-61 µS
}
