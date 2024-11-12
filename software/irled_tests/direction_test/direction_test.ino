#define mid_threshold 30

IntervalTimer sensor_readTimer;
IntervalTimer process_resultsTimer;

enum tracking_state{
  SCANNING,
  LOCKED,
};

enum posistion_state{
  LEFT, MID_LEFT, MID, MID_RIGHT, RIGHT,
};



int left;
int mid;
int right;
int edge_dif;
int left_mid_dif;
int mid_right_dif;
tracking_state track_state;
posistion_state pos_state;


void setup() {
  //pinMode(ledPin, OUTPUT);
  //pinMode(irpin, OUTPUT);
  Serial.begin(9600);
  sensor_readTimer.priority(0);
  process_resultsTimer.priority(1);
  sensor_readTimer.begin(read_sensors, 100000);  // blinkLED to run every 0.15 seconds
  process_resultsTimer.begin(process_results,100000);
}

void read_sensors() {
  left = analogRead(A10);
  mid = analogRead(A11);
  right = analogRead(A12);
  //delay(10);
}

void process_results(){
  edge_dif = left - right;
  left_mid_dif = left - mid;
  mid_right_dif = mid - right;

  // LEFT: -x -x 0
  // MIDL -x ? -x
  // MID - + -
  // MIDR + + ?
  // RIGHT: + 0 +

  if(abs(edge_dif - left_mid_dif) < 100){
    pos_state = LEFT;
  }

  if(edge_dif < 0 && mid_right_dif < 0){
    pos_state = MID_LEFT;
  }

  if(abs(edge_dif) < abs(left_mid_dif) && abs(edge_dif) < abs(mid_right_dif)){
    pos_state = MID;
  }

  if(edge_dif > 0 && left_mid_dif > 0){
    pos_state = MID_RIGHT;
  }

  if(abs(edge_dif - mid_right_dif) < 100){
    pos_state = RIGHT;
  }


  Serial.print(edge_dif);
  Serial.print(" | ");
  Serial.print(left_mid_dif);
  Serial.print(" | ");
  Serial.println(mid_right_dif);
}

// The main program will print the blink count
// to the Arduino Serial Monitor


void loop() {

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
