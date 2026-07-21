#include <gbdk/platform.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <gb/sgb.h>
#include <stdio.h>
#include "ezgb.h"

#pragma bank 4

// calculate the distance between objects
#define object_distance(a, b) ((void *)&(b) - (void *)&(a))

// those are function pointer variables, we can initialize them right here
typedef void (*inc_t)(void);

void ram_code(void) NONBANKED
{
    inc_t func = (inc_t)0xD100;

    // Disable interrupts now so we dont get accidentally thrown
    // into the location the interrupt vector points to
    // Which would be the new ROM being loaded by the FPGA..
    disable_interrupts();

    EZGB_COMMAND_PACKET(EZJR_REG_ROM_LOAD_SRAM_MAP = 0x3);

    func();
}

// dummy function, needed to calculate ram_code() size, must be after it
void ram_code_end(void) NONBANKED {}

void ram_code2(void) NONBANKED
{
    inc_t func = (inc_t)0xD100;
    inc_t func2 = (inc_t)0xD200;

    if (_SRAM[0] == 0)
        func();

    func2();
}

// dummy function, needed to calculate ram_code() size, must be after it
void ram_code2_end(void) NONBANKED {}

void ram_code3(void) NONBANKED
{
    inc_t func = (inc_t)0xD200;
    inc_t func2 = (inc_t)0xD300;

    if (_SRAM[0] == 1)
        func();

    func2();
}

// dummy function, needed to calculate ram_code() size, must be after it
void ram_code3_end(void) NONBANKED {}

void ram_code4(void) NONBANKED
{
    EZGB_COMMAND_PACKET(EZJR_REG_ROM_LOAD_SRAM_MAP = 0x0);
    EZGB_COMMAND_PACKET(EZJR_REG_31 = 0x0; EZJR_REG_32 = 0x0);
    EZGB_COMMAND_PACKET(EZJR_REG_RESET = 0x80);
}

// dummy function, needed to calculate ram_code() size, must be after it
void ram_code4_end(void) NONBANKED {}

void call_from_wram(void) NONBANKED
{
    printf("Setting up WRAM Trampoline...\n");

    memcpy((void *)0xD000, (void *)&ram_code, (uint16_t)object_distance(ram_code, ram_code_end));
    memcpy((void *)0xD100, (void *)&ram_code2, (uint16_t)object_distance(ram_code2, ram_code2_end));
    memcpy((void *)0xD200, (void *)&ram_code3, (uint16_t)object_distance(ram_code3, ram_code3_end));
    memcpy((void *)0xD300, (void *)&ram_code4, (uint16_t)object_distance(ram_code4, ram_code4_end));

    inc_t func = (inc_t)0xD000;
    func();
}