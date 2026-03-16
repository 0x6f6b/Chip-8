#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "fonts.h"
#include <SDL3/SDL.h>

#define SCALE 10
#define WIDTH  64 * SCALE
#define HEIGHT 32 * SCALE

// Chip-8    →    Keyboard
// 1 2 3 C        1 2 3 4
// 4 5 6 D        Q W E R
// 7 8 9 E        A S D F
// A 0 B F        Z X C V



typedef struct {
  uint8_t memory[4096]; // main memory
  uint8_t V[16]; // Registers
  uint16_t I; // Special register
  uint16_t pc; // program counter
  uint16_t stack[16]; // Stack
  uint8_t sp; // Stack pointer (points to top of stack)
  uint8_t dt; // Delay timer
  uint8_t st; // Sound timer
  
  uint8_t keys[16]; // States of all possible keys
  uint8_t display[8 * 32];
} Chip8;

void copy_byte_to_display(uint8_t byte, Chip8 *chip8, uint8_t byte_x, uint8_t byte_y);
void display_sprite(uint8_t *sprite, Chip8 *chip8, uint8_t x, uint8_t y, uint8_t n);
void jump(uint16_t nnn, Chip8 *chip8);
void set_key_state(int scancode, uint8_t state, Chip8 *chip8);

int main(int argc, char* argv[]) {
  // INITIALISE THE VM
  Chip8 chip8;
  memset(&chip8, 0, sizeof(chip8));
  chip8.pc = 0x200;

  // Get randomness :-)
  srand(time(NULL));

  if (argc == 1) {
    printf("Usage: ./chip8 <program.rom>\n");
  }

  FILE *fptr;

  fptr = fopen(argv[1], "rb");

  if (fptr == NULL) {
    printf("Not able to open the ROM.\n");
    return 1;
  }

  // LOAD ROM INTO MEMORY
  fseek(fptr, 0, SEEK_END);
  long size = ftell(fptr);
  fseek(fptr, 0, SEEK_SET);
  fread(chip8.memory + 0x200, 1, size, fptr);
  fclose(fptr);
  printf("Loaded ROM, %ld bytes\n", size);


  // LOAD FONTS INTO MEMORY
  memcpy(chip8.memory, fonts, sizeof(fonts));



  // SDL shite
  if (SDL_Init(SDL_INIT_VIDEO) != 1) {
    printf("SDL init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow(
    "Chip-8",
    WIDTH, HEIGHT,
    0
  );

  if (!window) {
    printf("Window creation failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);


  // main loop
  int running = 1;
  int halted = 0;
  SDL_Event event;

  Uint64 frame1 = SDL_GetTicksNS();
  Uint64 frame2 = SDL_GetTicksNS();
  while (running) {
    frame2 = SDL_GetTicksNS();

    if (frame2 - frame1 >= SDL_NS_PER_SECOND / 60) {
      // runs at 60hz
      frame1 = frame2;

      if (chip8.dt > 0) chip8.dt--;
      if (chip8.st > 0) chip8.st--;
    }

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = 0;
      }

      if (event.type == SDL_EVENT_KEY_DOWN) {
        set_key_state(event.key.scancode, 1, &chip8);
      } else if (event.type == SDL_EVENT_KEY_UP) {
        set_key_state(event.key.scancode, 0, &chip8);
      }
    }

    // Fetch
    uint16_t instruction = (chip8.memory[chip8.pc] << 8) | chip8.memory[chip8.pc + 1];
    

    

    // instruction is (1111 1111) (1111 1111)

    // Decode/Execute
    uint16_t nnn = instruction & 0x0FFF; // last 12 bits 0011 1111
    uint16_t n = (instruction & 0xF000) >> 12;
    uint16_t x = (instruction & 0x0F00) >> 8;
    uint16_t y = (instruction & 0x00F0) >> 4;
    uint16_t kk = (instruction & 0x00FF);

    switch (n) {
      case 0:
        switch (instruction) {
          case 0x00E0:
            memset(&chip8.display, 0, sizeof(chip8.display));
            break;
          case 0x00EE:
            chip8.pc = chip8.stack[chip8.sp--];
            break;
          default:
            jump(nnn, &chip8);
            continue;
        }
        break;
      case 1:
        jump(nnn, &chip8);
        continue;
      case 2:
        chip8.stack[++chip8.sp] = chip8.pc;
        jump(nnn, &chip8);
        continue;
      case 3:
        if (chip8.V[x] == kk) chip8.pc += 2;
        break;
      case 4:
        if (chip8.V[x] != kk) chip8.pc += 2;
        break;
      case 5:
        if (chip8.V[x] == chip8.V[y]) chip8.pc += 2;
        break;
      case 6:
        chip8.V[x] = kk;
        break;
      case 7:
        chip8.V[x] += kk;
        break;
      case 8:
        switch (instruction & 0x000F) {
          case 0:
            chip8.V[x] = chip8.V[y];
            break;
          case 1:
            chip8.V[x] = chip8.V[x] | chip8.V[y];
            break;
          case 2:
            chip8.V[x] = chip8.V[x] & chip8.V[y];
            break;
          case 3: 
            chip8.V[x] = chip8.V[x] ^ chip8.V[y];
            break;
          case 4:
            uint16_t result = chip8.V[x] + chip8.V[y];
            chip8.V[0xF] = result > 255;
            chip8.V[x] = result;
            break;
          case 5:
            chip8.V[0xF] = chip8.V[x] > chip8.V[y];
            chip8.V[x] -= chip8.V[y];
            break;
          case 6:
            chip8.V[0xF] = chip8.V[x] & 0x01;
            chip8.V[x] = chip8.V[x] >> 1;
            break;
          case 7:
            chip8.V[0xF] = chip8.V[y] > chip8.V[x];
            chip8.V[x] = chip8.V[y] - chip8.V[x];
            break;
          case 0xE:
            chip8.V[0xF] = (chip8.V[x] & 0x80) >> 7;
            chip8.V[x] = chip8.V[x] << 1;
            break;
        }
        break;
      case 9:
        if (chip8.V[x] != chip8.V[y]) chip8.pc += 2;
        break;
      case 0xA:
        chip8.I = nnn;
        break;
      case 0xB:
        jump(nnn + chip8.V[0x0], &chip8);
        continue;
      case 0xC:
        chip8.V[x] = (rand() % 256) & kk;
        break;
      case 0xD:
        display_sprite(&chip8.memory[chip8.I], &chip8, chip8.V[x], chip8.V[y], instruction & 0x000F);
        break;
      case 0xE:
        switch(kk) {
          case 0x9E:
            if (chip8.keys[chip8.V[x]] == 1) chip8.pc += 2;
            break;
          case 0xA1:
            if (chip8.keys[chip8.V[x]] == 0) chip8.pc += 2;
            break;
        }
        break;
      case 0xF:
        switch (kk) {
          case 0x07:
            chip8.V[x] = chip8.dt;
            break;
          case 0x0A:
            halted = chip8.keys[chip8.V[x]] == 1 ? 0 : 1;
            if (halted == 0) chip8.V[x] = 1;
            break;
          case 0x15:
            chip8.dt = chip8.V[x];
            break;
          case 0x18:
            chip8.st = chip8.V[x];
            break;
          case 0x1E:
            chip8.I += chip8.V[x];
            break;
          case 0x29:
            chip8.I = chip8.V[x] * 5;
            break;
          case 0x33:
            int hundreds = chip8.V[x] / 100;
            int tens = (chip8.V[x] % 100) / 10;
            int ones = chip8.V[x] % 10;

            chip8.memory[chip8.I] = hundreds;
            chip8.memory[chip8.I+1] = tens;
            chip8.memory[chip8.I+2] = ones;

            break;
          case 0x55:
            for (int i = 0; i <= x; i++) {
              chip8.memory[chip8.I + i] = chip8.V[i];
            }
            break;
          case 0x65:
            for (int i = 0; i <= x; i++) {
              chip8.V[i] = chip8.memory[chip8.I + i];
            }
            break;
        }
        break;
      default:
        printf("Unknown instruction: %x\n", instruction);
    }

    if (halted == 0) {
      chip8.pc += 2;
    }

    SDL_SetRenderDrawColor(renderer, 154, 102, 0, 255);
    SDL_RenderClear(renderer);

    // Put stuff on the screen here each frame
    for (int y = 0; y < 32; y++) {
      // printf("\n");
      for (int x = 0; x < 8; x++) {
        // printf(" ");
        // for each bit of each byte
        uint8_t current_byte = chip8.display[y * 8 + x];
        for (int i = 0; i < 8; i++) {
          int most_significant_bit = (current_byte & 0x80) >> 7;
          // printf("%i", most_significant_bit);
          if (most_significant_bit == 1) {
            SDL_FRect rect = {(x * 8 + i) * SCALE, y * SCALE, SCALE, SCALE};
            SDL_SetRenderDrawColor(renderer, 255, 204, 0, 255);
            SDL_RenderFillRect(renderer, &rect);
          }
          current_byte = current_byte << 1;
        }
      }
    }

    // printf("\n\n");
        
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void jump(uint16_t nnn, Chip8 *chip8) {
  chip8->pc = nnn;
}

void display_sprite(uint8_t *sprite, Chip8 *chip8, uint8_t x, uint8_t y, uint8_t n) {
  // set collision to zero
  chip8->V[0xF] = 0;

  int pixel_offset = x % 8;

  
  for (int i = 0; i < n; i++) {    
    if (pixel_offset == 0) {
      copy_byte_to_display(sprite[i], chip8, x / 8, y + i);
    } else {
      uint8_t left_byte = sprite[i] >> pixel_offset;
      uint8_t right_byte = sprite[i] << (8 - pixel_offset);
      
      copy_byte_to_display(left_byte, chip8, x/8, y + i);

      if (x/8 + 1 <= 7) {
        copy_byte_to_display(right_byte, chip8, x/8 + 1, y + i);
      } else {
        copy_byte_to_display(right_byte, chip8, 0, y + i);
      }
    }
  }
}

void copy_byte_to_display(uint8_t byte, Chip8 *chip8, uint8_t byte_x, uint8_t byte_y) {
  int collision = byte & chip8->display[(byte_y % 32) * 8 + byte_x];
  if (collision) chip8->V[0xF] = 1;

  chip8->display[(byte_y % 32) * 8 + byte_x] = byte ^ chip8->display[(byte_y % 32) * 8 + byte_x];
}

void set_key_state(int scancode, uint8_t state, Chip8 *chip8) {
  switch (scancode) {
    case 27:
      chip8->keys[0] = state;
      break;
    case 30:
      chip8->keys[1] = state;
      break;
    case 31:
      chip8->keys[2] = state;
      break;
    case 32:
      chip8->keys[3] = state;
      break;
    case 20:
      chip8->keys[4] = state;
      break;
    case 26:
      chip8->keys[5] = state;
      break;
    case 8:
      chip8->keys[6] = state;
      break;
    case 4:
      chip8->keys[7] = state;
      break;
    case 22:
      chip8->keys[8] = state;
      break;
    case 7:
      chip8->keys[9] = state;
      break;
    case 29:
      chip8->keys[10] = state;
      break;
    case 6:
      chip8->keys[11] = state;
      break;
    case 33:
      chip8->keys[12] = state;
      break;
    case 21:
      chip8->keys[13] = state;
      break;
    case 9:
      chip8->keys[14] = state;
      break;
    case 25:
      chip8->keys[15] = state;
      break;
  }
}