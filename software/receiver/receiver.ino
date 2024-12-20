#include "CRC8.h"

#define _PWM_LOGLEVEL_        0

#include <ADC.h>
#include <RingBuf.h>
#include <IntervalTimer.h>
#include <Teensy_PWM.h>

//Microswitch Pins
#define MOTOR_SIDE_SWITCH 22
#define OTHER_SIDE_SWITCH 21

#define USING_FLEX_TIMERS      true //Using flex timers for HW PWM

#define M2  9
#define M1  10
#define M0  11
#define STEP_PIN       7 //PWM Pins
#define DIR_PIN        6

//Threshold values
#define SENSOR_DIF_THRESH 200
#define SENSOR_SMALL_THRESH 50
#define SENSOR_BIG_THRESH 200

#define SENSOR_ON_THRESH 3600 // Sensor detecting for data
#define SENSOR_OFF_THRESH 3800 // Sensor not detecting for data

#define SENSOR_ABS_THRESH 3600 // Sensor not detecting for motor control

#define SAMPLING_FREQ 40000 // Hz
// Convert Sampling Frequency (Hz) to period (us)
double sampling_period = 1000000/SAMPLING_FREQ; // us

#define SAMPLING_RATIO 5 // SAMPLING_FREQ/
#define ON_THRESH 0 // SAMPLING_RATIO - ON_THRESH for 1's
#define OFF_THRESH 1  // OFF_THRESH for 0's

#define MOVEMENT_FREQ 500
 
//PRINT_SAMPLE, PRINT_RAW, PRINT_BIT, PRINT_CHAR, PRINT_SCAN, PRINT_NONE
#define PRINT_CHAR

//Timers (to emulate multithreading)
IntervalTimer sensor_readTimer;
IntervalTimer motor_controlTimer;

const int left_pin = A0;
const int mid_pin = A1;
const int right_pin = A2;

ADC *adc = new ADC(); 

//PWM Values:
float frequency;
float dutyCycle;

Teensy_PWM* stepper; //Stepper object


const int ledPin = LED_BUILTIN; // the pin with a LED

//States
enum tracking_state{
  SCANNING,
  LOCKED,
};

enum position_state{
  LEFT, MID_LEFT, MID, MID_RIGHT, RIGHT, UNKNOWN,
};

enum receiver_state{
  INIT,
  SYNCH,
  LOST,
};

enum message_state{
  START,
  MESSAGE,
  CRC,
  END,
};

//Sensor variables
//State variables
tracking_state track_state = SCANNING;
position_state pos_state;
message_state msg_state = START;
receiver_state rcv_state = INIT;

volatile char incoming_counter[10];
volatile int incoming_counter_counter = 0;
volatile int lost_counts = 0;
volatile int last_counter = -1;
volatile int current_counter = 0;

// Created photodiode array struct
struct photodiode_array{
  uint16_t left;
  uint16_t mid;
  uint16_t right;
  uint8_t bit : 1;
  uint8_t bit_confidence : 1;
  uint8_t : 3;
  uint8_t position : 3;
  
};

// RingBuffers
RingBuf<photodiode_array, 1024> adcBuffer; // for raw ADC outputs + other metrics
RingBuf<photodiode_array, 1024> bitBuffer; // extra buffer before message parsing
RingBuf<photodiode_array, 1024> motorBuffer; // extra buffer before motor control
RingBuf<int, 1024> messageBuffer; // buffer for collecting bits for message parsing

// Counters
int lostLockCount = 0;
int messageByteCount = 0;
int missingStartCounter = 0;

// Received Message
int receivedByte = 0;
int crcByte = 0;
CRC8 crc;
volatile int crc_index;
volatile int crccode;

// Flags
volatile bool bufferOverflow = 0;

// Performance Calcs
volatile int start = 0;
volatile int end = 0;

volatile unsigned long startMillis;
volatile unsigned long currentMillis;

// Position/Motor
int movement_counter = 0;
volatile position_state previous_position = MID;
volatile position_state last_known_state = MID;
int current_speed = 0;
bool hit_wall = false;
int button_debounce = 0;

void setup() {
  pinMode(left_pin, INPUT);
  pinMode(mid_pin, INPUT);
  pinMode(right_pin, INPUT);
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);

  ///// ADC0 ////
  adc->adc0->setAveraging(4); // set number of averages
  adc->adc0->setResolution(16); // set bits of resolution
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // change the conversion speed
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED); // change the sampling speed

  ///// ADC1 ////
  adc->adc1->setAveraging(4); // set number of averages
  adc->adc1->setResolution(16); // set bits of resolution
  adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // change the conversion speed
  adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED); // change the sampling speed

  sensor_readTimer.priority(0); //Set sensor priority higher
  motor_controlTimer.priority(1);
  
  sensor_readTimer.begin(read_sensors,(int)sampling_period); 
  motor_controlTimer.begin(pwm_control,(int)sampling_period*MOVEMENT_FREQ);

  //// PWM ////
  pinMode(DIR_PIN, OUTPUT);
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  stepper = new Teensy_PWM(STEP_PIN, 500, 0);
  /////

  //Microswitch pins
  pinMode(MOTOR_SIDE_SWITCH, INPUT_PULLUP);
  pinMode(OTHER_SIDE_SWITCH, INPUT_PULLUP);

}

