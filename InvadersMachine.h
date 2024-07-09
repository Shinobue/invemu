#include <SDL.h>
void Interrupt(State8080*, FILE *, int *, int);
uint8_t ProcessorIN(State8080*, uint8_t);
void ProcessorOUT(State8080*, uint8_t);
void Render(State8080 *, SDL_Window *, SDL_Renderer *, SDL_Texture *);
