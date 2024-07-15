#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "8080Emulator.h"
#include <SDL.h>
#include <SDL_mixer.h>

static uint16_t shiftRegister;
static uint8_t shiftOffset;

void Interrupt(State8080* state, FILE *output, int *instruction, int number){

    switch (number){

    //Mid-screen interrupt (scan line 96).
    case 1:
        //Run an RST instruction, except save the current PC rather than PC + 1, since we want to go back exactly to the instruction we were at (it hasn't executed yet).
        state->memory[state->sp - 1 & 0xFFFF] = ((state->pc) >> 8) & 0xff;
        state->memory[state->sp - 2 & 0xFFFF] = (state->pc) & 0xff;
        state->sp -= 2;
        state->pc = 0x8;
        state->cyclecount += 11;
        break;

    //End-screen interrupt (scan line 224).
    case 2:
        //Same as case 1, except go to instruction 0x10.
        state->memory[state->sp - 1 & 0xFFFF] = ((state->pc) >> 8) & 0xff;
        state->memory[state->sp - 2 & 0xFFFF] = (state->pc) & 0xff;
        state->sp -= 2;
        state->pc = 0x10;
        state->cyclecount += 11;
        break;
    default:
        printf("Error: invalid interrupt!\n");
        break;
    }
    return;
}

uint8_t ProcessorIN(State8080* state, uint8_t port){
    uint8_t processorInput;

    //Get keyboard input.
    SDL_Event keyPress;
    SDL_PollEvent(&keyPress);

    switch (port){
        case 0:
        /*
        //port = port | 0x0E; //bit 1-3 are always 1: 0000 1110
        //If firing, set bit 4.
        //If left input, set bit 5.
        //If right input, set bit 6.
        bit 0 = DIP4 (not sure what this does).
        bit 1 = Always 1
        bit 2 = Always 1
        bit 3 = Always 1
        bit 4 = Fire
        bit 5 = Left
        bit 6 = Right
        bit 7 = Tied to demux port 7 (not sure what this does).
        */

        //Set bits 1-3.
        processorInput = 0x0E;
        break;

        case 1:
        /*
        bit 0 = Insert coin (1 if deposit)
        bit 1 = 2P start (1 if pressed)
        bit 2 = 1P start (1 if pressed)
        bit 3 = Always 1
        bit 4 = 1P shot (1 if pressed)
        bit 5 = 1P left (1 if pressed)
        bit 6 = 1P right (1 if pressed)
        bit 7 = not connected
        */

        //Bit 3 is always 1
        processorInput = 0x8;

        //Check state of keyboard. The function returns an array, with each key on the keyboard as its index, so the array (pointer) + SDL_SCANCODE_C is the index corresponding to the c character on the keyboard.
        if (*(SDL_GetKeyboardState(NULL) + SDL_SCANCODE_C) == 1){
            processorInput |= 0x1;
        }

        if (*(SDL_GetKeyboardState(NULL) + SDL_SCANCODE_RETURN) == 1){
            processorInput |= 0x4;
        }

        if (*(SDL_GetKeyboardState(NULL) + SDL_SCANCODE_SPACE) == 1){
            processorInput |= 0x10;
        }

        if (*(SDL_GetKeyboardState(NULL) + SDL_SCANCODE_LEFT) == 1){
            processorInput |= 0x20;
        }

        if (*(SDL_GetKeyboardState(NULL) + SDL_SCANCODE_RIGHT) == 1){
            processorInput |= 0x40;
        }
        break;

        case 2:
        /*
        bit 0 = + 1 extra ship if enabled (3 by default).
        bit 1 = + 2 extra ships if enabled
        bit 2 = Tilt (not sure what this does).
        bit 3 = 0 = extra ship at 1500 points, 1 = extra ship at 1000 points.
        bit 4 = 2P shot (1 if pressed)
        bit 5 = 2P left  (1 if pressed)
        bit 6 = 2P right (1 if pressed)
        bit 7 = Coin info displayed in demo screen (asks user to insert coin). 0 = On, 1 = off.
        */

        //Set bit 0, 1, and 3.
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
        shiftOffset = state->a & 0x07; //Mask out the 3 rightmost bits (bit 0-2), since they determine how much to shift by.
        break;
        case 3:
        /*
        bit 0 = UFO (repeats)
        bit 1 = Shot
        bit 2 = Flash (player death)
        bit 3 = Invader death
        bit 4 = Extended play
        bit 5 = AMP enable
        bit 6 = NC (not wired)
        bit 7 = NC (not wired)
        */

        static uint8_t prevSoundPort3 = 0;

//        if (state->memory[0x2094] & 0x1 && ((prevSoundPort3 & 0x1) == 0)){
//            Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024);
//            static Mix_Music *UFOsound = Mix_LoadWAV("Sounds\\0.wav");
//            Mix_PlayChannel(-1, sound, 0);
//        }

        if (state->memory[0x2094] & 0x2 && ((prevSoundPort3 & 0x2) == 0)){
            Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024);
            Mix_Chunk *sound = Mix_LoadWAV("Sounds\\1.wav");
            Mix_PlayChannel(1, sound, 0);
        }

        if (state->memory[0x2094] & 0x4 && ((prevSoundPort3 & 0x4) == 0)){
            Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024);
            Mix_Chunk *sound = Mix_LoadWAV("Sounds\\2.wav");
            Mix_PlayChannel(2, sound, 0);
        }

        if (state->memory[0x2094] & 0x8 && ((prevSoundPort3 & 0x8) == 0)){
            Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024);
            Mix_Chunk *sound = Mix_LoadWAV("Sounds\\3.wav");
            Mix_PlayChannel(3, sound, 0);
        }

        //Mix_FreeChunk(sound);

