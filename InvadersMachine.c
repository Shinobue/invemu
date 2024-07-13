#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "8080Emulator.h"
#include <SDL.h>

static uint16_t shiftRegister;
static uint8_t shiftOffset;
static int coin = 1;

void Interrupt(State8080* state, FILE *output, int *instruction, int number){
    clock_t time;

    switch (number){
    case 1:
        //Run an RST instruction, only save the current PC rather than PC + 1, since we want to go back exactly to the instruction we were at.
        state->memory[state->sp - 1 & 0xFFFF] = ((state->pc) >> 8) & 0xff;
        state->memory[state->sp - 2 & 0xFFFF] = (state->pc) & 0xff;
        state->sp -= 2;
        state->pc = 0x8;
        state->cyclecount += 11;

        //Run all the instructions in the interrupt. When pc is 0x87, the interrupt will return. Inserting a coin gets interrupt 2 stuck in an infinite loop, so quit if cycle count gets too high.
        while (state->pc != 0x87 && state->cyclecount < 16667){
            Emulate8080Op(state, output);
            (*instruction)++;
        }
        //Run return instruction.
        Emulate8080Op(state, output);
        (*instruction)++;
        break;
    case 2:
        //Same as case 1, except go to instruction 0x10.
        state->memory[state->sp - 1 & 0xFFFF] = ((state->pc) >> 8) & 0xff;
        state->memory[state->sp - 2 & 0xFFFF] = (state->pc) & 0xff;
        state->sp -= 2;
        state->pc = 0x10;
        state->cyclecount += 11;

        while (state->pc != 0x87 && state->cyclecount < 33333){
            Emulate8080Op(state, output);
            (*instruction)++;
        }
        Emulate8080Op(state, output);
        (*instruction)++;
        break;
    default:
        printf("Error: invalid interrupt!\n");
        break;
    }

    return;
}

uint8_t ProcessorIN(State8080* state, uint8_t port){
    uint8_t processorInput;
    clock_t time;
    time = clock();

    SDL_Event keyPress;

    if (SDL_PollEvent(&keyPress)){
        switch (keyPress.type){
            case SDL_KEYDOWN:
            printf("Test1: %d\n", keyPress.key.keysym.sym);
            break;
            case SDL_KEYUP:
            printf("Test2: %d\n", keyPress.key.keysym.sym);
            break;
        }
    }

    switch (port){
        case 0:
        //port = port | 0x0E; //bit 1-3 are always 1: 0000 1110
        //If firing, set bit 4.
        //If left input, set bit 5.
        //If right input, set bit 6.
        processorInput = 0x0E;
        break;

        case 1:
        /*
        bit 0 = Insert coin
        bit 1 = 2P start
        bit 2 = 1P start
        bit 3 = Always 1
        bit 4 = 1P shot
        bit 5 = 1P left
        bit 6 = 1P right
        bit 7 = not connected
        */
//        if (keyPress.key.keysym.sym == SDLK_c)
        if (coin && (((float) time / CLOCKS_PER_SEC) > 1)){
//            processorInput = 0xD;
//            coin = 0;
//            printf("Test, coinSwitch = %X\n", state->memory[0x20EA]);
//            printflag = 0;
            processorInput = 0x4;
        }
        else{
            processorInput = 0;
        }
//        if ((((float) time / CLOCKS_PER_SEC) > 9)){
//            coin = 0;
//            processorInput = 0xD;
//        }

        break;

        case 2:
        processorInput = 0xB;
        break;

        case 3: //Bit shift register read.
        processorInput = (shiftRegister >> (8 - shiftOffset)) & 0xFF; //Output contents of right byte, after shifting the register by 8 - offset.
        break;
    }

    //Goes to A register (accumulator).
    return processorInput;
}

void ProcessorOUT(State8080* state, uint8_t port){
    switch (port){
        case 2: //Shift amount (3 bits).
        shiftOffset = state->a & 0x07; //Mask out the 3 rightmost bits, since they determine how much to shift by.
        break;
        case 3:
        break;
        case 4: //Shift data.
        shiftRegister >>= 8; //Shift previous input to the right by 8 bits to make room in the left byte for the new data.
        shiftRegister = (state->a << 8) | shiftRegister; //Load new data into leftmost byte.
        break;
        case 5:
        break;
        case 6: //Watch-dog.
        break;
    }
}

void Render(State8080 *state, SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *Game){
    //Memory offset
    int memOffset = 0x2400;
    int vRamSize = 0x1c00;
    int i = 0;
    int byte = 0;
    Uint32 pixels[0x1c00 * 8]; //8 pixels in each byte.

    //Draw white border around screen. For testing only. Comment out when not in use.
//        while (i < 224){
//            state->memory[memOffset + i * 32] = 0x1;
//            i++;
//        }
//        i = 0;
//        while (i < 224){
//            state->memory[memOffset + i * 32 + 31] = 0x80;
//            i++;
//        }
//        i = 0;
//        while (i < 32){
//            state->memory[memOffset + i] = 0xFF;
//            i++;
//        }
//        i = 0;
//        while (i < 32){
//            state->memory[memOffset + 32 * 223 + i] = 0xFF;
//            i++;
//        }
//        i = 0;

    //Convert each byte into a stream of 8 pixels per byte. Space invaders is 1 bit per pixel, but modern GPUs (as of writing, 2024) need 4 bytes per pixel.
    while (byte < vRamSize){
        //Render starting from the least significant bit. Output white if bit is 1, 0 if black. Loop through all 8 bits in each byte.
        if ((state->memory[memOffset + byte] << (7 - (i % 8))) & 0x80){
            pixels[i] = 0xFFFFFFFF; //White
        }
        else{
            pixels[i] = 0x00000000; //Black
        }
        i++;
        //Increment byte when final bit is processed.
        if (i % 8 == 0){
            byte++;
        }
    }


    //Clear screen
    //SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    //SDL_SetRenderTarget(renderer, NULL);
    //SDL_RenderClear(renderer);
    //SDL_RenderPresent(renderer);

    SDL_Rect screen;
    screen.x = 0;
    screen.y = 0;
    screen.w = 512*2;
    screen.h = 448*2;

    SDL_Point corner;
    corner.x = 512;
    corner.y = 512;

    SDL_SetRenderTarget(renderer, Game);
    SDL_UpdateTexture(Game, &screen, pixels, 1024);
    SDL_SetRenderTarget(renderer, NULL);
    //SDL_RenderCopy(renderer, Game, NULL, &screen);
    SDL_RenderCopyEx(renderer, Game, NULL, &screen, -90, &corner, 0);
    SDL_RenderPresent(renderer);
}
