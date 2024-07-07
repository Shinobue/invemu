#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

//Declare a type called ConditionCodes (the second instance of the word declares the type). This type will be of a "ConditionCodes" struct (the first instance is the struct tag).
typedef struct ConditionCodes{
    uint8_t    z:1;
    uint8_t    s:1;
    uint8_t    p:1;
    uint8_t    cy:1;
    uint8_t    ac:1;
    uint8_t    pad:3;
} ConditionCodes;

typedef struct State8080 {
    uint8_t    a;
    uint8_t    b;
    uint8_t    c;
    uint8_t    d;
    uint8_t    e;
    uint8_t    h;
    uint8_t    l;
    uint16_t    sp;
    uint16_t    pc;
    uint8_t     *memory;
    struct      ConditionCodes      cc;
    uint8_t     int_enable;
    int         cyclecount;
} State8080;

//Function declarations.
int LoadFile(uint8_t *);
void UnimplementedInstruction(State8080*);
void Disassemble8080Op(unsigned char *, int);
void Emulate8080Op(State8080*, FILE *);
void Jump(State8080*, unsigned char *);
void Call(State8080*, unsigned char *);
void Return(State8080*);
void Restart(State8080*, uint16_t);
void Pop(State8080*, uint8_t *, uint8_t *);
void Push(State8080*, uint8_t *, uint8_t *);
int Parity(uint8_t);
//File output function
void Disassemble8080OpToFile(unsigned char *, int, FILE *);

