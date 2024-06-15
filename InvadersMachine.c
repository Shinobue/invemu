#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "8080Emulator.h"

uint8_t MachineIN(State8080* state, uint8_t port){

    switch (port){
        case 0:
        port = port | 0x0E; //bit 1-3 are always 1: 0000 1110
        //If firing, set bit 4.
        //If left input, set bit 5.
        //If right input, set bit 6.
        break;

        case 1:
        break;

        case 2:
        break;

        case 3:
        break;
        return port;
    }

    //Goes to A register
    return;
}
uint8_t MachineOUT(State8080* state, uint8_t port){
}