//        if (state->memory[0x2094] & 0x1 && ((prevSoundPort3 & 0x1) == 0)){
//            SDL_AudioDeviceID device;
//            SDL_AudioSpec spec;
//            Uint8 *data;
//            Uint32 len;
//            SDL_LoadWAV("Sounds\\0.wav", &spec, &data, &len);
//            SDL_OpenAudioDevice(NULL, 0, &spec, NULL, SDL_AUDIO_ALLOW_ANY_CHANGE);
//            SDL_QueueAudio(device, data, len);
//            SDL_PauseAudioDevice(device, 0);
//
//            SDL_FreeWAV(data);
//            //SDL_CloseAudioDevice(device);
//        }

//        if (state->memory[0x2094] & 0x1 && ((prevSoundPort3 & 0x1) == 0)){
//            SDL_AudioSpec spec;
//            Uint8 *data;
//            Uint32 len;
//            SDL_LoadWAV("Sounds\\0.wav", &spec, &data, &len);
//            SDL_OpenAudio(&spec, NULL);
//            SDL_QueueAudio(1, data, len);
//            SDL_PauseAudio(0);
//            SDL_FreeWAV(data);
//        }

        prevSoundPort3 = state->memory[0x2094];
        break;
        case 4: //Shift data.
        shiftRegister >>= 8; //Shift previous input to the right by 8 bits to make room in the left byte for the new data.
        shiftRegister = (state->a << 8) | shiftRegister; //Load new data into leftmost byte.
        break;
        case 5:
        /*
        bit 0 = Fleet movement 1
        bit 1 = Fleet movement 2
        bit 2 = Fleet movement 3
        bit 3 = Fleet movement 4
        bit 4 = UFO Hit
        bit 5 = NC (Cocktail mode control, to flip screen (unused))
        bit 6 = NC (not wired)
        bit 7 = NC (not wired)
        */

        static uint8_t prevSoundPort5 = 1; //Plays alien move sound on startup if not set to 1, because game sets RAM register to 1 for some reason.

        //printf("sound port 5 = %X\n", state->memory[0x2098]);

        if (state->memory[0x2098] & 0x1 && ((prevSoundPort5 & 0x1) == 0)){
            Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024);
            Mix_Chunk *sound = Mix_LoadWAV("Sounds\\4.wav");
            Mix_PlayChannel(4, sound, 0);
        }

        if (state->memory[0x2098] & 0x2 && ((prevSoundPort5 & 0x2) == 0)){
            Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024);
            Mix_Chunk *sound = Mix_LoadWAV("Sounds\\5.wav");
            Mix_PlayChannel(5, sound, 0);
        }

        if (state->memory[0x2098] & 0x4 && ((prevSoundPort5 & 0x4) == 0)){
            Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024);
            Mix_Chunk *sound = Mix_LoadWAV("Sounds\\6.wav");
            Mix_PlayChannel(6, sound, 0);
        }

        if (state->memory[0x2098] & 0x8 && ((prevSoundPort5 & 0x8) == 0)){
            Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024);
            Mix_Chunk *sound = Mix_LoadWAV("Sounds\\7.wav");
            Mix_PlayChannel(7, sound, 0);
        }

        if (state->memory[0x2098] & 0x10 && ((prevSoundPort5 & 0x10) == 0)){
            Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 1024);
            Mix_Chunk *sound = Mix_LoadWAV("Sounds\\8.wav");
            Mix_PlayChannel(-1, sound, 0);
        }

        prevSoundPort5 = state->memory[0x2098];
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
    Uint32 pixels[0x1c00 * 8]; //8 pixels in each byte, so make an array 8 times the size of the vRAM, one index for each bit.

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
