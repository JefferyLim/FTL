//Threshold values
#include <ADC.h>
#include <RingBuf.h>
#include <IntervalTimer.h>

#define SENSOR_THRESH 1000
#define SENSOR_DIF_THRESH 100
#define SENSOR_HIGH_THRESH 3000 //1023 max per person

#define SAMPLING_FREQ 5000 // Hz
// Convert Sampling Frequency (Hz) to period (us)
double sampling_period = 1000000/SAMPLING_FREQ; // us

#define SAMPLING_RATIO 10 // SAMPLING_FREQ/TRANSMIT_FREQ

//PRINT_SAMPLE, PRINT_RAW, PRINT_BIT, PRINT_CHAR, PRINT_NONE
#define PRINT_CHAR 

//Timers (to emulate multithreading)
IntervalTimer sensor_readTimer; 
IntervalTimer posistionTimer;

const int left_pin = A0;
const int mid_pin = A1;
const int right_pin = A2;

ADC *adc = new ADC(); 

//States
enum tracking_state{
  SCANNING,
  LOCKED,
};

enum posistion_state{
  LEFT, MID_LEFT, MID, MID_RIGHT, RIGHT, UNKNOWN,
};

enum receiver_state{
  SYNCH,
  LOST,
};

enum message_state{
  START,
  MESSAGE,
  END,
};

volatile int counter = 0;
//Sensor variables
uint16_t left;
uint16_t mid;
uint16_t right;


const int irPin = 2;            // the pin with IR diode


//State variables
tracking_state track_state = SCANNING;
posistion_state pos_state;
message_state msg_state = START;
receiver_state rcv_state = LOST;
int last_x_samples;

void setup() {
  
  pinMode(irPin, OUTPUT);
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
  posistionTimer.priority(1);
  
  sensor_readTimer.begin(read_sensors,(int)sampling_period); //Read senors every 0.01 seconds
  //posistionTimer.begin(find_posistion,100000); //Process every 0.1 seconds
}

// Created photodiode array struct
struct photodiode_array{
  uint16_t left;
  uint16_t mid;
  uint16_t right;
  uint8_t bit : 1;
  uint8_t bit_unknown : 1;
  uint8_t : 3;
  uint8_t position : 3;
  
};

// RingBuffer for ADC sampling
RingBuf<photodiode_array, 500> myRingBuffer;
volatile bool bufferOverflow = 0;

volatile int start = 0;
volatile int end = 0;

void read_sensors() { //Read analog sensor input
  start = micros();
  digitalWriteFast(irPin, HIGH);
  struct photodiode_array dataset = {0};

  //difference variables
  int edge_dif;
  int left_mid_dif;
  int mid_right_dif;

  dataset.left = adc->adc0->analogRead(left_pin);
  dataset.mid = adc->adc0->analogRead(mid_pin);
  dataset.right = adc->adc0->analogRead(right_pin);

  //if(dataset.left + dataset.right + dataset.mid <= SENSOR_HIGH_THRESH){
  if(dataset.left <= 400 || dataset.right <= 400 || dataset.mid <= 400){
    dataset.bit = true;
    dataset.bit_unknown = false;
  }else if(dataset.left >= 1000 && dataset.right >= 1000 && dataset.mid >= 1000) {
    dataset.bit = false;
    dataset.bit_unknown = false;
  }else{
    dataset.bit = true;
    dataset.bit_unknown = true;
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

    if(abs(edge_dif - mid_right_dif) < SENSOR_DIF_THRESH && right < left){ //If right of diodes
      dataset.position = RIGHT;
    }
  }

  if(!myRingBuffer.push(dataset)){
    Serial.println("ERROR: Can't push to buffer");
    bufferOverflow = 1;
  }
  end = micros();
  digitalWriteFast(irPin, LOW);
  
}

void find_posistion(){ //Determine posistion of transmitter
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

bool message_buf[1024];
int i = 0;
int last_counter = 0;


unsigned int process_start = 0;
unsigned int process_end = 0;

RingBuf<photodiode_array, 500> bitBuffer;
RingBuf<int, 500> messageBuffer;

int lost_lock_count = 0;

int received_bit = 0;
int received_byte = 0;

int missing_start = 0;


void print_photodiode_struct(struct photodiode_array input){

  Serial.print(input.left);
  Serial.print(" | ");
  Serial.print(input.mid);
  Serial.print(" | ");
  Serial.print(input.right );
  Serial.print(" | ");
  Serial.print(input.bit );
  Serial.print(" | ");
  Serial.print(input.bit_unknown );
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
      Serial.println("Unknown state");
      break;
  }
}

