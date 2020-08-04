//Imports and instance variables below

#include <Wire.h> //Import necessary for I2C communication
#include <Servo.h> //import necessary for servos.

Servo servo1;
Servo servo2;

//VARIABLES:
//**Ones are placeholder. Using metric**
float az;                         //Accelerationin z direction
bool launch = false;              //Tracks if aruduino has detected launch
bool launch_init = false;         //True whenever acceleration exceeds threshold
int launch_az_thresh = 1;         //Minimum acceleration for launch to be detected
int launch_time_thresh = 1;       //Amount of time (ms) acceleration must exceed threshold to detect launch
float launch_time = 0;            //First time acceleration above launch threshold is detected
float burn_timer = 0;             //Measures length of burrn for launch time threshold
bool free_fall = false;           //Tracks if rocket is past burnout
bool free_fall_init = false;      //True whenever negative acceleration exceeds threshold after launch
int free_fall_thresh = 1;         //Minimum negative acceleration for burnout to be detected **should be negative**
int freefall_time_thresh = 1;     //Amount of time (ms) acceleration must exceed threshold to detect burnout
float burnout_time = 0;           //First time negaative acceleration exceeding threshold is detected
float coast_timer = 0;            //Measures length of negative acceleration for free fall time threshold
float roll_rate;                  //Rate of roatation around the z axis
int roll_rate_thresh = 1;         //Minimum roll rate for roll control activation
float alt;                        //Altitude measured from altimeter
int roll_control_cutoff = 1;      //Altitude at which the rocket switches exclusively to active drag
bool apogee = false;              //Defines the end of the coast phase when true
float vel;                        //Velocity data received from the Pi
float des_alt;                    //Desired altitude of the flight, either 2345 ft or 3456 ft
float g = 9.81;                   //Acceleration due to gravity in metres/sec
float native_drag;                //Native drag force, have to see how to calculate this
float flaps_drag;                 //Additional drag force due to the deployed flaps
float m;                          //Mass of the rocket in kilograms
float buffer;                     //Buffer percentage for active drag system
float Kp;                         //Proportional Gain for the roll control
float theta;                      //Angle command to the flaps for roll control
int initialAngle = 90; //The initial angle (vertical position) of the servos. Note that this may not be 90.
int currentAngle;
int analogPin = A3; //analog pin number that will be used for the pitot static system.
int initPressure = 0;
int pressureVal;

void setup() {
  Serial.begin(115200);           //  setup serial (We need a baud rate of 100,000 - 400,000)
  //Defining servo signal inputs. Needs to be in the Digital PWM ports on the arduino (3, 5, 6, or 9 usually).
  servo1.attach(3);
  servo2.attach(5);
  Wire.begin();                   // join i2c bus
  Wire.setClock(120000);          //set the clock (in hertz) to the same as the serial transmission.
  currentAngle = initialAngle;

  //For loops test the functionality of the servo's movements;
  for (currentAngle; currentAngle <= initialAngle + 90; currentAngle++) {
    servo1.write(currentAngle);
    servo2.write(currentAngle);
  }
  for (currentAngle; currentAngle >= initialAngle; currentAngle--) {
    servo1.write(currentAngle);
    servo2.write(currentAngle);
  }
  /**
   * Recieve Analog output from Pitot static system.
   * Good article on setup and averaging the values:
   * https://makersportal.com/blog/2019/02/06/arduino-pitot-tube-wind-speed-theory-and-experiment
   */
  pressureVal = initPressure;

}

