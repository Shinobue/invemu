#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "8080Emulator.h"
#include "InvadersMachine.h"
#include <SDL.h>

int LoadFile(uint8_t *);

const int fileoutputflag = 0;
const int printflag = 0;
const int cpmflag = 0;

uint16_t RAMoffset;

int main(int argc, char *argv[]){
    FILE *output;
    int i = 0;
    clock_t start, stop;
    int nextInterrupt = 1;
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

    //For debugging.
    if (printflag){printf(" Ins#   pc   op  mnem   byte(s)\n");}

    //Create output file if applicable.
    if (fileoutputflag){
        output = fopen("output.txt", "w");
        fprintf(output, " Ins#   pc   op  mnem   byte(s)\n"); //Also print to file if flag enabled.
    }

    i = 0;

    //while (state->pc != 0){ //For CPU diagnostics.
    while (1){
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

        //33333 cycles per frame, and screen is updated twice per frame (interrupt 1 and 2), so after half a frame's worth of cycles (16667) have passed, trigger 1st interrupt.
        while (state->cyclecount >= 16667 && state->int_enable && nextInterrupt == 1){
            start = clock(); //Start tick counter. Wait until it's time for the first render (top half of screen).
            //Once 16667 cycles have run, wait until 1/120 of a second has passed. Framerate is 60hz, and you run two interrupts per frame, so that makes 1/120.
            if (((float) (start - stop) / CLOCKS_PER_SEC) > 1.0/120.0){ //Compare the current clock tick count (start) with the amount of ticks since the last refresh (which was at "stop", so start - stop). (start - stop) divided by CLOCKS_PER_SEC equals time elapsed since last refresh.
                //If cyclecount is much higher, set it to 16667 to keep the timing (note: might not be necessary?).
                state->cyclecount = 16667;

                //Interrupt 1.
                Interrupt(state, output, &i, 1);

                Render(state, window, renderer, Game);

                nextInterrupt = 2;
            }
        }
        //Trigger 2nd interrupt.
        while (state->cyclecount >= 33333 && state->int_enable && nextInterrupt == 2){
            start = clock(); //Restart tick counter.
            if (((float) (start - stop) / CLOCKS_PER_SEC) > 1.0/120.0){
                //Reset cycle count. Once 33333 cycles have run, it should reset in order to correctly count the cycles that occur before the next interrupt.
                state->cyclecount = 0;

                //Interrupt 2.
                Interrupt(state, output, &i, 2);

                Render(state, window, renderer, Game);

                nextInterrupt = 1;
            }
        }

        i++;
    }
    Render(state, window, renderer, Game);

    //Clear memory.
    free(state->memory);

    //Destroy SDL stuff.
    SDL_DestroyWindow(window);
    SDL_DestroyTexture(Game);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}

int LoadFile(uint8_t *memory){
    FILE *invaders;
    long int filesize;
    unsigned char *buffer;
    int filecount = 4;
    uint16_t memOffset = 0;

    while (filecount > 0){
        //Processor diagnostics.
        if (cpmflag == 1){

            //cpudiag //Cleared
            //invaders = fopen("Processor diagnostics\\cpudiag\\cpudiag.bin", "rb"); memory += 0x100; //Offset memory by 0x100, since that's where cpudiag starts. Instructions begin there, and the pc should also start there.

            //8080EXER
            //invaders = fopen("Processor diagnostics\\8080EXER\\8080EXER.bin", "rb"); memory += 0x100;

            //8080EXM //Cleared
            //invaders = fopen("Processor diagnostics\\8080EXM\\8080EXM.bin", "rb"); memory += 0x100;

            //8080PRE //Cleared
            //invaders = fopen("Processor diagnostics\\8080PRE\\8080PRE.bin", "rb"); memory += 0x100;

            //CPUTEST //Cleared
            //invaders = fopen("Processor diagnostics\\CPUTEST\\CPUTEST.bin", "rb"); memory += 0x100;

            //TST8080 //Cleared
            //invaders = fopen("Processor diagnostics\\TST8080\\TST8080.bin", "rb"); memory += 0x100;

            filecount = 1;
        }
        //Space invaders.
        else{
            switch (filecount){
                case 4:
                //Open file in read binary mode. "r" by itself would be read text, which stops 0x1B from being read (it gets read as an EOF if the file is opened in text mode).
                invaders = fopen("Place Game ROMs Here\\Invaders.h", "rb");
                if (invaders == NULL){ printf("Error: Invaders.h file not found!\n");}
                break;
                case 3:
                invaders = fopen("Place Game ROMs Here\\Invaders.g", "rb");
                if (invaders == NULL){ printf("Error: Invaders.g file not found!\n");}
                break;
                case 2:
                invaders = fopen("Place Game ROMs Here\\Invaders.f", "rb");
                if (invaders == NULL){ printf("Error: Invaders.f file not found!\n");}
                break;
                case 1:
                invaders = fopen("Place Game ROMs Here\\Invaders.e", "rb");
                if (invaders == NULL){ printf("Error: Invaders.e file not found!\n");}
                break;
            }
        }
        //Load the entire file into memory. Start by finding the end of the file.
        if (fseek(invaders, 0L, SEEK_END) == 0){
            //Get the current position (position at the end of the file). Remember it so that we know how much memory to allocate for storage.
            filesize = ftell(invaders);

            //Error checking.
            if (filesize == -1){ printf("Error: could not create buffer size!\n"); }

            //Go back to the start of the file.
            if (fseek(invaders, 0L, SEEK_SET) != 0){ printf("Error: could not set the file pointer to the start of the file!\n"); }

            //Read the entire file into memory (into the buffer).
            fread(memory + memOffset, sizeof(uint8_t), filesize, invaders);
            memOffset += filesize;
        }
        filecount--;
    }

    fclose(invaders);

    //Address where the RAM starts.
    return memOffset;
}
