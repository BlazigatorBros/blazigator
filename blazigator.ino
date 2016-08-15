/*
    Blazigator main sketch file
 
 reads commands over serial port, and exicutes tasks accordingly
 
 **************************
 *Input/Output explination*
 **************************
 
 ->RX                        -  <-TX                     -  Explanation
 
 dose <duration>             -  dosing,<timestamp>       -   The arduino responds with a timestamp (seconds since program began running), 
 and then carries out the burn/delivery sequence. If a <duration> is specified, 
 then this will dictate the burn time, in milliseconds. 
 Otherwise a default burn time will be used.
 
 disp <duration>             -  dispensing,<timestamp>   -   The arduino responds with a timestamp (seconds since program began running),
 and then activates the pump to release sucrose reward from the reservoir. 
 if <num_steps> is specified, then this will dictate the number of steps the pump with turn.
 Otherwise a default amount will be used.
 
 lever_state <lever_number>  -  <state>,<timestamp>      -   Each connected lever will be assigned a number, referenced  by ‘lever_number’. 
 Responds with ‘0’ if the selected lever is not pressed, otherwise responds ‘1’. 
 Response is followed by timestamp (seconds since program began running).
 
 lever_out <lever_numberttyextending <lever_number>,<timestamp>   - Responds with a timestamp (seconds since program began running), 
 and then signals to extend the appropriate lever. 
 
 lever_in <lever_number>     -  retracting <lever_number>,<timestamp>  - Responds with a timestamp (seconds since program began running),
 and then signals to retract the appropriate lever. 
 
 get_speed <fan_number>          -  <fan_rpm>,<timestamp>              - Responds with the current fan speed in RPM,
 followed by a timestamp (seconds since program began running).
 
 set_speed <fan_speed>       -  changing speed,<timestamp>             - Responds with a timestamp (seconds since the program began running),
 and then signals to change the fan speed to <fan_speed>.
 <fan_speed> is a value between 80 (1000 RPM) and 255 (2800RPM).
 :q
 :w                              To disable fan, set <fan_speed> = 0.  To enable, set <fan_speed> non-zero 
 
 time                        - <timestamp>               -   Returns a timestamp
 
 debug                       -  <debug information>      -   This should output any and all useful information for development and debugging of the system.
 
 ***********
 *Interupts*
 ***********
 Several conditions will use an interupt signal. Each of these events has a corisponding serial output
 Module                        -  <-TX                                    - Explanation
 Lever                         -  L<lever_number> pressed, <timestamp>    - A lever has been pressed
 Eye                           -  Beam broken, <timestamp>                - The IR breakbeam has been broked
 Lickometer                    -  Lick Detected, <timestamp>              - A lick has been detected
 Smoke Despencer               -  Dose Queue Empty, <timestamp>           - There are no more doses avalable in the chute
 */
#include <Instrument.h>
#include <Lever.h>
#include <Fan.h>
#include <LiquidReward.h>
#include <SmokeEmitter.h>
#include <Dootdoot.h>
#include <SPI.h>

//pins
#define LEVER_0_STATE_PIN 		20
#define LEVER_1_STATE_PIN 		19
#define LEVER_0_IN_LIMIT_PIN 	26
#define LEVER_1_IN_LIMIT_PIN 	40
#define LEVER_0_OUT_LIMIT_PIN   24
#define LEVER_1_OUT_LIMIT_PIN   38
#define SLAVE_SEL_PIN 			53
#define EYE_PIN 				3
#define LIQUID_REWARD_PIN 		A0
#define LICKOMETER_PIN 			2
#define HOUSELIGHT 				47
#define DOOT 					46

#define LEVER_0_OUT_CMD         0x0789
#define LEVER_1_OUT_CMD         0x0783
#define LEVER_0_IN_CMD			0x0791
#define LEVER_1_IN_CMD			0x0785

extern volatile byte lever_0_pressed = false;
extern volatile byte lever_1_pressed = false;
extern volatile byte break_beam_pressed = false;
extern volatile byte lickometer_pressed = false;

void lever0_isr() {
    lever_0_pressed = true;
}

void lever1_isr() {
    lever_1_pressed = true;
}

void beamBroken_isr() {
  if(break_beam_pressed == false){
    break_beam_pressed = true;
    Serial.println("Beam broken," + String(millis()));
  }
}

void lickDetected_isr() {
  if(lickometer_pressed == false){
    lickometer_pressed = true;
    Serial.println("Lick triggered," + String(millis()));
  }
}

//instantiate eye and licko
Instrument eye = Instrument(EYE_PIN, beamBroken_isr);
Instrument lickometer = Instrument(LICKOMETER_PIN, lickDetected_isr);

//instantiate fan
//Fan fan = Fan(22);

//instantiate levers
Lever levers[] = {
    Lever(LEVER_0_STATE_PIN,
        LEVER_0_IN_LIMIT_PIN,
        LEVER_0_OUT_LIMIT_PIN,
        LEVER_0_OUT_CMD,
        LEVER_0_IN_CMD,
        SLAVE_SEL_PIN,
        lever0_isr),

    Lever(LEVER_1_STATE_PIN,
      	LEVER_1_IN_LIMIT_PIN,
      	LEVER_1_OUT_LIMIT_PIN,
      	LEVER_1_OUT_CMD,
     	LEVER_1_IN_CMD,
      	SLAVE_SEL_PIN,
      	lever1_isr)
};    



