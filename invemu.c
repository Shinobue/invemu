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

int LoadFile(uint8_t *);
void UnimplementedInstruction(State8080*);
void Emulate8080Op(State8080*);
int Parity(uint8_t);

int main(){

    int RAMoffset;

    //Init State8080.
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
        case 0xc0: UnimplementedInstruction(state); break;
        case 0xc1: UnimplementedInstruction(state); break;
        case 0xc2: UnimplementedInstruction(state); break;
        case 0xc3: UnimplementedInstruction(state); break;
        case 0xc4: UnimplementedInstruction(state); break;
        case 0xc5: UnimplementedInstruction(state); break;
        case 0xc6: UnimplementedInstruction(state); break;
        case 0xc7: UnimplementedInstruction(state); break;
        case 0xc8: UnimplementedInstruction(state); break;
        case 0xc9: UnimplementedInstruction(state); break;
        case 0xca: UnimplementedInstruction(state); break;
        case 0xcb: UnimplementedInstruction(state); break;
        case 0xcc: UnimplementedInstruction(state); break;
        case 0xcd: UnimplementedInstruction(state); break;
        case 0xce: UnimplementedInstruction(state); break;
        case 0xcf: UnimplementedInstruction(state); break;
        case 0xd0: UnimplementedInstruction(state); break;
        case 0xd1: UnimplementedInstruction(state); break;
        case 0xd2: UnimplementedInstruction(state); break;
        case 0xd3: UnimplementedInstruction(state); break;
        case 0xd4: UnimplementedInstruction(state); break;
        case 0xd5: UnimplementedInstruction(state); break;
        case 0xd6: UnimplementedInstruction(state); break;
        case 0xd7: UnimplementedInstruction(state); break;
        case 0xd8: UnimplementedInstruction(state); break;
        case 0xd9: UnimplementedInstruction(state); break;
        case 0xda: UnimplementedInstruction(state); break;
        case 0xdb: UnimplementedInstruction(state); break;
        case 0xdc: UnimplementedInstruction(state); break;
        case 0xdd: UnimplementedInstruction(state); break;
        case 0xde: UnimplementedInstruction(state); break;
        case 0xdf: UnimplementedInstruction(state); break;
        case 0xe0: UnimplementedInstruction(state); break;
        case 0xe1: UnimplementedInstruction(state); break;
        case 0xe2: UnimplementedInstruction(state); break;
        case 0xe3: UnimplementedInstruction(state); break;
        case 0xe4: UnimplementedInstruction(state); break;
        case 0xe5: UnimplementedInstruction(state); break;
        case 0xe6: UnimplementedInstruction(state); break;
        case 0xe7: UnimplementedInstruction(state); break;
        case 0xe8: UnimplementedInstruction(state); break;
        case 0xe9: UnimplementedInstruction(state); break;
        case 0xea: UnimplementedInstruction(state); break;
        case 0xeb: UnimplementedInstruction(state); break;
        case 0xec: UnimplementedInstruction(state); break;
        case 0xed: UnimplementedInstruction(state); break;
        case 0xee: UnimplementedInstruction(state); break;
        case 0xef: UnimplementedInstruction(state); break;
        case 0xf0: UnimplementedInstruction(state); break;
        case 0xf1: UnimplementedInstruction(state); break;
        case 0xf2: UnimplementedInstruction(state); break;
        case 0xf3: UnimplementedInstruction(state); break;
        case 0xf4: UnimplementedInstruction(state); break;
        case 0xf5: UnimplementedInstruction(state); break;
        case 0xf6: UnimplementedInstruction(state); break;
        case 0xf7: UnimplementedInstruction(state); break;
        case 0xf8: UnimplementedInstruction(state); break;
        case 0xf9: UnimplementedInstruction(state); break;
        case 0xfa: UnimplementedInstruction(state); break;
        case 0xfb: UnimplementedInstruction(state); break;
        case 0xfc: UnimplementedInstruction(state); break;
        case 0xfd: UnimplementedInstruction(state); break;
        case 0xfe: UnimplementedInstruction(state); break;
        case 0xff: UnimplementedInstruction(state); break;



    }
    printf("\n");
    state->pc+=1;
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

