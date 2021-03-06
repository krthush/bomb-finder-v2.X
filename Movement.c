#include <xc.h>
#include "dc_motor.h"
#define _XTAL_FREQ 8000000
#include "IR_Reading.h"
#include "Movement.h"
#include "LCD.h"

void initTimer(void){
    // Timer setup
    T0CONbits.TMR0ON=0; // Turn off timer0.
    T0CONbits.T016BIT=1; // 8-bit mode.
    T0CONbits.T0CS=0; // Use internal clock (Fosc/4).
    T0CONbits.PSA=0; // Enable prescaler.
    T0CONbits.T0PS=0b010; // 1:8 Prescale.
    // With 8 MHz clock this gives the clock incrementing 980 times every 
    // second.
    
    INTCONbits.TMR0IE=1; // Enable interrupt on overflow.
}

// Function to delay in seconds
// __delay_ms() is limited to a maximum delay of 89ms with an 8Mhz
// clock so you need to write a function to make longer delays.
void delay_s(char seconds) {
    unsigned int i=0;
    for (i=1; i<=seconds*20; i++) {
        // Repeat 50ms delay 20 times to match no. of seconds
        __delay_ms(50);
    }
}

// Function similar to delay in seconds, but for a 1/10th of a second
void delay_tenth_s(char tenth_seconds) {
    unsigned int i=0;
    for (i=1; i<=tenth_seconds*2; i++) {
        // Repeat 50ms delay 20 times to match no. of tenth seconds
        __delay_ms(50);
    }
}

// Simple search routine that compares the signal strength of the left IR and
// right IR readers to figure out the direction of the IR beacon.
// The drone moves while scanning until there is a change in which case
// it switches to ScanWithRange through the main loop.
char ScanIR(struct DC_motor *mL, struct DC_motor *mR){
    
    // Initialize variable that is used to judge the strength of signals
    unsigned int SensorResult[2]={0,0};
    char buf[40]; // Buffer for characters for LCD.
    // USERVARIABLE TOLERANCES
    const unsigned int DirectionMoveThreshold=500; // Minimum signal strength 
    // required for both sensors to be considered directly aimed at beacon
    // while moving.
    
    // Scan Data
    SensorResult[0]=grabLeftIR();
    SensorResult[1]=grabRightIR();

    // Reset the timers to avoid same reading being picked up if there is
    // no signal. (basically problem with 100% duty cycle PWM)
    CAP1BUFH=0;
    CAP1BUFL=0;
    CAP2BUFH=0;
    CAP2BUFL=0;       

    // Output signal strength to LCD
    SendLCD(0b00000001,0); // Clear Display.
    __delay_us(50); // Delay to let display clearing finish.
    SendLCD(0b00000010,0); // Move cursor to home.
    __delay_ms(2);
    SetLine(1); // Set Line 1.
    LCD_String("     ScanIR");
    SetLine(2); // Set Line 2, for signal strength readings.
    sprintf(buf,"     %04d, %04d",SensorResult[0],SensorResult[1]);
    LCD_String(buf);
    
    // If there is significant signal from both sensors keep going
    // Average of signals - tolerance vs. threshold.
    if ((SensorResult[0]>DirectionMoveThreshold)&&(SensorResult[1]>
            DirectionMoveThreshold)) {
        return 2; // Direction of bomb is roughly directly ahead.
    } else {
        stop(mL,mR);
        return 0; // No clear signal found.
    }
}

