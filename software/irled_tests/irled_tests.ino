// Create an IntervalTimer object 
IntervalTimer myTimer;

const int ledPin = LED_BUILTIN;  // the pin with a LED
const int irpin = 32;  // the pin with a LED

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(irpin, OUTPUT);
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
  digitalWrite(irpin, ledState);
}

// The main program will print the blink count
// to the Arduino Serial Monitor
int val;

void loop() {

  // to read a variable which the interrupt code writes, we
  // must temporarily disable interrupts, to be sure it will
  // not change while we are reading.  To minimize the time
  // with interrupts off, just quickly make a copy, and then
  // use the copy while allowing the interrupt to keep working

  val = analogRead(A14);
  Serial.print("analog A14 is: ");
  Serial.println(val);
  delay(250);

}