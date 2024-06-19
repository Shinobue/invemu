#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "8080Emulator.h"
#include <SDL.h>

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
    return 1;
}

uint8_t MachineOUT(State8080* state, uint8_t port){
}

void Render(State8080 *state, SDL_Window *window){
    //Memory offset
    int memOffset = 0x2400;
    int vRamSize = 0x1c00;
    int i = 0;
    int byte = 0;
    Uint32 pixels[0x1c00 * 8]; //8 pixels in each byte.

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

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *Game = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 256, 224); //Use Stream for real emulation.

    //Clear screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    SDL_Rect screen;
    screen.x = 0;
    screen.y = 0;
    screen.w = 512;
    screen.h = 448;

    SDL_SetRenderTarget(renderer, Game);
    SDL_UpdateTexture(Game, &screen, pixels, 1024);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, Game, NULL, &screen);
    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(Game);
    SDL_DestroyRenderer(renderer);
}
