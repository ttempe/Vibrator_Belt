#include <Wire.h>
#include <LSM303.h>

#define data1Pin 7
#define clkPin 5
#define ledPin 13
#define maxNbVibrators 16
const char off = maxNbVibrators + 1;
const char nbUsefulVibrators = 14;
char currentVibrator = maxNbVibrators + 1;

/////////////////////////////////////////////////////////////////////////////////
LSM303 compass;

void setup() {
  pinMode(clkPin, OUTPUT);
  pinMode(data1Pin, OUTPUT);
  flick(maxNbVibrators, 0);

  Serial.begin(38400);  Wire.begin();
  compass.init();
  compass.enableDefault();
  compass.m_min = (LSM303::vector<int16_t>){-2569, -2656, -2405};  compass.m_max = (LSM303::vector<int16_t>){+2348, +2482, +2538}; // board + aluminium buckle
  compass.writeReg(0x20, 0x67); //CTRL1: accelerometer set to 100 Hz
  compass.writeReg(0x24, 0x74); //CTRL5: magnetometer set to 100 Hz
}

/////////////////////////////////////////////////////////////////////////////////
char outputMap1(char i){
  return i/8*8+(8-i%8);
}

char outputMap(char i){
  i *= 2;
  return i/8*8+(8-i%8);
}

void flick(char count, char turnOn){
  if (turnOn){
    digitalWrite(data1Pin, HIGH);
    delay(.001);
    digitalWrite(clkPin, HIGH);
    delay(.001);
    digitalWrite(clkPin, LOW);
    delay(.001);
    digitalWrite(data1Pin, LOW);
    delay(.001);
    count--;
    currentVibrator=1;
  }
  while(count-- > 0){
    digitalWrite(clkPin, HIGH);
    delay(.001);
    digitalWrite(clkPin, LOW); 
    delay(.001);
    currentVibrator++;
  }
}

void lightOne(char target){
  if (target == currentVibrator){
    //Do nothing 
  } else if (target>currentVibrator){
    flick(target-currentVibrator, 0);
  } else if (target+currentVibrator > maxNbVibrators) {
    flick(target, 1);
  } else {
    flick(maxNbVibrators-target, 0);
    flick(target, 1);
  }
}
/////////////////////////////////////////////////////////////////////////////////
void printHeading(float heading){
    for (int i = int(heading*78/360); i>0; i--) {
    Serial.print(" ");
  }
  Serial.println("*");
}


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
//Loop
void loop(){ loopa();}
/////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
//Normal loop
void loopn() {
  //Serial.print("Reading...");
  compass.read();
  //Serial.print("Done\n");
    
  float heading = compass.heading((LSM303::vector<int>){0, 0, 1});
//  char headingVibrator = char((1-heading/360)*nbUsefulVibrators);
  char headingVibrator = char((heading/360)*nbUsefulVibrators);
  if (millis()%1000 > 150){ //turn on 150 ms every second 
    lightOne(off);
  }else {
    lightOne(outputMap(headingVibrator));
  }
  //Serial.println(heading);
  Serial.print(int(headingVibrator));
  printHeading(heading);
  delay(100);
}


/////////////////////////////////////////////////////////////////////////////////
//Calibrate
void loopc() {  
  static LSM303::vector<int16_t> running_min = {32767, 32767, 32767}, running_max = {-32768, -32768, -32768};
  char report[80]; 
  static int i=0;
  compass.read();
  running_min.x = min(running_min.x, compass.m.x);
  running_min.y = min(running_min.y, compass.m.y);
  running_min.z = min(running_min.z, compass.m.z);

  running_max.x = max(running_max.x, compass.m.x);
  running_max.y = max(running_max.y, compass.m.y);
  running_max.z = max(running_max.z, compass.m.z);
  
  snprintf(report, sizeof(report), "min: {%+6d, %+6d, %+6d}    max: {%+6d, %+6d, %+6d}",
    running_min.x, running_min.y, running_min.z,
    running_max.x, running_max.y, running_max.z);
  snprintf(report, sizeof(report), "%d, %d, %d", compass.m.x, compass.m.y, compass.m.z);
  Serial.println(report);
//  Serial.print(i++);  
  delay(100);
}

/////////////////////////////////////////////////////////////////////////////////
//Loop through all vibrators 

void loopa(){
  for (int i=0; i<nbUsefulVibrators; i++){
    lightOne(outputMap(i));
    digitalWrite(ledPin, i%2);
    delay(200);
  }
}


/////////////////////////////////////////////////////////////////////////////////
//Debugging

//Todo: try to correlate the values found below with physical vibrators. It's strange that we should get such high perturbation with so many vibrators, including possibly some which are not wired.

void loopd12(){
  lightOne(off);
  flick(0, 1);
  delay(1000);
}

//read the magnetic field X axis while flicking the vibrators
//Conclusion:
//There is a clear response, but it's actually coming from multiple vibrators: 1, 2, 3, 10, 11, 12, 25,33, 34, 35, 36, 41.
void loopd11(){ 
  int x=0;
  flick(0, 1);
  for (int v=0; v<off; v++){
    compass.read();
    Serial.print(compass.m.x);
    Serial.print(", ");
    flick(1,0);
    delay(20);
  }  
  Serial.print("\n");
}

