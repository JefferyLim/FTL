#define DEBUG

// Create an IntervalTimer object
IntervalTimer myTimer;

const int ledPin = LED_BUILTIN;  // the pin with a LED
const int irPin = 19;            // the pin with IR diode

volatile bool toggle = 1;
char* message;
int messageLength = -1;

volatile bool halt = 0;
volatile int baud = 9600;
int ledState = LOW;
volatile unsigned long blinkCount = 0;  // use volatile for shared variables
volatile int messageCount = 0;

// functions called by IntervalTimer should be short, run as quickly as
// possible, and should avoid calling other functions if possible.
void blinkLED() {
  if (!halt) {
    if (toggle) {
      if (ledState == LOW) {
        ledState = HIGH;
        blinkCount = blinkCount + 1;  // increase when LED turns on
      } else {
        ledState = LOW;
      }
      digitalWrite(ledPin, ledState);
      digitalWrite(irPin, ledState);
      messageCount = 0;
    } else {

#ifdef DEBUG
      if (messageCount % 8 == 0) {
        Serial.println(message[int(floor(messageCount / 8))]);
      }

      Serial.print(bitRead(message[int(floor(messageCount / 8))], messageCount % 8));
#endif
      messageCount++;
#ifdef DEBUG
      if (messageCount % 8 == 0) {
        Serial.println();
      }
#endif
      if (messageCount >= messageLength * 8) {
        messageCount = 0;
      }
    }
  }
}


void usage() {
  Serial.println();
  Serial.println("FTL Transmitter");
  Serial.println("h          - prints this message");
  Serial.println("s          - stop transmitter");
  Serial.println("b [number] - set baud rate ");
  Serial.println("m [string] - set transmit message ");
  Serial.println("t          - set toggle mode (blink) ");
}


void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(irPin, OUTPUT);
  Serial.begin(9600);
  myTimer.begin(blinkLED, 1500000);  // blinkLED to run every 0.15 seconds
  usage();
}

const byte numChars = 16;
char receivedChars[numChars];
int parseInput() {
  int i = 0;
  char incomingByte;

  while (Serial.available() != 0) {
    incomingByte = Serial.read();
    if (incomingByte != ' ') {
      receivedChars[i] = incomingByte;
      i++;
    }
  }
  receivedChars[i] = '\0';
  return atoi(receivedChars);
}

void parseMessage() {
  int i = 0;
  char incomingByte;
  char tempChars[100];

  while (Serial.available() != 0 && i < 100) {
    incomingByte = Serial.read();
    tempChars[i] = incomingByte;
    i++;
  }
  tempChars[i] = '\0';

  messageLength = i;
  free(message);
  message = (char*)malloc(sizeof(char) * (messageLength + 1));
  while (i >= 0) {
    message[i] = tempChars[i];
    i--;
  }
}

void flushInput() {
  while (Serial.available() > 0) Serial.read();
}

void loop() {
  char incomingByte;
  int new_baud;
  int i;

  while (Serial.available() != 0) {
      incomingByte = Serial.read();
      switch (incomingByte) {
        case 's':  // start or stop transmitter
          halt = !halt;
          if (halt) {
            Serial.println("starting...");
          } else {
            Serial.println("stopping...");
          }
          break;
        case 'b':  // set baud rate
          new_baud = parseInput();
          if (new_baud == 0) {
            Serial.println("ERROR: baud input should be a integer");
          } else {
            halt = 0;
            Serial.print("baud set to ");
            Serial.println(new_baud);
            baud = new_baud;
            halt = 1;
          }
          break;
        case 'm':  // set new message
          if(Serial.read() != ' '){
            Serial.println("ERROR: improper format. Expected m [string]");
            break;
          }else{
            parseMessage();
            Serial.print("message set: ");
            Serial.print(message);
            Serial.print(" (Binary: ");
            for(i = 0; i < messageLength * 8; i++){
              Serial.print(bitRead(message[int(floor(i / 8))], i % 8));
            }
            Serial.println(")");
          }
          break;
        case 't':
          toggle = !toggle;
          if (toggle) {
            Serial.println("enable LED toggling...");
          } else {
            Serial.println("disable LED toggling...");
          }
          break;
        case '\n':
          break;
        case '\r':
          break;
        default:
          usage();
          break;
      }
    flushInput();
  }
}