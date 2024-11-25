#include "CRC8.h"

#define _PWM_LOGLEVEL_        0

#include <ADC.h>
#include <RingBuf.h>
#include <IntervalTimer.h>
#include <Teensy_PWM.h>

#define USING_FLEX_TIMERS      true //Using flex timers for HW PWM

#define STEP_PIN       7 //PWM Pins
#define DIR_PIN        6

//Threshold values
#define SENSOR_DIF_THRESH 100

#define SENSOR_ON_THRESH 400 // Sensor detecting
#define SENSOR_OFF_THRESH 1000 // Sensor not detecting

#define SAMPLING_FREQ 5000 // Hz
// Convert Sampling Frequency (Hz) to period (us)
double sampling_period = 1000000/SAMPLING_FREQ; // us

#define SAMPLING_RATIO 10 // SAMPLING_FREQ/TRANSMIT_FREQ

#define MOVEMENT_FREQ 50

//PRINT_SAMPLE, PRINT_RAW, PRINT_BIT, PRINT_CHAR, PRINT_NONE
#define PRINT_NONE

//Timers (to emulate multithreading)
IntervalTimer sensor_readTimer; 
IntervalTimer positionTimer;

const int left_pin = A0;
const int mid_pin = A1;
const int right_pin = A2;

ADC *adc = new ADC(); 

//PWM Values:
float frequency;
float dutyCycle;

Teensy_PWM* stepper; //Stepper object

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
receiver_state rcv_state = LOST;

void setup() {
  pinMode(left_pin, INPUT);
  pinMode(mid_pin, INPUT);
  pinMode(right_pin, INPUT);

  Serial.begin(115200);

  ///// ADC0 ////
  adc->adc0->setAveraging(4); // set number of averages
  adc->adc0->setResolution(10); // set bits of resolution
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // change the conversion speed
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED); // change the sampling speed

  ///// ADC1 ////
  adc->adc1->setAveraging(4); // set number of averages
  adc->adc1->setResolution(10); // set bits of resolution
  adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // change the conversion speed
  adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED); // change the sampling speed

  sensor_readTimer.priority(0); //Set sens  or priority higher
  positionTimer.priority(1);
  
  sensor_readTimer.begin(read_sensors,(int)sampling_period); //Read senors every 0.01 seconds
  //positionTimer.begin(find_position,100000); //Process every 0.1 seconds

  //// PWM ////
  pinMode(DIR_PIN, OUTPUT);
  #if USING_FLEX_TIMERS
    Serial.print(F("\nStarting PWM_StepperControl using FlexTimers on "));
  #else
    Serial.print(F("\nStarting PWM_StepperControl using QuadTimers on "));
  #endif

  stepper = new Teensy_PWM(STEP_PIN, 500, 0);
  /////

}

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
RingBuf<photodiode_array, 500> adcBuffer; // for raw ADC outputs + other metrics
RingBuf<photodiode_array, 500> bitBuffer; // extra buffer before message parsing
RingBuf<int, 500> messageBuffer; // buffer for collecting bits for message parsing

CRC8 crc;

// Counters
int lostLockCount = 0;
int messageByteCount = 0;
int missingStartCounter = 0;

// Received Message
int receivedByte = 0;
int crcByte = 0;
volatile int crc_index;
volatile int crccode;

// Flags
volatile bool bufferOverflow = 0;

// Performance Calcs
volatile int start = 0;
volatile int end = 0;

void read_sensors() { //Read analog sensor input
  start = micros();
  struct photodiode_array dataset = {0}; 

  //difference variables
  int edge_dif;
  int left_mid_dif;
  int mid_right_dif;

  dataset.left = adc->adc0->analogRead(left_pin);
  dataset.mid = adc->adc1->analogRead(mid_pin);
  dataset.right = adc->adc0->analogRead(right_pin);

  if(dataset.left <= SENSOR_ON_THRESH || dataset.right <= SENSOR_ON_THRESH \
   || dataset.mid <= SENSOR_ON_THRESH){
    dataset.bit = true;
    dataset.bit_confidence = true;
  }else if(dataset.left >= SENSOR_OFF_THRESH && dataset.right >= SENSOR_OFF_THRESH \
    && dataset.mid >= SENSOR_OFF_THRESH) {
    dataset.bit = false;
    dataset.bit_confidence = true;
  }else{
    dataset.bit = true;
    dataset.bit_confidence = false;
  }

  edge_dif = dataset.left - dataset.right;
  left_mid_dif = dataset.left - dataset.mid;
  mid_right_dif = dataset.mid - dataset.right;


  if(dataset.left > 1000 && dataset.mid > 1000 && dataset.right > 1000){
    dataset.position = UNKNOWN;
  }else{

    if(abs(edge_dif - left_mid_dif) < SENSOR_DIF_THRESH){ //If left of diodes
      dataset.position = LEFT;
    }

    if(edge_dif < 0 && mid_right_dif < 0){ //If between mid and left diodes
      dataset.position = MID_LEFT;
    }

    if(abs(edge_dif) < abs(left_mid_dif) && abs(edge_dif) < abs(mid_right_dif)){ //iF on mid diode
      dataset.position = MID;
    }

    if(edge_dif > 0 && left_mid_dif > 0){  //If between mid and right diodes
      dataset.position = MID_RIGHT;
    }

    if(abs(edge_dif - mid_right_dif) < SENSOR_DIF_THRESH && dataset.right < dataset.left){ //If right of diodes
      dataset.position = RIGHT;
    }
  }

  if(!adcBuffer.push(dataset)){
    Serial.println("ERROR: Can't push to buffer");
  }

  end = micros();
}

