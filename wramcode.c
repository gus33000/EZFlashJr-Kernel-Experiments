#include <gbdk/platform.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <gb/sgb.h>
#include <stdio.h>
#include "ezflashjr.h"

#pragma bank 4

// calculate the distance between objects 
#define object_distance(a, b) ((void *)&(b) - (void *)&(a))

// those are function pointer variables, we can initialize them right here
typedef void (*inc_t)(void);

inline void ezjr_unlock(void)
{
    EZJR_REG_UNLOCK1 = EZJR_UNLOCK1;
    EZJR_REG_UNLOCK2 = EZJR_UNLOCK2;
    EZJR_REG_UNLOCK3 = EZJR_UNLOCK3;
}

inline void ezjr_lock(void)
{
    EZJR_REG_LOCK = EZJR_LOCK;
}

void ram_code(void) NONBANKED {
    inc_t func = (inc_t)0xD100;

    // Disable interrupts now so we dont get accidentally thrown
    // into the location the interrupt vector points to
    // Which would be the new ROM being loaded by the FPGA..
    disable_interrupts();

    ezjr_unlock();
    EZJR_REG_ROM_LOAD_SRAM_MAP = 0x3;
    ezjr_lock();

    func();
}

// dummy function, needed to calculate ram_code() size, must be after it
void ram_code_end(void) NONBANKED {} 

void ram_code2(void) NONBANKED {
    inc_t func = (inc_t)0xD100;
    inc_t func2 = (inc_t)0xD200;

    if (_SRAM[0] == 0)
        func();

    func2();
}

// dummy function, needed to calculate ram_code() size, must be after it
void ram_code2_end(void) NONBANKED {} 

void ram_code3(void) NONBANKED {
    inc_t func = (inc_t)0xD200;
    inc_t func2 = (inc_t)0xD300;

    if (_SRAM[0] == 1)
        func();

    func2();
}

// dummy function, needed to calculate ram_code() size, must be after it
void ram_code3_end(void) NONBANKED {} 

void ram_code4(void) NONBANKED {
    ezjr_unlock();
    EZJR_REG_ROM_LOAD_SRAM_MAP = 0x0;
    ezjr_lock();

    ezjr_unlock();
    EZJR_REG_31 = 0x0;
    EZJR_REG_32 = 0x0;
    ezjr_lock();

    ezjr_unlock();
    EZJR_REG_RESET = 0x80;
    ezjr_lock();
}

// dummy function, needed to calculate ram_code() size, must be after it
void ram_code4_end(void) NONBANKED {} 

void call_from_wram(void) NONBANKED {
    memcpy((void*)0xD000, (void *)&ram_code, (uint16_t)object_distance(ram_code, ram_code_end));
    memcpy((void*)0xD100, (void *)&ram_code2, (uint16_t)object_distance(ram_code2, ram_code2_end));
    memcpy((void*)0xD200, (void *)&ram_code3, (uint16_t)object_distance(ram_code3, ram_code3_end));
    memcpy((void*)0xD300, (void *)&ram_code4, (uint16_t)object_distance(ram_code4, ram_code4_end));

    inc_t func = (inc_t)0xD000;
    func();
}