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
} State8080;

//Function declarations.
int LoadFile(uint8_t *);
void UnimplementedInstruction(State8080*);
void Emulate8080Op(State8080*);
void Jump(State8080*, unsigned char *);
void Call(State8080*, unsigned char *);
void Restart(State8080*, uint16_t);
void Pop(State8080*, uint8_t *, uint8_t *);
void Push(State8080*, uint8_t *, uint8_t *);
int Parity(uint8_t);

int main(){

    int RAMoffset;

    //Init State8080 and program counter.
    State8080 mystate;
    State8080 *state = &mystate;

    state->pc = 0;

    //Allocate 64K. 8K is used for the ROM, 8K for the RAM (of which 7K is VRAM). Processor has an address width of 16 bits however, so 2^16 = 65536 possible addresses.
    state->memory = malloc(sizeof(uint8_t) * 0x10000);
    RAMoffset = LoadFile(state->memory);

    int i = 0;
    while (i < 32){
        printf("%02x ", state->memory[i]);
        i += 1;
    }
    printf("\nRAMoffset = %d\n\n\n", RAMoffset);

    i = 0;
    while (i < 0x1350){
        Emulate8080Op(state);
        i += 1;
    }


    free(state->memory);

}


void UnimplementedInstruction(State8080* state){
    //printf("Error: Unimplemented instruction");
}

