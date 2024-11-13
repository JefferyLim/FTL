//Threshold values
#define sensor_thresh 1000
#define sensor_dif_thresh 100

//Timers (to emulate multithreading)
IntervalTimer sensor_readTimer; 
IntervalTimer process_resultsTimer;

//States
enum tracking_state{
  SCANNING,
  LOCKED,
};

enum posistion_state{
  LEFT, MID_LEFT, MID, MID_RIGHT, RIGHT,
};

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


void setup() {
  Serial.begin(9600);
  sensor_readTimer.priority(0); //Set sensor priority higher
  process_resultsTimer.priority(1);
  sensor_readTimer.begin(read_sensors, 10000); //Read senors every 0.01 seconds
  process_resultsTimer.begin(process_results,100000); //Process every 0.1 seconds
}

void read_sensors() { //Read analog sensor input
  left = analogRead(A10);
  mid = analogRead(A11);
  right = analogRead(A12);
}

void process_results(){ //Determine posistion of transmitter
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

void loop() {
  //Print states
  if(track_state == LOCKED){
    switch (pos_state) {
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
  }else{
    Serial.println("Scanning");
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