//read the accelerometer at high frequency, and determine how hard it's vibrating
//Conclusion:
//Good discrimination when holding lightly in hand (immobile)
//Noise from start of belt when just laying
//When wearing and standing, Too much interference from movement to be useful.
void loopd10(){
  int x=0, i, v,val[20], sum=0, avg, std;
  compass.writeReg(0x20, 0x97); //CTRL1: accelerometer set to 800 Hz
  compass.writeReg(0x24, 0x00); //CTRL5: no magnetometer
  flick(0,1);

  for (v=0; v<off; v++){
    flick(1,0);
    delay(100);
    sum = 0;std=0;
    for (i=0; i<20; i++){
      compass.readAcc();
      val[i]=compass.a.x;
      sum += compass.a.x;
    }
    //calculate average
    avg = sum/20;
    for (i=0; i<20; i++){
      std+=abs(val[i]-avg)/4;
    }
    Serial.print(std);
    Serial.print(", ");
  }  
  Serial.print("\n");
}


//Reset the belt, measure magnetic field X, then activate vibrator V, and re-measure.
//Do this for each vibrator, in turn.
//If the belt is buckled, one should see a spike at corresponding vibrator.
//Conclusion:
//we can see something, but not so clearly...
void loopd9(){
  int x;
  for (int v=0; v<off; v++){
    delay(10);
    lightOne(off);
    compass.read();
    x = compass.m.x;
    lightOne(outputMap(v));
    delay(10);
    compass.read();
    Serial.print(compass.m.x-x);
    Serial.print(", ");
  }
  Serial.print("\n");
}

//read the magnetic field X axis while flicking the vibrators really fast.
//(X is where you see the strongest disturbance from the solenoids)
//Conclusion:
//Can't see clearly which vibrator is active
void loopd8(){
  int x=0;
  for (int v=0; v<off; v++){
    compass.read();
    Serial.print(compass.m.x-x);
    x = compass.m.x;
    Serial.print(", ");
    flick(1,0);
    delay(10);
  }  
  Serial.print("\n");
}

//read the magnetometer in a fast loop, and measure the time before it refreshes
//Conclusion: 
// * at default value: one update every 158 ms (long enough to read the magnetometer 75 times) 
// * at 100Hz nominal refresh: can get a new value every 10ms (long enough to read the magnetometer 4~5 times)
void loopd7(){
      int i, x;
      long int m;
      m = micros();
      x = compass.m.x;
      while (x ==  compass.m.x){
        x = compass.m.x;
        compass.read();
        i++;
      }
      Serial.print(micros()-m);    
      Serial.print(", ");    
      Serial.println(i);    


}


bool checkVibrator(char vibrator){
  //Make sure we're not reading the active vibrator
  if (vibrator==currentVibrator){ lightOne(off);}
  //Wait for the magnetometer to update
  int current = compass.m.x+compass.m.y+compass.m.z;
  while( compass.m.x+compass.m.y+compass.m.z == current){compass.read();};
  //Activate the vibrator we're checking
  delay(100);
  lightOne(vibrator);
  //Then wait until the new reading comes
  int x= compass.m.x;
  current = compass.m.x+compass.m.y+compass.m.z;
  while( compass.m.x+compass.m.y+compass.m.z == current){compass.read();};
  lightOne(off);
  Serial.print("Detected: ");Serial.println(compass.m.x-x);
  return (compass.m.x-x>100); 
}

void loopd6(){
  checkVibrator(outputMap(3));
  delay(500);
  lightOne(outputMap(3));
  delay(500);
}

void loopd5(){
    lightOne(outputMap(3));
    for (int i=0; i<1000; i++){
      compass.readMag();
      Serial.print(micros());Serial.print(" ");Serial.println(compass.m.z);
    }
}

void loopd4(){
  for (int i=0; i<maxNbVibrators; i++){
    compass.read();
    int mag = compass.m.x;
    lightOne(outputMap(i));
    delay(160);
    compass.read();
    int mag2 = compass.m.x;
    if (mag-mag2 > 100){
      Serial.print(mag-mag2); Serial.print(" ");
    }
    lightOne(off);
    delay(40);
  }
  Serial.print(".\n");
}

int intensity(){
  return sqrt(pow(compass.m.x-(compass.m_max.x-compass.m_min.x)/2, 2) + pow(compass.m.y-(compass.m_max.y-compass.m_min.y)/2,2) + pow(compass.m.z-(compass.m_max.z-compass.m_min.z)/2,2));
}

void loopd3(){
  lightOne(outputMap(1));
  for (int i=0; i<1000; i++){
    compass.read();Serial.println(intensity());
    Serial.print(compass.m.x-(compass.m_max.x-compass.m_min.x)/2);Serial.print(", ");
    Serial.print(compass.m.y-(compass.m_max.y-compass.m_min.y)/2);Serial.print(", ");
    Serial.print(compass.m.z-(compass.m_max.z-compass.m_min.z)/2);Serial.print("\n");
    delay(160);
  }
  Serial.println("");
  delay(0);
}


void loopd2(){
  compass.read();printHeading(compass.heading((LSM303::vector<int>){0, 0, 1}));
  lightOne(outputMap(14));
  for (int i=0; i<10; i++){
    compass.read();printHeading(compass.heading((LSM303::vector<int>){0, 0, 1}));
  }
  delay(1000);
  lightOne(outputMap(0));
  delay(1000);
  for (int i=0; i<10; i++){
    compass.read();printHeading(compass.heading((LSM303::vector<int>){0, 0, 1}));
  }
  delay(3000); 
}

