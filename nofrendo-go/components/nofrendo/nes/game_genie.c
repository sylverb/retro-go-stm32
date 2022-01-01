#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>

enum GGDecodeResponse {
  GGDecodeOk = 0,
  GGDecodeCodeIsNull = -1,
  GGDecodeInvalidCodeLength = -2,
  GGDecodeInvalidCharacter = -3
}; 

const char GGToHex[26] = {
  0x00, // A
  0xFF, // B
  0xFF, // C
  0xFF, // D
  0x08, // E
  0xFF, // F
  0x04, // G
  0xFF, // H
  0x05, // I
  0xFF, // J
  0x0C, // K
  0x03, // L
  0xFF, // M
  0x0F, // N
  0x09, // O
  0x01, // P
  0xFF, // Q
  0xFF, // R
  0x0D, // S
  0x06, // T
  0x0B, // U
  0x0E, // V
  0xFF, // W
  0x0A, // X
  0x07, // Y
  0x02  // Z
};

char gg(char code) {
  char h;
  if (code >= 'A' && code <= 'Z') {
    h = GGToHex[code - 'A'];
  }
  else if (code >= 'a' && code <= 'z') {
    h = GGToHex[code - 'a'];
  }
  else {
    return -1;
  }

  return h;
}


enum GGDecodeResponse decodeGameGenieCode(const char *code, unsigned short *addressOut, unsigned char *valueOut, short *compareValueOut) {
  *addressOut = 0;
  *valueOut = 0;
  *compareValueOut = -1;

  if (code == NULL) {
    return GGDecodeCodeIsNull;
  }

  int len = strnlen(code, 9); // 8 + nil
  if (len != 6 && len != 8) {
    return GGDecodeInvalidCodeLength;
  }

  for (int i=0; i<len; i++) {
    if (gg(code[i]) == -1) {
      return GGDecodeInvalidCharacter;
    }
  }

  *addressOut = 0x8000 |
                ((gg(code[3]) & 0x7) << 12) |
                ((gg(code[5]) & 0x7) <<  8) |
                ((gg(code[4]) & 0x8) <<  8) |
                ((gg(code[2]) & 0x7) <<  4) |
                ((gg(code[1]) & 0x8) <<  4) |
                ((gg(code[4]) & 0x7) <<  0) |
                ((gg(code[3]) & 0x8) <<  0);

  if (len == 6) {
    *valueOut = ((gg(code[1]) & 0x7) << 4) |
                ((gg(code[0]) & 0x8) << 4) |
                ((gg(code[0]) & 0x7) << 0) |
                ((gg(code[5]) & 0x8) << 0);
  } else { // len == 8
    *valueOut = ((gg(code[1]) & 0x7) << 4) |
                ((gg(code[0]) & 0x8) << 4) |
                ((gg(code[0]) & 0x7) << 0) |
                ((gg(code[7]) & 0x8) << 0);
    *compareValueOut = ((gg(code[7]) & 0x7) << 4) |
                       ((gg(code[6]) & 0x8) << 4) |
                       ((gg(code[6]) & 0x7) << 0) |
                       ((gg(code[5]) & 0x8) << 0);;
  }

  return GGDecodeOk;
}

typedef struct game_genie_code
{
    unsigned short addr;
    unsigned char val;
    short cmp;
} game_genie_code_t;

static game_genie_code_t *gg_active_codes = NULL;
static int gg_active_codes_count = 0;
static int gg_active_codes_size = 1;

void gameGeniePatchRom(unsigned char (*getbyte)(unsigned int addr), void (*putbyte)(unsigned int addr, unsigned char val)) {
    for(int i=0; i<gg_active_codes_count; i++) {
        game_genie_code_t code = gg_active_codes[i];
        if (code.cmp == -1) { // Size 6 code
            putbyte(code.addr, code.val);
        } else { // Size 8 code
            unsigned char val = getbyte(code.addr);
            if((unsigned char)code.cmp == val) {
                putbyte(code.addr, code.val);
            }
        }
    }
}

void patchAddress(unsigned short addr, unsigned char val, short cmp) {
    gg_active_codes[gg_active_codes_count].addr = addr;
    gg_active_codes[gg_active_codes_count].val = val;
    gg_active_codes[gg_active_codes_count].cmp = cmp;
    gg_active_codes_count++;

    if (gg_active_codes_count == gg_active_codes_size) {
        gg_active_codes_size *= 2;
        gg_active_codes = realloc(gg_active_codes, gg_active_codes_size * sizeof(game_genie_code_t));
    }
}

