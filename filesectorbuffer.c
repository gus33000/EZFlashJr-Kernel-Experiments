#include <gbdk/platform.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <gb/sgb.h>
#include "ezgb.h"

#include "FileSystem/diskio.h"
#include "FileSystem/pff.h"
#include "wramcode.h"

bool filesystem_inited = false;
FATFS filesystem;
uint8_t scratch_buffer[0x200] = {0};

#define ABORT(err)           \
    {                        \
        filesystem.flag = 0; \
        return err;          \
    }

static uint16_t ld_word(const uint8_t *ptr) /*   Load a 2-byte little-endian word */
{
    uint16_t rv;

    rv = ptr[1];
    rv = rv << 8 | ptr[0];
    return rv;
}

static uint32_t ld_dword(const uint8_t *ptr) /* Load a 4-byte little-endian word */
{
    uint32_t rv;

    rv = ptr[3];
    rv = rv << 8 | ptr[2];
    rv = rv << 8 | ptr[1];
    rv = rv << 8 | ptr[0];
    return rv;
}

static uint32_t clust2sect(           /* !=0: Sector number, 0: Failed - invalid cluster# */
                           CLUST clst /* Cluster# to be converted */
)
{
    clst -= 2;
    if (clst >= (filesystem.n_fatent - 2))
        return 0; /* Invalid cluster# */
    return (uint32_t)clst * filesystem.csize + filesystem.database;
}

static CLUST get_fat(           /* 1:IO error, Else:Cluster status */
                     CLUST clst /* Cluster# to get the link information */
)
{
    uint8_t buf[4];
#if PF_FS_FAT12
    uint16_t wc, bc, ofs;
#endif

    if (clst < 2 || clst >= filesystem.n_fatent)
        return 1; /* Range check */

    switch (filesystem.fs_type)
    {
#if PF_FS_FAT12
    case FS_FAT12:
    {
        bc = (uint16_t)clst;
        bc += bc / 2;
        ofs = bc % 512;
        bc /= 512;
        if (ofs != 511)
        {
            if (disk_readp(buf, filesystem.fatbase + bc, ofs, 2))
                break;
        }
        else
        {
            if (disk_readp(buf, filesystem.fatbase + bc, 511, 1))
                break;
            if (disk_readp(buf + 1, filesystem.fatbase + bc + 1, 0, 1))
                break;
        }
        wc = ld_word(buf);
        return (clst & 1) ? (wc >> 4) : (wc & 0xFFF);
    }
#endif
#if PF_FS_FAT16
    case FS_FAT16:
        if (disk_readp(buf, filesystem.fatbase + clst / 256, ((uint16_t)clst % 256) * 2, 2))
            break;
        return ld_word(buf);
#endif
#if PF_FS_FAT32
    case FS_FAT32:
        if (disk_readp(buf, filesystem.fatbase + clst / 128, ((uint16_t)clst % 128) * 4, 4))
            break;
        return ld_dword(buf) & 0x0FFFFFFF;
#endif
    }

    return 1; /* An error occured at the disk I/O layer */
}