// Scans a range turning RIGHT for a number of loops.
// Begins by flicking to the left so that it is more likely to pick up the
// beacon if it has just passed it.
// It compares the IR readings from both sensors: if both pick up signal it
// finds midpoint, otherwise it uses just one of the signals as a guess.
// Once accurate direction is found, function will return DirectionFound=2, 
// which switches the program to move mode.
// If no accurate signal is found it spins in an adjusted manoeuver such that
// the robot spins out a expanding spiral path
//  - making sure the signal is always found.
char ScanWithRange(struct DC_motor *mL, struct DC_motor *mR, int loops,
        int *MoveTime, char *Move, char *MoveType, char *RFID_Read, 
        unsigned int *millis) {
    
    // Initialize variables that is used to judge the strength of signals
    unsigned int SensorResult[2]={0,0}; // Left and right sensor values.
    unsigned int LeftFlag=0; // Millis value when left sensor first reads.
    unsigned int RightFlag=0; // Millis value when right sensor first reads.
    char buf[40]; // Buffer for characters for LCD.
    unsigned int i=0; // Counter.
    unsigned int TimeAboveThreshold=0;
    // USERVARIABLE TOLERANCES
    const unsigned int DirectionFoundThreshold=500; // Minimum signal strength .
    // required for sensor to be considered directly aimed at beacon.
    const unsigned char MotorPower=40; // Adjusts the speed of the turning,
    // currently seems to lose clarity past 43ish.
    const signed char LeftFlick=2; // The flick before scanning is started.
    // Value should ideally be as low as possible for speed, but low values
    // are less robust as the robot can miss the beacon if it's overshot to
    // the left.
    const unsigned char MiniLeftFlick=1; // The flick before movement if only
    // one signal is picked up. Value should tuned to match the most common
    // amount of overlap.
    
    // Flip left before starting scan from right side, this increases chance of
    // bot quickly finding the beacon if it just missed it
    (MoveType[*Move]) = 2;
    (MoveTime[*Move]) = LeftFlick;
    *Move = *Move+1;
    turnLeft(mL,mR, 100);
    delay_tenth_s(LeftFlick);
    stop(mL,mR);
    
    // Turn right slowly for scanning
    turnRight(mL,mR, MotorPower);
    
    // Initialize Timer0 vales to 0
    TMR0L = 0;
    TMR0H = 0;
    *millis = 0;
    T0CONbits.TMR0ON=1; // Turn on timer0.
    // This loop is turning Right, while scanning
    for (i=1; i<=loops; i++) {
        
        // Scan Data
        SensorResult[0]=grabLeftIR();
        SensorResult[1]=grabRightIR();

        // Reset the timers to avoid same reading being picked up if there is
        // no signal.
        CAP1BUFH=0;
        CAP1BUFL=0;
        CAP2BUFH=0;
        CAP2BUFL=0;
        
        // Output signal strength to LCD - this can be left out for increase in
        // efficiency but at lack of clarity when testing.
        SendLCD(0b00000001,0); // Clear Display.
        __delay_us(50); // Delay to let display clearing finish.
        SendLCD(0b00000010,0); // Move cursor to home.
        __delay_ms(2);
        SetLine(1); // Set Line 1.
        LCD_String("     ScanIR");
        SetLine(2); // Set Line 2, for signal strength readings.
        sprintf(buf,"     %04d, %04d",SensorResult[0],SensorResult[1]);
        LCD_String(buf);
        
        // Obtain millis values when right/left IR sensors have seen beacon
        if (SensorResult[1]>DirectionFoundThreshold) {
            RightFlag=*millis;
        }
        if (SensorResult[0]>DirectionFoundThreshold) {
            LeftFlag=*millis;
        }
        
        // If left sensor has seen beacon
        if (LeftFlag>0) {  
            // Both Sensors have seen the beacon, travel back to
            // half the length of the FlagCounter and go!
            if (RightFlag>0) {
                TimeAboveThreshold=LeftFlag-RightFlag;
                TMR0L = 0; //Reset the timer
                TMR0H = 0;
                *millis = 0;
                stop(mL,mR);
                while (*millis<(TimeAboveThreshold>>1)) {
                    // Keep turning left until it's gone for half the time 
                    // between the two readings
                    turnLeft(mL,mR, MotorPower);
                }
                T0CONbits.TMR0ON=0; // Stop the timer
                stop(mL,mR);
                //Let's record the net time spent turning left 
                (MoveType[*Move]) = 1;
                (MoveTime[*Move]) = -(RightFlag + (TimeAboveThreshold>>1));
                *Move = *Move+1;
                
                return 2; // Direction of bomb is directly ahead
            // Only one sensor has seen the beacon, guess direction with that    
            } else {
                T0CONbits.TMR0ON=0; // Stop the timer
                
                // Signal was only found once
                // Record movement of turn up to now then stop
                (MoveType[*Move]) = 1;
                (MoveTime[*Move]) = -(*millis);
                *Move = *Move+1;  
                stop(mL,mR);
                
                // Small flick to adjust rough movement slightly + recording of
                // it.
                turnLeft(mL,mR,100);
                delay_tenth_s(MiniLeftFlick);
                stop(mL,mR);
                (MoveType[*Move]) = 2;
                (MoveTime[*Move]) = MiniLeftFlick;
                *Move = *Move+1; 

                // Just go in that direction roughly
                return 2; // Direction of bomb is roughly ahead, so switch to
                // move forward mode.
            }     
        } 
        // Break out of loop if RFID read or button pressed (Global variables!)
        if (*RFID_Read==1) { // If we've got something on the RFID.
            return 2; // This sends it into mode 2 (Move Forward Mode).
        }
    }
    
    // No clear signal found, rotate and move a bit and hope to find it!
    // This is what causes the spiral and make the robot definitely find the 
    // signal.
    (MoveType[*Move]) = 2;
    (MoveTime[*Move]) = -2;
    *Move = *Move+1;
    turnRight(mL,mR, 100);
    delay_tenth_s(LeftFlick);
    stop(mL,mR);
    return -1; // No clear signal found. Go to lost mode!
}