void Emulate8080Op(State8080* state){
    unsigned char *opcode = &state->memory[state->pc];
    
    printf("%04x, 0x%02x ", state->pc, *opcode);
    switch(*opcode){
        case 0x00:  //NOP
            //No op
            //printf("NOP");
            break;
        case 0x01:  //LXI    B, word
            //Load register pair immediate
            //B <- Byte 3, C <- Byte 2

            /*
            Verbose version, not needed. Covered by the "opcode" variable instead.
            state->b = state->memory[state->pc + 2];
            state->c = state->memory[state->pc + 1];
            printf("state->c = %02X, state->b = %02X, state->pc = %02X\n", state->c, state->b, state->pc);
            */

            state->b = opcode[2];
            state->c = opcode[1];
            state->pc += 2;

            //printf("LXI    B,D16. C = %02X, B = %02X", state->c, state->b);
            break;
        case 0x02:  //STAX   B
            //Store accumulator indirect
            //(BC) <- A
            {  //Add curly braces to localise declared variables.
            uint16_t offset;
            offset = (state->b << 8) | state->c; //Create memory offset. Address width is 16 bits, so shift the B part 8 bits and OR in (or use +) the c part. Leftmost 8 bits are b, rightmost 8 bits are c.
            state->memory[offset] = state->a; //(BC) <- A. Memory located at the address pointed to by the contents of BC is loaded with a.

            //printf("STAX   B. A = %02X, B = %02X, C = %02X, BC = %02X, (BC) = %02X", state->a, state->b, state->c, (state->b << 8) + state->c, state->memory[(state->b << 8) + state->c]);
            }
            break;
        case 0x03:  //INX    B
            //Increment register pair
            //BC <- BC + 1.

            //Increment c. If c is now 0 (meaning it has wrapped around), increment b by one too.
            state->c++;
            if (state->c == 0){
                state->b++;
            }
            break;
        case 0x04:  //INR    B
            //Increment Register
            //B <- B + 1
            {
            uint16_t result; //Result of operation, 16-bit.

            //Store the result in a 16-bit integer. This makes it easier to check if a carry was generated (not needed in this operation specifically, but needed in others).
            result = state->b + 1;

            //The "& 0xff" part of the following operations is a mask to make sure only the rightmost bits are evaluated (the bits to be stored in the register, which in this operation is the B register).

            //If the result of the operation is 0, z is set, otherwise it is reset. Remember that true = 1 and false = 0, so ((result & 0xff) == 0) becomes 1 if the result is 0, otherwise the expression evaluates to 0 (false).
            state->cc.z = ((result & 0xff) == 0);
             //If the MSB (most significant bit) of the result of the operation (which is actually 8-bit) is 1, s is set, otherwise reset. So check if result & 0000 0000 1000 0000 (0x80) is true (The 1 in 0x80 is the MSB in an 8-bit number).
            state->cc.s = ((result & 0x80) != 0);
            //See parity function.
            state->cc.p = Parity((uint8_t) (result & 0xff));
            //Set actual result to be "result & 0xff", since the actual result is an 8-bit number, and 0xff is 0000 0000 1111 1111. I.e mask out the leftmost 8 bits.
            state->b = result & 0xff;

            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x05:  //DCR    B
            //Decrement Register
            //B <- B - 1
            {
            uint16_t result;
            result = state->b - 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->b = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x06:  //MVI    B
            //Move immediate
            //B <- Byte 2
            state->b = opcode[1];
            state->pc += 1;
            break;
        case 0x07:  //RLC
            //Rotate left. Bit 7 (leftmost bit) becomes bit 0, and it also becomes cy.
            //A = A << 1; bit 0 = prev bit 7; CY = prev bit 7
            state->cc.cy = (state->a & 0x80) >> 7;
            state->a = (state->a << 1) | state->cc.cy;
            break;
        case 0x08:  //NOP
            //No op
            break;
        case 0x09:  //DAD    B
            //Add register pair to H and L
            //HL = HL + BC
            {
            uint32_t result; //Result of operation, 32-bit.
            uint16_t hl; //HL registers' contents. 16-bit (2 8-bit registers).
            uint16_t bc;

            hl = (state->h << 8) | state->l; //Get the bits in h and shift them, and get the bits in l. E.g if h = 0xFF and l = 0x12, then hl becomes: 1111 1111 0001 0010. The end result is both h and l. Instead of the | you could just use +.
            bc = (state->b << 8) | state->c;

            result = (uint32_t) hl + (uint32_t) bc; //The OR operator | works here too just as well, since the rightmost 16 bits of the result are 0 before bc is added.
            state->h = (result >> 8) & 0xff;
            state->l = result & 0xff;
            state->cc.cy = (result > 0xffff); //If the result is greater than 1111 1111 1111 1111, set the carry bit.
            }
            break;
        case 0x0a:  //LDAX   B
            //Load accumulator indirect
            //A <- (BC)
            {
            uint16_t offset;
            offset = (state->b << 8) | state->c;
            state->a = state->memory[offset];
            }
            break;
        case 0x0b:  //DCX    B
            //Decrement register pair
            //BC = BC - 1.

            //Decrement c. If c was 0 (meaning it has wrapped around), decrement b by one too.
            if (state->c == 0){
                state->b--;
            }
            state->c--;
            break;
        case 0x0c:  //INR    C
            //Increment Register
            //C <- C + 1
            {
            uint16_t result;
            result = state->c + 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->c = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x0d:  //DCR    C
            //Decrement Register
            //C <- C - 1
            {
            uint16_t result;
            result = state->c - 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->c = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x0e:  //MVI    C
            //Move immediate
            //C <- Byte 2
            state->c = opcode[1];
            state->pc += 1;
            break;
        case 0x0f:  //RRC
            //Rotate right. Bit 0 (rightmost bit) becomes bit 7, and it also becomes cy.
            //A = A >> 1; bit 7 = prev bit 0; CY = prev bit 0
            state->cc.cy = state->a & 0x01; //Get rightmost value in A register.
            state->a = (state->a >> 1) | (state->cc.cy << 7); //Shift A register to the right 1 step, insert previous rightmost value into bit 7.
            break;
        case 0x10:  //NOP
            //No op
            break;
        case 0x11:  //LXI    D, word
            //Load register pair immediate
            //D <- Byte 3, E <- Byte 2
            state->d = opcode[2];
            state->e = opcode[1];
            state->pc += 2;
            break;
        case 0x12:  //STAX   D
            //Store accumulator indirect
            //(DE) <- A
            {
            uint16_t offset;
            offset = (state->d << 8) | state->e;
            state->memory[offset] = state->a;
            }
            break;
        case 0x13:  //INX    D
            //Increment register pair
            //DE <- DE + 1.
            state->e++;
            if (state->e == 0){
                state->d++;
            }
            break;
        case 0x14:  //INR    D
            //Increment Register
            //D <- D + 1
            {
            uint16_t result;
            result = state->d + 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->d = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x15:  //DCR    D
            //Decrement Register
            //D <- D - 1
            {
            uint16_t result;
            result = state->d - 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->d = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x16:  //MVI    D
            //Move immediate
            //D <- Byte 2
            state->d = opcode[1];
            state->pc += 1;
            break;
        case 0x17:  //RAL
            //Rotate left through carry
            //A = A << 1; bit 0 = prev CY; CY = prev bit 7
            {
            uint8_t result;
            result = (state->a << 1) | state->cc.cy;
            state->cc.cy = (state->a & 0x80) >> 7;
            state->a = result;
            }
            break;
        case 0x18:  //NOP
            //No op
            break;
        case 0x19:  //DAD    D
            //Add register pair to H and L
            //HL = HL + DE
            {
            uint32_t result;
            uint16_t hl;
            uint16_t de;

            hl = (state->h << 8) | state->l;
            de = (state->d << 8) | state->e;

            result = (uint32_t) hl + (uint32_t) de;
            state->h = (result >> 8) & 0xff;
            state->l = result & 0xff;
            state->cc.cy = (result > 0xffff);
            }
            break;
        case 0x1a:  //LDAX   D
            //Load accumulator indirect
            //A <- (DE)
            {
            uint16_t offset;
            offset = (state->d << 8) | state->e;
            state->a = state->memory[offset];
            }
            break;
        case 0x1b:  //DCX    D
            //Decrement register pair
            //DE = DE - 1.
            if (state->e == 0){
                state->d--;
            }
            state->e--;
            break;
        case 0x1c:  //INR    E
            //Increment Register
            //E <- E + 1
            {
            uint16_t result;
            result = state->e + 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->e = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x1d:  //DCR    E
            //Decrement Register
            //E <- E - 1
            {
            uint16_t result;
            result = state->e - 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->e = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x1e:  //MVI    E
            //Move immediate
            //E <- Byte 2
            state->e = opcode[1];
            state->pc += 1;
            break;
        case 0x1f:  //RAR
            //Rotate right through carry
            //A = A >> 1; bit 7 = prev CY; CY = prev bit 0
            {
            uint8_t result;
            result = (state->a >> 1) | (state->cc.cy << 7);
            state->cc.cy = state->a & 0x01;
            state->a = result;
            }
            break;
        case 0x20:  //NOP
            //No op
            break;
        case 0x21:  //LXI    H, word
            //Load register pair immediate
            //H <- Byte 3, L <- Byte 2
            state->h = opcode[2];
            state->l = opcode[1];
            state->pc += 2;
            break;
        case 0x22:  //SHLD   adr
            //Store H and L direct
            //(adr) <-L; (adr+1)<-H
            {
            uint16_t offset;
            offset = (opcode[2] << 8) | opcode[1];
            state->memory[offset] = state->l;
            state->memory[offset + 1] = state->h;
            state->pc += 2;
            }
            break;
        case 0x23:  //INX    H
            //Increment register pair
            //HL <- HL + 1.
            state->l++;
            if (state->l == 0){
                state->h++;
            }
            break;
        case 0x24:  //INR    H
            //Increment Register
            //H <- H + 1
            {
            uint16_t result;
            result = state->h + 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->h = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x25:  //DCR    H
            //Decrement Register
            //H <- H - 1
            {
            uint16_t result;
            result = state->h - 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->h = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x26:  //MVI    H
            //Move immediate
            //H <- Byte 2
            state->h = opcode[1];
            state->pc += 1;
            break;
        case 0x27:  //DAA
            //Decimal Adjust Accumulator
            //Not used in space invaders
            break;
        case 0x28:  //NOP
            //No op
            break;
        case 0x29:  //DAD    H
            //Add register pair to H and L
            //HL <- HL + HL 
            {
            uint32_t result;
            uint16_t hl1;
            uint16_t hl2;

            hl1 = (state->h << 8) | state->l;
            hl2 = (state->h << 8) | state->l;

            result = (uint32_t) hl1 + (uint32_t) hl2;
            state->h = (result >> 8) & 0xff;
            state->l = result & 0xff;
            state->cc.cy = (result > 0xffff);
            }
            break;
        case 0x2a:  //LHLD   adr
            //Load H and L direct
            //L <- (adr); H <- (adr + 1)
            {
            uint16_t offset;
            offset = (opcode[2] << 8) | opcode[1];
            state->l = state->memory[offset];
            state->h = state->memory[offset + 1];
            state->pc += 2;
            }
            break;
        case 0x2b:  //DCX    H
            //Decrement register pair
            //HL = HL - 1.
            if (state->l == 0){
                state->h--;
            }
            state->l--;
            break;
        case 0x2c:  //INR    L
            //Increment Register
            //L <- L + 1
            {
            uint16_t result;
            result = state->l + 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->l = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x2d:  //DCR    L
            //Decrement Register
            //L <- L - 1
            {
            uint16_t result;
            result = state->l - 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->l = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x2e:  //MVI    L
            //Move immediate
            //L <- Byte 2
            state->l = opcode[1];
            state->pc += 1;
            break;
        case 0x2f:  //CMA
            //Complement accumulator
            //A <- !A
            state->a = ~(state->a);
            break;
        case 0x30:  //NOP
            //No op
            break;
        case 0x31:  //LXI    SP, word
            //Load register pair immediate
            //SP(high) <- Byte 3, SP(low) <- Byte 2
            state->sp = opcode[2];
            state->sp = (state->sp << 8) | opcode[1];
            state->pc += 2;
            break;
        case 0x32:  //STA    adr
            //Store Accumulator direct
            //(adr) <- A
            {
            uint16_t offset;
            offset = (opcode[2] << 8) | opcode[1];
            state->memory[offset] = state->a;
            }
            break;
        case 0x33:  //INX    SP
            //Increment register pair
            //SP <- SP + 1.
            state->sp++;
            break;
        case 0x34:  //INR    M
            //Increment memory
            //(HL) <- (HL) + 1
            {
            uint16_t result;
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            result = state->memory[offset] + 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->memory[offset] = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x35:  //DCR    M
            //Decrement memory
            //(HL) <- (HL) - 1
            {
            uint16_t result;
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            result = state->h - 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->memory[offset] = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x36:  //MVI    M
            //Move to memory immediate
            //(HL) <- Byte 2
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->memory[offset] = opcode[1];
            state->pc += 1;
            }
            break;
        case 0x37:  //STC
            //Set carry
            //CY = 1
            state->cc.cy = 0x01;
            break;
        case 0x38:  //NOP
            //No op
            break;
        case 0x39:  //DAD    SP
            //Add register pair to H and L
            //HL = HL + SP
            {
            uint32_t result;
            uint16_t hl;

            hl = (state->h << 8) | state->l;

            result = (uint32_t) hl + (uint32_t) state->sp;
            state->h = (result >> 8) & 0xff;
            state->l = result & 0xff;
            state->cc.cy = (result > 0xffff);
            }
            break;
        case 0x3a:  //LDA    adr
            //Load Accumulator direct
            //A <- (adr)
            {
            uint16_t offset;
            offset = (opcode[2] << 8) | opcode[1];
            state->a = state->memory[offset];
            state->pc += 2;
            }
            break;
        case 0x3b:  //DCX    SP
            //Decrement register pair
            //SP = SP - 1.
            state->sp--;
            break;
        case 0x3c:  //INR    A
            //Increment Register
            //A <- A + 1
            {
            uint16_t result;
            result = state->a + 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->a = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x3d:  //DCR    A
            //Decrement Register
            //A <- A - 1
            {
            uint16_t result;
            result = state->a - 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->a = result & 0xff;
            //Note: auxiliary carry not implemented.
            }
            break;
        case 0x3e:  //MVI    A
            //Move immediate
            //A <- Byte 2
            state->a = opcode[1];
            state->pc += 1;
            break;
        case 0x3f:  //CMC
            //Complement carry
            state->cc.cy = ~(state->cc.cy);
            break;
        case 0x40:  //MOV    B,B
            //Move Register
            //B <- B
            state->b = state->b;
            break;
        case 0x41:  //MOV    B,C
            //Move Register
            //B <- C
            state->b = state->c;
            break;
        case 0x42:  //MOV    B,D
            //Move Register
            //B <- D
            state->b = state->d;
            break;
        case 0x43:  //MOV    B,E
            //Move Register
            //B <- E
            state->b = state->e;
            break;
        case 0x44:  //MOV    B,H
            //Move Register
            //B <- H
            state->b = state->h;
            break;
        case 0x45:  //MOV    B,L
            //Move Register
            //B <- L
            state->b = state->l;
            break;
        case 0x46:  //MOV    B,M
            //Move from memory
            //B <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->b = state->memory[offset];
            }
            break;
        case 0x47:  //MOV    B,A
            //Move Register
            //B <- A
            state->b = state->a;
            break;
        case 0x48:  //MOV    C,B
            //Move Register
            //C <- B
            state->c = state->b;
            break;
        case 0x49:  //MOV    C,C
            //Move Register
            //C <- C
            state->c = state->c;
            break;
        case 0x4a:  //MOV    C,D
            //Move Register
            //C <- D
            state->c = state->d;
            break;
        case 0x4b:  //MOV    C,E
            //Move Register
            //C <- E
            state->c = state->e;
            break;
        case 0x4c:  //MOV    C,H
            //Move Register
            //C <- H
            state->c = state->h;
            break;
        case 0x4d:  //MOV    C,L
            //Move Register
            //C <- L
            state->c = state->l;
            break;
        case 0x4e:  //MOV    C,M
            //Move from memory
            //C <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->c = state->memory[offset];
            }
            break;
        case 0x4f:  //MOV    C,A
            //Move Register
            //C <- A
            state->c = state->a;
            break;
        case 0x50:  //MOV    D,B
            //Move Register
            //D <- B
            state->d = state->b;
            break;
        case 0x51:  //MOV    D,C
            //Move Register
            //D <- C
            state->d = state->c;
            break;
        case 0x52:  //MOV    D,D
            //Move Register
            //D <- D
            state->d = state->d;
            break;
        case 0x53:  //MOV    D,E
            //Move Register
            //D <- E
            state->d = state->e;
            break;
        case 0x54:  //MOV    D,H
            //Move Register
            //D <- H
            state->d = state->h;
            break;
        case 0x55:  //MOV    D,L
            //Move Register
            //D <- L
            state->d = state->l;
            break;
        case 0x56:  //MOV    D,M
            //Move from memory
            //D <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->d = state->memory[offset];
            }
            break;
        case 0x57:  //MOV    D,A
            //Move Register
            //D <- A
            state->d = state->a;
            break;
        case 0x58:  //MOV    E,B
            //Move Register
            //E <- B
            state->e = state->b;
            break;
        case 0x59:  //MOV    E,C
            //Move Register
            //E <- C
            state->e = state->c;
            break;
        case 0x5a:  //MOV    E,D
            //Move Register
            //E <- D
            state->e = state->d;
            break;
        case 0x5b:  //MOV    E,E
            //Move Register
            //E <- E
            state->e = state->e;
            break;
        case 0x5c:  //MOV    E,H
            //Move Register
            //E <- H
            state->e = state->h;
            break;
        case 0x5d:  //MOV    E,L
            //Move Register
            //E <- L
            state->e = state->l;
            break;
        case 0x5e:  //MOV    E,M
            //Move from memory
            //E <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->e = state->memory[offset];
            }
            break;
        case 0x5f:  //MOV    E,A
            //Move Register
            //E <- A
            state->e = state->a;
            break;
        case 0x60:  //MOV    H,B
            //Move Register
            //H <- B
            state->h = state->b;
            break;
        case 0x61:  //MOV    H,C
            //Move Register
            //H <- C
            state->h = state->c;
            break;
        case 0x62:  //MOV    H,D
            //Move Register
            //H <- D
            state->h = state->d;
            break;
        case 0x63:  //MOV    H,E
            //Move Register
            //H <- E
            state->h = state->e;
            break;
        case 0x64:  //MOV    H,H
            //Move Register
            //H <- H
            state->h = state->h;
            break;
        case 0x65:  //MOV    H,L
            //Move Register
            //H <- L
            state->h = state->l;
            break;
        case 0x66:  //MOV    H,M
            //Move from memory
            //H <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->h = state->memory[offset];
            }
            break;
        case 0x67:  //MOV    H,A
            //Move Register
            //H <- A
            state->h = state->a;
            break;
        case 0x68:  //MOV    L,B
            //Move Register
            //L <- B
            state->l = state->b;
            break;
        case 0x69:  //MOV    L,C
            //Move Register
            //L <- C
            state->l = state->c;
            break;
        case 0x6a:  //MOV    L,D
            //Move Register
            //L <- D
            state->l = state->d;
            break;
        case 0x6b:  //MOV    L,E
            //Move Register
            //L <- E
            state->l = state->e;
            break;
        case 0x6c:  //MOV    L,H
            //Move Register
            //L <- H
            state->l = state->h;
            break;
        case 0x6d:  //MOV    L,L
            //Move Register
            //L <- L
            state->l = state->l;
            break;
        case 0x6e:  //MOV    L,M
            //Move from memory
            //L <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->l = state->memory[offset];
            }
            break;
        case 0x6f:  //MOV    L,A
            //Move Register
            //L <- A
            state->l = state->a;
            break;
        case 0x70:  //MOV    M,B
            //Move Register
            //(HL) <- B
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->memory[offset] = state->b;
            }
            break;
        case 0x71:  //MOV    M,C
            //Move Register
            //(HL) <- C
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->memory[offset] = state->c;
            }
            break;
        case 0x72:  //MOV    M,D
            //Move Register
            //(HL) <- D
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->memory[offset] = state->d;
            }
            break;
        case 0x73:  //MOV    M,E
            //Move Register
            //(HL) <- E
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->memory[offset] = state->e;
            }
            break;
        case 0x74:  //MOV    M,H
            //Move Register
            //(HL) <- H
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->memory[offset] = state->h;
            }
            break;
        case 0x75:  //MOV    M,L
            //Move Register
            //(HL) <- L
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->memory[offset] = state->l;
            }
            break;
        case 0x76:  //HLT
            //exit(0);
            break;
        case 0x77:  //MOV    M,A
            //Move Register
            //(HL) <- A
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->memory[offset] = state->a;
            }
            break;
        case 0x78:  //MOV    A,B
            //Move Register
            //A <- B
            state->a = state->b;
            break;
        case 0x79:  //MOV    A,C
            //Move Register
            //A <- C
            state->a = state->c;
            break;
        case 0x7a:  //MOV    A,D
            //Move Register
            //A <- D
            state->a = state->d;
            break;
        case 0x7b:  //MOV    A,E
            //Move Register
            //A <- E
            state->a = state->e;
            break;
        case 0x7c:  //MOV    A,H
            //Move Register
            //A <- H
            state->a = state->h;
            break;
        case 0x7d:  //MOV    A,L
            //Move Register
            //A <- L
            state->a = state->l;
            break;
        case 0x7e:  //MOV    A,M
            //Move from memory
            //A <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->a = state->memory[offset];
            }
            break;
        case 0x7f:  //MOV    A,A
            //Move Register
            //A <- A
            state->a = state->a;
            break;
        case 0x80:  //ADD    B
            //Add Register
            //A <- A + B
            {
            uint16_t result;
            result = state->a + state->b;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x81:  //ADD    C
            //Add Register
            //A <- A + C
            {
            uint16_t result;
            result = state->a + state->c;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x82:  //ADD    D
            //Add Register
            //A <- A + D
            {
            uint16_t result;
            result = state->a + state->d;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x83:  //ADD    E
            //Add Register
            //A <- A + E
            {
            uint16_t result;
            result = state->a + state->e;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x84:  //ADD    H
            //Add Register
            //A <- A + H
            {
            uint16_t result;
            result = state->a + state->h;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x85:  //ADD    L
            //Add Register
            //A <- A + L
            {
            uint16_t result;
            result = state->a + state->l;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x86:  //ADD    M
            //Add memory
            //A <- A + (HL)
            {
            uint16_t result;
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            result = state->a + state->memory[offset];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x87:  //ADD    A
            //Add Register
            //A <- A + A
            {
            uint16_t result;
            result = state->a + state->a;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x88:  //ADC    B
            //Add Register with carry
            //A <- A + B + CY
            {
            uint16_t result;
            result = state->a + state->b + state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x89:  //ADC    C
            //Add Register with carry
            //A <- A + C + CY
            {
            uint16_t result;
            result = state->a + state->c + state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x8a:  //ADC    D
            //Add Register with carry
            //A <- A + D + CY
            {
            uint16_t result;
            result = state->a + state->d + state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x8b:  //ADC    E
            //Add Register with carry
            //A <- A + E + CY
            {
            uint16_t result;
            result = state->a + state->e + state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x8c:  //ADC    H
            //Add Register with carry
            //A <- A + H + CY
            {
            uint16_t result;
            result = state->a + state->h + state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x8d:  //ADC    L
            //Add Register with carry
            //A <- A + L + CY
            {
            uint16_t result;
            result = state->a + state->l + state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x8e:  //ADC    M
            //Add memory with carry
            //A <- A + (HL) + CY
            {
            uint16_t result;
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            result = state->a + state->memory[offset] + state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x8f:  //ADC    A
            //Add Register with carry
            //A <- A + A + CY
            {
            uint16_t result;
            result = state->a + state->a + state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x90:  //SUB    B
            //Subtract Register
            //A <- A - B
            {
            uint16_t result;
            result = state->a - state->b;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x91:  //SUB    C
            //Subtract Register
            //A <- A - C
            {
            uint16_t result;
            result = state->a - state->c;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x92:  //SUB    D
            //Subtract Register
            //A <- A - D
            {
            uint16_t result;
            result = state->a - state->d;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x93:  //SUB    E
            //Subtract Register
            //A <- A - E
            {
            uint16_t result;
            result = state->a - state->e;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x94:  //SUB    H
            //Subtract Register
            //A <- A - H
            {
            uint16_t result;
            result = state->a - state->h;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x95:  //SUB    L
            //Subtract Register
            //A <- A - L
            {
            uint16_t result;
            result = state->a - state->l;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x96:  //SUB    M
            //Subtract memory
            //A <- A - (HL)
            {
            uint16_t result;
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            result = state->a - state->memory[offset];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x97:  //SUB    A
            //Subtract Register
            //A <- A - A
            {
            uint16_t result;
            result = state->a - state->a;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x98:  //SBB    B
            //Subtract Register with borrow
            //A <- A - B - CY
            {
            uint16_t result;
            result = state->a - state->b - state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x99:  //SBB    C
            //Subtract Register with borrow
            //A <- A - C - CY
            {
            uint16_t result;
            result = state->a - state->c - state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x9a:  //SBB    D
            //Subtract Register with borrow
            //A <- A - D - CY
            {
            uint16_t result;
            result = state->a - state->d - state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x9b:  //SBB    E
            //Subtract Register with borrow
            //A <- A - E - CY
            {
            uint16_t result;
            result = state->a - state->e - state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x9c:  //SBB    H
            //Subtract Register with borrow
            //A <- A - H - CY
            {
            uint16_t result;
            result = state->a - state->h - state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x9d:  //SBB    L
            //Subtract Register with borrow
            //A <- A - L - CY
            {
            uint16_t result;
            result = state->a - state->l - state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x9e:  //SBB    M
            //Add memory with borrow
            //A <- A - (HL) - CY
            {
            uint16_t result;
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            result = state->a + state->memory[offset] + state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0x9f:  //SBB    A
            //Subtract Register with borrow
            //A <- A - A - CY
            {
            uint16_t result;
            result = state->a - state->a - state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xa0:  //ANA    B
            //AND Register
            //A <- A & B
            {
            uint16_t result;
            result = state->a & state->b;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xa1:  //ANA    C
            //AND Register
            //A <- A & C
            {
            uint16_t result;
            result = state->a & state->c;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xa2:  //ANA    D
            //AND Register
            //A <- A & D
            {
            uint16_t result;
            result = state->a & state->d;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xa3:  //ANA    E
            //AND Register
            //A <- A & E
            {
            uint16_t result;
            result = state->a & state->e;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xa4:  //ANA    H
            //AND Register
            //A <- A & H
            {
            uint16_t result;
            result = state->a & state->h;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xa5:  //ANA    L
            //AND Register
            //A <- A & L
            {
            uint16_t result;
            result = state->a & state->l;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xa6:  //ANA    M
            //AND memory
            //A <- A & (HL)
            {
            uint16_t offset;
            uint16_t result;
            offset = (state->h << 8) | state->l;
            result = state->a & state->memory[offset];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xa7:  //ANA    A
            //AND Register
            //A <- A & A
            {
            uint16_t result;
            result = state->a & state->a;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->a = result & 0xff;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xa8:  //XRA    B
            //Exclusive OR Register
            //A <- A ^ B
            {
            uint16_t result;
            result = state->a ^ state->b;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xa9:  //XRA    C
            //Exclusive OR Register
            //A <- A ^ C
            {
            uint16_t result;
            result = state->a ^ state->c;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xaa:  //XRA    D
            //Exclusive OR Register
            //A <- A ^ D
            {
            uint16_t result;
            result = state->a ^ state->d;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xab:  //XRA    E
            //Exclusive OR Register
            //A <- A ^ E
            {
            uint16_t result;
            result = state->a ^ state->e;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xac:  //XRA    H
            //Exclusive OR Register
            //A <- A ^ H
            {
            uint16_t result;
            result = state->a ^ state->h;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xad:  //XRA    L
            //Exclusive OR Register
            //A <- A ^ L
            {
            uint16_t result;
            result = state->a ^ state->l;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xae:  //XRA    M
            //Exclusive OR Memory
            //A <- A ^ (HL)
            {
            uint16_t offset;
            uint16_t result;
            offset = (state->h << 8) | state->l;
            result = state->a ^ state->memory[offset];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xaf:  //XRA    A
            //Exclusive OR Register
            //A <- A ^ A
            {
            uint16_t result;
            result = state->a ^ state->a;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xb0:  //ORA    B
            //OR Register
            //A <- A | B
            {
            uint16_t result;
            result = state->a | state->b;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xb1:  //ORA    C
            //OR Register
            //A <- A | C
            {
            uint16_t result;
            result = state->a | state->c;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xb2:  //ORA    D
            //OR Register
            //A <- A | D
            {
            uint16_t result;
            result = state->a | state->d;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xb3:  //ORA    E
            //OR Register
            //A <- A | E
            {
            uint16_t result;
            result = state->a | state->e;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xb4:  //ORA    H
            //OR Register
            //A <- A | H
            {
            uint16_t result;
            result = state->a | state->h;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xb5:  //ORA    L
            //OR Register
            //A <- A | L
            {
            uint16_t result;
            result = state->a | state->l;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xb6:  //ORA    M
            //OR Memory
            //A <- A | (HL)
            {
            uint16_t offset;
            uint16_t result;
            offset = (state->h << 8) | state->l;
            result = state->a | state->memory[offset];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xb7:  //ORA    A
            //OR Register
            //A <- A | A
            {
            uint16_t result;
            result = state->a | state->a;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            }
            break;
        case 0xb8:  //CMP    B
            //Compare Register
            //A - B
            {
            uint16_t result;
            result = state->a - state->b;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff); //If (A - B) > 0xff, then a was less than b (since it loops around, starting at 0xFFFF and going down). Neither A nor B can be larger than 0xff. Same logic as in other commands higher up in this file.
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xb9:  //CMP    C
            //Compare Register
            //A - C
            {
            uint16_t result;
            result = state->a - state->c;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xba:  //CMP    D
            //Compare Register
            //A - D
            {
            uint16_t result;
            result = state->a - state->d;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xbb:  //CMP    E
            //Compare Register
            //A - E
            {
            uint16_t result;
            result = state->a - state->e;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xbc:  //CMP    H
            //Compare Register
            //A - H
            {
            uint16_t result;
            result = state->a - state->h;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xbd:  //CMP    L
            //Compare Register
            //A - L
            {
            uint16_t result;
            result = state->a - state->l;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xbe:  //CMP    M
            //Compare Memory
            //A - (HL)
            {
            uint16_t offset;
            uint16_t result;
            offset = (state->h << 8) | state->l;
            result = state->a - state->memory[offset];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xbf:  //CMP    A
            //Compare Register
            //A - A
            {
            uint16_t result;
            result = state->a - state->a;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xc0:  //RNZ
            //Conditional Return
            //If NZ (If the zero flag is zero, i.e if the result of the previous operation was not zero), do the following (operation is identical to RET):
            //(PCL) <- ((SP))
            //(PCH) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            if (state->cc.z == 0){
                //SP contains a memory address. The contents of this address + 1 are put into the high order bits, and the contents of the address itself (i.e without the +1) are the low order bits.
                state->pc = (state->memory[state->sp + 1] << 8) | state->memory[state->sp];
                state->sp += 2;
            }
            break;
        case 0xc1:  //POP    B
            //Pop stack
            //(C) <- ((SP))
            //(B) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            Pop(state, &state->b, &state->c);
            break;
        case 0xc2:  //JNZ    D16
            //Conditional jump (not zero)
            if (state->cc.z == 0){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xc3:  //JMP    D16
            //Jump
            //(PC) <- (byte 3) (byte 2)
            Jump(state, opcode);
            break;
        case 0xc4:  //CNZ    D16
            //Condition call (not zero)
            if (state->cc.z == 0){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xc5:  //PUSH    B
            //Push to stack
            //(B) <- ((SP) - 1)
            //(C) <- ((SP) - 2)
            //(SP) <- (SP) - 2
            Push(state, &state->b, &state->c);
            break;
        case 0xc6:  //ADI    D8
            //Add Immediate
            //A <- A + byte 2
            {
            uint16_t result;
            result = state->a + opcode[1];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->pc += 1;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xc7:  //RST    0
            //Restart. CALL 0x0000
            Restart(state, 0x0000);
            break;
        case 0xc8:  //RZ
            //Conditional Return
            //If Z, RET:
            if (state->cc.z == 1){
                state->pc = (state->memory[state->sp + 1] << 8) | state->memory[state->sp];
                state->sp += 2;
            }
            break;
        case 0xc9:  //RET
            //Return
            //(PCL) <- ((SP))
            //(PCH) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            state->pc = (state->memory[state->sp + 1] << 8) | state->memory[state->sp];
            state->sp += 2;
            break;
        case 0xca:  //JZ    D16
            //Conditional jump (zero)
            if (state->cc.z == 1){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xcb:  //JMP    D16
            //Jump
            //(PC) <- (byte 3) (byte 2)
            Jump(state, opcode);
            break;
        case 0xcc:  //CZ     D16
            //Condition call (zero)
            if (state->cc.z == 1){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xcd:  //CALL   D16
            //Call
            //((SP) - 1) <- (PCH)
            //((SP) - 2) <- (PCL)
            //(SP) <- (SP) - 2
            //(PC) <- (byte 3) (byte 2)
            Call(state, opcode);
            break;
        case 0xce:  //ACI    D8
            //Add Immediate with carry
            //A <- A + byte 2 + CY
            {
            uint16_t result;
            result = state->a + opcode[1] + state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->pc += 1;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xcf:  //RST    1
            //Restart. CALL 0x0008
            Restart(state, 0x0008);
            break;
        case 0xd0:  //RNC
            //Conditional Return
            //If NCY, RET:
            if (state->cc.cy == 0){
                state->pc = (state->memory[state->sp + 1] << 8) | state->memory[state->sp];
                state->sp += 2;
            }
            break;
        case 0xd1:  //POP    D
            //Pop stack
            //(E) <- ((SP))
            //(D) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            Pop(state, &state->d, &state->e);
            break;
        case 0xd2:  //JNC    D16
            //Conditional jump (no carry)
            if (state->cc.cy == 0){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xd3:  //OUT    D8
            //Output
            state->pc++;
            break;
        case 0xd4:  //CNC    D16
            //Condition call (no carry)
            if (state->cc.cy == 0){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xd5:  //PUSH    D
            //Push to stack
            //(D) <- ((SP) - 1)
            //(E) <- ((SP) - 2)
            //(SP) <- (SP) - 2
            Push(state, &state->d, &state->e);
            break;
        case 0xd6:  //SUI    D8
            //Subtract Immediate
            //A <- A - byte 2
            {
            uint16_t result;
            result = state->a - opcode[1];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->pc += 1;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xd7:  //RST    2
            //Restart. CALL 0x0010
            Restart(state, 0x0010);
            break;
        case 0xd8:  //RC
            //Conditional Return
            //If CY, RET:
            //(PCL) <- ((SP))
            //(PCH) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            if (state->cc.cy == 1){
                state->pc = (state->memory[state->sp + 1] << 8) | state->memory[state->sp];
                state->sp += 2;
            }
            break;
        case 0xd9:  //RET
            //Return
            //(PCL) <- ((SP))
            //(PCH) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            state->pc = (state->memory[state->sp + 1] << 8) | state->memory[state->sp];
            state->sp += 2;
            break;
        case 0xda:  //JC    D16
            //Conditional jump (carry)
            if (state->cc.cy == 1){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xdb:  //IN     D8
            //Input
            state->pc++;
            break;
        case 0xdc:  //CC     D16
            //Condition call (carry)
            if (state->cc.cy == 1){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xdd:  //CALL   D16
            //Call
            //((SP) - 1) <- (PCH)
            //((SP) - 2) <- (PCL)
            //(SP) <- (SP) - 2
            //(PC) <- (byte 3) (byte 2)
            Call(state, opcode);
            break;
        case 0xde:  //SBI    D8
            //Subtract Immediate with borrow
            //A <- A - byte 2 - CY
            {
            uint16_t result;
            result = state->a - opcode[1] - state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->pc += 1;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xdf:  //RST    3
            //Restart. CALL 0x0018
            Restart(state, 0x0018);
            break;
        case 0xe0:  //RPO
            //Conditional Return
            //If Parity is odd, RET:
            if (state->cc.p == 0){
                state->pc = (state->memory[state->sp + 1] << 8) | state->memory[state->sp];
                state->sp += 2;
            }
            break;
        case 0xe1:  //POP    H
            //Pop stack
            //(L) <- ((SP))
            //(H) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            Pop(state, &state->d, &state->e);
            break;
        case 0xe2:  //JPO    D16
            //Conditional jump (odd parity)
            if (state->cc.p == 0){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xe3:  //XTHL
            //Exchange stack top with H and L
            //(L) <-> ((SP))
            //(H) <-> ((SP) + 1)
            {
            uint8_t temp;
            temp = state->l;
            state->l = state->memory[state->sp];
            state->memory[state->sp] = temp;
            temp = state->h;
            state->h = state->memory[state->sp + 1];
            state->memory[state->sp + 1] = temp;
            }
            break;
        case 0xe4:  //CPO    D16
            //Condition call (odd parity)
            if (state->cc.p == 0){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xe5:  //PUSH    H
            //Push to stack
            //(H) <- ((SP) - 1)
            //(L) <- ((SP) - 2)
            //(SP) <- (SP) - 2
            Push(state, &state->h, &state->l);
            break;
        case 0xe6:  //ANI    D8
            //AND Immediate
            //A <- A & byte 2
            {
            uint16_t result;
            result = state->a & opcode[1];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0;
            state->cc.ac = 0;
            state->a = result & 0xff;
            state->pc += 1;
            }
            break;
        case 0xe7:  //RST    4
            //Restart. CALL 0x0020
            Restart(state, 0x0020);
            break;
        case 0xe8:  //RPE
            //Conditional Return
            //If Parity is even, RET:
            if (state->cc.p == 1){
                state->pc = (state->memory[state->sp + 1] << 8) | state->memory[state->sp];
                state->sp += 2;
            }
            break;
        case 0xe9:  //PCHL
            //Jump H and L indirect - move H and L to PC
            state->pc = (state->h << 8) | state->l;
            state->pc--;
            break;
        case 0xea:  //JPE    D16
            //Conditional jump (even parity)
            if (state->cc.p == 1){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xeb:  //XCHG
            //Exchange H and L with D and E
            // (H) <-> (D)
            // (L) <-> (E)
            {
            uint8_t temp;
            temp = state->h;
            state->h = state->d;
            state->d = temp;
            temp = state->l;
            state->l = state->e;
            state->e = temp;
            }
            break;
        case 0xec:  //CPE    D16
            //Condition call (even parity)
            if (state->cc.p == 1){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xed:  //CALL   D16
            //Call
            //((SP) - 1) <- (PCH)
            //((SP) - 2) <- (PCL)
            //(SP) <- (SP) - 2
            //(PC) <- (byte 3) (byte 2)
            Call(state, opcode);
            break;
        case 0xee:  //XRI    D8
            //Exclusive OR Immediate
            //A <- A ^ byte 2
            {
            uint16_t result;
            result = state->a ^ opcode[1];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0x0;
            state->cc.ac = 0x0;
            state->a = result & 0xff;
            state->pc += 1;
            }
            break;
        case 0xef:  //RST    5
            //Restart. CALL 0x0028
            Restart(state, 0x0028);
            break;
        case 0xf0:  //RP
            //Conditional Return
            //If sign flag is 0 (i.e result was a positive integer), RET:
            if (state->cc.s == 0){
                state->pc = (state->memory[state->sp + 1] << 8) | state->memory[state->sp];
                state->sp += 2;
            }
            break;
        case 0xf1:  //POP    PSW
            //Pop processor status word
            //(CY) <- ((SP))0 //bit 0 (rightmost bit) of the byte contained at the address in the stack pointer.
            //(P) <- ((SP))2 //bit 2, etc.
            //(AC) <- ((SP))4
            //(Z) <- ((SP))6
            //(S) <- ((SP))7
            //(A) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            state->cc.cy = state->memory[state->sp] & 0x1;
            state->cc.p = (state->memory[state->sp] >> 2) & 0x1;
            state->cc.ac = (state->memory[state->sp] >> 4) & 0x1;
            state->cc.z = (state->memory[state->sp] >> 6) & 0x1;
            state->cc.s = (state->memory[state->sp] >> 7) & 0x1;
            state->a = state->memory[state->sp + 1];
            state->sp += 2;
            break;
        case 0xf2:  //JP     D16
            //Conditional jump (positive integer)
            if (state->cc.s == 0){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xf3:  //DI
            //Disable interrupts
            state->int_enable = 0; break;
        case 0xf4:  //CP     D16
            //Condition call (positive integer)
            if (state->cc.s == 0){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xf5:  //PUSH   PSW
            //Push processor status word
            //((SP) - 1) <- (A)
            //((SP) - 2)0 <- (CY)
            //((SP) - 2)1 <- 1
            //((SP) - 2)2 <- (P)
            //((SP) - 2)3 <- 0
            //((SP) - 2)4 <- (AC)
            //((SP) - 2)5 <- 0
            //((SP) - 2)6 <- (Z)
            //((SP) - 2)7 <- (S)
            //(SP) <- (SP) - 2
            state->memory[state->sp - 1] = state->a;
            state->memory[state->sp - 2] = state->cc.s & 0x01;
            state->memory[state->sp - 2] = (state->memory[state->sp - 2] << 1) | state->cc.z;
            state->memory[state->sp - 2] = (state->memory[state->sp - 2] << 1) | 0x0;
            state->memory[state->sp - 2] = (state->memory[state->sp - 2] << 1) | state->cc.ac;
            state->memory[state->sp - 2] = (state->memory[state->sp - 2] << 1) | 0x0;
            state->memory[state->sp - 2] = (state->memory[state->sp - 2] << 1) | state->cc.p;
            state->memory[state->sp - 2] = (state->memory[state->sp - 2] << 1) | 0x01;
            state->memory[state->sp - 2] = (state->memory[state->sp - 2] << 1) | state->cc.cy;
            state->sp -= 2;
            break;
        case 0xf6:  //ORI    D8
            //OR Immediate
            //A <- A | byte 2
            {
            uint16_t result;
            result = state->a | opcode[1];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = 0;
            state->cc.ac = 0;
            state->a = result & 0xff;
            state->pc += 1;
            }
            break;
        case 0xf7:  //RST    6
            //Restart. CALL 0x0030
            Restart(state, 0x0030);
            break;
        case 0xf8:  //RM
            //Conditional Return
            //If sign flag is 1 (i.e result was a negative integer), RET:
            if (state->cc.s == 0){
                state->pc = (state->memory[state->sp + 1] << 8) | state->memory[state->sp];
                state->sp += 2;
            }
            break;
        case 0xf9:  //SPHL
            //Move HL to SP
            //(SP) <- (H) (L)
            state->sp = state->h << 8;
            state->sp = state->sp | state->l;
            break;
        case 0xfa:  //JM     D16
            //Conditional jump (negative integer ("minus"))
            if (state->cc.s == 1){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xfb:  //EI
            //Enable interrupts
            state->int_enable = 1; break;
        case 0xfc:  //CM     D16
            //Condition call (negative integer ("minus"))
            if (state->cc.s == 1){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
            }
            break;
        case 0xfd:  //CALL   D16
            //Call
            //((SP) - 1) <- (PCH)
            //((SP) - 2) <- (PCL)
            //(SP) <- (SP) - 2
            //(PC) <- (byte 3) (byte 2)
            Call(state, opcode);
            break;
        case 0xfe:  //CPI    D8
            //Compare Immediate
            //A - byte 2
            {
            uint16_t result;
            result = state->a - opcode[1];
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.cy = (result > 0xff);
            state->pc += 1;
            }
            //Note: auxiliary carry not implemented.
            break;
        case 0xff:  //RST    7
            //Restart. CALL 0x0038
            Restart(state, 0x0038);
            break;
    }
    printf("\n");
    state->pc+=1;
}

void Jump(State8080* state, unsigned char *opcode){
    state->pc = (opcode[2] << 8) | opcode[1];
    state->pc--;
}

void Call(State8080* state, unsigned char *opcode){
    //Store the address of the next instruction (which is state->pc + 2) on the stack pointer (remember the stack "grows downward").
    state->memory[state->sp - 1] = (state->pc + 2) >> 8;
    state->memory[state->sp - 2] = (state->pc + 2) & 0xff;
    state->sp -= 2;
    state->pc = (opcode[2] << 8) | opcode[1];
    state->pc--;
}

void Restart(State8080* state, uint16_t newaddr){
    state->memory[state->sp - 1] = ((state->pc + 1) >> 8) & 0xff;
    state->memory[state->sp - 2] = (state->pc + 1) & 0xff;
    state->sp -= 2;
    state->pc = newaddr;
}

void Pop(State8080* state, uint8_t *high, uint8_t *low){
    *low = state->memory[state->sp];
    *high = state->memory[state->sp + 1];
    state->sp += 2;
}

void Push(State8080* state, uint8_t *high, uint8_t *low){
    state->memory[state->sp - 1] = *high;
    state->memory[state->sp - 2] = *low;
    state->sp -= 2;
}

int Parity(uint8_t result){
    //Parity flag is set (meaning set to 1) if the sum of the 1 bits in the result of the operation is even, cleared (meaning set to 0) if it is odd. E.g if the result is 1000 0111, you have four 1s (even), so the flag is set.

    int count = 0; //"1"s so far
    int i = 0;

    //Run 8 times total, once for each bit.
    while (i < 8){
        //Get current rightmost bit. Add to count.
        count += (result & 1);
        //Shift bits to the right.
        result >>= 1;
        i++;
    }
    
    return (count % 2 == 0);
}

int LoadFile(uint8_t *state){
    FILE *invaders;
    long int bufsize;
    unsigned char *buffer;

    //Open file in read binary mode. "r" by itself would be read text, which stops 0x1B from being read (it gets read as an EOF if the file is opened in text mode).
    invaders = fopen("Place Game ROM Here\\InvadersFull.h", "rb");
    if (invaders == NULL){ printf("Error: File not found!");}

    //Load the entire file into memory. Start by finding the end of the file.
    if (fseek(invaders, 0L, SEEK_END) == 0){
        //Get the current position (position at the end of the file). Remember it so that we know how much memory to allocate for storage.
        bufsize = ftell(invaders);

        //Error checking.
        if (bufsize == -1){ printf("Error: could not create buffer size!\n"); }

        //Go back to the start of the file.
        if (fseek(invaders, 0L, SEEK_SET) != 0){ printf("Error: could not set the file pointer to the start of the file!\n"); }
    
        //Read the entire file into memory (into the buffer).
        fread(state, sizeof(uint8_t), bufsize, invaders);

    }

    fclose(invaders);

    return bufsize;

}

