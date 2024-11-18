//Threshold values
#include <ADC.h>
#include <RingBuf.h>
#include <IntervalTimer.h>

#define sensor_thresh 1000
#define sensor_dif_thresh 100

#define baud_rate 1000
double sensor_period = 1000000/baud_rate;

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
  LEFT, MID_LEFT, MID, MID_RIGHT, RIGHT,
};

enum message_state{
  SYNCH, 
};


volatile bool just_read = false;
volatile int counter = 0;
//Sensor variables
int left;
int mid;
int right;
//difference variables
int edge_dif;
int left_mid_dif;
int mid_right_dif;
//State variables
tracking_state track_state = SCANNING;
posistion_state pos_state;

int last_x_samples;


void setup() {
  pinMode(left_pin, INPUT);
  pinMode(mid_pin, INPUT);
  pinMode(right_pin, INPUT);

  Serial.begin(115200);

  ///// ADC0 ////
  adc->adc0->setAveraging(0); // set number of averages
  adc->adc0->setResolution(10); // set bits of resolution
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // change the conversion speed
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED); // change the sampling speed

  ///// ADC1 ////
  adc->adc1->setAveraging(0); // set number of averages
  adc->adc1->setResolution(10); // set bits of resolution
  adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // change the conversion speed
  adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED); // change the sampling speed


  sensor_readTimer.priority(0); //Set sens  or priority higher
  posistionTimer.priority(1);
  
  sensor_readTimer.begin(read_sensors,(int)sensor_period); //Read senors every 0.01 seconds
  //posistionTimer.begin(find_posistion,100000); //Process every 0.1 seconds
}

struct photodiode_read{
  int left;
  int mid;
  int right;
};

RingBuf<photodiode_read, 500> myRingBuffer;
struct photodiode_read dataset;

void read_sensors() { //Read analog sensor input
  dataset.left = adc->adc0->analogRead(left_pin);
  dataset.mid = adc->adc1->analogRead(mid_pin);
  dataset.right = adc->adc0->analogRead(right_pin);
  if(!myRingBuffer.push(dataset)){
    Serial.println("ERROR: Can't push to buffer");
  }

  counter++;
}

void find_posistion(){ //Determine posistion of transmitter
  //Difference calculations
  edge_dif = left - right;
  left_mid_dif = left - mid;
  mid_right_dif = mid - right;

  if(left > sensor_thresh && mid > sensor_thresh && right > sensor_thresh){ //If no good signal
    track_state = SCANNING; 
  }else{
    track_state = LOCKED;
    if(abs(edge_dif - left_mid_dif) < sensor_dif_thresh){ //If left of diodes
      pos_state = LEFT;
    }

    if(edge_dif < 0 && mid_right_dif < 0){ //If between mid and left diodes
      pos_state = MID_LEFT;
    }

    if(abs(edge_dif) < abs(left_mid_dif) && abs(edge_dif) < abs(mid_right_dif)){ //iF on mid diode
      pos_state = MID;
    }

    if(edge_dif > 0 && left_mid_dif > 0){  //If between mid and right diodes
      pos_state = MID_RIGHT;
    }

    if(abs(edge_dif - mid_right_dif) < sensor_dif_thresh && right < left){ //If right of diodes
      pos_state = RIGHT;
    }
  }

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

void loop() {
  bool current_bit = false;
  struct photodiode_read readdata;

  if(myRingBuffer.pop(readdata)){
    left = readdata.left;
    right = readdata.right;
    mid = readdata.mid;

    if(last_counter != counter - 1){
      //Serial.println("ERROR");
    }
    last_counter = counter;

    if(left + right + mid <= 3000){
      current_bit = true;
    }else{
      current_bit = false;
    }
    //Serial.println(current_bit);
    message_buf[i] = current_bit;
    i++;
    just_read = false;
  }

  if(i == 1024){
    sensor_readTimer.end();
    myRingBuffer.clear();
    //char ascii_string[128];
    //int ascii_index = 0;
    for(int j = 0; j < 1024; j++){
      Serial.print(message_buf[j]);
    }
    // for(int j = 7; j < 1024; j = j + 8){
    //   //char temp_byte;
    //   int temp_sum = 0;
    //   int multiplier = 0;
    //   for(int k = j; k < j + 7; k++){
    //     temp_sum += message_buf[k] * (1 << multiplier++);
    //   }
    //   ascii_string[ascii_index] = (char)temp_sum;
    //   ascii_index++;
    // }

    // for(int f = 0; f < 128; f++){
    //     if(ascii_string[f] == 0){
    //       Serial.print("NULL");
    //     }else{
    //       Serial.print(ascii_string[f]);
    //     }
    // }
    Serial.println();
    delay(10000);
    sensor_readTimer.begin(read_sensors,(int)sensor_period);
  }

  //edge_difference = val0 - val2;
  //Serial.println(edge_difference);
  //if(edge_difference > 0){
  //  Serial.println("Move left!");
  //}else if(edge_difference < 0){
  //  Serial.println("Move right!");
  //}else{
  //  Serial.println("Stay still!");
  //}

  delay(1);

}