void loop() {
  if (Serial.available() > 0) { //serial.available returns the number of bytes in the serial buffer
    byte dataBuffer[16];
    Serial.readBytes(dataBuffer, 16);
    byte az_byte_array[4];
    byte rr_byte_array[4];
    byte alt_byte_array[4];
    byte vel_byte_array[4];
    for(int i = 15; i >= 0; i--){
      if (i < 4) {
        az_byte_array[i % 4] = dataBuffer[i];
      } else if (i < 8) { 
        rr_byte_array[i % 4] = dataBuffer[i];
      } else if (i < 12) {
        alt_byte_array[i % 4] = dataBuffer[i];
      } else {
        vel_byte_array[i % 4] = dataBuffer[i];
      }
    }

    az = *( (float*) az_byte_array ); 
    roll_rate = *( (float*) rr_byte_array );
    alt = *( (float*) alt_byte_array );
    vel = *( (float*) vel_byte_array );
    
    //debug purposes
    //Serial.write(dataBuffer, 16);
    //Serial.println(String(az) + " " + String(roll_rate) + " " + String(alt) + " " + String(vel));
  }


  /**
   *  //gets the value for the pressure. Number is an integer between 0 and 1023
   *  Can go up to as high as 4095 if we convert to 12 bits.
   *  https://www.arduino.cc/reference/en/language/functions/analog-io/analogread/
   */
  pressureVal = analogRead(analogPin); //***Need to change into a 2 byte array buffer.***
  Serial.println("Pressure: " + pressureVal); //for debugging purposes
  
  
  //az must be updated from sensor each time through loop
  
  //LAUNCH DETECTION LOGIC:
  if(az > launch_az_thresh && launch_init == false)     //If high acceleration is observed in z direction...
  {
    launch_time = millis();                             //...assume launch and store launch time
    launch_init = true;
  }
  
  if(az > launch_az_thresh && launch_init == true && free_fall == false)   //If the acceleration continues...
  {
    burn_timer = millis() - launch_time;                //...start measuring the lenght of the burn time
    if(burn_timer > launch_time_thresh)                 //If the acceleration lasts long enough...
    {
      launch = true;                                    //...the launch has occured
      Serial.println(String(millis()/1000) + "s: Launch detected");
    }
  }
  
  else if(az < launch_az_thresh && launch_init == true && free_fall == false)   //If the acceleration was too brief...
  {
    launch_init = false;                                //...reset the launch detection (the acceleration was just an anomaly)
  }

  //BURNOUT DETECTION LOGIC:
  if(az < free_fall_thresh && launch == true && free_fall_init == false)    //If large negative acceleration is observed after launch...
  {
    burnout_time = millis();                            //...assume burnout and store time of burnout
    free_fall_init = true;
  }

  if(az < free_fall_thresh && free_fall_init == true && free_fall == false)   //If the negative acceleration continues...
  {
     coast_timer = millis() - burnout_time;             //...start measuring the lenght of the coast time
     if(coast_timer > freefall_time_thresh)             //If the negative acceleration lasts long enough...  
     {
      free_fall = true;                                 //...burnout has occured, and the rocket is now coasting
      Serial.println(String(millis()/1000) + "s: Burnout detected");
     }
  }

  else if(az > free_fall_thresh && free_fall_init == true && free_fall == false)   //If the negative acceleration was too brief...
  {
    free_fall_init = false;                             //...reset the burnout detection (the acceleration was just an anomaly)
  }
  
  //ROLL CONTROL
  if((roll_rate > roll_rate_thresh || roll_rate < -roll_rate_thresh) && alt < roll_control_cutoff && free_fall == true && apogee == false)    //If the rocket is rolling and is in the coast phase below a certain altitude...
  {
    //...reduce the roll
    //Proportional controller to reduce the roll of the rocket. Kp is the proportional gain.
    roll_err = -roll_rate; //The desired roll rate is 0, so the error in roll rate is -ve roll rate, thus giving the correct direction.
    theta = Kp*roll_err; //Make sure the Kp accounts for radian-degree conversion!!!
    
    myservo1.write(constrain(theta,-15,15)); //offset these for the actual servos angle shift
    myservo1.write(constrain(theta,-15,15)); //offset these for the actual servos angle shift
    
  }
  //ACTIVE DRAG
  else if(free_fall == true && apogee == false)         //If the rocket is in the coast phase...
  {
    //Runs active drag
    //control_vel = f(altitude)
    float control_vel = sqrt(2*(g+(native_drag/m)*(des_alt-alt)); //From v^2 - u^2 = 2*a*s equation. THe final velocity is zero, u is the desired velocity considering flap drag isn't applied throughout
    if (alt > des_alt)
    {
      //deploy flaps
    }
    else {
      if (vel > control_vel*(1+buffer/100))
      {
        //deploy flaps
      }
      else {
        //go to vertical
      }
    } 
  }
}
