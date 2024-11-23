#include "CRC8.h"

CRC8 crc;
// Create an IntervalTimer object
IntervalTimer txTimer;

const int ledPin = LED_BUILTIN; // the pin with a LED
const int irPin = 2;            // the pin with IR diode

// messages for transmission
char* message;
int messageLength = -1;
int counter = 48; //ascii character

// Controls
volatile int baud = 9600;
volatile bool halt = 0;

// Transmit variables
int ledState = LOW;
volatile int bitCount = 0;
volatile int messageCount = 0;
volatile int crc_index;
volatile int crccode; 

// States for transmitter mode and state
enum transmitter_mode{
  BLINK, COUNTER, MESSAGE,
}
enum transmitter_state{
  START, MESSAGE, CRC, END,
};

transmitter_mode tx_mode = BLINK;
transmitter_state tx_state = START;

// functions called by IntervalTimer should be short, run as quickly as
// possible, and should avoid calling other functions if possible.
void transmitter() {
  if (!halt) {
    // toggle mode: blink LED
    if (tx_mode == BLINK) {
      if (ledState == HIGH) {
        ledState = LOW;
      } else {
        ledState = HIGH;
      }
      digitalWriteFast(ledPin, ledState);
      digitalWriteFast(irPin, ledState);
      messageCount = 0;
      tx_state = START;
      bitCount = 0;
    } else if (tx_mode != BLINK) {
      // transmitter mode: transmit message
      if(tx_state == START){
        bitCount++;
        //Send the preamble
        if(bitCount == 8){
          tx_state = MESSAGE
          bitCount = 0;
          digitalWriteFast(ledPin, LOW);
          digitalWriteFast(irPin, LOW);
        //send the start bit
        }else{
          digitalWriteFast(ledPin, HIGH);
          digitalWriteFast(irPin, HIGH);
        }
      //set LEDs high so resting state is on
      }else if(tx_state == END){
        bitCount++;
        if(bitCount == 8){
          tx_state = START;
          bitCount = 0;
        }
        digitalWriteFast(ledPin, HIGH);
        digitalWriteFast(irPin, HIGH);
      //write the CRC code to the LEDs
      }else if (tx_state == CRC){
        if (crc_index > 7) {
          tx_state = END;
          if (messageCount >= messageLength * 8) {
            messageCount = 0;
          }
          crc.restart(); //remove all previous letters from the CRC calculation
        }else{
  #ifdef DEBUG
          Serial.println(bitRead(crccode, crc_index)); //prints the current bit of CRC code
  #endif
          digitalWriteFast(ledPin, bitRead(crccode, crc_index));
          digitalWriteFast(irPin, bitRead(crccode, crc_index));
        }
        crc_index++;
        
      }else{
         char currentByte;
         int currentBit;

        if(tx_mode == MESSAGE){
          currentByte = message[int(floor(messageCount / 8))];
          currentBit = messageCount % 8;
        }else if(tx_mode == COUNTER){
          if(counter > 57){ //ascii for 9
            counter = 48; //ascii for 0
          }
          currentByte = counter;
          currentBit = messageCount % 8;
        } 
        
  #ifdef DEBUG
        //prints the current byte in ASCII
        if (currentBit == 0) {
          Serial.println(message[int(floor(messageCount / 8))]);
        }
        //prints the current byte in binary
        Serial.print(bitRead(currentByte, currentBit));
  #endif
        digitalWriteFast(ledPin, bitRead(currentByte, currentBit));
        digitalWriteFast(irPin, bitRead(currentByte, currentBit));
        messageCount++;
  #ifdef DEBUG
        if (messageCount % 8 == 0) {
          Serial.println();
        }
  #endif
      //at the end of each byte
      if(messageCount % 8 == 0){
          //create crc
          crc.add(currentByte);
          crccode = crc.calc();
          tx_state = CRC;
          crc_index = 0;
          counter++;
        }
      }
    }
  }else{
    digitalWriteFast(ledPin, HIGH);
    digitalWriteFast(irPin, HIGH);
  }
}

// Usage print helper function
void usage() {
  Serial.println();
  Serial.println("FTL Transmitter");
  Serial.println("h          - prints this message");
  Serial.println("s          - start/stop transmitter");
  Serial.println("b [number] - set baud rate ");
  Serial.println("m [string] - set transmit message ");
  Serial.println("t          - set toggle mode");
}


void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(irPin, OUTPUT);
  // Enables fastest possible mode on pins 
  // https://forum.pjrc.com/index.php?threads/high-speed-digital-i-o-in-teensy-4-1.75179/
  CORE_PIN13_PADCONFIG |= 0xF9;
  CORE_PIN19_PADCONFIG |= 0xF9;
  Serial.begin(9600);
  txTimer.begin(transmitter, 1000000);  // transmitter to run every 1 seconds
  usage();
}

// Parse user input for baud (really any integer)
int parseInput() {
  int i = 0;
  char incomingByte;
  const byte numChars = 16; // arbitrary size...
  char receivedChars[numChars];

  while (Serial.available() != 0) {
    incomingByte = Serial.read();
    // ignore spaces...
    if (incomingByte != ' ') {
      receivedChars[i] = incomingByte;
      i++;
    }
  }
  receivedChars[i] = '\0'; // null terminate 
  return atoi(receivedChars); //return integer
}

// Parse a message for the transmitter
void parseMessage() {
  int i = 0;
  char incomingByte;
  char tempChars[100]; // support up to 100 bytes (arbitrary)

  while (Serial.available() != 0 && i < 100) {
    incomingByte = Serial.read();
    if (incomingByte == '\n') {
      break;
    }
    tempChars[i] = incomingByte;
    i++;
  }
  tempChars[i] = '\0';

  messageLength = i;
  // Free message before allocating
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
        Serial.println();
        if (halt) {
          Serial.println("stopping...");
        } else {
          Serial.println("starting...");
        }
        break;
      case 'b':  // set baud rate
        new_baud = parseInput();
        if (new_baud == 0) {
          Serial.println("ERROR: baud input should be a integer");
        } else {
          halt = 1;
          Serial.print("baud set to ");
          Serial.println(new_baud);
          Serial.println(int(round(1.0L / ((double)new_baud) * 1000000.0L)));
          baud = new_baud;
          txTimer.end();
          txTimer.begin(transmitter, int(round(0.5L / ((double)new_baud) * 1000000.0L)));
          //txTimer.update(round(1/baud * 1000000));
        }
        break;
      case 'm':  // set new message
        if (Serial.read() != ' ') {
          Serial.println("ERROR: improper format. Expected m [string]");
          break;
        } else {
          parseMessage();
          Serial.print("message set: ");
          Serial.print(message);
          Serial.print(" (Binary: ");
          for (i = 0; i < messageLength * 8; i++) {
            Serial.print(bitRead(message[int(floor(i / 8))], i % 8));
          }
          Serial.println(")");
        }
        break;
      case 't':
        halt = 1;

        if(tx_mode == BLINK){
          tx_mode = MESSAGE;
        }else if(tx_mode == MESSAGE){
          tx_mode = COUNTER;
        }else if(tx_mode == COUNTER){
          tx_mode = BLINK;
        }

        Serial.println();
        if (tx_mode == BLINK) {
          Serial.println("LED blinking...");
        } else if(tx_mode == MESSAGE){
          Serial.println("Message transmit..");
        } else if(tx_mode == COUNTER){
          Serial.println("Counter transmit..");
          counter = 48;
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
