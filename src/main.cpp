#include <Arduino.h>

// Code for grip exoskeleton PCB. Reads voltage at sensor inputs, if certain thresholds are reached, change Valve configuration

// The PCB has 10 sensor ports for option of using both FSR's and flex Sensors (A and B)
// Sensor 1 is the highest port on the left of the ESP
struct sensor{
  int pin;      // pin def
  bool active;  //is it in use? 
};

sensor Flex[]= {
  // 5 flex Sensors
  {39,false},
  {35,false},
  {33,false},
  {26,false},
  {14,false}
};

sensor FSR[]{
  // 5 FSRs
  {36,false},
  {34,false},
  {32,false},
  {25,false},
  {27,false}
};

struct valve{
  int pinA;     //motor 1 pin
  int pinB;     //motor 2 pin
  bool active;  //is it in use? 
};

valve Valves[] {
  {2,15,true},
  {16,4,true},
  {5,17,true},
  {19,18,false},
  {3,21,false},  
};


struct flex_values{
  int current_value;
  int prev_value;
  int difference;
};

flex_values Flex_Values[5]{
};


sensor active_flex[5];
int activeFlex = 0;

sensor active_fsr[5];
int activeFSR = 0;

valve active_valves[5];
int activeValves = 0;

void setup() {

for (int i=0; i<5; i++){
  if(Flex[i].active){
    active_flex[activeFlex] = Flex[i];
    activeFlex++;
  }
}

for (int i=0; i<5; i++){
  if(FSR[i].active){
    active_fsr[activeFSR] = FSR[i];
    activeFSR++;
  }
}

for (int i=0; i<5; i++){
  if(Valves[i].active){
    active_valves[activeValves] = Valves[i];
    activeValves++;
  }
}

  Serial.begin(9600);



// Set pin modes for active sensors only
for (int i = 0; i < activeFlex; i++) {
  pinMode(active_flex[i].pin, INPUT);
}

for (int i = 0; i < activeFSR; i++) {
  pinMode(active_fsr[i].pin, INPUT);
}

  // Set pin modes for active valves only
for (int i = 0; i < activeValves; i++) {
  pinMode(active_valves[i].pinA, INPUT);
  pinMode(active_valves[i].pinB, INPUT);
}

}

void loop() {
//Each loop cycle, read all active Sensors, check against change conditions
// For flex sensor, check change against previous value, if change greater/less than some +-preset then inflate/deflate muscle, else hold
// For FSR, if value is low then deflate muscle, if it is above a middle threshold then hold, and if above highest threshold inflate
  for (int i = 0; i < activeFlex; i++){
    Flex_Values[i].current_value = analogRead(active_flex[i].pin);
    Flex_Values[i].difference = Flex_Values[i].current_value - Flex_Values[i].prev_value;
    if(Flex_Values[i].difference > 0.1){
      digitalWrite(active_valves[i].pinA, HIGH);
      digitalWrite(active_valves[i].pinB, LOW);
    }

    if(Flex_Values[i].difference < -0.1){
      digitalWrite(active_valves[i].pinA, LOW);
      digitalWrite(active_valves[i].pinB, HIGH);
    }

    else{
      digitalWrite(active_valves[i].pinA, LOW);
      digitalWrite(active_valves[i].pinB, LOW);
    }

  }
}