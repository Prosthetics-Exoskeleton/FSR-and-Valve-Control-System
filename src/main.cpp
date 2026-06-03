#include <Arduino.h>

// Code for grip exoskeleton PCB v1. Reads voltage at sensor inputs, if certain thresholds are reached, change Valve configuration

// The PCB has 10 sensor ports for option of using both FSR's and flex Sensors 
// Flex sensors to be connected on left side, FSR's on right

#define BUFFER_SIZE 10

struct sensor{
  int pin;      // pin def
  bool active;  //is it in use? 
};



struct flex_values {
  int buffer[BUFFER_SIZE];
  int index;        // current position in buffer
  int count;        // how many values stored so far
  int difference;   
};


struct valve {
  int pinA;  // valve 1
  int pinB;  // valve 2
  int channelA; // valve 1 pwm ledc 
  int channelB; // valve 2 pwm ledc
  bool active;  //is it in use ? For testing mostly
};


sensor Flex[]= {
  // 5 flex Sensors
  {39,true},
  {35,true},
  {33,true},
  {26,true},
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

valve Valves[] = {
  {2,  15, 0, 1, true},
  {16,  4, 2, 3, true},
  {5,  17, 4, 5, true},
  {19, 18, 6, 7, true},
  {3,  21, 8, 9, false},
};

sensor active_flex[5];
int activeFlex = 0;

sensor active_fsr[5];
int activeFSR = 0;

valve active_valves[5];
int activeValves = 0;


flex_values Flex_Values[5];

// This function works as a circular buffer - write new values in next position, and when the buffer is full flip index back to 0 and start overwriting
void push_value(flex_values &f, int new_value) {
  f.buffer[f.index] = new_value;
  f.index = (f.index + 1) % BUFFER_SIZE;  // wrap around
  if (f.count < BUFFER_SIZE) f.count++;
}

int get_average(flex_values &f) {
  if (f.count == 0) return 0;
  long sum = 0;
  for (int i = 0; i < f.count; i++) sum += f.buffer[i];
  return sum / f.count;
}

void setup() {

  Serial.begin(9600);
  // Loop through all sensors, check if active, if they are then move them to active arrays
  for (int i=0; i<5; i++){
    if(Flex[i].active){    active_flex[activeFlex] = Flex[i];    activeFlex++;  Serial.println(Flex[i].pin);}
  }
  for (int i=0; i<5; i++){
    if(FSR[i].active){    active_fsr[activeFSR] = FSR[i];    activeFSR++;  }
  }
  for (int i=0; i<5; i++){
    if(Valves[i].active){    active_valves[activeValves] = Valves[i];    activeValves++;  Serial.println(Valves[i].pinA);}
  }
  // Set pin modes for active sensors only
  for (int i = 0; i < activeFlex; i++) {  pinMode(active_flex[i].pin, INPUT); }
  for (int i = 0; i < activeFSR; i++) {  pinMode(active_fsr[i].pin, INPUT); }
    // Set pin modes for active valves only
  for (int i = 0; i < activeValves; i++) {
    pinMode(active_valves[i].pinA, OUTPUT);
    pinMode(active_valves[i].pinB, OUTPUT);
    ledcSetup(active_valves[i].channelA, 50, 8);
    ledcAttachPin(active_valves[i].pinA, active_valves[i].channelA);
    ledcSetup(active_valves[i].channelB, 50, 8);
    ledcAttachPin(active_valves[i].pinB, active_valves[i].channelB);
  }
}

void loop() {
  // If flex sensor is bent beyond a threshold, slowly inflate the active muscles,
  // hold the grip briefly, then deflate.

  for (int i = 0; i < activeFlex; i++){
    int current = analogRead(active_flex[i].pin);
    
    Serial.print("Sensor ");
    Serial.print(i);
    Serial.print(" raw=");
    Serial.println(current);

    if (current < 500) { //TEST FOR CURRENT VALUE
      Serial.println("FLEX DETECTED: STARTING GRIP DEMO");

      // Slowly inflate
      for (int duty = 0; duty <= 180; duty += 5) {
        for (int j = 0; j < activeValves; j++) {
          ledcWrite(active_valves[j].channelA, duty);  // inflate
          ledcWrite(active_valves[j].channelB, 0);
        }

        Serial.print("Inflating, duty=");
        Serial.println(duty);
        delay(150);
      }

      // Hold grip
      Serial.println("HOLDING GRIP");
      for (int j = 0; j < activeValves; j++) {
        ledcWrite(active_valves[j].channelA, 0);
        ledcWrite(active_valves[j].channelB, 0);
      }

      delay(2000);

      // Deflate
      Serial.println("DEFLATING");
      for (int j = 0; j < activeValves; j++) {
        ledcWrite(active_valves[j].channelA, 0);
        ledcWrite(active_valves[j].channelB, 180);
      }

      delay(2000);

      // Stop all valves
      Serial.println("DEMO COMPLETE");
      for (int j = 0; j < activeValves; j++) {
        ledcWrite(active_valves[j].channelA, 0);
        ledcWrite(active_valves[j].channelB, 0);
      }

      // Wait before allowing another trigger
      delay(3000);
    }
  }
  
  delay(200);
}