#ifndef _EZFLASHJR_H_DEFINED
#define _EZFLASHJR_H_DEFINED

//
// Registers
//

// Unlock sequence
static volatile uint8_t AT(0x7F00) EZJR_REG_UNLOCK1;
static volatile uint8_t AT(0x7F10) EZJR_REG_UNLOCK2;
static volatile uint8_t AT(0x7F20) EZJR_REG_UNLOCK3;

// Lock sequence
static volatile uint8_t AT(0x7FF0) EZJR_REG_LOCK;

// Switch SD Card SRAM
//
// Writting to this register will configure the SRAM area as different things. Use the $7FBx registers to configure which SD Sector to read.
// Observed possible values:
//
// $00 Unmap the SD Card from SRAM area
// $01 Map SD Card sector data to SRAM, data will be available at $A000+, up to 4 sectors $800 bytes are available, and wraps after that.
// $03 Map SD Card read status to SRAM, status is available any SRAM area byte, $01 indicates read is done, $E1 indicates busy. (usage seems to be to wait till reading is done)
//
static volatile uint8_t AT(0x7F30) EZJR_REG_TF_SRAM_MAP;

static volatile uint8_t AT(0x7F31) EZJR_REG_31;
static volatile uint8_t AT(0x7F32) EZJR_REG_32;

static volatile uint8_t AT(0x7F36) EZJR_REG_ROM_LOAD_SRAM_MAP;

static volatile uint8_t AT(0x7F37) EZJR_REG_ROM_MBC;

// SD Card Sector number
static volatile uint32_t AT(0x7FB0) EZJR_REG_TF_SECTOR;

// SD Card read/write count + trigger.
static volatile uint8_t AT(0x7FB4) EZJR_REG_TF_COMMAND;
static volatile uint8_t AT(0x7FC0) EZJR_REG_SRAM_MAP;
static volatile uint16_t AT(0x7FC1) EZJR_REG_ROM_BANK_MASK;
static volatile uint16_t AT(0x7FC3) EZJR_REG_ROM_CRC;
static volatile uint8_t AT(0x7FC4) EZJR_REG_SRAM_BANK_MASK;
static volatile uint8_t AT(0x7FD0) EZJR_REG_SYNC_RTC;
static volatile uint8_t AT(0x7FD2) EZJR_REG_FW_SRAM_MAP;
static volatile uint8_t AT(0x7FD3) EZJR_REG_D3;
static volatile uint8_t AT(0x7FD4) EZJR_REG_D4;
static volatile uint8_t AT(0x7FE0) EZJR_REG_RESET;

//
// RTC registers
//

// SRAM Area.. (start A000, end C000), needs rtc mapped
static volatile uint8_t AT(0xA008) EZJR_RTC_SECONDS;
static volatile uint8_t AT(0xA009) EZJR_RTC_MINUTES;
static volatile uint8_t AT(0xA00A) EZJR_RTC_HOURS;
static volatile uint8_t AT(0xA00B) EZJR_RTC_DAY;
static volatile uint8_t AT(0xA00C) EZJR_RTC_WEEKDAY;
static volatile uint8_t AT(0xA00D) EZJR_RTC_MONTH;
static volatile uint8_t AT(0xA00E) EZJR_RTC_YEAR;

static volatile uint8_t AT(0xA000) EZJR_REG_SRAM_PAGE_SELECT;

// Constants
#define EZJR_UNLOCK1 0xE1
#define EZJR_UNLOCK2 0xE2
#define EZJR_UNLOCK3 0xE3

#define EZJR_TF_SRAM_MAP_NONE 0x00
#define EZJR_TF_SRAM_MAP_DATA 0x01
#define EZJR_TF_SRAM_MAP_STATUS 0x03

#define EZJR_TF_STATUS_BUSY 0xE0

#define EZJR_ROM_MBC_NONE 0x00
#define EZJR_ROM_MBC_MBC1 0x01
#define EZJR_ROM_MBC_MBC2 0x02
#define EZJR_ROM_MBC_MBC3 0x03
#define EZJR_ROM_MBC_MBC5 0x04
#define EZJR_ROM_MBC_MBC1M 0x05
#define EZJR_ROM_MBC_FALLBACK 0x06
#define EZJR_ROM_MBC_FLAG_RTC 0x80
#define EZJR_ROM_MBC_MBC3_RTC 0x83

#define EZJR_SRAM_MAP_NONE 0x00
#define EZJR_SRAM_MAP_SRAM 0x03
#define EZJR_SRAM_MAP_FW_VERSION 0x04
#define EZJR_SRAM_MAP_FW_UPDATE 0x05
#define EZJR_SRAM_MAP_RTC 0x06

#define EZJR_TF_COMMAND_READ(n) (n)
#define EZJR_TF_COMMAND_WRITE(n) (0x80 | (n))

#define EZJR_FW_SRAM_MAP_NONE 0x00
#define EZJR_FW_SRAM_MAP_STATUS 0x01

#define EZJR_RESET 0x80
#define EZJR_LOCK 0xE4

#endif /* _EZFLASHJR_H_DEFINED */
