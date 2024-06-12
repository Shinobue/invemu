#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "8080Emulator.h"
#include "InvadersMachine.h"

int main(){

    int RAMoffset;
    int fileoutputflag = 1;
    FILE *output;
    int i = 0;

    //Init State8080 and program counter.
    State8080 mystate;
    State8080 *state = &mystate;

    state->pc = 0;

    //Set all registers and condition codes to 0.
    state->cc.z = state->cc.s = state->cc.p = state->cc.cy = state->cc.ac = state->a = state->b = state->c = state->d = state->e = state->h = state->l = state->sp = 0;

    //Allocate 64K. 8K is used for the ROM, 8K for the RAM (of which 7K is VRAM). Processor has an address width of 16 bits however, so 2^16 = 65536 possible addresses.
    state->memory = malloc(sizeof(uint8_t) * 0x10000);

    //Load ROM file(s) into memory.
    RAMoffset = LoadFile(state->memory);

    while (i < 32){
        printf("%02x ", state->memory[i]);
        i += 1;
    }
    printf("\nRAMoffset = %d\n\n\n", RAMoffset);

    i = 0;
    printf(" Ins#   pc   op  mnem   byte(s)\n");

    //Create output file if applicable.
    if (fileoutputflag){
        output = fopen("output.txt", "w");
        fprintf(output, " Ins#   pc   op  mnem   byte(s)\n"); //Also print to file if flag enabled.
    }
    while (i < 1000){
        //Print instruction count.
        printf("%6d ", i);
        if (fileoutputflag){fprintf(output, "%6d ", i);} //Also print to file.

        uint8_t *opcode = &state->memory[state->pc];

        if (*opcode == 0xdb){ //Machine-specific handling for IN.
            uint8_t port = opcode[1];
            state->a = MachineIN(state, port);
            state->pc++;
        }
        else if (*opcode == 0xd3){ //OUT.
            uint8_t port = opcode[1];
            MachineOUT(state, port);
            state->pc++;
        }
        else{
            Emulate8080Op(state, fileoutputflag, output);
        }
        i += 1;
    }


    free(state->memory);

}