void read_sensors() { //Read analog sensor input
  start = micros();
  struct photodiode_array dataset = {0}; 

  // difference variables
  // int edge_dif;
  // int left_mid_dif;
  // int mid_right_dif;

  dataset.left = adc->adc0->analogRead(left_pin);
  dataset.mid = adc->adc1->analogRead(mid_pin);
  dataset.right = adc->adc0->analogRead(right_pin);

  if(dataset.left <= SENSOR_ON_THRESH || dataset.right <= SENSOR_ON_THRESH \
   || dataset.mid <= SENSOR_ON_THRESH){
    dataset.bit = true;
    
    #ifdef PRINT_RAW
    Serial.print(1);
    #endif
    dataset.bit_confidence = true;
  }else if(dataset.left >= SENSOR_OFF_THRESH && dataset.right >= SENSOR_OFF_THRESH \
    && dataset.mid >= SENSOR_OFF_THRESH) {
    dataset.bit = false;
    dataset.bit_confidence = true;
    
    #ifdef PRINT_RAW
    Serial.print(0);
    #endif
  }else{
    
    #ifdef PRINT_RAW
    Serial.print(0);
    #endif
    dataset.bit = false;
    dataset.bit_confidence = false;
  }

  // edge_dif = dataset.left - dataset.right;
  // left_mid_dif = dataset.left - dataset.mid;
  // mid_right_dif = dataset.mid - dataset.right;

if(dataset.left > SENSOR_ABS_THRESH && dataset.mid > SENSOR_ABS_THRESH && dataset.right > SENSOR_OFF_THRESH){
    dataset.position = UNKNOWN;
  }else{

    if(dataset.mid < dataset.left && dataset.mid < dataset.right){
      dataset.position = MID; 
    }

    if(dataset.left < dataset.mid && dataset.left < dataset.right){
      dataset.position = LEFT; 
      if(abs(dataset.mid - dataset.left) < SENSOR_SMALL_THRESH){
        dataset.position = MID_LEFT;
      }
    }

    if(dataset.right < dataset.mid && dataset.right < dataset.left){
      dataset.position = RIGHT;
      if(abs(dataset.mid - dataset.right) < SENSOR_SMALL_THRESH){
        dataset.position = MID_RIGHT;
      }
    }
  }

  if(!adcBuffer.push(dataset)){
    Serial.println("ERROR: Can't push to buffer");
  }

  end = micros();
  // Serial.print("ADC: ");
  // Serial.println(end - start);
}

// Print function for printing the photodiode struct
void print_photodiode_struct(struct photodiode_array input){
  Serial.print(input.left);
  Serial.print(" | ");
  Serial.print(input.mid);
  Serial.print(" | ");
  Serial.print(input.right );
  Serial.print(" | ");
  Serial.print(input.bit );
  Serial.print(" | ");
  Serial.print(input.bit_confidence );
  Serial.print(" | ");
  printPosition(input.position);
  Serial.print(" | ");
  printPosition(last_known_state);
  Serial.println();
}

void printPosition(int input){
  switch (input) {
    case LEFT:
      Serial.print("LEFT");
      break;
    case MID_LEFT:
      Serial.print("MID_LEFT");
      break;
    case MID:
      Serial.print("MID");
      break;
    case MID_RIGHT:
      Serial.print("MID_RIGHT");
      break;
    case RIGHT:
      Serial.print("RIGHT");
      break;
    default:
      Serial.print("UNKNOWN");
      break;
  }
}

