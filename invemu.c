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
    state->cc.z = state->cc.s = state->cc.p = state->cc.cy = state->cc.ac = state->a = state->b = state->c = state->d = state->e = state->h = state->l = state->sp = state->int_enable = state->cyclecount = 0;

    //Allocate 64K. 8K is used for the ROM, 8K for the RAM (of which 7K is VRAM). Processor has an address width of 16 bits however, so 2^16 = 65536 possible addresses.
    state->memory = malloc(sizeof(uint8_t) * 0x10000);
    while (i < 0x10000){
        state->memory[i] = 0;
        i++;
    }

    if (cpmflag) state->memory[0x05] = 0xD3; state->memory[0x06] = 0x01; state->memory[0x07] = 0xC9; //Set OUT 1. Return after OS call (CP/M diagnostics only).

    //Load ROM file(s) into memory.
    RAMoffset = LoadFile(state->memory);

    //Create window
    SDL_Window *window = SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 896, 1024, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *Game = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 256, 224); //Use Stream for real emulation.

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
    //while (state->pc != 0){ //For CPU diagnostics.
    while (i < 500000000){
        //CPU Diagnostics
        if (cpmflag && state->pc == 5){
            int z = 0;
            if (state->c == 2){
                printf("%c", state->e);
            }
            else if (state->c == 9){
                while ((state->memory[((state->d << 8) | state->e) + z] != 0x24)){
                    printf("%c", state->memory[((state->d << 8) | state->e) + z]);
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

        stop = clock(); //Check the current clock tick count. This divided by CLOCKS_PER_SEC is the amount of clock ticks elapsed in a second.
        //printf("start: %ld, stop: %ld, stop - start = %ld, clocks/sec = %f\n", start, stop, stop - start, (float) (stop - start) / CLOCKS_PER_SEC);

        while (state->cyclecount >= 33333 && state->int_enable){
            //printf("cycle count = %d\n", state->cyclecount);
            start = clock(); //Restart tick counter.
            if (((float) (start - stop) / CLOCKS_PER_SEC) > 1.0/60.0){ //Compare the current clock tick count (stop) with the amount of ticks since the last refresh (which was at "start", so stop - start). (stop - start) divided by CLOCKS_PER_SEC equals time elapsed since last refresh.

                //Interrupt 1.
                Restart(state, 0x8); //Equivalent to RST 1 (middle of screen).
                state->pc++;
                //Run instructions in interrupt until return.
                while (state->pc != 0x87){
                    Emulate8080Op(state, output);
                    i++;
                }
                Emulate8080Op(state, output);
                i++;

                //Render(state, window, renderer, Game); //Render top half (implement later).

                //Interrupt 2.
                Restart(state, 0x10); //Equivalent to RST 2 (bottom of screen).
                state->pc++; //Increment pc like any other instruction.
                while (state->pc != 0x87){
                    Emulate8080Op(state, output);
                    i++;
                }
                Emulate8080Op(state, output);
                i++;

                Render(state, window, renderer, Game);

                state->cyclecount = 0;
            }
        }

        i++;
    }
    Render(state, window, renderer, Game);

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
    SDL_DestroyTexture(Game);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}
