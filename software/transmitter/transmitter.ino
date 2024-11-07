// Create an IntervalTimer object 
IntervalTimer myTimer;

const int ledPin = LED_BUILTIN;  // the pin with a LED
const int irPin = 19;  // the pin with IR diode

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(irPin, OUTPUT);
  Serial.begin(9600);
  myTimer.begin(blinkLED, 1500000);  // blinkLED to run every 0.15 seconds
}

// The interrupt will blink the LED, and keep
// track of how many times it has blinked.
int ledState = LOW;
volatile unsigned long blinkCount = 0; // use volatile for shared variables

// functions called by IntervalTimer should be short, run as quickly as
// possible, and should avoid calling other functions if possible.
void blinkLED() {
  if (ledState == LOW) {
    ledState = HIGH;
    blinkCount = blinkCount + 1;  // increase when LED turns on
  } else {
    ledState = LOW;
  }
  digitalWrite(ledPin, ledState);
  digitalWrite(irPin, ledState);
}


void usage(){
  Serial.println("FTL Transmitter");
  Serial.println("h          - prints this message")
  Serial.println("s          - stop transmitter")
  Serial.println("b [number] - set baud rate ")
  Serial.println("m [string] - set transmit message ")
}


void loop() {
    char incomingByte;
    usage();
    
    while (Serial.available()) {
      incomingByte = Serial.read();
      switch (incomingByte) {
        case 's':  // your hand is on the sensor
          Serial.println("stop");
          break;
        case 'b':  // your hand is close to the sensor
          Serial.println("baud");
          break;
        case 'm':  // your hand is a few inches from the sensor
          Serial.println("message");
          break;
        default:
          usage();
          break;
      }
  }

}