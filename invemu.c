#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "8080Emulator.h"
#include "InvadersMachine.h"
#include <SDL.h>

const int fileoutputflag = 0;
const int printflag = 0;
const int cpmflag = 0;

int main(int argc, char *argv[]){
    int RAMoffset;
    FILE *output;
    int i = 0;
    clock_t start, stop;
    start = clock(); //Start counting clock ticks.

    //Init SDL.
    SDL_Init(SDL_INIT_EVERYTHING);

    //Init State8080 and program counter.
    State8080 mystate;
    State8080 *state = &mystate;

    state->pc = 0;
    if (cpmflag == 1) state->pc = 0x100; //For CP/M cpu diagnostics.

    //Set all registers and condition codes to 0.
    state->cc.z = state->cc.s = state->cc.p = state->cc.cy = state->cc.ac = state->a = state->b = state->c = state->d = state->e = state->h = state->l = state->sp = 0;

    //Allocate 64K. 8K is used for the ROM, 8K for the RAM (of which 7K is VRAM). Processor has an address width of 16 bits however, so 2^16 = 65536 possible addresses.
    state->memory = malloc(sizeof(uint8_t) * 0x10000);

    if (cpmflag) state->memory[0x05] = 0xC9; //Return after OS call (CP/M diagnostics only).

    //Load ROM file(s) into memory.
    RAMoffset = LoadFile(state->memory);

    //Create window
    SDL_Window *window = SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 896, 1024, SDL_WINDOW_SHOWN);

    //Temporary, print info to cmd.
    while (i < 32){
        printf("%02x ", state->memory[i]);
        i += 1;
    }
    printf("\nRAMoffset = %d\n\n\n", RAMoffset);

    i = 0;

    if (printflag){printf(" Ins#   pc   op  mnem   byte(s)\n");}

    //Create output file if applicable.
    if (fileoutputflag){
        output = fopen("output.txt", "w");
        fprintf(output, " Ins#   pc   op  mnem   byte(s)\n"); //Also print to file if flag enabled.
    }
    while (i < 500000){
        //CPU Diagnostics
        if (cpmflag && state->pc == 5){
            int z = 0;
            if (state->c == 2){
                printf("%c\n", state->e);
            }
            else if (state->c == 9){
                while ((state->memory[((state->d << 8) | state->e) + z] != 0x24)){
                    printf("%c", state->memory[((state->d << 8) | state->e) + z], state->memory[((state->d << 8) | state->e) + z]);
                    z++;
                }
                printf("\n");
            }
        }

        //Print instruction count.
        if (printflag){printf("%6d ", i);}
        if (fileoutputflag){fprintf(output, "%6d ", i);} //Also print to file.

        //Emulate instruction
        Emulate8080Op(state, output);

        stop = clock(); //Check the current clock tick count. This divided by CLOCKS_PER_SEC is the amount of clock ticks elapsed.
        //printf("start: %ld, stop: %ld, stop - start = %ld, clocks/sec = %f\n", start, stop, stop - start, (float) (stop - start) / CLOCKS_PER_SEC);

        //Refresh the screen every 1/60th of a second (~0.017s).
        if (((float) (stop - start) / CLOCKS_PER_SEC) > 1.0/60.0){ //Compare the current clock tick count (stop) with the amount of ticks since the last refresh (which was at "start", so stop - start). (stop - start) divided by CLOCKS_PER_SEC equals time elapsed since last refresh.
            if (state->int_enable){
                Restart(state, 8 * 1); //Interrupt 1. Equivalent to RST 1 (middle of screen).
                state->pc++;
                //Render(state, window); //Render top half (implement later).

                Restart(state, 8 * 2); //Interrupt 2. Equivalent to RST 2 (bottom of screen).
                state->pc++; //Increment pc like any other instruction.
                Render(state, window);
                //printf("start = %d, stop - start = %f, stop = %d\n", start, (float) (stop - start) / CLOCKS_PER_SEC, stop);
                start = clock(); //Restart tick counter.
            }
        }

        i++;
    }
    Render(state, window);

    //Print VRAM values
//    i = 0x2400;
//    while (i < 0x4000){
//        if (state->memory[i] != 0)
//            printf("x %d, y %d: %X\n", (i - 0x2400) / 32, (i - 0x2400) % 32, state->memory[i]);
//        i++;
//    }

    //Clear memory.
    free(state->memory);

    //Destroy SDL stuff.
    SDL_DestroyWindow(window);
    SDL_Quit();
}