int bitShift = 0;
void messageParse(){
  struct photodiode_array adcValues;
  int bitCount = 0;
  int receivedBit = 0;

  // Double check that buffer is >5
  if(bitBuffer.size() < SAMPLING_RATIO){
    return;
  }

  // If we are in a "LOST" state... try and find presence of a signal...
  // A presence of a signal is 5 '1's or IR LED ON 
  if(rcv_state == LOST || rcv_state == INIT){
    
    digitalWriteFast(ledPin, 1);
    for(int i = 0; i < SAMPLING_RATIO; i++){
      // Look at the next 5 values
      bitBuffer.peek(adcValues, i);
      // Note, that if the bit confidence is high, then we can use the these samples
      // We only want to use 5 confident bits for now...
      if(adcValues.bit_confidence == 1){
        bitCount += adcValues.bit;
      }else{
        bitCount = 0;
        break;
      }
    }
  
    // If we find at least 4 '1' samples, lets start parsing
    // Move from LOST -> SYNCH
    if(bitCount >= (SAMPLING_RATIO - ON_THRESH)){
      #ifdef PRINT_CHAR
      Serial.println("Sync");
      #endif
      rcv_state = SYNCH;
      bitCount = 0;
      
      incoming_counter_counter = 0;
    }else{
      bitBuffer.pop(adcValues);
    }
  }else if(rcv_state == SYNCH){
    digitalWriteFast(ledPin, 0);
    // Begin counting bits
    for(int i = 0; i < SAMPLING_RATIO; i++){
      bitBuffer.pop(adcValues);
      bitCount += adcValues.bit;
    }

    // Do a majority vote
    // Anything inbetween is ambiguous, and we will exit SYNCH
    if(bitCount >= SAMPLING_RATIO - ON_THRESH){
      messageBuffer.push(1);
    }else if(bitCount <= OFF_THRESH){
      messageBuffer.push(0);
    }else{
      #ifdef PRINT_CHAR
      Serial.println();
      Serial.print("Ambiguous bits: ");
      Serial.print(bitCount);
      Serial.println("; returning to LOST...");
      #endif
      bitCount = 0;
      rcv_state = LOST;
      lostLockCount++;
      messageBuffer.clear();
      missingStartCounter = 0;
    }

    // Parse the bits for the message
    if (messageBuffer.size() > 6){
      if(msg_state == START) {
        missingStartCounter++;
        // If we dont' find the start within 1 second, then we exit SYNCH
        if(missingStartCounter > SAMPLING_FREQ/10){
          #ifdef PRINT_CHAR
          Serial.println("Can't find start bit...");
          Serial.println("Receiver can't find start bit; returning to LOST...");
          #endif
          messageBuffer.clear();
          missingStartCounter = 0;
          rcv_state = LOST;
        }else{
          bitCount = 0;
          // Look for a 111110
          for(int i = 0; i < 6; i++){
            messageBuffer.peek(receivedBit, i);
            if(i != 5 && receivedBit == 0){
              break;
            }
            if(i == 5 && receivedBit == 0){
              bitCount = 5;
            }
          }

          // 5 1's + 1 0's means we found the start bit
          if(bitCount == 5){
            msg_state = MESSAGE;
            for(bitCount = 5; bitCount >= 0; bitCount--){
              messageBuffer.pop(receivedBit);
              #ifdef PRINT_BIT
              Serial.print(receivedBit);
              #endif
            }
            bitCount = 0;
            bitShift = 0;
            receivedByte = 0;
            missingStartCounter = 0;
            #ifdef PRINT_BIT
              Serial.print("||");
            #endif
          }else{
            messageBuffer.pop(receivedBit);
            #ifdef PRINT_BIT
            Serial.print(receivedBit);
            #endif
          }
        }
      // found start
      }else if (msg_state == MESSAGE){
        // Pop 8 bits for the message
        messageBuffer.pop(receivedBit);
        #ifdef PRINT_BIT
        Serial.print(receivedBit);
        #endif
        receivedByte |= (receivedBit) << (bitShift);
        
        bitShift++;
        if(bitShift >= 8){
          msg_state = CRC;
          bitShift = 0;
          #ifdef PRINT_CHAR
          if(receivedByte == 4){

            incoming_counter[incoming_counter_counter] = '\0';
            incoming_counter_counter = 0;
            current_counter = atoi(incoming_counter);       // Convert to int using atoi

            // For the first count, just ignore this...
            if(last_counter == -1){
              last_counter = current_counter;
            }else if(last_counter+1 != current_counter){
              lost_counts += current_counter - last_counter;
            }

            last_counter = current_counter;

            Serial.print(", Lost counts: ");
            Serial.print(lost_counts);



            Serial.println();
            
          }else{
            Serial.print((char)receivedByte);
            incoming_counter[incoming_counter_counter] = (char)receivedByte;
            incoming_counter_counter++;
          }
          #endif
          
          #ifdef PRINT_BIT
            Serial.print("/");
          #endif
          crc.add((uint8_t)receivedByte);
          crccode = crc.calc();
        }
      }else if (msg_state == CRC){
        // Pop 8 bits for the message
        messageBuffer.pop(receivedBit);
        #ifdef PRINT_BIT
        Serial.print(receivedBit);
        #endif
        crcByte |= (receivedBit) << (bitShift);

        bitShift++;
        if(bitShift >= 8){
            
          #ifdef PRINT_BIT
            Serial.print("/");
          #endif
          if(crccode != crcByte){
            Serial.print(" (ERROR: CRC does not match: ");
            Serial.print(crcByte, HEX);
            Serial.print(", Expected: ");
            Serial.print(crccode, HEX);
            Serial.println(")");
          }

          
          crcByte = 0;
          crc.restart(); //remove all previous letters from the CRC calculation
          msg_state = END;
        }
        
      // grabbed message, just peek to make sure there isn't any more '0's after...
      }else if (msg_state == END){
        messageByteCount++;
        messageBuffer.peek(receivedBit, 0);
        if(receivedBit == 0){
          Serial.println("Found extra 0...");
        }
        msg_state = START;

      }
    }
  }
  //end = micros();
  //Serial.println(end - start);
}

