
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
                                                                and then opens the solenoid to release sucrose reward from the reservoir. 
                                                                if <duration> is specified, then this will dictate the time the solenoid remains open, 
                                                                in milliseconds. Otherwise a default burn time will be used.
                                             
    lever_state <lever_number>  -  <state>,<timestamp>      -   Each connected lever will be assigned a number, referenced  by ‘lever_number’. 
                                                                Responds with ‘0’ if the selected lever is not pressed, otherwise responds ‘1’. 
                                                                Response is followed by timestamp (seconds since program began running).
  
    lever_out <lever_number>    -  extending <lever_number>,<timestamp>   - Responds with a timestamp (seconds since program began running), 
                                                                            and then signals to extend the appropriate lever. 
  
    lever_in <lever_number>     -  retracting <lever_number>,<timestamp>  - Responds with a timestamp (seconds since program began running),
                                                                            and then signals to retract the appropriate lever. 
  
    get_speed <fan_number>          -  <fan_rpm>,<timestamp>              - Responds with the current fan speed in RPM,
                                                                            followed by a timestamp (seconds since program began running).

    set_speed <fan_speed>       -  changing speed,<timestamp>             - Responds with a timestamp (seconds since the program began running),
                                                                            and then signals to change the fan speed to <fan_speed>.
                                                                            <fan_speed> is a value between 80 (1000 RPM) and 255 (2800RPM).
                                                                            To disable fan, set <fan_speed> = 0.  To enable, set <fan_speed> non-zero 
                                                            
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

extern volatile boolean lev0pressed = false;
extern volatile boolean lev1pressed = false;
extern volatile boolean beamBroken = false;

//instantiate fan
Fan fan = Fan(22);

//instantiate levers
Lever levers[] = {Lever(7,8,9,2,3), Lever(10,5,6,48,50)};

//instantiate eye
Instrument eye = Instrument(5);

//instantiate Liquid Reward 
LiquidReward reward = LiquidReward(4);

//const int houseLight = 

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

void setup() {
    // initialize serial:
    Serial.begin(9600);     //Port for commands/confirmation
    // reserve 200 bytes for the inputString:
    inputString.reserve(200);
}

void loop() {
    // run command when a newline arrives:
    if (stringComplete) {
        run(inputString);
        // clear the string:
        inputString = "";
        stringComplete = false;
    }

    if (lev0pressed) { //activates on moving L0, sometimes
        delay(500);
        Serial.println("L0 pressed," + String(millis()));
        lev0pressed = false;
    }

    if (lev1pressed) {
        delay(500);
        Serial.println("L1 pressed," + String(millis()));
        lev1pressed = false;
    }

    if (beamBroken) { //activates on retracting L1
        Serial.println("Beam broken," + String(millis()));
        delay(500);
        beamBroken = false;
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

// return the substring  of data split by seperator
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

void run(String command) {

    // isolate function
    String f = getValue(command, ' ', 0);
  
    // isolate paramater
    int param = getValue(command, ' ', 1).toInt();
  
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
        Serial.println(String(levers[param].getState()) + "," + String(millis()));
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

    if (f == "time\n") {
        Serial.println(String(millis()));
    }

    if (f == "debug\n") {
        // debug code here
        Serial.println("this is a test," + String(millis()));
    }
}
