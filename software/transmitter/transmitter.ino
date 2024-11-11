// Create an IntervalTimer object 
IntervalTimer myTimer;

const int ledPin = LED_BUILTIN;  // the pin with a LED
const int irPin = 19;  // the pin with IR diode

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(irPin, OUTPUT);
  Serial.begin(9600);
  myTimer.begin(blinkLED, 1500000);  // blinkLED to run every 0.15 seconds

  usage();
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
  Serial.println("h          - prints this message");
  Serial.println("s          - stop transmitter");
  Serial.println("b [number] - set baud rate ");
  Serial.println("m [string] - set transmit message ");
}

const byte numChars = 16;
char receivedChars[numChars];
int parseInput(){
  int i = 0;
  char incomingByte;
    
  while (Serial.available() != 0){
    incomingByte = Serial.read();
    if (incomingByte != ' '){
      receivedChars[i] = incomingByte;
      i++;
    }
  }
  receivedChars[i] = '\0';
  Serial.println(receivedChars);
  return atoi(receivedChars);
}


char* message;
int messageLength;
void parseMessage(){
  int i = 0;
  char incomingByte;
  char tempChars[100];
  
  while (Serial.available() != 0 && i < 100){
    incomingByte = Serial.read();
    tempChars[i] = incomingByte;
    i++;
  }
  tempChars[i] = '\0';

  messageLength = i;
  free(message);
  message = (char*)malloc( sizeof(char) * ( messageLength + 1 ) );
  while (i >= 0){
    message[i] = tempChars[i];
    i--;
  }
}

void flushInput(){
   while(Serial.available() > 0 ) Serial.read();
}

volatile bool halt = 0;
volatile int baud = 9600;
void loop() {
    char incomingByte;
    int new_baud;
    int i = 0;
    int j = 0;

    while (Serial.available() != 0) {
      if(Serial.available() < 16){
        Serial.println(Serial.available());
        incomingByte = Serial.read();
        switch (incomingByte) {
          case 's':  // start or stop transmitter
            halt = !halt;
            if(halt){
              Serial.println("starting...");
            }else{
              Serial.println("stopping...");
            }
            break;
          case 'b':  // set baud rate
            new_baud = parseInput();
            if(new_baud == 0){
              Serial.println("ERROR: baud input should be a integer");
            }else{
              halt = 0;
              Serial.print("baud set to ");
              Serial.println(new_baud);
              baud = new_baud;
              halt = 1;
            }
            break;
          case 'm':  // set new message
            Serial.println("message");
            parseMessage();
            Serial.println(message);
            for(i=0;i < messageLength; i++){
                Serial.println(message[i]);
              for(j=0; j < 8; j++){
                Serial.println(bitRead(message[i],j));
              }
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
      }else{
        flushInput();
        Serial.println("Input too long");
      }
  }

}