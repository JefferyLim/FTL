[code]

/*
BIPOLAR STEPPER MOTOR CONTROL WITH EVALUATION BOARD  OF THE STSPIN820 BY STMicroelectronicss
Versione 1.0 del 07/03/2018
AUTORE:  microst
*/

// set pin numbers:
const int EN = 23;     
const int  M0 = 25;      
const int M1 = 27;
const int M2 = 29;
const int STDBY =  33;
const int STEP = 35;
const int DIR = 37;


// variables will  change:

int number = 0;        
int freq = 10;
int swpulse = 0;
int  lettura = 0;
int dur = 0.1;
int mode = 0;
int m0 = 0;
int m1 = 0;
int  m2 = 0;
int dir= 0;
unsigned long tempMillis;  di PWM
byte caratteri; 
byte  tempChar; 


void setup() {
  // pin definitions
  Serial.begin(9600);
  pinMode(STDBY, OUTPUT);
  pinMode(M0, OUTPUT );
  pinMode(M1, OUTPUT );
  pinMode(M2, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(STEP, OUTPUT);
  digitalWrite(STDBY, LOW); //device in stand-by
  pinMode(EN, OUTPUT); digitalWrite(EN,  LOW); // output stage disabled
  digitalWrite(M0, HIGH);digitalWrite(M1, LOW);digitalWrite(M2,  LOW); //initial mode configuration 
  delay (1000);
  // Serial monitor menu
  Serial.println(F(" m = mode")); 
  Serial.println(F(" r = dir anti clockwise  "));
  Serial.println(F(" l = dir clockwise"));
  Serial.println(F(" e  = driver on "));
  Serial.println(F(" o = driver in stand by"));
  Serial.println(F("  s = step"));
  Serial.println(F(" f = frequency or speed"));
  //
}

void  loop() {
 //check serial
  if (Serial.available()) {
    byte command =  Serial.read(); //leggo il primo byte
    switch (command) { //controllo che sia  un comando valido
  
  // enable the device ( out of stand by )
    case  'e': 
          Serial.println(" driver on ");
          digitalWrite(STDBY,  HIGH);
          delay(1);
    break;
  // disable the device - stand by
    case 'o': 
          Serial.println(" driver in stand-by");
          digitalWrite(STDBY,  LOW);
    break;
  // define the cw motor direction
    case 'l': 
          Serial.println(" direction clock wise");
          dir=1;
    break;
  // define the ccw motor direction 
    case 'r': 
          Serial.println("  direction counter clock wise");
          dir=0;
    break;
    case 's':  //step number
           readnumb();
           swpulse=lettura;
           Serial.println(F("step  number  is  "));
           Serial.println(swpulse, DEC);
           Serial.println(F("  motor actived  "));
           stepper(swpulse,m0,m1,m2,dir,dur);
           Serial.println(F("motor  stopped  "));
    break;
    case 'f': // speed number
           readnumb();
           freq=lettura;
           dur= freq/2;
           Serial.print(F("speed  is  "));
           Serial.println(freq, DEC);
    break;
    case 'm':  //mode selection 
           readnumb();
           mode=lettura;
           if(mode>6){mode=6;}
           m0=0;m1=0;m2=0;
           if ( mode==0 ) { Serial.println(F("mode  1/1 step selected"));}
           if ( mode==1 ) { Serial.println(F("mode 1/2  step selected"));m0=1;};
           if ( mode==2 ) { Serial.println(F("mode  1/4 step selected"));m1=1;}
           if ( mode==3 ) { Serial.println(F("mode  1/8 step selected"));m0=1;m1=1;}
           if ( mode==4 ) { Serial.println(F("mode  1/16 step selected"));m2=1;}
           if ( mode==5 ) { Serial.println(F("mode  1/128 step selected"));m2=1;m1=1;}
           if ( mode==6 ) { Serial.println(F("mode  1/256 step selected"));m2=1;m1=1;m0=1;}
    break;
    }
    }
}



//  function stepper for EVALSP820-XS
int stepper (int swpulse, int m0, int m1, int  m2, int dir, int dur){
  if (m0==0) {digitalWrite(M0, LOW);} else {digitalWrite(M0,  HIGH);}
  if (m1==0) {digitalWrite(M1, LOW);} else {digitalWrite(M1, HIGH);}
  if (m2==0) {digitalWrite(M2, LOW);} else {digitalWrite(M2, HIGH);}
  if (dir==0)  {digitalWrite(DIR, LOW);} else {digitalWrite(DIR, HIGH);}
  pinMode(EN, INPUT_PULLUP);  // output stage  on
  delay(10);
  if (digitalRead(EN)==LOW)  {Serial.println(F("  VM UVLO condition: check the VM power supply ")); }
  while (digitalRead(EN)==LOW)  {;}
  // STEP GENERATOR
  for ( int i = 0; i < swpulse; i++) {
    digitalWrite(STEP,  HIGH);
    delay(dur);
    digitalWrite(STEP, LOW); 
    delay(dur);   
  if (digitalRead(EN)==LOW)  {Serial.println(F("fault  ")); i=swpulse; }
  }
  pinMode(EN, OUTPUT);  
  digitalWrite(EN, LOW); // output stage off
  return;
}
  
  int readnumb (){
  lettura = 0; tempMillis = millis(); caratteri = 0;
  do {
      if (Serial.available()) {
               tempChar = Serial.read();
               caratteri++;
               if ((tempChar >= 48) && (tempChar  <= 57)) { //è un numero? ok
                   lettura = (lettura * 10) + (tempChar  - 48);
                   } else if ((tempChar == 10) || (tempChar == 13)) {
                   //con invio e/o nuova linea esco
                   break;
                   }
                }
     } while ((millis() - tempMillis  < 500) && (lettura <= 100) && (caratteri < 10));
          //se il valore di  PWM è valido, lo imposto sul pin
          if (lettura >= 500) {lettura=500;}
  return (lettura);
}
