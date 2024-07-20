#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "8080Emulator.h"
#include "InvadersMachine.h"

extern const int fileoutputflag;
extern const int printflag;
extern const int cpmflag;

extern uint16_t RAMoffset;

void Emulate8080Op(State8080* state, FILE *output){
    unsigned char *opcode = &state->memory[state->pc];

    //Prints location of current instruction (current value of pc), as well as its opcode and mnemonic. Also prints the byte(s) that follow the instruction, if applicable.
    if (printflag){
        Disassemble8080Op(state->memory, state->pc);
    }

    //If file output is enabled, print to file.
    if (fileoutputflag){
        Disassemble8080OpToFile(state->memory, state->pc, output);
    }

    switch(*opcode){
        case 0x00:  //NOP
            //No op
            //printf("NOP");
            state->cyclecount += 4;
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
            state->cyclecount += 10;
            break;
        case 0x02:  //STAX   B
            //Store accumulator indirect
            //(BC) <- A
            {  //Add curly braces to localise declared variables.
            uint16_t offset;
            offset = (state->b << 8) | state->c; //Create memory offset. Address width is 16 bits, so shift the B part 8 bits and OR in (or use +) the c part. Leftmost 8 bits are b, rightmost 8 bits are c.
            MemWrite(state, offset, state->a); //(BC) <- A. Memory located at the address pointed to by the contents of BC is loaded with a.
            state->cyclecount += 7;
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
            state->cyclecount += 5;
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
            //If the 4 rightmost bits of the result are less than those of the original value, then a value was carried into bit 4 (0001 0000), so set, otherwise reset.
            state->cc.ac = ((result & 0x0F) < (state->b & 0x0F));
            //Set actual result to be "result & 0xff", since the actual result is an 8-bit number, and 0xff is 0000 0000 1111 1111. I.e mask out the leftmost 8 bits.
            state->b = result & 0xff;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->b & 0x0F)); //For subtraction, processor performs addition on the two's complement of the number to subtrcat (i.e -1 here), and checks for a borrow in that addition. Therefore less than (<), just like with addition.
            state->b = result & 0xff;
            state->cyclecount += 5;
            }
            break;
        case 0x06:  //MVI    B
            //Move immediate
            //B <- Byte 2
            state->b = opcode[1];
            state->pc += 1;
            state->cyclecount += 7;
            break;
        case 0x07:  //RLC
            //Rotate left. Bit 7 (leftmost bit) becomes bit 0, and it also becomes cy.
            //A = A << 1; bit 0 = prev bit 7; CY = prev bit 7
            state->cc.cy = (state->a & 0x80) >> 7;
            state->a = (state->a << 1) | state->cc.cy;
            state->cyclecount += 4;
            break;
        case 0x08:  //NOP
            //No op
            state->cyclecount += 4;
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
            state->cyclecount += 10;
            }
            break;
        case 0x0a:  //LDAX   B
            //Load accumulator indirect
            //A <- (BC)
            {
            uint16_t offset;
            offset = (state->b << 8) | state->c;
            state->a = state->memory[offset];
            state->cyclecount += 7;
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
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->c & 0x0F));
            state->c = result & 0xff;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->c & 0x0F));
            state->c = result & 0xff;
            state->cyclecount += 5;
            }
            break;
        case 0x0e:  //MVI    C
            //Move immediate
            //C <- Byte 2
            state->c = opcode[1];
            state->pc += 1;
            state->cyclecount += 7;
            break;
        case 0x0f:  //RRC
            //Rotate right. Bit 0 (rightmost bit) becomes bit 7, and it also becomes cy.
            //A = A >> 1; bit 7 = prev bit 0; CY = prev bit 0
            state->cc.cy = state->a & 0x01; //Get rightmost value in A register.
            state->a = (state->a >> 1) | (state->cc.cy << 7); //Shift A register to the right 1 step, insert previous rightmost value into bit 7.
            state->cyclecount += 4;
            break;
        case 0x10:  //NOP
            //No op
            state->cyclecount += 4;
            break;
        case 0x11:  //LXI    D, word
            //Load register pair immediate
            //D <- Byte 3, E <- Byte 2
            state->d = opcode[2];
            state->e = opcode[1];
            state->pc += 2;
            state->cyclecount += 10;
            break;
        case 0x12:  //STAX   D
            //Store accumulator indirect
            //(DE) <- A
            {
            uint16_t offset;
            offset = (state->d << 8) | state->e;
            MemWrite(state, offset, state->a);
            state->cyclecount += 7;
            }
            break;
        case 0x13:  //INX    D
            //Increment register pair
            //DE <- DE + 1.
            state->e++;
            if (state->e == 0){
                state->d++;
            }
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->d & 0x0F));
            state->d = result & 0xff;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->d & 0x0F));
            state->d = result & 0xff;
            state->cyclecount += 5;
            }
            break;
        case 0x16:  //MVI    D
            //Move immediate
            //D <- Byte 2
            state->d = opcode[1];
            state->pc += 1;
            state->cyclecount += 7;
            break;
        case 0x17:  //RAL
            //Rotate left through carry
            //A = A << 1; bit 0 = prev CY; CY = prev bit 7
            {
            uint8_t result;
            result = (state->a << 1) | state->cc.cy;
            state->cc.cy = (state->a & 0x80) >> 7;
            state->a = result;
            state->cyclecount += 4;
            }
            break;
        case 0x18:  //NOP
            //No op
            state->cyclecount += 4;
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
            state->cyclecount += 10;
            }
            break;
        case 0x1a:  //LDAX   D
            //Load accumulator indirect
            //A <- (DE)
            {
            uint16_t offset;
            offset = (state->d << 8) | state->e;
            state->a = state->memory[offset];
            state->cyclecount += 7;
            }
            break;
        case 0x1b:  //DCX    D
            //Decrement register pair
            //DE = DE - 1.
            if (state->e == 0){
                state->d--;
            }
            state->e--;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->e & 0x0F));
            state->e = result & 0xff;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->e & 0x0F));
            state->e = result & 0xff;
            state->cyclecount += 5;
            }
            break;
        case 0x1e:  //MVI    E
            //Move immediate
            //E <- Byte 2
            state->e = opcode[1];
            state->pc += 1;
            state->cyclecount += 7;
            break;
        case 0x1f:  //RAR
            //Rotate right through carry
            //A = A >> 1; bit 7 = prev CY; CY = prev bit 0
            {
            uint8_t result;
            result = (state->a >> 1) | (state->cc.cy << 7);
            state->cc.cy = state->a & 0x01;
            state->a = result;
            state->cyclecount += 4;
            }
            break;
        case 0x20:  //NOP
            //No op
            state->cyclecount += 4;
            break;
        case 0x21:  //LXI    H, word
            //Load register pair immediate
            //H <- Byte 3, L <- Byte 2
            state->h = opcode[2];
            state->l = opcode[1];
            state->pc += 2;
            state->cyclecount += 10;
            break;
        case 0x22:  //SHLD   adr
            //Store H and L direct
            //(adr) <-L; (adr+1)<-H
            {
            uint16_t offset;
            offset = (opcode[2] << 8) | opcode[1];
            MemWrite(state, offset, state->l);
            MemWrite(state, offset + 1, state->h);
            state->pc += 2;
            state->cyclecount += 16;
            }
            break;
        case 0x23:  //INX    H
            //Increment register pair
            //HL <- HL + 1.
            state->l++;
            if (state->l == 0){
                state->h++;
            }
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->h & 0x0F));
            state->h = result & 0xff;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->h & 0x0F));
            state->h = result & 0xff;
            state->cyclecount += 5;
            }
            break;
        case 0x26:  //MVI    H
            //Move immediate
            //H <- Byte 2
            state->h = opcode[1];
            state->pc += 1;
            state->cyclecount += 7;
            break;
        case 0x27:  //DAA
            //Decimal Adjust Accumulator
            //Not used in space invaders
            {
            uint16_t result = state->a;
            if (((result & 0x0F) > 9) || state->cc.ac){
                result = result + 6; //Add 6 to the four rightmost bits.
                state->cc.ac = ((state->a & 0x0F) + 6 > 0x0F);
            }
            else{
                state->cc.ac = 0;
            }
            if (((result >> 4) > 9) || state->cc.cy){
                result = result + 96; //Add 6 to the four leftmost bits. Instead of shifting, just add 6 (0000 0110) shifted 4 bits to the left, which equals 96 (0110 0000).
            }
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            if (result > 0xff){
                state->cc.cy = 1; //Regular carry is unaffected on the DAA instruction if the result of the calculation did not produce a carry.
            }
            state->a = result;
            }
            state->cyclecount += 4;
            break;
        case 0x28:  //NOP
            //No op
            state->cyclecount += 4;
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
            state->cyclecount += 10;
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
            state->cyclecount += 16;
            }
            break;
        case 0x2b:  //DCX    H
            //Decrement register pair
            //HL = HL - 1.
            if (state->l == 0){
                state->h--;
            }
            state->l--;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->l & 0x0F));
            state->l = result & 0xff;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->l & 0x0F));
            state->l = result & 0xff;
            state->cyclecount += 5;
            }
            break;
        case 0x2e:  //MVI    L
            //Move immediate
            //L <- Byte 2
            state->l = opcode[1];
            state->pc += 1;
            state->cyclecount += 7;
            break;
        case 0x2f:  //CMA
            //Complement accumulator
            //A <- !A
            state->a = ~(state->a);
            state->cyclecount += 4;
            break;
        case 0x30:  //NOP
            //No op
            state->cyclecount += 4;
            break;
        case 0x31:  //LXI    SP, word
            //Load register pair immediate
            //SP(high) <- Byte 3, SP(low) <- Byte 2
            state->sp = opcode[2];
            state->sp = (state->sp << 8) | opcode[1];
            state->pc += 2;
            state->cyclecount += 10;
            break;
        case 0x32:  //STA    adr
            //Store Accumulator direct
            //(adr) <- A
            {
            uint16_t offset;
            offset = (opcode[2] << 8) | opcode[1];
            MemWrite(state, offset, state->a);
            state->pc += 2;
            state->cyclecount += 13;
            }
            break;
        case 0x33:  //INX    SP
            //Increment register pair
            //SP <- SP + 1.
            state->sp++;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->memory[offset] & 0x0F));
            MemWrite(state, offset, result & 0xff);
            state->cyclecount += 10;
            }
            break;
        case 0x35:  //DCR    M
            //Decrement memory
            //(HL) <- (HL) - 1
            {
            uint16_t result;
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            result = state->memory[offset] - 1;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.ac = ((result & 0x0F) < (state->memory[offset] & 0x0F));
            MemWrite(state, offset, result & 0xff);
            state->cyclecount += 10;
            }
            break;
        case 0x36:  //MVI    M
            //Move to memory immediate
            //(HL) <- Byte 2
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            MemWrite(state, offset, opcode[1]);
            state->pc += 1;
            state->cyclecount += 10;
            }
            break;
        case 0x37:  //STC
            //Set carry
            //CY = 1
            state->cc.cy = 0x01;
            state->cyclecount += 4;
            break;
        case 0x38:  //NOP
            //No op
            state->cyclecount += 4;
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
            state->cyclecount += 10;
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
            state->cyclecount += 13;
            }
            break;
        case 0x3b:  //DCX    SP
            //Decrement register pair
            //SP = SP - 1.
            state->sp--;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->a & 0x0F));
            state->a = result & 0xff;
            state->cyclecount += 5;
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
            state->cc.ac = ((result & 0x0F) < (state->a & 0x0F));
            state->a = result & 0xff;
            state->cyclecount += 5;
            }
            break;
        case 0x3e:  //MVI    A
            //Move immediate
            //A <- Byte 2
            state->a = opcode[1];
            state->pc += 1;
            state->cyclecount += 7;
            break;
        case 0x3f:  //CMC
            //Complement carry
            state->cc.cy = ~(state->cc.cy);
            state->cyclecount += 4;
            break;
        case 0x40:  //MOV    B,B
            //Move Register
            //B <- B
            state->b = state->b;
            state->cyclecount += 5;
            break;
        case 0x41:  //MOV    B,C
            //Move Register
            //B <- C
            state->b = state->c;
            state->cyclecount += 5;
            break;
        case 0x42:  //MOV    B,D
            //Move Register
            //B <- D
            state->b = state->d;
            state->cyclecount += 5;
            break;
        case 0x43:  //MOV    B,E
            //Move Register
            //B <- E
            state->b = state->e;
            state->cyclecount += 5;
            break;
        case 0x44:  //MOV    B,H
            //Move Register
            //B <- H
            state->b = state->h;
            state->cyclecount += 5;
            break;
        case 0x45:  //MOV    B,L
            //Move Register
            //B <- L
            state->b = state->l;
            state->cyclecount += 5;
            break;
        case 0x46:  //MOV    B,M
            //Move from memory
            //B <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->b = state->memory[offset];
            state->cyclecount += 7;
            }
            break;
        case 0x47:  //MOV    B,A
            //Move Register
            //B <- A
            state->b = state->a;
            state->cyclecount += 5;
            break;
        case 0x48:  //MOV    C,B
            //Move Register
            //C <- B
            state->c = state->b;
            state->cyclecount += 5;
            break;
        case 0x49:  //MOV    C,C
            //Move Register
            //C <- C
            state->c = state->c;
            state->cyclecount += 5;
            break;
        case 0x4a:  //MOV    C,D
            //Move Register
            //C <- D
            state->c = state->d;
            state->cyclecount += 5;
            break;
        case 0x4b:  //MOV    C,E
            //Move Register
            //C <- E
            state->c = state->e;
            state->cyclecount += 5;
            break;
        case 0x4c:  //MOV    C,H
            //Move Register
            //C <- H
            state->c = state->h;
            state->cyclecount += 5;
            break;
        case 0x4d:  //MOV    C,L
            //Move Register
            //C <- L
            state->c = state->l;
            state->cyclecount += 5;
            break;
        case 0x4e:  //MOV    C,M
            //Move from memory
            //C <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->c = state->memory[offset];
            state->cyclecount += 7;
            }
            break;
        case 0x4f:  //MOV    C,A
            //Move Register
            //C <- A
            state->c = state->a;
            state->cyclecount += 5;
            break;
        case 0x50:  //MOV    D,B
            //Move Register
            //D <- B
            state->d = state->b;
            state->cyclecount += 5;
            break;
        case 0x51:  //MOV    D,C
            //Move Register
            //D <- C
            state->d = state->c;
            state->cyclecount += 5;
            break;
        case 0x52:  //MOV    D,D
            //Move Register
            //D <- D
            state->d = state->d;
            state->cyclecount += 5;
            break;
        case 0x53:  //MOV    D,E
            //Move Register
            //D <- E
            state->d = state->e;
            state->cyclecount += 5;
            break;
        case 0x54:  //MOV    D,H
            //Move Register
            //D <- H
            state->d = state->h;
            state->cyclecount += 5;
            break;
        case 0x55:  //MOV    D,L
            //Move Register
            //D <- L
            state->d = state->l;
            state->cyclecount += 5;
            break;
        case 0x56:  //MOV    D,M
            //Move from memory
            //D <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->d = state->memory[offset];
            state->cyclecount += 7;
            }
            break;
        case 0x57:  //MOV    D,A
            //Move Register
            //D <- A
            state->d = state->a;
            state->cyclecount += 5;
            break;
        case 0x58:  //MOV    E,B
            //Move Register
            //E <- B
            state->e = state->b;
            state->cyclecount += 5;
            break;
        case 0x59:  //MOV    E,C
            //Move Register
            //E <- C
            state->e = state->c;
            state->cyclecount += 5;
            break;
        case 0x5a:  //MOV    E,D
            //Move Register
            //E <- D
            state->e = state->d;
            state->cyclecount += 5;
            break;
        case 0x5b:  //MOV    E,E
            //Move Register
            //E <- E
            state->e = state->e;
            state->cyclecount += 5;
            break;
        case 0x5c:  //MOV    E,H
            //Move Register
            //E <- H
            state->e = state->h;
            state->cyclecount += 5;
            break;
        case 0x5d:  //MOV    E,L
            //Move Register
            //E <- L
            state->e = state->l;
            state->cyclecount += 5;
            break;
        case 0x5e:  //MOV    E,M
            //Move from memory
            //E <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->e = state->memory[offset];
            state->cyclecount += 7;
            }
            break;
        case 0x5f:  //MOV    E,A
            //Move Register
            //E <- A
            state->e = state->a;
            state->cyclecount += 5;
            break;
        case 0x60:  //MOV    H,B
            //Move Register
            //H <- B
            state->h = state->b;
            state->cyclecount += 5;
            break;
        case 0x61:  //MOV    H,C
            //Move Register
            //H <- C
            state->h = state->c;
            state->cyclecount += 5;
            break;
        case 0x62:  //MOV    H,D
            //Move Register
            //H <- D
            state->h = state->d;
            state->cyclecount += 5;
            break;
        case 0x63:  //MOV    H,E
            //Move Register
            //H <- E
            state->h = state->e;
            state->cyclecount += 5;
            break;
        case 0x64:  //MOV    H,H
            //Move Register
            //H <- H
            state->h = state->h;
            state->cyclecount += 5;
            break;
        case 0x65:  //MOV    H,L
            //Move Register
            //H <- L
            state->h = state->l;
            state->cyclecount += 5;
            break;
        case 0x66:  //MOV    H,M
            //Move from memory
            //H <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->h = state->memory[offset];
            state->cyclecount += 7;
            }
            break;
        case 0x67:  //MOV    H,A
            //Move Register
            //H <- A
            state->h = state->a;
            state->cyclecount += 5;
            break;
        case 0x68:  //MOV    L,B
            //Move Register
            //L <- B
            state->l = state->b;
            state->cyclecount += 5;
            break;
        case 0x69:  //MOV    L,C
            //Move Register
            //L <- C
            state->l = state->c;
            state->cyclecount += 5;
            break;
        case 0x6a:  //MOV    L,D
            //Move Register
            //L <- D
            state->l = state->d;
            state->cyclecount += 5;
            break;
        case 0x6b:  //MOV    L,E
            //Move Register
            //L <- E
            state->l = state->e;
            state->cyclecount += 5;
            break;
        case 0x6c:  //MOV    L,H
            //Move Register
            //L <- H
            state->l = state->h;
            state->cyclecount += 5;
            break;
        case 0x6d:  //MOV    L,L
            //Move Register
            //L <- L
            state->l = state->l;
            state->cyclecount += 5;
            break;
        case 0x6e:  //MOV    L,M
            //Move from memory
            //L <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->l = state->memory[offset];
            state->cyclecount += 7;
            }
            break;
        case 0x6f:  //MOV    L,A
            //Move Register
            //L <- A
            state->l = state->a;
            state->cyclecount += 5;
            break;
        case 0x70:  //MOV    M,B
            //Move Register
            //(HL) <- B
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            MemWrite(state, offset, state->b);
            state->cyclecount += 7;
            }
            break;
        case 0x71:  //MOV    M,C
            //Move Register
            //(HL) <- C
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            MemWrite(state, offset, state->c);
            state->cyclecount += 7;
            }
            break;
        case 0x72:  //MOV    M,D
            //Move Register
            //(HL) <- D
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            MemWrite(state, offset, state->d);
            state->cyclecount += 7;
            }
            break;
        case 0x73:  //MOV    M,E
            //Move Register
            //(HL) <- E
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            MemWrite(state, offset, state->e);
            state->cyclecount += 7;
            }
            break;
        case 0x74:  //MOV    M,H
            //Move Register
            //(HL) <- H
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            MemWrite(state, offset, state->h);
            state->cyclecount += 7;
            }
            break;
        case 0x75:  //MOV    M,L
            //Move Register
            //(HL) <- L
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            MemWrite(state, offset, state->l);
            state->cyclecount += 7;
            }
            break;
        case 0x76:  //HLT
            //exit(0);
            state->cyclecount += 7;
            break;
        case 0x77:  //MOV    M,A
            //Move Register
            //(HL) <- A
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            MemWrite(state, offset, state->a);
            state->cyclecount += 7;
            }
            break;
        case 0x78:  //MOV    A,B
            //Move Register
            //A <- B
            state->a = state->b;
            state->cyclecount += 5;
            break;
        case 0x79:  //MOV    A,C
            //Move Register
            //A <- C
            state->a = state->c;
            state->cyclecount += 5;
            break;
        case 0x7a:  //MOV    A,D
            //Move Register
            //A <- D
            state->a = state->d;
            state->cyclecount += 5;
            break;
        case 0x7b:  //MOV    A,E
            //Move Register
            //A <- E
            state->a = state->e;
            state->cyclecount += 5;
            break;
        case 0x7c:  //MOV    A,H
            //Move Register
            //A <- H
            state->a = state->h;
            state->cyclecount += 5;
            break;
        case 0x7d:  //MOV    A,L
            //Move Register
            //A <- L
            state->a = state->l;
            state->cyclecount += 5;
            break;
        case 0x7e:  //MOV    A,M
            //Move from memory
            //A <- (HL)
            {
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            state->a = state->memory[offset];
            state->cyclecount += 7;
            }
            break;
        case 0x7f:  //MOV    A,A
            //Move Register
            //A <- A
            state->a = state->a;
            state->cyclecount += 5;
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
            state->cc.ac = ((((state->b ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->c ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->d ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->e ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->h ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->l ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->memory[offset] ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 7;
            }
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
            state->cc.ac = ((((state->a ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->b ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->c ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->d ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->e ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->h ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->l ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((((state->memory[offset] ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 7;
            }
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
            state->cc.ac = ((((state->a ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->b ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->c ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->d ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->e ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->h ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->l ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->memory[offset] ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 7;
            }
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
            state->cc.ac = (~(((state->a ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->b ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->c ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->d ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->e ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->h ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->l ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
            break;
        case 0x9e:  //SBB    M
            //Subtract memory with borrow
            //A <- A - (HL) - CY
            {
            uint16_t result;
            uint16_t offset;
            offset = (state->h << 8) | state->l;
            result = state->a - state->memory[offset] - state->cc.cy;
            state->cc.z = ((result & 0xff) == 0);
            state->cc.s = ((result & 0x80) != 0);
            state->cc.p = Parity((uint8_t) (result & 0xff));
            state->cc.ac = (~(((state->memory[offset] ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 7;
            }
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
            state->cc.ac = (~(((state->a ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((state->a & 0x8) | (state->b & 0x8)) >> 3; //ac is set to the logical OR of bit 3 of the values in operation. In this case, the OR of bit 3 (0000 1000) in A & B
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((state->a & 0x8) | (state->c & 0x8)) >> 3;
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((state->a & 0x8) | (state->d & 0x8)) >> 3;
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((state->a & 0x8) | (state->e & 0x8)) >> 3;
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((state->a & 0x8) | (state->h & 0x8)) >> 3;
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((state->a & 0x8) | (state->l & 0x8)) >> 3;
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = ((state->a & 0x8) | (state->memory[offset] & 0x8)) >> 3;
            state->a = result & 0xff;
            state->cyclecount += 7;
            }
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
            state->cc.ac = ((state->a & 0x8) | (state->a & 0x8)) >> 3;
            state->a = result & 0xff;
            state->cyclecount += 4;
            }
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
            state->cc.ac = 0x0; //Cleared according to the user's manual.
            state->a = result & 0xff;
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 7;
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
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 4;
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
            state->cyclecount += 7;
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
            state->cyclecount += 4;
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
            state->cc.ac = (~(((state->b ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff); //If (A - B) > 0xff, then a was less than b (since it loops around, starting at 0xFFFF and going down). Neither A nor B can be larger than 0xff. Same logic as in other commands higher up in this file.
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->c ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->d ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->e ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->h ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->l ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->cyclecount += 4;
            }
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
            state->cc.ac = (~(((state->memory[offset] ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->cyclecount += 7;
            }
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
            state->cc.ac = (~(((state->a ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->cyclecount += 4;
            }
            break;
        case 0xc0:  //RNZ
            //Conditional Return
            //If NZ (If the zero flag is zero, i.e if the result of the previous operation was not zero), do the following (operation is identical to RET):
            //(PCL) <- ((SP))
            //(PCH) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            if (state->cc.z == 0){
                Return(state);
                state->cyclecount++;
            }
            else{
                state->cyclecount += 5;
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
                state->cyclecount += 10;
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
                state->cyclecount += 11;
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
            state->cc.ac = ((((opcode[1] ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->pc += 1;
            state->cyclecount += 7;
            }
            break;
        case 0xc7:  //RST    0
            //Restart. CALL 0x0000
            Restart(state, 0x0000);
            break;
        case 0xc8:  //RZ
            //Conditional Return
            //If Z, RET:
            if (state->cc.z == 1){
                Return(state);
                state->cyclecount++;
            }
            else{
                state->cyclecount += 5;
            }
            break;
        case 0xc9:  //RET
            //Return
            //(PCL) <- ((SP))
            //(PCH) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            Return(state);
            break;
        case 0xca:  //JZ    D16
            //Conditional jump (zero)
            if (state->cc.z == 1){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 10;
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
                state->cyclecount += 11;
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
            state->cc.ac = ((((opcode[1] ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->pc += 1;
            state->cyclecount += 7;
            }
            break;
        case 0xcf:  //RST    1
            //Restart. CALL 0x0008
            Restart(state, 0x0008);
            break;
        case 0xd0:  //RNC
            //Conditional Return
            //If NCY, RET:
            if (state->cc.cy == 0){
                Return(state);
                state->cyclecount++;
            }
            else{
                state->cyclecount += 5;
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
                state->cyclecount += 10;
            }
            break;
        case 0xd3:  //OUT    D8
            //Output
            {
            uint8_t port = opcode[1];
            ProcessorOUT(state, port);
            state->pc++;
            state->cyclecount += 10;
            }
            break;
        case 0xd4:  //CNC    D16
            //Condition call (no carry)
            if (state->cc.cy == 0){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 11;
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
            state->cc.ac = (~(((opcode[1] ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->pc += 1;
            state->cyclecount += 7;
            }
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
                Return(state);
                state->cyclecount++;
            }
            else{
                state->cyclecount += 5;
            }
            break;
        case 0xd9:  //RET
            //Return
            //(PCL) <- ((SP))
            //(PCH) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            Return(state);
            break;
        case 0xda:  //JC    D16
            //Conditional jump (carry)
            if (state->cc.cy == 1){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 10;
            }
            break;
        case 0xdb:  //IN     D8
            //Input
            {
            uint8_t port = opcode[1];
            state->a = ProcessorIN(state, port);
            state->pc++;
            state->cyclecount += 10;
            }
            break;
        case 0xdc:  //CC     D16
            //Condition call (carry)
            if (state->cc.cy == 1){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 11;
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
            state->cc.ac = (~(((opcode[1] ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->a = result & 0xff;
            state->pc += 1;
            state->cyclecount += 7;
            }
            break;
        case 0xdf:  //RST    3
            //Restart. CALL 0x0018
            Restart(state, 0x0018);
            break;
        case 0xe0:  //RPO
            //Conditional Return
            //If Parity is odd, RET:
            if (state->cc.p == 0){
                Return(state);
                state->cyclecount++;
            }
            else{
                state->cyclecount += 5;
            }
            break;
        case 0xe1:  //POP    H
            //Pop stack
            //(L) <- ((SP))
            //(H) <- ((SP) + 1)
            //(SP) <- (SP) + 2
            Pop(state, &state->h, &state->l);
            break;
        case 0xe2:  //JPO    D16
            //Conditional jump (odd parity)
            if (state->cc.p == 0){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 10;
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
            MemWrite(state, state->sp, temp);
            temp = state->h;
            state->h = state->memory[state->sp + 1 & 0xFFFF];
            MemWrite(state, state->sp + 1 & 0xFFFF, temp);
            state->cyclecount += 18;
            }
            break;
        case 0xe4:  //CPO    D16
            //Condition call (odd parity)
            if (state->cc.p == 0){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 11;
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
            //state->cc.ac = 0; //According to the user's guide, auxiliary carry should be set to 0. Doing so will cause CPUTEST.COM to fail, so this emulator does not do this.
            state->cc.ac = ((state->a & 0x8) | (opcode[1] & 0x8)) >> 3; //Programmer's guide says that AND operations use this logic, does not say anything about excluding ANI D8, so this is included.
            state->a = result & 0xff;
            state->pc += 1;
            state->cyclecount += 7;
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
                Return(state);
                state->cyclecount++;
            }
            else{
                state->cyclecount += 5;
            }
            break;
        case 0xe9:  //PCHL
            //Jump H and L indirect - move H and L to PC
            state->pc = (state->h << 8) | state->l;
            state->pc--;
            state->cyclecount += 5;
            break;
        case 0xea:  //JPE    D16
            //Conditional jump (even parity)
            if (state->cc.p == 1){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 10;
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
            state->cyclecount += 5;
            }
            break;
        case 0xec:  //CPE    D16
            //Condition call (even parity)
            if (state->cc.p == 1){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 11;
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
            state->cyclecount += 7;
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
                Return(state);
                state->cyclecount++;
            }
            else{
                state->cyclecount += 5;
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
            state->a = state->memory[state->sp + 1 & 0xFFFF];
            state->sp += 2;
            state->cyclecount += 10;
            break;
        case 0xf2:  //JP     D16
            //Conditional jump (positive integer)
            if (state->cc.s == 0){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 10;
            }
            break;
        case 0xf3:  //DI
            //Disable interrupts
            state->int_enable = 0; break;
            state->cyclecount += 4;
        case 0xf4:  //CP     D16
            //Condition call (positive integer)
            if (state->cc.s == 0){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 11;
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
            MemWrite(state, state->sp - 1 & 0xFFFF, state->a);
            MemWrite(state, state->sp - 2 & 0xFFFF, state->cc.s & 0x01);
            MemWrite(state, state->sp - 2 & 0xFFFF, (state->memory[state->sp - 2 & 0xFFFF] << 1) | state->cc.z);
            MemWrite(state, state->sp - 2 & 0xFFFF, (state->memory[state->sp - 2 & 0xFFFF] << 1) | 0x0);
            MemWrite(state, state->sp - 2 & 0xFFFF, (state->memory[state->sp - 2 & 0xFFFF] << 1) | state->cc.ac);
            MemWrite(state, state->sp - 2 & 0xFFFF, (state->memory[state->sp - 2 & 0xFFFF] << 1) | 0x0);
            MemWrite(state, state->sp - 2 & 0xFFFF, (state->memory[state->sp - 2 & 0xFFFF] << 1) | state->cc.p);
            MemWrite(state, state->sp - 2 & 0xFFFF, (state->memory[state->sp - 2 & 0xFFFF] << 1) | 0x01);
            MemWrite(state, state->sp - 2 & 0xFFFF, (state->memory[state->sp - 2 & 0xFFFF] << 1) | state->cc.cy);
            state->sp -= 2;
            state->cyclecount += 11;
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
            state->cyclecount += 7;
            }
            break;
        case 0xf7:  //RST    6
            //Restart. CALL 0x0030
            Restart(state, 0x0030);
            break;
        case 0xf8:  //RM
            //Conditional Return
            //If sign flag is 1 (i.e result was a negative integer), RET:
            if (state->cc.s == 1){
                Return(state);
                state->cyclecount++;
            }
            else{
                state->cyclecount += 5;
            }
            break;
        case 0xf9:  //SPHL
            //Move HL to SP
            //(SP) <- (H) (L)
            state->sp = state->h << 8;
            state->sp = state->sp | state->l;
            state->cyclecount += 5;
            break;
        case 0xfa:  //JM     D16
            //Conditional jump (negative integer ("minus"))
            if (state->cc.s == 1){
                Jump(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 10;
            }
            break;
        case 0xfb:  //EI
            //Enable interrupts
            state->int_enable = 1;
            state->cyclecount += 4;
            break;
        case 0xfc:  //CM     D16
            //Condition call (negative integer ("minus"))
            if (state->cc.s == 1){
                Call(state, opcode);
            }
            else{
                state->pc += 2;
                state->cyclecount += 11;
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
            state->cc.ac = (~(((opcode[1] ^ state->a) & 0x10) ^ (result & 0x10)) >> 4);
            state->cc.cy = (result > 0xff);
            state->pc += 1;
            state->cyclecount += 7;
            }
            break;
        case 0xff:  //RST    7
            //Restart. CALL 0x0038
            Restart(state, 0x0038);
            break;
    }

    if (printflag){
    //Print processor state.
        printf("\t");
        printf("%c", state->cc.z ? 'z' : '.');
        printf("%c", state->cc.s ? 's' : '.');
        printf("%c", state->cc.p ? 'p' : '.');
        printf("%c", state->cc.cy ? 'c' : '.');
        printf("%c", state->cc.ac ? 'a' : '.');
        printf(" A %02X B %02X C %02X D %02X E %02X H %02X L %02X SP %04X", state->a, state->b, state->c, state->d, state->e, state->h, state->l, state->sp);
        printf("\n");
    }

    if (fileoutputflag){
        fprintf(output, "\t");
        fprintf(output, "%c", state->cc.z ? 'z' : '.');
        fprintf(output, "%c", state->cc.s ? 's' : '.');
        fprintf(output, "%c", state->cc.p ? 'p' : '.');
        fprintf(output, "%c", state->cc.cy ? 'c' : '.');
        fprintf(output, "%c", state->cc.ac ? 'a' : '.');
        fprintf(output, " A %02X B %02X C %02X D %02X E %02X H %02X L %02X SP %04X", state->a, state->b, state->c, state->d, state->e, state->h, state->l, state->sp);
        fprintf(output, "\n");
    }

    state->pc+=1;
}

void MemWrite(State8080* state, uint16_t location, uint8_t value){
    /*
    Value conversion (bytes):
    1k = 1024 = 0x400
    7k = 7168 = 0x1c00
    8k = 8192 = 0x2000
    16k = 16384 = 0x4000

    Memory map for Space Invaders:
    0x0000 - 0x1FFF = ROM
    0x2000 - 0x23FF = Work RAM
    0x2400 - 0x3FFF = Video RAM
    0x4000+ Memory mirror (goes back to RAM).

    First 8k is ROM. Must not be modified.
    Next 8k is RAM (of which 1k is work RAM, 7k is VRAM).
    Any attempt to write to a location at or above 0x2000 + 0x2000 = 0x4000 should instead write to RAM, since ram ends at 0x3FFF.
    Since RAM is 8k in size, mask off everything above 0x1FFF (8k contains values from 0x0000 - 0x1FFF), then add 0x2000, since you need to offset the location from ROM.
    By doing this, you will essentially get the modulus 8192 of that location, and then write to that location in RAM.
    So if the program attempts to write at location 0x5132, AND that with 0x1FFF which equals 0x1132, then add that to 0x2000 which equals RAM location 0x3132.

    Note: If using this processor emulator for non-space invaders emulation, this function may need to be changed.
    */

    if (location < RAMoffset && cpmflag == 0){
        //printf("Error: program attempts to write into ROM! Memory write was attempted at: %04X using the value %02X\n", location, value);
    }
    else if (location >= 0x4000 && cpmflag == 0){
        //printf("Program attempts to write outside of RAM space @ %04X, mirroring to RAM location %04X...\n", location, (location & 0x1FFF) + 0x2000);
        state->memory[(location & 0x1FFF) + 0x2000] = value;
    }
    else{
        state->memory[location] = value;
    }
}

void Jump(State8080* state, unsigned char *opcode){
    state->pc = (opcode[2] << 8) | opcode[1];
    state->pc--;
    state->cyclecount += 10;
}

void Call(State8080* state, unsigned char *opcode){
    //Store the address of the next instruction on the stack pointer (remember the stack "grows downward").
    MemWrite(state, state->sp - 1 & 0xFFFF, (state->pc + 3) >> 8); //The & 0xFFFF is for the event that sp is 0 or 1. Due to what I THINK is type promotion, state->sp - 1 becomes 0xFFFFFFFF instead of 0xFFFF.
    MemWrite(state, state->sp - 2 & 0xFFFF, (state->pc + 3) & 0xff);
    state->sp -= 2;
    state->pc = (opcode[2] << 8) | opcode[1];
    state->pc--; //Decrement pc, since the pc will get incremented after this function ends. Not decrementing would mean that the program starts at the CALLed address + 1.
    state->cyclecount += 17;
}

void Return(State8080* state){
    //SP contains a memory address. The contents of this address + 1 are put into the high order bits, and the contents of the address itself (i.e without the +1) are the low order bits.
    state->pc = (state->memory[state->sp + 1 & 0xFFFF] << 8) | state->memory[state->sp];
    state->sp += 2;
    state->pc--; //Decrement the pc so that the program starts at the return address rather than the return address +1 (pc will increment after this instruction).
    state->cyclecount += 10;
}

void Restart(State8080* state, uint16_t newaddr){
    MemWrite(state, state->sp - 1 & 0xFFFF, ((state->pc + 1) >> 8) & 0xff);
    MemWrite(state, state->sp - 2 & 0xFFFF, (state->pc + 1) & 0xff);
    state->sp -= 2;
    state->pc = newaddr;
    state->pc--;
    state->cyclecount += 11;
}

void Pop(State8080* state, uint8_t *high, uint8_t *low){
    *low = state->memory[state->sp];
    *high = state->memory[state->sp + 1 & 0xFFFF];
    state->sp += 2;
    state->cyclecount += 10;
}

void Push(State8080* state, uint8_t *high, uint8_t *low){
    MemWrite(state, state->sp - 1 & 0xFFFF, *high);
    MemWrite(state, state->sp - 2 & 0xFFFF, *low);
    state->sp -= 2;
    state->cyclecount += 11;
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

void Disassemble8080Op(unsigned char *codebuffer, int pc){
    unsigned char *code = &codebuffer[pc];
    int opbytes = 1;
    printf("%04x 0x%02x ", pc, *code);
    switch (*code)
    {
        case 0x00: printf("NOP            "); break;
        case 0x01: printf("LXI    B,#$%02x%02x", code[2], code[1]); opbytes=3; break;
        case 0x02: printf("STAX   B       "); break;
        case 0x03: printf("INX    B       "); break;
        case 0x04: printf("INR    B       "); break;
        case 0x05: printf("DCR    B       "); break;
        case 0x06: printf("MVI    B,#$%02x  ", code[1]); opbytes=2; break;
        case 0x07: printf("RLC            "); break;
        case 0x08: printf("NOP            "); break;
        case 0x09: printf("DAD    B       "); break;
        case 0x0a: printf("LDAX   B       "); break;
        case 0x0b: printf("DCX    B       "); break;
        case 0x0c: printf("INR    C       "); break;
        case 0x0d: printf("DCR    C       "); break;
        case 0x0e: printf("MVI    C,#$%02x  ", code[1]); opbytes = 2; break;
        case 0x0f: printf("RRC            "); break;
        case 0x10: printf("NOP            "); break;
        case 0x11: printf("LXI    D,#$%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0x12: printf("STAX   D       "); break;
        case 0x13: printf("INX    D       "); break;
        case 0x14: printf("INR    D       "); break;
        case 0x15: printf("DCR    D       "); break;
        case 0x16: printf("MVI    D,#$%02x  ", code[1]); opbytes = 2; break;
        case 0x17: printf("RAL            "); break;
        case 0x18: printf("NOP            "); break;
        case 0x19: printf("DAD    D       "); break;
        case 0x1a: printf("LDAX   D       "); break;
        case 0x1b: printf("DCX    D       "); break;
        case 0x1c: printf("INR    E       "); break;
        case 0x1d: printf("DCR    E       "); break;
        case 0x1e: printf("MVI    E,#$%02x  ", code[1]); opbytes = 2; break;
        case 0x1f: printf("RAR            "); break;
        case 0x20: printf("NOP            "); break;
        case 0x21: printf("LXI    H,#$%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0x22: printf("SHLD   $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0x23: printf("INX    H       "); break;
        case 0x24: printf("INR    H       "); break;
        case 0x25: printf("DCR    H       "); break;
        case 0x26: printf("MVI    H,#$%02x  ", code[1]); opbytes = 2; break;
        case 0x27: printf("DAA            "); break;
        case 0x28: printf("NOP            "); break;
        case 0x29: printf("DAD    H       "); break;
        case 0x2a: printf("LHLD   $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0x2b: printf("DCX    H       "); break;
        case 0x2c: printf("INR    L       "); break;
        case 0x2d: printf("DCR    L       "); break;
        case 0x2e: printf("MVI    L,#$%02x  ", code[1]); opbytes = 2; break;
        case 0x2f: printf("CMA            "); break;
        case 0x30: printf("NOP            "); break;
        case 0x31: printf("LXI    SP,#$%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0x32: printf("STA    $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0x33: printf("INX    SP      "); break;
        case 0x34: printf("INR    M       "); break;
        case 0x35: printf("DCR    M       "); break;
        case 0x36: printf("MVI    M,#$%02x  ", code[1]); opbytes = 2; break;
        case 0x37: printf("STC            "); break;
        case 0x38: printf("NOP            "); break;
        case 0x39: printf("DAD    SP      "); break;
        case 0x3a: printf("LDA    $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0x3b: printf("DCX    SP      "); break;
        case 0x3c: printf("INR    A       "); break;
        case 0x3d: printf("DCR    A       "); break;
        case 0x3e: printf("MVI    A,#$%02x  ", code[1]); opbytes = 2; break;
        case 0x3f: printf("CMC            "); break;
        case 0x40: printf("MOV    B,B     "); break;
        case 0x41: printf("MOV    B,C     "); break;
        case 0x42: printf("MOV    B,D     "); break;
        case 0x43: printf("MOV    B,E     "); break;
        case 0x44: printf("MOV    B,H     "); break;
        case 0x45: printf("MOV    B,L     "); break;
        case 0x46: printf("MOV    B,M     "); break;
        case 0x47: printf("MOV    B,A     "); break;
        case 0x48: printf("MOV    C,B     "); break;
        case 0x49: printf("MOV    C,C     "); break;
        case 0x4a: printf("MOV    C,D     "); break;
        case 0x4b: printf("MOV    C,E     "); break;
        case 0x4c: printf("MOV    C,H     "); break;
        case 0x4d: printf("MOV    C,L     "); break;
        case 0x4e: printf("MOV    C,M     "); break;
        case 0x4f: printf("MOV    C,A     "); break;
        case 0x50: printf("MOV    D,B     "); break;
        case 0x51: printf("MOV    D,C     "); break;
        case 0x52: printf("MOV    D,D     "); break;
        case 0x53: printf("MOV    D,E     "); break;
        case 0x54: printf("MOV    D,H     "); break;
        case 0x55: printf("MOV    D,L     "); break;
        case 0x56: printf("MOV    D,M     "); break;
        case 0x57: printf("MOV    D,A     "); break;
        case 0x58: printf("MOV    E,B     "); break;
        case 0x59: printf("MOV    E,C     "); break;
        case 0x5a: printf("MOV    E,D     "); break;
        case 0x5b: printf("MOV    E,E     "); break;
        case 0x5c: printf("MOV    E,H     "); break;
        case 0x5d: printf("MOV    E,L     "); break;
        case 0x5e: printf("MOV    E,M     "); break;
        case 0x5f: printf("MOV    E,A     "); break;
        case 0x60: printf("MOV    H,B     "); break;
        case 0x61: printf("MOV    H,C     "); break;
        case 0x62: printf("MOV    H,D     "); break;
        case 0x63: printf("MOV    H,E     "); break;
        case 0x64: printf("MOV    H,H     "); break;
        case 0x65: printf("MOV    H,L     "); break;
        case 0x66: printf("MOV    H,M     "); break;
        case 0x67: printf("MOV    H,A     "); break;
        case 0x68: printf("MOV    L,B     "); break;
        case 0x69: printf("MOV    L,C     "); break;
        case 0x6a: printf("MOV    L,D     "); break;
        case 0x6b: printf("MOV    L,E     "); break;
        case 0x6c: printf("MOV    L,H     "); break;
        case 0x6d: printf("MOV    L,L     "); break;
        case 0x6e: printf("MOV    L,M     "); break;
        case 0x6f: printf("MOV    L,A     "); break;
        case 0x70: printf("MOV    M,B     "); break;
        case 0x71: printf("MOV    M,C     "); break;
        case 0x72: printf("MOV    M,D     "); break;
        case 0x73: printf("MOV    M,E     "); break;
        case 0x74: printf("MOV    M,H     "); break;
        case 0x75: printf("MOV    M,L     "); break;
        case 0x76: printf("HLT            "); break;
        case 0x77: printf("MOV    M,A     "); break;
        case 0x78: printf("MOV    A,B     "); break;
        case 0x79: printf("MOV    A,C     "); break;
        case 0x7a: printf("MOV    A,D     "); break;
        case 0x7b: printf("MOV    A,E     "); break;
        case 0x7c: printf("MOV    A,H     "); break;
        case 0x7d: printf("MOV    A,L     "); break;
        case 0x7e: printf("MOV    A,M     "); break;
        case 0x7f: printf("MOV    A,A     "); break;
        case 0x80: printf("ADD    B       "); break;
        case 0x81: printf("ADD    C       "); break;
        case 0x82: printf("ADD    D       "); break;
        case 0x83: printf("ADD    E       "); break;
        case 0x84: printf("ADD    H       "); break;
        case 0x85: printf("ADD    L       "); break;
        case 0x86: printf("ADD    M       "); break;
        case 0x87: printf("ADD    A       "); break;
        case 0x88: printf("ADC    B       "); break;
        case 0x89: printf("ADC    C       "); break;
        case 0x8a: printf("ADC    D       "); break;
        case 0x8b: printf("ADC    E       "); break;
        case 0x8c: printf("ADC    H       "); break;
        case 0x8d: printf("ADC    L       "); break;
        case 0x8e: printf("ADC    M       "); break;
        case 0x8f: printf("ADC    A       "); break;
        case 0x90: printf("SUB    B       "); break;
        case 0x91: printf("SUB    C       "); break;
        case 0x92: printf("SUB    D       "); break;
        case 0x93: printf("SUB    E       "); break;
        case 0x94: printf("SUB    H       "); break;
        case 0x95: printf("SUB    L       "); break;
        case 0x96: printf("SUB    M       "); break;
        case 0x97: printf("SUB    A       "); break;
        case 0x98: printf("SBB    B       "); break;
        case 0x99: printf("SBB    C       "); break;
        case 0x9a: printf("SBB    D       "); break;
        case 0x9b: printf("SBB    E       "); break;
        case 0x9c: printf("SBB    H       "); break;
        case 0x9d: printf("SBB    L       "); break;
        case 0x9e: printf("SBB    M       "); break;
        case 0x9f: printf("SBB    A       "); break;
        case 0xa0: printf("ANA    B       "); break;
        case 0xa1: printf("ANA    C       "); break;
        case 0xa2: printf("ANA    D       "); break;
        case 0xa3: printf("ANA    E       "); break;
        case 0xa4: printf("ANA    H       "); break;
        case 0xa5: printf("ANA    L       "); break;
        case 0xa6: printf("ANA    M       "); break;
        case 0xa7: printf("ANA    A       "); break;
        case 0xa8: printf("XRA    B       "); break;
        case 0xa9: printf("XRA    C       "); break;
        case 0xaa: printf("XRA    D       "); break;
        case 0xab: printf("XRA    E       "); break;
        case 0xac: printf("XRA    H       "); break;
        case 0xad: printf("XRA    L       "); break;
        case 0xae: printf("XRA    M       "); break;
        case 0xaf: printf("XRA    A       "); break;
        case 0xb0: printf("ORA    B       "); break;
        case 0xb1: printf("ORA    C       "); break;
        case 0xb2: printf("ORA    D       "); break;
        case 0xb3: printf("ORA    E       "); break;
        case 0xb4: printf("ORA    H       "); break;
        case 0xb5: printf("ORA    L       "); break;
        case 0xb6: printf("ORA    M       "); break;
        case 0xb7: printf("ORA    A       "); break;
        case 0xb8: printf("CMP    B       "); break;
        case 0xb9: printf("CMP    C       "); break;
        case 0xba: printf("CMP    D       "); break;
        case 0xbb: printf("CMP    E       "); break;
        case 0xbc: printf("CMP    H       "); break;
        case 0xbd: printf("CMP    L       "); break;
        case 0xbe: printf("CMP    M       "); break;
        case 0xbf: printf("CMP    A       "); break;
        case 0xc0: printf("RNZ            "); break;
        case 0xc1: printf("POP    B       "); break;
        case 0xc2: printf("JNZ    $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xc3: printf("JMP    $%02x%02x   ",code[2],code[1]); opbytes = 3; break;
        case 0xc4: printf("CNZ    $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xc5: printf("PUSH   B       "); break;
        case 0xc6: printf("ADI    #$%02x    ", code[1]); opbytes = 2; break;
        case 0xc7: printf("RST    0       "); break;
        case 0xc8: printf("RZ             "); break;
        case 0xc9: printf("RET            "); break;
        case 0xca: printf("JZ     $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xcb: printf("JMP    $%02x%02x   ",code[2],code[1]); opbytes = 3; break;
        case 0xcc: printf("CZ     $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xcd: printf("CALL   $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xce: printf("ACI    #$%02x    ", code[1]); opbytes = 2; break;
        case 0xcf: printf("RST    1       "); break;
        case 0xd0: printf("RNC            "); break;
        case 0xd1: printf("POP    D       "); break;
        case 0xd2: printf("JNC    $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xd3: printf("OUT    #$%02x    ", code[1]); opbytes = 2; break;
        case 0xd4: printf("CNC    $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xd5: printf("PUSH   D       "); break;
        case 0xd6: printf("SUI    #$%02x    ", code[1]); opbytes = 2; break;
        case 0xd7: printf("RST    2       "); break;
        case 0xd8: printf("RC             "); break;
        case 0xd9: printf("RET            "); break;
        case 0xda: printf("JC     $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xdb: printf("IN     #$%02x    ", code[1]); opbytes = 2; break;
        case 0xdc: printf("CC     $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xdd: printf("CALL   $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xde: printf("SBI    #$%02x    ", code[1]); opbytes = 2; break;
        case 0xdf: printf("RST    3       "); break;
        case 0xe0: printf("RPO            "); break;
        case 0xe1: printf("POP    H       "); break;
        case 0xe2: printf("JPO    $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xe3: printf("XTHL           "); break;
        case 0xe4: printf("CPO    $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xe5: printf("PUSH   H       "); break;
        case 0xe6: printf("ANI    #$%02x    ", code[1]); opbytes = 2; break;
        case 0xe7: printf("RST    4       "); break;
        case 0xe8: printf("RPE            "); break;
        case 0xe9: printf("PCHL           "); break;
        case 0xea: printf("JPE    $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xeb: printf("XCHG           "); break;
        case 0xec: printf("CPE    $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xed: printf("CALL   $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xee: printf("XRI    #$%02x    ", code[1]); opbytes = 2; break;
        case 0xef: printf("RST    5       "); break;
        case 0xf0: printf("RP             "); break;
        case 0xf1: printf("POP    PSW     "); break;
        case 0xf2: printf("JP     $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xf3: printf("DI             "); break;
        case 0xf4: printf("CP     $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xf5: printf("PUSH   PSW     "); break;
        case 0xf6: printf("ORI    #$%02x    ", code[1]); opbytes = 2; break;
        case 0xf7: printf("RST    6       "); break;
        case 0xf8: printf("RM             "); break;
        case 0xf9: printf("SPHL           "); break;
        case 0xfa: printf("JM     $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xfb: printf("EI             "); break;
        case 0xfc: printf("CM     $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xfd: printf("CALL   $%02x%02x   ", code[2], code[1]); opbytes = 3; break;
        case 0xfe: printf("CPI    #$%02x    ", code[1]); opbytes = 2; break;
        case 0xff: printf("RST    7       "); break;

        /* ........ */
        /* ........ */
        /* ........ */
    }
}

void Disassemble8080OpToFile(unsigned char *codebuffer, int pc, FILE *output){
    unsigned char *code = &codebuffer[pc];
    int opbytes = 1;
    fprintf(output, "%04x 0x%02x ", pc, *code);
    switch (*code)
    {
        case 0x00: fprintf(output, "NOP             "); break;
        case 0x01: fprintf(output, "LXI    B,#$%02x%02x ", code[2], code[1]); opbytes=3; break;
        case 0x02: fprintf(output, "STAX   B        "); break;
        case 0x03: fprintf(output, "INX    B        "); break;
        case 0x04: fprintf(output, "INR    B        "); break;
        case 0x05: fprintf(output, "DCR    B        "); break;
        case 0x06: fprintf(output, "MVI    B,#$%02x   ", code[1]); opbytes=2; break;
        case 0x07: fprintf(output, "RLC             "); break;
        case 0x08: fprintf(output, "NOP             "); break;
        case 0x09: fprintf(output, "DAD    B        "); break;
        case 0x0a: fprintf(output, "LDAX   B        "); break;
        case 0x0b: fprintf(output, "DCX    B        "); break;
        case 0x0c: fprintf(output, "INR    C        "); break;
        case 0x0d: fprintf(output, "DCR    C        "); break;
        case 0x0e: fprintf(output, "MVI    C,#$%02x   ", code[1]); opbytes = 2; break;
        case 0x0f: fprintf(output, "RRC             "); break;
        case 0x10: fprintf(output, "NOP             "); break;
        case 0x11: fprintf(output, "LXI    D,#$%02x%02x ", code[2], code[1]); opbytes = 3; break;
        case 0x12: fprintf(output, "STAX   D        "); break;
        case 0x13: fprintf(output, "INX    D        "); break;
        case 0x14: fprintf(output, "INR    D        "); break;
        case 0x15: fprintf(output, "DCR    D        "); break;
        case 0x16: fprintf(output, "MVI    D,#$%02x   ", code[1]); opbytes = 2; break;
        case 0x17: fprintf(output, "RAL             "); break;
        case 0x18: fprintf(output, "NOP             "); break;
        case 0x19: fprintf(output, "DAD    D        "); break;
        case 0x1a: fprintf(output, "LDAX   D        "); break;
        case 0x1b: fprintf(output, "DCX    D        "); break;
        case 0x1c: fprintf(output, "INR    E        "); break;
        case 0x1d: fprintf(output, "DCR    E        "); break;
        case 0x1e: fprintf(output, "MVI    E,#$%02x   ", code[1]); opbytes = 2; break;
        case 0x1f: fprintf(output, "RAR             "); break;
        case 0x20: fprintf(output, "NOP             "); break;
        case 0x21: fprintf(output, "LXI    H,#$%02x%02x ", code[2], code[1]); opbytes = 3; break;
        case 0x22: fprintf(output, "SHLD   $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0x23: fprintf(output, "INX    H        "); break;
        case 0x24: fprintf(output, "INR    H        "); break;
        case 0x25: fprintf(output, "DCR    H        "); break;
        case 0x26: fprintf(output, "MVI    H,#$%02x   ", code[1]); opbytes = 2; break;
        case 0x27: fprintf(output, "DAA             "); break;
        case 0x28: fprintf(output, "NOP             "); break;
        case 0x29: fprintf(output, "DAD    H        "); break;
        case 0x2a: fprintf(output, "LHLD   $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0x2b: fprintf(output, "DCX    H        "); break;
        case 0x2c: fprintf(output, "INR    L        "); break;
        case 0x2d: fprintf(output, "DCR    L        "); break;
        case 0x2e: fprintf(output, "MVI    L,#$%02x   ", code[1]); opbytes = 2; break;
        case 0x2f: fprintf(output, "CMA             "); break;
        case 0x30: fprintf(output, "NOP             "); break;
        case 0x31: fprintf(output, "LXI    SP,#$%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0x32: fprintf(output, "STA    $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0x33: fprintf(output, "INX    SP       "); break;
        case 0x34: fprintf(output, "INR    M        "); break;
        case 0x35: fprintf(output, "DCR    M        "); break;
        case 0x36: fprintf(output, "MVI    M,#$%02x   ", code[1]); opbytes = 2; break;
        case 0x37: fprintf(output, "STC             "); break;
        case 0x38: fprintf(output, "NOP             "); break;
        case 0x39: fprintf(output, "DAD    SP       "); break;
        case 0x3a: fprintf(output, "LDA    $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0x3b: fprintf(output, "DCX    SP       "); break;
        case 0x3c: fprintf(output, "INR    A        "); break;
        case 0x3d: fprintf(output, "DCR    A        "); break;
        case 0x3e: fprintf(output, "MVI    A,#$%02x   ", code[1]); opbytes = 2; break;
        case 0x3f: fprintf(output, "CMC             "); break;
        case 0x40: fprintf(output, "MOV    B,B      "); break;
        case 0x41: fprintf(output, "MOV    B,C      "); break;
        case 0x42: fprintf(output, "MOV    B,D      "); break;
        case 0x43: fprintf(output, "MOV    B,E      "); break;
        case 0x44: fprintf(output, "MOV    B,H      "); break;
        case 0x45: fprintf(output, "MOV    B,L      "); break;
        case 0x46: fprintf(output, "MOV    B,M      "); break;
        case 0x47: fprintf(output, "MOV    B,A      "); break;
        case 0x48: fprintf(output, "MOV    C,B      "); break;
        case 0x49: fprintf(output, "MOV    C,C      "); break;
        case 0x4a: fprintf(output, "MOV    C,D      "); break;
        case 0x4b: fprintf(output, "MOV    C,E      "); break;
        case 0x4c: fprintf(output, "MOV    C,H      "); break;
        case 0x4d: fprintf(output, "MOV    C,L      "); break;
        case 0x4e: fprintf(output, "MOV    C,M      "); break;
        case 0x4f: fprintf(output, "MOV    C,A      "); break;
        case 0x50: fprintf(output, "MOV    D,B      "); break;
        case 0x51: fprintf(output, "MOV    D,C      "); break;
        case 0x52: fprintf(output, "MOV    D,D      "); break;
        case 0x53: fprintf(output, "MOV    D,E      "); break;
        case 0x54: fprintf(output, "MOV    D,H      "); break;
        case 0x55: fprintf(output, "MOV    D,L      "); break;
        case 0x56: fprintf(output, "MOV    D,M      "); break;
        case 0x57: fprintf(output, "MOV    D,A      "); break;
        case 0x58: fprintf(output, "MOV    E,B      "); break;
        case 0x59: fprintf(output, "MOV    E,C      "); break;
        case 0x5a: fprintf(output, "MOV    E,D      "); break;
        case 0x5b: fprintf(output, "MOV    E,E      "); break;
        case 0x5c: fprintf(output, "MOV    E,H      "); break;
        case 0x5d: fprintf(output, "MOV    E,L      "); break;
        case 0x5e: fprintf(output, "MOV    E,M      "); break;
        case 0x5f: fprintf(output, "MOV    E,A      "); break;
        case 0x60: fprintf(output, "MOV    H,B      "); break;
        case 0x61: fprintf(output, "MOV    H,C      "); break;
        case 0x62: fprintf(output, "MOV    H,D      "); break;
        case 0x63: fprintf(output, "MOV    H,E      "); break;
        case 0x64: fprintf(output, "MOV    H,H      "); break;
        case 0x65: fprintf(output, "MOV    H,L      "); break;
        case 0x66: fprintf(output, "MOV    H,M      "); break;
        case 0x67: fprintf(output, "MOV    H,A      "); break;
        case 0x68: fprintf(output, "MOV    L,B      "); break;
        case 0x69: fprintf(output, "MOV    L,C      "); break;
        case 0x6a: fprintf(output, "MOV    L,D      "); break;
        case 0x6b: fprintf(output, "MOV    L,E      "); break;
        case 0x6c: fprintf(output, "MOV    L,H      "); break;
        case 0x6d: fprintf(output, "MOV    L,L      "); break;
        case 0x6e: fprintf(output, "MOV    L,M      "); break;
        case 0x6f: fprintf(output, "MOV    L,A      "); break;
        case 0x70: fprintf(output, "MOV    M,B      "); break;
        case 0x71: fprintf(output, "MOV    M,C      "); break;
        case 0x72: fprintf(output, "MOV    M,D      "); break;
        case 0x73: fprintf(output, "MOV    M,E      "); break;
        case 0x74: fprintf(output, "MOV    M,H      "); break;
        case 0x75: fprintf(output, "MOV    M,L      "); break;
        case 0x76: fprintf(output, "HLT             "); break;
        case 0x77: fprintf(output, "MOV    M,A      "); break;
        case 0x78: fprintf(output, "MOV    A,B      "); break;
        case 0x79: fprintf(output, "MOV    A,C      "); break;
        case 0x7a: fprintf(output, "MOV    A,D      "); break;
        case 0x7b: fprintf(output, "MOV    A,E      "); break;
        case 0x7c: fprintf(output, "MOV    A,H      "); break;
        case 0x7d: fprintf(output, "MOV    A,L      "); break;
        case 0x7e: fprintf(output, "MOV    A,M      "); break;
        case 0x7f: fprintf(output, "MOV    A,A      "); break;
        case 0x80: fprintf(output, "ADD    B        "); break;
        case 0x81: fprintf(output, "ADD    C        "); break;
        case 0x82: fprintf(output, "ADD    D        "); break;
        case 0x83: fprintf(output, "ADD    E        "); break;
        case 0x84: fprintf(output, "ADD    H        "); break;
        case 0x85: fprintf(output, "ADD    L        "); break;
        case 0x86: fprintf(output, "ADD    M        "); break;
        case 0x87: fprintf(output, "ADD    A        "); break;
        case 0x88: fprintf(output, "ADC    B        "); break;
        case 0x89: fprintf(output, "ADC    C        "); break;
        case 0x8a: fprintf(output, "ADC    D        "); break;
        case 0x8b: fprintf(output, "ADC    E        "); break;
        case 0x8c: fprintf(output, "ADC    H        "); break;
        case 0x8d: fprintf(output, "ADC    L        "); break;
        case 0x8e: fprintf(output, "ADC    M        "); break;
        case 0x8f: fprintf(output, "ADC    A        "); break;
        case 0x90: fprintf(output, "SUB    B        "); break;
        case 0x91: fprintf(output, "SUB    C        "); break;
        case 0x92: fprintf(output, "SUB    D        "); break;
        case 0x93: fprintf(output, "SUB    E        "); break;
        case 0x94: fprintf(output, "SUB    H        "); break;
        case 0x95: fprintf(output, "SUB    L        "); break;
        case 0x96: fprintf(output, "SUB    M        "); break;
        case 0x97: fprintf(output, "SUB    A        "); break;
        case 0x98: fprintf(output, "SBB    B        "); break;
        case 0x99: fprintf(output, "SBB    C        "); break;
        case 0x9a: fprintf(output, "SBB    D        "); break;
        case 0x9b: fprintf(output, "SBB    E        "); break;
        case 0x9c: fprintf(output, "SBB    H        "); break;
        case 0x9d: fprintf(output, "SBB    L        "); break;
        case 0x9e: fprintf(output, "SBB    M        "); break;
        case 0x9f: fprintf(output, "SBB    A        "); break;
        case 0xa0: fprintf(output, "ANA    B        "); break;
        case 0xa1: fprintf(output, "ANA    C        "); break;
        case 0xa2: fprintf(output, "ANA    D        "); break;
        case 0xa3: fprintf(output, "ANA    E        "); break;
        case 0xa4: fprintf(output, "ANA    H        "); break;
        case 0xa5: fprintf(output, "ANA    L        "); break;
        case 0xa6: fprintf(output, "ANA    M        "); break;
        case 0xa7: fprintf(output, "ANA    A        "); break;
        case 0xa8: fprintf(output, "XRA    B        "); break;
        case 0xa9: fprintf(output, "XRA    C        "); break;
        case 0xaa: fprintf(output, "XRA    D        "); break;
        case 0xab: fprintf(output, "XRA    E        "); break;
        case 0xac: fprintf(output, "XRA    H        "); break;
        case 0xad: fprintf(output, "XRA    L        "); break;
        case 0xae: fprintf(output, "XRA    M        "); break;
        case 0xaf: fprintf(output, "XRA    A        "); break;
        case 0xb0: fprintf(output, "ORA    B        "); break;
        case 0xb1: fprintf(output, "ORA    C        "); break;
        case 0xb2: fprintf(output, "ORA    D        "); break;
        case 0xb3: fprintf(output, "ORA    E        "); break;
        case 0xb4: fprintf(output, "ORA    H        "); break;
        case 0xb5: fprintf(output, "ORA    L        "); break;
        case 0xb6: fprintf(output, "ORA    M        "); break;
        case 0xb7: fprintf(output, "ORA    A        "); break;
        case 0xb8: fprintf(output, "CMP    B        "); break;
        case 0xb9: fprintf(output, "CMP    C        "); break;
        case 0xba: fprintf(output, "CMP    D        "); break;
        case 0xbb: fprintf(output, "CMP    E        "); break;
        case 0xbc: fprintf(output, "CMP    H        "); break;
        case 0xbd: fprintf(output, "CMP    L        "); break;
        case 0xbe: fprintf(output, "CMP    M        "); break;
        case 0xbf: fprintf(output, "CMP    A        "); break;
        case 0xc0: fprintf(output, "RNZ             "); break;
        case 0xc1: fprintf(output, "POP    B        "); break;
        case 0xc2: fprintf(output, "JNZ    $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xc3: fprintf(output, "JMP    $%02x%02x    ",code[2],code[1]); opbytes = 3; break;
        case 0xc4: fprintf(output, "CNZ    $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xc5: fprintf(output, "PUSH   B        "); break;
        case 0xc6: fprintf(output, "ADI    #$%02x     ", code[1]); opbytes = 2; break;
        case 0xc7: fprintf(output, "RST    0        "); break;
        case 0xc8: fprintf(output, "RZ              "); break;
        case 0xc9: fprintf(output, "RET             "); break;
        case 0xca: fprintf(output, "JZ     $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xcb: fprintf(output, "JMP    $%02x%02x    ",code[2],code[1]); opbytes = 3; break;
        case 0xcc: fprintf(output, "CZ     $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xcd: fprintf(output, "CALL   $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xce: fprintf(output, "ACI    #$%02x     ", code[1]); opbytes = 2; break;
        case 0xcf: fprintf(output, "RST    1        "); break;
        case 0xd0: fprintf(output, "RNC             "); break;
        case 0xd1: fprintf(output, "POP    D        "); break;
        case 0xd2: fprintf(output, "JNC    $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xd3: fprintf(output, "OUT    #$%02x     ", code[1]); opbytes = 2; break;
        case 0xd4: fprintf(output, "CNC    $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xd5: fprintf(output, "PUSH   D        "); break;
        case 0xd6: fprintf(output, "SUI    #$%02x     ", code[1]); opbytes = 2; break;
        case 0xd7: fprintf(output, "RST    2        "); break;
        case 0xd8: fprintf(output, "RC              "); break;
        case 0xd9: fprintf(output, "RET             "); break;
        case 0xda: fprintf(output, "JC     $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xdb: fprintf(output, "IN     #$%02x     ", code[1]); opbytes = 2; break;
        case 0xdc: fprintf(output, "CC     $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xdd: fprintf(output, "CALL   $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xde: fprintf(output, "SBI    #$%02x     ", code[1]); opbytes = 2; break;
        case 0xdf: fprintf(output, "RST    3        "); break;
        case 0xe0: fprintf(output, "RPO             "); break;
        case 0xe1: fprintf(output, "POP    H        "); break;
        case 0xe2: fprintf(output, "JPO    $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xe3: fprintf(output, "XTHL            "); break;
        case 0xe4: fprintf(output, "CPO    $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xe5: fprintf(output, "PUSH   H        "); break;
        case 0xe6: fprintf(output, "ANI    #$%02x     ", code[1]); opbytes = 2; break;
        case 0xe7: fprintf(output, "RST    4        "); break;
        case 0xe8: fprintf(output, "RPE             "); break;
        case 0xe9: fprintf(output, "PCHL            "); break;
        case 0xea: fprintf(output, "JPE    $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xeb: fprintf(output, "XCHG            "); break;
        case 0xec: fprintf(output, "CPE    $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xed: fprintf(output, "CALL   $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xee: fprintf(output, "XRI    #$%02x     ", code[1]); opbytes = 2; break;
        case 0xef: fprintf(output, "RST    5        "); break;
        case 0xf0: fprintf(output, "RP              "); break;
        case 0xf1: fprintf(output, "POP    PSW      "); break;
        case 0xf2: fprintf(output, "JP     $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xf3: fprintf(output, "DI              "); break;
        case 0xf4: fprintf(output, "CP     $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xf5: fprintf(output, "PUSH   PSW      "); break;
        case 0xf6: fprintf(output, "ORI    #$%02x     ", code[1]); opbytes = 2; break;
        case 0xf7: fprintf(output, "RST    6        "); break;
        case 0xf8: fprintf(output, "RM              "); break;
        case 0xf9: fprintf(output, "SPHL            "); break;
        case 0xfa: fprintf(output, "JM     $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xfb: fprintf(output, "EI              "); break;
        case 0xfc: fprintf(output, "CM     $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xfd: fprintf(output, "CALL   $%02x%02x    ", code[2], code[1]); opbytes = 3; break;
        case 0xfe: fprintf(output, "CPI    #$%02x     ", code[1]); opbytes = 2; break;
        case 0xff: fprintf(output, "RST    7        "); break;

        /* ........ */
        /* ........ */
        /* ........ */
    }
}

