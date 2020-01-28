#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

#include "instruction.h"
#include "printRoutines.h"

// By setting the flag, we can know whether we use the stack pointer or not
int useRsp = 0;
/* Reads one byte from memory, at the specified address. Stores the
   read value into *value. Returns 1 in case of success, or 0 in case
   of failure (e.g., if the address is beyond the limit of the memory
   size). */
int memReadByte(machine_state_t *state,	uint64_t address, uint8_t *value) {
  if(address > state->programSize || address < 0) {
    return 0;
  }

    // address = relative addr
  *value = state->programMap[address];

  /* THIS PART TO BE COMPLETED BY THE STUDENT */
  return 1;
}

/* Reads one quad-word (64-bit number) from memory in little-endian
   format, at the specified starting address. Stores the read value
   into *value. Returns 1 in case of success, or 0 in case of failure
   (e.g., if the address is beyond the limit of the memory size). */

// little -> big
int memReadQuadLE(machine_state_t *state, uint64_t address, uint64_t *value) {
  if (useRsp == 0) {
    if(address > state->programSize || address < 0) {
      return 0;
    }
  }

  // 8-byte array
  uint64_t read64Bytes[8];

  for (int i = 0; i < 8 ;i++) {
     read64Bytes[7 - i] = (state->programMap)[address + i];
  }


  // big-endian -> little-endian
  *value =  ((read64Bytes[7]) + (read64Bytes[6] << 8) + (read64Bytes[5] << 16) + (read64Bytes[4] << 24) +
             (read64Bytes[3] << 32) + (read64Bytes[2] << 40) + (read64Bytes[1] << 48)  + (read64Bytes[0] << 56));

  /* THIS PART TO BE COMPLETED BY THE STUDENT */
  return 1;

}


/* Stores the specified one-byte value into memory, at the specified
   address. Returns 1 in case of success, or 0 in case of failure
   (e.g., if the address is beyond the limit of the memory size). */
int memWriteByte(machine_state_t *state,  uint64_t address, uint8_t value) {

  if(address > state->programSize || address < 0) {
    return 0;
  }
  state->programMap[address] = value;
  /* THIS PART TO BE COMPLETED BY THE STUDENT */
  return 1;
}

/* Stores the specified quad-word (64-bit) value into memory, at the
   specified start address, using little-endian format. Returns 1 in
   case of success, or 0 in case of failure (e.g., if the address is
   beyond the limit of the memory size). */
int memWriteQuadLE(machine_state_t *state, uint64_t address, uint64_t value) {
  // big-endian to little-endian

  if (useRsp == 0) {
    if(address > state->programSize || address < 0) {
      return 0;
    }
  }

  uint64_t eachByte[8];

  eachByte[0] = value & 0x00000000000000ff;
  eachByte[1] = (value & 0x000000000000ff00) >> 8;
  eachByte[2] = (value & 0x0000000000ff0000) >> 16;
  eachByte[3] = (value & 0x00000000ff000000) >> 24;
  eachByte[4] = (value & 0x000000ff00000000) >> 32;
  eachByte[5] = (value & 0x0000ff0000000000) >> 40;
  eachByte[6] = (value & 0x00ff000000000000) >> 48;
  eachByte[7] = (value & 0xff00000000000000) >> 56;

  for (int i=0 ; i<8 ; i++) {
    state->programMap[address + i] = (uint8_t) eachByte[i];

  }

  /* THIS PART TO BE COMPLETED BY THE STUDENT */
  return 1;
}


/* Fetches one instruction from memory, at the address specified by
   the program counter. Does not modify the machine's state. The
   resulting instruction is stored in *instr. Returns 1 if the
   instruction is a valid non-halt instruction, or 0 (zero)
   otherwise. */