FRESULT test_read() BANKED
{
    // DRESULT dr;
    CLUST clst;
    uint32_t sect, remain;
    uint16_t rcnt;
    uint8_t cs;

    uint32_t prev_sect = 0xFFFFFFFF;
    uint32_t prev_sect_count = 0x0;
    uint32_t offsetIntoBuffer = 1;

    uint32_t curdatasect = 0;
    CLUST curr_clust = 0;

    if (!(filesystem.flag & FA_OPENED))
        return FR_NOT_OPENED; /* Check if opened */

    remain = filesystem.fsize - filesystem.fptr;
    curdatasect = filesystem.dsect;
    curr_clust = filesystem.curr_clust;

    while (remain)
    { /* Repeat until all data transferred */
        if ((filesystem.fptr % 512) == 0)
        {                                                                   /* On the sector boundary? */
            cs = (uint8_t)(filesystem.fptr / 512 & (filesystem.csize - 1)); /* Sector offset in the cluster */
            if (!cs)
            { /* On the cluster boundary? */
                if (filesystem.fptr == 0)
                { /* On the top of the file? */
                    clst = filesystem.org_clust;
                }
                else
                {
                    clst = get_fat(curr_clust);
                }
                if (clst <= 1)
                    ABORT(FR_DISK_ERR);
                curr_clust = clst; /* Update current cluster */
            }
            sect = clust2sect(curr_clust); /* Get current sector */
            if (!sect)
                ABORT(FR_DISK_ERR);
            curdatasect = sect + cs;
        }
        rcnt = 512 - (uint16_t)filesystem.fptr % 512; /* Get partial sector data from sector buffer */
        if (rcnt > remain)
            rcnt = remain;

        if (prev_sect != 0xFFFFFFFF && curdatasect != prev_sect + 1)
        {
            printf("START: %02X%02X\n",
                   (uint16_t)(((prev_sect - prev_sect_count) & 0xFFFF0000) >> 16),
                   (uint16_t)((prev_sect - prev_sect_count) & 0xFFFF));
            printf("END: %02X%02X\n",
                   (uint16_t)((prev_sect_count & 0xFFFF0000) >> 16),
                   (uint16_t)(prev_sect_count & 0xFFFF));

            ((uint32_t *)scratch_buffer)[offsetIntoBuffer++] = prev_sect - prev_sect_count;
            ((uint32_t *)scratch_buffer)[offsetIntoBuffer++] = prev_sect_count;

            prev_sect_count = 0;
        }
        else
        {
            prev_sect_count++;
        }

        prev_sect = curdatasect;

        filesystem.fptr += rcnt; /* Advances file read pointer */
        remain -= rcnt;
    }

    uint32_t firstSectorToReadLast = prev_sect - prev_sect_count;

    printf("START: %02X%02X\n",
           (uint16_t)((firstSectorToReadLast & 0xFFFF0000) >> 16),
           (uint16_t)(firstSectorToReadLast & 0xFFFF));
    printf("END: 0xFFFFFFFF\n");

    ((uint32_t *)scratch_buffer)[offsetIntoBuffer++] = prev_sect - prev_sect_count;
    ((uint32_t *)scratch_buffer)[offsetIntoBuffer++] = 0xFFFFFFFF;
    prev_sect_count = 0;

    return FR_OK;
}

void prepare_rom_load_buffer()
{
    // Always 0
    ((uint32_t *)scratch_buffer)[0] = 0;

    test_read();

    // Size of the to be loaded ROM in bytes.
    ((uint32_t *)scratch_buffer)[0x7C] = filesystem.fsize;

    ((uint32_t *)scratch_buffer)[0x7D] = 0x01;
    ((uint32_t *)scratch_buffer)[0x7E] = 0x04;
}

uint16_t get_rom_bank_mask()
{
    uint8_t rom_size_type = scratch_buffer[0x0148];
    uint16_t rom_bank_count = 2 * ((uint16_t)1 << rom_size_type);
    return rom_bank_count - 1;
}

uint8_t get_ram_bank_mask()
{
    uint8_t rom_ram_type = scratch_buffer[0x0149];
    uint8_t ram_bank_mask = 0;

    switch (rom_ram_type)
    {
    case 2:
    {
        ram_bank_mask = 1;
        break;
    }
    case 3:
    {
        // ram_bank_mask = 4;
        ram_bank_mask = 3; // (???)
        break;
    }
    case 4:
    {
        ram_bank_mask = 16;
        break;
    }
    case 5:
    {
        ram_bank_mask = 8;
        break;
    }
    default:
    {
        ram_bank_mask = 0;
    }
    }

    return ram_bank_mask;
}