void find_position(){ //Determine position of transmitter
//   //Difference calculations
//   edge_dif = left - right;
//   left_mid_dif = left - mid;
//   mid_right_dif = mid - right;

//   if(left > SENSOR_THRESH && mid > SENSOR_THRESH && right > SENSOR_THRESH){ //If no good signal
//     track_state = SCANNING; 
//   }else{
//     track_state = LOCKED;
//     if(abs(edge_dif - left_mid_dif) < SENSOR_DIF_THRESH){ //If left of diodes
//       pos_state = LEFT;
//     }

//     if(edge_dif < 0 && mid_right_dif < 0){ //If between mid and left diodes
//       pos_state = MID_LEFT;
//     }

//     if(abs(edge_dif) < abs(left_mid_dif) && abs(edge_dif) < abs(mid_right_dif)){ //iF on mid diode
//       pos_state = MID;
//     }

//     if(edge_dif > 0 && left_mid_dif > 0){  //If between mid and right diodes
//       pos_state = MID_RIGHT;
//     }

//     if(abs(edge_dif - mid_right_dif) < SENSOR_DIF_THRESH && right < left){ //If right of diodes
//       pos_state = RIGHT;
//     }
//   }

  //Print states
  // if(track_state == LOCKED){
  //   switch (pos_state) {
  //     case LEFT:
  //       Serial.println("LEFT");
  //       break;
  //     case MID_LEFT:
  //       Serial.println("MID_LEFT");
  //       break;
  //     case MID:
  //       Serial.println("MID");
  //       break;
  //     case MID_RIGHT:
  //       Serial.println("MID_RIGHT");
  //       break;
  //     case RIGHT:
  //       Serial.println("RIGHT");
  //       break;
  //     default:
  //       Serial.println("Unknown state");
  //       break;
  //   }
  // }else{
  //   Serial.println("Scanning");
  // }
  // Serial.print(left);
  // Serial.print(" | ");
  // Serial.print(mid);
  // Serial.print(" | ");
  // Serial.println(right);
  // LEFT: -x -x 0
  // MIDL - ? -
  // MID - + -
  // MIDR + + ?
  // RIGHT: + 0 +


  // Serial.print(edge_dif);
  // Serial.print(" | ");
  // Serial.print(left_mid_dif);
  // Serial.print(" | ");
  // Serial.println(mid_right_dif);
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

  switch (input.position) {
    case LEFT:
      Serial.println("LEFT");
      break;
    case MID_LEFT:
      Serial.println("MID_LEFT");
      break;
    case MID:
      Serial.println("MID");
      break;
    case MID_RIGHT:
      Serial.println("MID_RIGHT");
      break;
    case RIGHT:
      Serial.println("RIGHT");
      break;
    default:
      Serial.println("UNKNOWN");
      break;
  }
}