int fetchInstruction(machine_state_t *state, y86_instruction_t *instr) {
    // reset rsp to be "unused" state
    useRsp = 0;

    uint8_t icodeAndifun;

    if (!memReadByte(state, state->programCounter, &icodeAndifun)) {
      instr->icode = I_TOO_SHORT;
      return 0;
    }

    // get icode and ifun
    y86_icode_t iicode = (icodeAndifun) >> 4;
    uint8_t iifun = icodeAndifun & 0x0F;


    // refactoring
    instr->location = state->programCounter;

  switch (iicode) {
    case 0:
      // halt

      if(iifun != 0) {
        instr->icode = I_INVALID;
        return 0;
      }

      instr->icode = I_HALT;
      instr->ifun = 0x0;
      instr->valP = state->programCounter + 1;
      break;

    case 1:
    // nop

      if(iifun != 0) {
        instr->icode = I_INVALID;
        return 0;
      }

      instr->icode = I_NOP;
      instr->ifun = 0x0;
      //instr->location = state->programCounter;
      instr->valP = state->programCounter + 1;
      break;

    case 2:
    // rrmovq rA, rB
    // cmovXX rA, rB


      if (iifun > 6 || iifun < 0) {
        instr->icode = I_INVALID;
        return 0;
      }

      instr->icode = I_RRMVXX;
      instr->ifun = iifun;


      instr->valP = state->programCounter + 2;
      uint8_t registers;
      if (!memReadByte(state, (state->programCounter) + 1, &registers)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->rA = registers >> 4;
      instr->rB = registers & 0x0f;
      instr->valP = state->programCounter + 2;
      break;

    case 3:
       // irmovq V, rB

       if(iifun != 0) {
         instr->icode = I_INVALID;
         return 0;
       }

      instr->icode = I_IRMOVQ;
      instr->ifun = 0x0;


      uint8_t rrB;
      if (!memReadByte(state, (state->programCounter)+1, &rrB)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->rB = rrB & 0x0f;

      uint64_t v3valC;
      if (!memReadQuadLE(state, (state->programCounter) + 2, &v3valC)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->valC = v3valC;
      instr->valP = state->programCounter + 10;
      break;

    case 4:
      // rmmovq rA, D(rB)

      if(iifun != 0) {
        instr->icode = I_INVALID;
        return 0;
      }

      instr->icode = I_RMMOVQ;

      instr->ifun = 0x0;


      uint8_t r4register;
      if (!memReadByte(state, (state->programCounter)+1, &r4register)){
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->rA = r4register >> 4;
      instr->rB = r4register & 0x0f;

      uint64_t vvvalC;
      if (!memReadQuadLE(state, (state->programCounter)+2, &vvvalC)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->valC = vvvalC;
      instr->valP = state->programCounter + 10;
      break;

    case 5:
      // mrmovq D(rB), rA

      if(iifun != 0) {
        instr->icode = I_INVALID;
        return 0;
      }

      instr->icode = I_MRMOVQ;
      instr->ifun = 0x0;

      uint8_t r5register;
      if (!memReadByte(state, state->programCounter + 1, &r5register)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->rA = r5register >> 4;
      instr->rB = r5register & 0x0F;

      uint64_t v5valC;
      if (!memReadQuadLE(state, state->programCounter + 2, &v5valC)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->valC = v5valC;
      instr->valP = state->programCounter + 10;

      break;

    case 6:
      // OPq rA, rB

      if (iifun >= 7 || iifun < 0) {
        instr->icode = I_INVALID;
        return 0;
      }

      instr->icode = I_OPQ;

      uint8_t i6ifun;
      if (!memReadByte(state, state->programCounter, &i6ifun)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->ifun = i6ifun & 0x0f;

      uint8_t r6register;
      if (!memReadByte(state, state->programCounter + 1, &r6register)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->rA = r6register >> 4;
      instr->rB = r6register & 0x0f;
      instr->valP = state->programCounter + 2;
      break;

    case 7:
      // jXX Dest

      if (iifun >= 7 || iifun < 0) {
        instr->icode = I_INVALID;
        return 0;
      }

      instr->icode = I_JXX;

      uint8_t i7ifun;
      if (!memReadByte(state, state->programCounter, &i7ifun)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->ifun = i7ifun & 0x0f;


      uint64_t v7valC;
      if (!memReadQuadLE(state, state->programCounter + 1, &v7valC)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->valC = v7valC;
      instr->valP = state->programCounter + 9;
      break;

    case 8:
      // call Dest

      if(iifun != 0) {
        instr->icode = I_INVALID;
        return 0;
      }

      instr->icode = I_CALL;
      instr->ifun = 0x0;

      uint64_t v8valC;
      if (!memReadQuadLE(state, state->programCounter + 1, &v8valC)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->valC = v8valC;
      instr->valP = state->programCounter + 9;
      break;

    case 9:
      // ret

      if(iifun != 0) {
        instr->icode = I_INVALID;
        return 0;
      }

      instr->icode = I_RET;
      instr->ifun = 0x0;
      instr->valP = state->programCounter + 1;
      break;

    case 0xA:
      // pushq rA

      if(iifun != 0) {
        instr->icode = I_INVALID;
        return 0;
      }

      instr->icode = I_PUSHQ;
      instr->ifun = 0x0;
      uint8_t rrA;
      if (!memReadByte(state, (state->programCounter) + 1, &rrA)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->rA = rrA >> 4;

      instr->valP = state->programCounter + 2;
      break;

    case 0xB:
       // popq rA

       if(iifun != 0) {
         instr->icode = I_INVALID;
         return 0;
       }

      instr->icode = I_POPQ;
      instr->ifun = 0x0;
      uint8_t rBrA;
      if (!memReadByte(state, (state->programCounter) + 1, &rBrA)) {
        instr->icode = I_TOO_SHORT;
        return 0;
      }
      instr->rA = rBrA >> 4;

      instr->valP = state->programCounter + 2;
      break;


    default:
      instr->icode = I_INVALID;
      return 0;
  }

  /* THIS PART TO BE COMPLETED BY THE STUDENT */
  return 1;
}



/* Executes the instruction specified by *instr, modifying the
   machine's state (memory, registers, condition codes, program
   counter) in the process. Returns 1 if the instruction was executed
   successfully, or 0 if there was an error. Typical errors include an
   invalid instruction or a memory access to an invalid address. */
// instr -> state
int executeInstruction(machine_state_t *state, y86_instruction_t *instr) {
  useRsp = 0;
  y86_icode_t iicode =  instr->icode;
  uint8_t iifun = instr->ifun;

  uint8_t ccZero;
  uint8_t ccSign;

  uint64_t valKK = 1;

  uint64_t valBB;

  switch(iicode) {
    case I_HALT:
    // halt
    state->programCounter = instr->valP ;
    return 0;
    break;

    case I_NOP:
    // nop
    state->programCounter = instr->valP ;
    break;

    case I_RRMVXX:
    // rrmovq rA, rB
    // cmovXX rA, rB
    ccZero = (state->conditionCodes & CC_ZERO_MASK);
    ccSign = (state->conditionCodes & CC_SIGN_MASK);

    // based on the ifun, check the validity
    switch(iifun){
      case C_NC:
        state->registerFile[instr->rB] = state->registerFile[instr->rA];
        break;
      case C_LE:
        if(ccZero || ccSign) {
          state->registerFile[instr->rB] = state->registerFile[instr->rA];
        }
        break;
      case C_L:
        if(ccSign) {
          state->registerFile[instr->rB] = state->registerFile[instr->rA];
        }
        break;
      case C_E:
        if(ccZero) {
          state->registerFile[instr->rB] = state->registerFile[instr->rA];
        }
        break;
      case C_NE:
        if(!ccZero) {
          state->registerFile[instr->rB] = state->registerFile[instr->rA];
        }
        break;
      case C_GE:
        if(!ccSign) {
          state->registerFile[instr->rB] = state->registerFile[instr->rA];
        }
        break;
      case C_G:
        if(ccZero && !ccSign) {
          state->registerFile[instr->rB] = state->registerFile[instr->rA];
        }
        break;
      default:
        return 0;
        break;
    }
    state->programCounter = instr->valP;
    break;

    case I_IRMOVQ:
    // irmovq V, rB
    state->registerFile[instr->rB]=instr->valC;
    state->programCounter = instr->valP ;
    break;

    case I_RMMOVQ:
    // rmmovq rA, D(rB)
    memWriteQuadLE(state, state->registerFile[instr->rB] + instr->valC, state->registerFile[instr->rA]);
    state->programCounter = instr->valP ;
    break;

    case I_MRMOVQ:
    // mrmovq D(rB), rA

    memReadQuadLE(state , state->registerFile[instr->rB] + instr->valC, &(state->registerFile[instr->rA]));
    state->programCounter = instr->valP ;
    break;

    case I_OPQ:
    // OPq rA, rB
    switch(instr->ifun) {
      case A_ADDQ:
      valKK = state->registerFile[instr->rA] + state->registerFile[instr->rB];
      break;
      case A_SUBQ:
      valKK = state->registerFile[instr->rB] - state->registerFile[instr->rA];
      break;
      case A_ANDQ:
      valKK = state->registerFile[instr->rB] & state->registerFile[instr->rA];
      break;
      case A_XORQ:
      valKK = state->registerFile[instr->rB] ^ state->registerFile[instr->rA];
      break;
      case A_MULQ:
      valKK = state->registerFile[instr->rB] * state->registerFile[instr->rA];
      break;
      case A_DIVQ:
      valKK = state->registerFile[instr->rB] / state->registerFile[instr->rA];
      break;
      case A_MODQ:
      valKK = state->registerFile[instr->rB] % state->registerFile[instr->rA];
      break;
      default:
      break;
    }

    state->registerFile[instr->rB] = valKK;
    if (valKK == 0) {
      state->conditionCodes = (uint8_t) 0x1;
    } else if ((valKK >> 63) == 1) {
      state->conditionCodes = (uint8_t) 0x2;
    } else {
      state->conditionCodes = (uint8_t) 0x0;
    }
    state->programCounter = instr->valP ;
    break;

    case I_JXX:
    // jXX Dest
    if(instr->valC > *(state->programMap) + state->programSize) {
      return 0;
      break;
    }

    ccZero = (state->conditionCodes & CC_ZERO_MASK);
    ccSign = (state->conditionCodes & CC_SIGN_MASK);

    switch(iifun){
      case C_NC:
      // no condition: PC = valC
        state->programCounter = instr->valC;
        break;

      case C_LE:
        if(ccZero || ccSign) {
          state->programCounter = instr->valC;
        }
        else {
          state->programCounter = instr->valP;
        }
        break;

      case C_L:
        if(ccSign) {
          state->programCounter = instr->valC;
        } else {
          state->programCounter = instr->valP;
        }
        break;

      case C_E:
        if(ccZero) {
         state->programCounter = instr->valC;
        } else {
          state->programCounter = instr->valP;
        }
        break;

      case C_NE:
        if(!ccZero) {
          state->programCounter = instr->valC;
        } else {
          state->programCounter = instr->valP;
        }
        break;

      case C_GE:
        if(!ccSign) {
          state->programCounter = instr->valC;
        } else {
          state->programCounter = instr->valP;
        }
        break;

      case C_G:
        if(ccZero && !ccSign) {
          state->programCounter = instr->valC;
        } else {
          state->programCounter = instr->valP;
        }
        break;

      default:
        return 0;
        break;
    }
    break;

    case I_CALL:
    // call Dest
    state->registerFile[4] -= 8;
    useRsp = 1;
    memWriteQuadLE(state, state->registerFile[4], instr->valP);

    state->programCounter = instr->valC;

    break;

    case I_RET:
    // ret
    useRsp = 1;
    memReadQuadLE(state, state->registerFile[4], &state->programCounter);
    state->registerFile[4] += 8;

    break;

    case I_PUSHQ:
    // pushq rA
    state->registerFile[4] -= 8;  // rsp -= 8
    valBB = state->registerFile[instr->rA];
    memWriteQuadLE(state, state->registerFile[4], valBB);
    state->programCounter = instr->valP ;
    break;

    case I_POPQ:
    // popq rB
    state->registerFile[4] += 8;
    memReadQuadLE(state, state->registerFile[4] - 8, state->registerFile + instr->rA);
    state->programCounter = instr->valP ;
    break;

    default:
      return 0;
      break;
  }
  /* THIS PART TO BE COMPLETED BY THE STUDENT */
  return 1;
}
