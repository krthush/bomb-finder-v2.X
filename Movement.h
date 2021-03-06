/* 
 * File:   Movement.h
 * Author: tr514
 *
 * Created on 13 March 2018, 16:31
 */

#ifndef MOVEMENT_H
#define	MOVEMENT_H

// Function to initialise timer0 registers for ScanWithRange.
void initTimer(void);

// Function that delays for given no. of seconds.
void delay_s(char seconds);

// Function similar to delay in seconds, but for a 1/10th of a second.
void delay_tenth_s(char tenth_seconds);

// Scans left, ahead and right for IR signal, turns to signal of greater strength
// This is all done while keeping previous motion action.
char ScanIR(struct DC_motor *mL, struct DC_motor *mR);

// Scans a range for a number of loops.
// Once accurate direction is found, function will return DirectionFound=2, 
// which switches the program to move mode.
char ScanWithRange(struct DC_motor *mL, struct DC_motor *mR, int milliseconds,
        int *MoveTime, char *Move, char *MoveType, char *RFID_Read, 
        unsigned int *millis);

#endif	/* MOVEMENT_H */