void setSpeed(int speed)
{
  if (speed == 0)
  {
    // Use DC = 0 to stop stepper
    stepper->setPWM(STEP_PIN, abs(speed), 0);
  }
  else
  {
    //  Set the frequency of the PWM output and a duty cycle of 50%
    digitalWrite(DIR_PIN, (speed < 0));
    stepper->setPWM(STEP_PIN, abs(speed), 50);
  }
}



// 0 -> full step
// 1 -> 1/2 step
// 2 -> 1/4 step
//..
// 7 0 -> 1/256 step
void stepMode(int mode){
  digitalWrite(M0, bitRead(mode, 0));
  digitalWrite(M1, bitRead(mode, 1));
  digitalWrite(M2, bitRead(mode, 2));
}



elapsedMicros sincePrint;
int last_direction = 0;
#define SPEED 800
#define SPEED1 600
void pwm_control(){
  start = micros();
  struct photodiode_array input;
  motorBuffer.pop(input);

  stepMode(0);
  switch (input.position) {
    case LEFT:
      current_speed = -SPEED;
      last_direction = 0;
      break;
    case MID_LEFT:
      stepMode(0);
      last_direction = 0;
      current_speed = -SPEED;
      break;
    case MID:
      if(input.mid < input.left && input.mid < input.right){
        current_speed = 0;
      }
      last_direction = 0;
      break;
    case MID_RIGHT:
      last_direction = 0;
      stepMode(0);
      current_speed = SPEED;
      break;
    case RIGHT:
      last_direction = 0;
      current_speed = SPEED;
      break;
    case UNKNOWN:
      // If the receiver is lost, then we have lost the signal entirely
      if(rcv_state == LOST){
        
        // If we know what the last direction was, we should pick an initial direction
        // and go that way
        if(last_direction == 0){
          last_direction = 1;
          //printPosition(last_known_state);
          if(last_known_state == RIGHT || last_known_state == MID_RIGHT){
            current_speed = SPEED;
          }else if(last_known_state == LEFT || last_known_state == MID_LEFT){
            current_speed = -SPEED;
          }else if(last_known_state == MID){
            current_speed = -SPEED;
          }
        }
      
        // debounce the button for wall detection
        if(hit_wall == true){
          button_debounce++;
          if(button_debounce >= 40){
            button_debounce = 0;
            hit_wall = false;
          }
        }

        // If either wall has been hit, then we want to change our direction
        if(digitalRead(MOTOR_SIDE_SWITCH) == LOW && hit_wall == false){
            hit_wall = true;
            button_debounce = 0;
            current_speed = SPEED;
        }else if(digitalRead(OTHER_SIDE_SWITCH) == LOW && hit_wall == false) {
            hit_wall = true;
            button_debounce = 0;
            current_speed = -SPEED;
        }

      }
      break;
    default:
      break;
  }

  // If we hit the wall, then we want to stop moving
  if(last_direction == 0){
    if(digitalRead(MOTOR_SIDE_SWITCH) == LOW){
      current_speed = 0;
    }else if(digitalRead(OTHER_SIDE_SWITCH) == LOW) {
      current_speed = 0;
    }
  }

  // Store the previous known position
  if(previous_position != UNKNOWN){
    last_known_state = previous_position;
  }

  // Obtain the latest position
  previous_position = input.position;

  setSpeed(current_speed);
  // end = micros();
  // Serial.print("Motor: ");
  // Serial.println(end - start);
}


int abc = 0;

void loop() {
  struct photodiode_array adcValues;

  if(bufferOverflow){
    Serial.println("ERROR: RingBuffer Overflow");
  }
  // Attempt to pop ADC values
  if(adcBuffer.pop(adcValues)){
    movement_counter++;
    if(movement_counter == MOVEMENT_FREQ){
      motorBuffer.pushOverwrite(adcValues);
      movement_counter = 0;
    }

    // motorBuffer.pushOverwrite(adcValues);

    #ifdef PRINT_SAMPLE
      print_photodiode_struct(adcValues);
    #endif
    // Pushing ADC bits to a buffer for messages
    if(!bitBuffer.push(adcValues)){
      Serial.println("ERROR: Can't push to bitBuffer");
    }
    // Parse the bit for a message
    if(bitBuffer.size() > 5){
      messageParse();
    }
    
  }
  
}