volatile int unknown_count = 0;
int bitShift = 0;
int messageByteCount = 0;
void messageParse(){
  struct photodiode_array adcValues;

  int messageSum = 0;
  int bitCount = 0;

  // Double check that buffer is >5
  if(bitBuffer.size() < 5){
    return;
  }

  // If we are in a "LOST" state... try and find presence of a signal...
  if(rcv_state == LOST){
    for(int i = 0; i < 5; i++){
      bitBuffer.peek(adcValues, i);
      // Look for 5 '1's
      if(adcValues.bit_unknown == 0){
        bitCount += adcValues.bit;
      }else{
        // if we see a single bit not confident, exit
        bitCount = 0;
        break;
      }
    }
  
    // If we find at least 4 '1' samples, lets start parsing
    if(bitCount >= 4){
      Serial.println("Sync");
      rcv_state = SYNCH;
      bitCount = 0;
    }else{
      bitBuffer.pop(adcValues);
    }
  }else if(rcv_state == SYNCH){
    // Begin counting 
    for(int i = 0; i < 5; i++){
      bitBuffer.pop(adcValues);
      messageSum += adcValues.bit;
    }

    if(messageSum >= 3){
      messageBuffer.push(1);
    }else if(messageSum <= 2){
      messageBuffer.push(0);
    }else{
      rcv_state = LOST;
      Serial.println("Ambiguous bits; returning to LOST...");
      lost_lock_count++;
      messageBuffer.clear();
      missing_start = 0;
    }

    // Parse messageBuffer
    // IDLE
    if (messageBuffer.size() > 6){
      if(msg_state == START) {
        missing_start++;
        if(missing_start > SAMPLING_FREQ){
          Serial.println("Can't find start bit...");
          messageBuffer.clear();
          missing_start = 0;
          rcv_state = LOST;
          Serial.println("Receiver can't find start bit; returning to LOST...");
        }else{
          bitCount = 0;
          // Look for a 111110
          for(int i = 0; i < 6; i++){
            messageBuffer.peek(received_bit, i);
            if(i != 5 && received_bit == 0){
              break;
            }
            if(i == 5 && received_bit == 0){
              bitCount = 5;
            }
          }

          // 5 1's + 1 0's means we found the start bit
          if(bitCount == 5){
            msg_state = MESSAGE;
            for(bitCount = 5; bitCount >= 0; bitCount--){
              messageBuffer.pop(received_bit);
              #ifdef PRINT_BIT
              Serial.print(received_bit);
              #endif
            }
            bitCount = 0;
            bitShift = 0;
            received_byte = 0;
            missing_start = 0;
            #ifdef PRINT_BIT
              Serial.print("|");
            #endif
          }else{
            messageBuffer.pop(received_bit);
            #ifdef PRINT_BIT
            Serial.print(received_bit);
            #endif
          }
        }
      // found start
      }else if (msg_state == MESSAGE){
        messageBuffer.pop(received_bit);
        #ifdef PRINT_BIT
        Serial.print(received_bit);
        #endif
        received_byte |= (received_bit) << (bitShift);
        //store tmp
        
        bitShift++;
        if(bitShift >= 8){
          msg_state = END;
          bitShift = 0;
        }

      // grabbed message
      }else if (msg_state == END){
        messageByteCount++;
        messageBuffer.peek(received_bit, 0);
        if(received_bit == 0){
          Serial.println("Found extra 0...");
        }
        msg_state = START;
        #ifdef PRINT_CHAR
        Serial.print(messageByteCount);
        Serial.print(": ");
        Serial.println((char)received_byte);
        #endif
        #ifdef PRINT_BIT
          Serial.println("/");
        #endif
      }
    }
  }
}

void loop() {
  struct photodiode_array adcValues;

  if(bufferOverflow){
    Serial.println("ERROR: RingBuffer Overflow");
  }

  // Attempt to pop ADC values
  if(myRingBuffer.pop(adcValues)){
    left = adcValues.left;
    right = adcValues.right;
    mid = adcValues.mid;

    #ifdef PRINT_SAMPLE
      print_photodiode_struct(adcValues);
    #endif


    #ifndef PRINT_SAMPLE
    // Pushing ADC bits to a buffer for messages
    if(!bitBuffer.push(adcValues)){
      Serial.println("ERROR: Can't push to bitBuffer");
    }
    // Parse message
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
  //   myRingBuffer.clear();
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
