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

valve Valves[] = {
  {2,  15, 0, 1, true},
  {16,  4, 2, 3, false},
  {5,  17, 4, 5, false},
  {19, 18, 6, 7, false},
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
    ledcSetup(active_valves[i].channelA, 100, 8);
    ledcAttachPin(active_valves[i].pinA, active_valves[i].channelA);
    ledcSetup(active_valves[i].channelB, 100, 8);
    ledcAttachPin(active_valves[i].pinB, active_valves[i].channelB);
  }
}

void loop() {
//Each loop cycle, read all active Sensors, check against change conditions
// For flex sensor, check change against previous value, if change greater/less than some +-preset then inflate/deflate muscle, else hold
// For FSR, if value is low then deflate muscle, if it is above a middle threshold then hold, and if above highest threshold inflate
  for (int i = 0; i < activeFlex; i++){
    int current = analogRead(active_flex[i].pin);
    push_value(Flex_Values[i], current);
    int avg = get_average(Flex_Values[i]);
    Flex_Values[i].difference = current - avg;
    // calculate pwm duty cycle by mapping expected values (100 - 1000, change as needed) to 0-255 (boundaries accepted by ledc)
    int duty = constrain(map(abs(Flex_Values[i].difference), 100, 1000, 50, 255), 0, 255); 
    Serial.print("Sensor "); Serial.print(i);
    Serial.print("  raw="); Serial.print(current);
    Serial.print("  avg="); Serial.print(avg);
    Serial.print("  diff="); Serial.println(Flex_Values[i].difference);


    if(Flex_Values[i].difference < -100){
      ledcWrite(active_valves[i].channelA, duty);   // Send PWM signal instead of pure acive high/low
      digitalWrite(active_valves[i].pinB, LOW);
      Serial.println("           INFLATING");
    }

    else if(Flex_Values[i].difference > 100){
      digitalWrite(active_valves[i].pinA, LOW);
      ledcWrite(active_valves[i].channelB, duty);
      Serial.println("           DEFLATING");
    }
    else{
      digitalWrite(active_valves[i].pinA, LOW);
      digitalWrite(active_valves[i].pinB, LOW);
      Serial.println("           HOLDING");
    }
  }
  delay(200);
}