int bitShift = 0;
void messageParse(){
  struct photodiode_array adcValues;
  int bitCount = 0;
  int receivedBit = 0;

  // Double check that buffer is >5
  if(bitBuffer.size() < 5){
    return;
  }

  // If we are in a "LOST" state... try and find presence of a signal...
  // A presence of a signal is 5 '1's or IR LED ON 
  if(rcv_state == LOST){
    for(int i = 0; i < 5; i++){
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
    if(bitCount >= 4){
      Serial.println("Sync");
      rcv_state = SYNCH;
      bitCount = 0;
    }else{
      bitBuffer.pop(adcValues);
    }
  }else if(rcv_state == SYNCH){
    // Begin counting bits
    for(int i = 0; i < 5; i++){
      bitBuffer.pop(adcValues);
      bitCount += adcValues.bit;
    }

    // Do a majority vote
    // Anything inbetween is ambiguous, and we will exit SYNCH
    if(bitCount >= 4){
      messageBuffer.push(1);
    }else if(bitCount <= 1){
      messageBuffer.push(0);
    }else{
      bitCount = 0;
      rcv_state = LOST;
      Serial.println();
      Serial.println("Ambiguous bits; returning to LOST...");
      lostLockCount++;
      messageBuffer.clear();
      missingStartCounter = 0;
    }

    // Parse the bits for the message
    if (messageBuffer.size() > 6){
      if(msg_state == START) {
        missingStartCounter++;
        // If we dont' find the start within 1 second, then we exit SYNCH
        if(missingStartCounter > SAMPLING_FREQ){
          Serial.println("Can't find start bit...");
          messageBuffer.clear();
          missingStartCounter = 0;
          rcv_state = LOST;
          Serial.println("Receiver can't find start bit; returning to LOST...");
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
          Serial.print(messageByteCount);
          Serial.print(": ");
          Serial.print((char)receivedByte);
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
          }else{
            Serial.println();
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
}

void setSpeed(int speed)
{
  if (speed == 0)
  {
    // Use DC = 0 to stop stepper
    stepper->setPWM(STEP_PIN, 500, 0);
  }
  else
  {
    //  Set the frequency of the PWM output and a duty cycle of 50%
    digitalWrite(DIR_PIN, (speed < 0));
    stepper->setPWM(STEP_PIN, abs(speed), 50);
  }
}

volatile position_state previous_position = MID;
int current_speed = 0;

void pwm_control(struct photodiode_array input){
  static int prev_speed;
  // static int left_speed;
  // static int right_speed;

  
  switch (input.position) {
    case LEFT:
      if(previous_position == LEFT){
        if(current_speed <= -3000){
          current_speed = -3000;
        }else{
          current_speed -= 100;
        }
      }else{
        current_speed = -1000;
      }
      break;
    case MID_LEFT:
      if(previous_position == LEFT){
        if(current_speed >= -500){
          current_speed = -500;
        }else{
          current_speed += 200;
        }
      }else{
        current_speed = -500;
      }
      break;
    case MID:
      current_speed = 0;
      break;
    case MID_RIGHT:
      if(previous_position == RIGHT){
        if(current_speed <= 500){
          current_speed = 500;
        }else{
          current_speed -= 200;
        }
      }else{
        current_speed = 500;
      }
      break;
    case RIGHT:
      if(previous_position == RIGHT){
        if(current_speed >= 3000){
          current_speed = 3000;
        }else{
          current_speed += 100;
        }
      }else{
        current_speed = 1000;
      }
      break;
    default:
      current_speed = 0;
      break;
  }

  previous_position = input.position;

  setSpeed(current_speed);
  prev_speed = current_speed;

  Serial.println(current_speed);
}


int movement_counter = 0;

void loop() {
  struct photodiode_array adcValues;

  if(bufferOverflow){
    Serial.println("ERROR: RingBuffer Overflow");
  }

  // Attempt to pop ADC values
  if(adcBuffer.pop(adcValues)){
    movement_counter++;
    if(movement_counter == MOVEMENT_FREQ){
      pwm_control(adcValues);
      movement_counter = 0;
    }

    #ifdef PRINT_SAMPLE
      print_photodiode_struct(adcValues);
    #endif

    #ifndef PRINT_SAMPLE
    // Pushing ADC bits to a buffer for messages
    if(!bitBuffer.push(adcValues)){
      Serial.println("ERROR: Can't push to bitBuffer");
    }
    // Parse the bit for a message
    if(bitBuffer.size() > 5){
      messageParse();
    }
    #endif
    //Serial.println(current_bit);
    // message_buf[i] = current_bit;
    // i++;
  }

  // if(i == 1024){
  //   sensor_readTimer.end();
  //   adcBuffer.clear();
  //   //char ascii_string[128];
  //   //int ascii_index = 0;
  //   for(int j = 0; j < 1024; j++){
  //     Serial.print(message_buf[j]);
  //   }
  //   // for(int j = 7; j < 1024; j = j + 8){
  //   //   //char temp_byte;
  //   //   int temp_sum = 0;
  //   //   int multiplier = 0;
  //   //   for(int k = j; k < j + 7; k++){
  //   //     temp_sum += message_buf[k] * (1 << multiplier++);
  //   //   }
  //   //   ascii_string[ascii_index] = (char)temp_sum;
  //   //   ascii_index++;
  //   // }

  //   // for(int f = 0; f < 128; f++){
  //   //     if(ascii_string[f] == 0){
  //   //       Serial.print("NULL");
  //   //     }else{
  //   //       Serial.print(ascii_string[f]);
  //   //     }
  //   // }
  //   Serial.println();
  //   delay(10000);
  //   sensor_readTimer.begin(read_sensors,(int)sampling_period);
  // }

  //edge_difference = val0 - val2;
  //Serial.println(edge_difference);
  //if(edge_difference > 0){
  //  Serial.println("Move left!");
  //}else if(edge_difference < 0){
  //  Serial.println("Move right!");
  //}else{
  //  Serial.println("Stay still!");
  //}
}