int loadGameGenieCode(const char *gameGenieCode) {
    unsigned short addr;
    unsigned char val;
    short cmp;
    enum GGDecodeResponse response;

    // Make a copy of gameGenieCode because strtok will stomp all over it
    char copy[27]; // 3 8-sized codes separated by +'s
    strncpy(copy, gameGenieCode, 26);
    copy[26] = '\0';

    // Codes at this point are assumed to be well formed. If not, we won't (shouldn't) crash,
    // but we will reject some or all of a code.
    const char* delimiter = "+";
    char *token = strtok(copy, delimiter);
    while(token != NULL) {
        response = decodeGameGenieCode(token, &addr, &val, &cmp);
        if (response != GGDecodeOk) {
            printf("GAME GENIE: Failed to decode %s\n", token);
            continue;
        }
        patchAddress(addr, val, cmp);
        printf("* GG: Loaded code %s (0x%x,%d)\n", token, addr, val);

        token = strtok(NULL, delimiter);
    }

    return 0;
}

void gameGenieInitialize(const char **codes, int numCodes) {
    if (gg_active_codes != NULL) {
        printf("GAME GENIE: Error: Should not reached here");
    }
    gg_active_codes_count = 0;
    gg_active_codes_size = 1;

    gg_active_codes = malloc(gg_active_codes_size * sizeof(game_genie_code_t));
    for (int i=0; i<numCodes; i++) {
       loadGameGenieCode(codes[i]);
    }
}

void gameGenieShutdown() {
    if (gg_active_codes != NULL) {
        free(gg_active_codes);
        gg_active_codes = NULL;
    }
    gg_active_codes_count = 0;
    gg_active_codes_size = 0;
}

void printGGCode(char *code) {
  unsigned short addr;
  unsigned char val;
  short cmp;
  decodeGameGenieCode(code, &addr, &val, &cmp);
  printf("%s -> addr: 0x%x value: %d key: %d\n", code, addr, val, cmp);
}

//int runTest(char *code,
//            unsigned int expectedAddress, 
//            unsigned int expectedValue, 
//            int expectedCompareValue,
//            enum GGDecodeResponse expectedResponse) {
//  unsigned int addr, val;
//  short cmp;
//  enum GGDecodeResponse response = decodeGameGenieCode(code, &addr, &val, &cmp);
//  return expectedAddress == addr && expectedValue == val && expectedCompareValue == cmp && expectedResponse == response;
//}

//int main(int argc, char **argv) {
//  printGGCode("APZLTG");
//  printGGCode("OZTLLX");
//  printGGCode("AATLGZ");
//  printGGCode("ALKXAAAZ");
//  printGGCode("KAOETLSA");
//  printGGCode("ZEXPYGLA");
//
//  assert(runTest("APZLTG", 0x3426, 16, 0, GGDecodeOk));
//  assert(runTest("OZTLLX", 0x3263, 169, 0, GGDecodeOk));
//  assert(runTest("AATLGZ", 0x3264, 0, 0, GGDecodeOk));
//  assert(runTest("ALKXAAAZ", 0x2048, 48, 32, GGDecodeOk));
//  assert(runTest("KAOETLSA", 0x031e, 132, 133, GGDecodeOk));
//  assert(runTest("ZEXPYGLA", 0x14a7, 2, 3, GGDecodeOk));
//
//  assert(runTest("ApZlTg", 0x3426, 16, 0, GGDecodeOk));
//  assert(runTest("zexpygla", 0x14a7, 2, 3, GGDecodeOk));
//
//  assert(runTest("0expygla", 0x0, 0, 0, GGDecodeInvalidCharacter));
//  assert(runTest("ygla", 0x0, 0, 0, GGDecodeInvalidCodeLength));
//  assert(runTest("APZLTGO", 0x0, 0, 0, GGDecodeInvalidCodeLength));
//  assert(runTest("APZLTGOAA", 0x0, 0, 0, GGDecodeInvalidCodeLength));
//
//  assert(runTest(NULL, 0x0, 0, 0, GGDecodeCodeIsNull));
//}