uint8_t get_mbc_type()
{
    uint8_t rom_cart_type = scratch_buffer[0x0147];
    uint8_t mbc_type = 0;
    uint8_t nintendo_copyright_header[0x30] = {0};

    switch (rom_cart_type)
    {
    case 0:
    case 8:
    case 9:
    {
        mbc_type = EZJR_ROM_MBC_NONE;
        break;
    }
    case 1:
    case 2:
    case 3:
    {
        mbc_type = EZJR_ROM_MBC_MBC1;

        if (filesystem.fsize >= 0x44000)
        {
            memcpy(nintendo_copyright_header, scratch_buffer + 0x104, 0x30);

            pf_lseek(0x40000);
            uint16_t readbytes = 0;
            pf_read(scratch_buffer, 0x200, &readbytes);

            if (memcmp(nintendo_copyright_header, scratch_buffer + 0x104, 0x30) == 0)
            {
                // Guess this is a MBC1m cart because of the duplicated Nintendo Copyright Header
                mbc_type = EZJR_ROM_MBC_MBC1M;
            }
        }

        break;
    }
    case 5:
    case 6:
    {
        mbc_type = EZJR_ROM_MBC_MBC2;
        break;
    }
    case 0xF:
    case 0x10:
    {
        mbc_type = EZJR_ROM_MBC_MBC3_RTC;
        break;
    }
    case 0x11:
    case 0x12:
    case 0x13:
    case 0xFC:
    {
        mbc_type = EZJR_ROM_MBC_MBC3;
        break;
    }
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    {
        mbc_type = EZJR_ROM_MBC_MBC5;
        break;
    }
    default:
    {
        mbc_type = EZJR_ROM_MBC_FALLBACK;
    }
    }

    return mbc_type;
}

void execute_gb_file_load(const char *path) NONBANKED
{
    // mount FS if not mounted yet
    if (!filesystem_inited)
    {
        printf("Initializing FS!\n");
        FRESULT result = pf_mount(&filesystem);
        if (filesystem_inited = (result != FR_OK))
        {
            printf("Failed to mount... %d\n", result);
            return;
        }
    }

    FRESULT res = pf_open(path);

    uint16_t readbytes = 0;
    pf_read(scratch_buffer, 0x200, &readbytes);

    uint16_t rom_bank_mask = get_rom_bank_mask();
    uint8_t ram_bank_mask = get_ram_bank_mask();
    uint8_t rom_crc = scratch_buffer[0x014D];
    uint8_t mbc_type = get_mbc_type();

    printf("Size: %02X%02X\n",
           (uint16_t)((filesystem.fsize & 0xFFFF0000) >> 16),
           (uint16_t)(filesystem.fsize & 0xFFFF));

    printf("MBC: %01X\n", mbc_type);
    printf("RAM: %01X\n", ram_bank_mask);
    printf("ROM: %01X\n", rom_bank_mask);
    printf("CRC: %01X\n", rom_crc);

    printf("Press [A] to begin ROM loading\n");
    waitpad(J_A);

    // Clear out/Initialize SRAM for the game
    if (ram_bank_mask != 0)
    {
        // Clear with all FFs
        memset(scratch_buffer, 0xFF, 0x200);

        for (int k = 0; k < ram_bank_mask; k++)
        {
            EZJR_REG_SRAM_PAGE_SELECT = k;

            for (int i = 0; i < 0x2000; i += 0x200)
            {
                EZGB_COMMAND_PACKET(EZJR_REG_SRAM_MAP = EZJR_SRAM_MAP_SRAM);

                memcpy(_SRAM + i, scratch_buffer, 0x200);

                EZGB_COMMAND_PACKET(EZJR_REG_SRAM_MAP = EZJR_SRAM_MAP_NONE);
            }
        }

        // Reset SRAM Page
        if (ram_bank_mask != 1)
        {
            EZJR_REG_SRAM_PAGE_SELECT = 0;
        }
    }

    // Construct rom load buffer
    prepare_rom_load_buffer();

    EZGB_COMMAND_PACKET(EZJR_REG_SRAM_MAP = 0x2);

    EZGB_COMMAND_PACKET(EZJR_REG_ROM_MBC = mbc_type);
    EZGB_COMMAND_PACKET(EZJR_REG_D4 = 0);
    EZGB_COMMAND_PACKET(EZJR_REG_SRAM_BANK_MASK = ram_bank_mask);
    // EZGB_COMMAND_PACKET(EZJR_REG_SRAM_BANK_MASK = 0);
    EZGB_COMMAND_PACKET(EZJR_REG_ROM_BANK_MASK = rom_bank_mask);
    EZGB_COMMAND_PACKET(EZJR_REG_ROM_CRC = rom_crc);

    EZGB_COMMAND_PACKET(EZJR_REG_ROM_LOAD_SRAM_MAP = 0x1);
    memcpy(_SRAM, scratch_buffer, 0x200);

    // Jump to work ram
    call_from_wram();
}