//instantiate Liquid Reward 
LiquidReward reward = LiquidReward(200,SLAVE_SEL_PIN);

//instatniate doot doot
Dootdoot doot = Dootdoot(DOOT, 1500);

//420$wag$wag
SmokeEmitter smoker = SmokeEmitter(&Serial3);

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

void setup() {
  // initialize serial:
  Serial.begin(9600);     //Port for commands/confirmation
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
  
  SPI.begin();
  digitalWrite(SLAVE_SEL_PIN, HIGH);

  // levers[0].init();
  // levers[1].init();
  //houselight
  pinMode(HOUSELIGHT, OUTPUT);
  digitalWrite(HOUSELIGHT, LOW);
}

void loop() {
  // run command when a newline arrives:
  if (stringComplete) {
    run(inputString);

    // clear the string:
    inputString = "";
    stringComplete = false;
  }

  //check interupt flags
  if (lever_0_pressed && levers[0].getState(100)) {
    Serial.println("L0 pressed," + String(millis()));
    lever_0_pressed = false;
  }

  if (lever_1_pressed && levers[1].getState(100)) {
    Serial.println("L1 pressed," + String(millis()));
    lever_1_pressed = false;
  }

  if (break_beam_pressed && !eye.getState(100)) {
    delay(500);
    break_beam_pressed = false;
  }

  if (lickometer_pressed && !lickometer.getState(100)) {
    delay(100);
    lickometer_pressed = false;
  }

}

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read(); 
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

void serialEvent3() {

  int s = smoker.checkStatus();
  //poll the smoke emitter
  switch(s) {
    case S_WAIT:
      return;
    case S_ERROR:
      Serial.println("ERROR_SMOKER_EMPTY");
      break; 
  }

  String m = smoker.getLastMessage();
  m.replace('\n',',');
  Serial.println(m + String(millis()));
}

//Return the substring  of data split by seperator
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++) {
    if(data.charAt(i)==separator || i==maxIndex) {
      found++;
      strIndex[0] = strIndex[1]+1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

//
void run(String command) {

  // isolate function
  String f = getValue(command, ' ', 0);

  // isolate paramater
  int param = getValue(command, ' ', 1).toInt();

  if(f == "houselight") {
    if (param) {
      digitalWrite(HOUSELIGHT, HIGH);
      Serial.println("houselight on," + String(millis()));
    } else {
      digitalWrite(HOUSELIGHT, LOW);
      Serial.println("houselight off," + String(millis()));
    }
  }

  if (f == "set_doot") {
    Serial.println("Setting doot," + String(millis()));
    doot.setFreq(param);
  }

  if (f == "doot") {
    Serial.println("Dooting," + String(millis()));
    doot.doot(param);
  }

  if (f == "doses") {
    Serial.println("dosing," + String(millis()));
  } 

  if (f == "disp") {
    // dispense liquid reward
    Serial.println("dispensing," + String(millis()));
    reward.dispense(param);
  }

  if (f == "lever_state") {
    // find and send state of appropriate lever
    Serial.println(String(levers[param].getState(100)) + "," + String(millis()));
  }

  if (f == "lever_out") {
    // send out lever
    Serial.println("extending " + String(param) + "," + String(millis()));
    levers[param].setCarriage(false);
  } 

  if (f == "lever_in") {
    // retract lever
    Serial.println("retracting " + String(param)  + "," + String(millis()));
    levers[param].setCarriage(true);
  }
/*
  if (f == "get_speed\n") {
    Serial.println(String(fan.getSpeed()) + "," + String(millis()));
  }

  if (f == "set_speed") {

    if (!param) {
      Serial.println("turning off," + String(millis()));
      fan.fanOff();
    } 
    else if (param) {
      if(fan.getSpeed()) {
        fan.fanOn();
      }
      Serial.println("changing speed," + String(millis()));
      fan.setSpeed(param);
    }
  }
*/
  if (f == "burn\n" || f == "load\n" || f == "empty\n") {
    smoker.sendCommand(f);
  }

  if (f == "time\n") {
    Serial.println(String(millis()));
  }

  if (f == "debug\n") {
    // debug code here
    Serial.println("this is a test," + String(millis()));
  }

  if (f == "victory\n") {
    victory();
  }

  if (f == "test") {
    Serial.println("returned," + String(test(param), HEX));
  }
}

void playTone(int tone, int duration) {
  for (long i = 0; i < duration * 1000L; i += tone * 2) {
    digitalWrite(DOOT, HIGH);
    delayMicroseconds(tone);
    digitalWrite(DOOT, LOW);
    delayMicroseconds(tone);
  }
}

void victory() {
  int tones[] = {1915, 1700, 1519, 1432, 2700};
  int notes[] = {2, 2, 2, 2, 0, 1, 2, 1, 2};
  int note = 0;
  int tempo[] = {200, 200, 200, 400, 400, 400, 200, 200, 600}; 
  int breaks[] = {80, 80, 80, 220, 200, 200, 300, 80, 220}; 
  for (int i = 0; i < 9; i = i + 1){
    note = notes[i];
    playTone(tones[note]/2, tempo[i]);
    delay(breaks[i] / 3);
  }
}

uint16_t test(uint16_t stuff) {
    return reward.SPIcmd(stuff);
}