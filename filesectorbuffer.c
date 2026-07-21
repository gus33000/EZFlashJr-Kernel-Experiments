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
uint32_t rom_header[0x200 / 4] = {0};

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

FRESULT test_read(
    // void* buff,     /* Pointer to the read buffer (NULL:Forward data to the stream)*/
    uint16_t btr //,       /* Number of bytes to read */
    // uint16_t* br        /* Pointer to number of bytes read */
    ) BANKED
{
    // DRESULT dr;
    CLUST clst;
    uint32_t sect, remain;
    uint16_t rcnt;
    uint8_t cs; //, *rbuff = buff;

    uint32_t prev_sect = 0xFFFFFFFF;
    uint32_t prev_sect_count = 0x0;
    uint32_t offsetIntoBuffer = 1;

    if (!(filesystem.flag & FA_OPENED))
        return FR_NOT_OPENED; /* Check if opened */

    remain = filesystem.fsize - filesystem.fptr;
    if (btr > remain)
        btr = (uint16_t)remain; /* Truncate btr by remaining bytes */

    while (btr)
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
                    clst = get_fat(filesystem.curr_clust);
                }
                if (clst <= 1)
                    ABORT(FR_DISK_ERR);
                filesystem.curr_clust = clst; /* Update current cluster */
            }
            sect = clust2sect(filesystem.curr_clust); /* Get current sector */
            if (!sect)
                ABORT(FR_DISK_ERR);
            filesystem.dsect = sect + cs;
        }
        rcnt = 512 - (uint16_t)filesystem.fptr % 512; /* Get partial sector data from sector buffer */
        if (rcnt > btr)
            rcnt = btr;

        if (prev_sect != 0xFFFFFFFF && filesystem.dsect != prev_sect + 1)
        {
            printf("Writing %04X\n", prev_sect - prev_sect_count);
            rom_header[offsetIntoBuffer++] = prev_sect - prev_sect_count;
            rom_header[offsetIntoBuffer++] = prev_sect_count;
            prev_sect_count = 0;
        }
        else
        {
            prev_sect_count++;
        }

        prev_sect = filesystem.dsect;

        filesystem.fptr += rcnt; /* Advances file read pointer */
        btr -= rcnt;
    }

    printf("Writing %04X\n", prev_sect - prev_sect_count);
    rom_header[offsetIntoBuffer++] = prev_sect - prev_sect_count;
    rom_header[offsetIntoBuffer++] = 0xFFFFFFFF;
    prev_sect_count = 0;

    return FR_OK;
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
    pf_read(rom_header, 0x200, &readbytes);

    // Always 0
    rom_header[0] = 0;

    test_read(filesystem.fsize);

    // Size of the to be loaded ROM in bytes.
    rom_header[0x7C] = filesystem.fsize; // 0x8000;
    printf("Size: %02X\n", filesystem.fsize);

    rom_header[0x7D] = 0x01;
    rom_header[0x7E] = 0x04;

    EZGB_COMMAND_PACKET(EZJR_REG_SRAM_MAP = 0x2);
    EZGB_COMMAND_PACKET(EZJR_REG_ROM_MBC = 0);
    EZGB_COMMAND_PACKET(EZJR_REG_D4 = 0);
    EZGB_COMMAND_PACKET(EZJR_REG_SRAM_BANK_MASK = 0);
    EZGB_COMMAND_PACKET(EZJR_REG_ROM_BANK_MASK = 0x003);
    EZGB_COMMAND_PACKET(EZJR_REG_ROM_CRC = 0x3D);
    EZGB_COMMAND_PACKET(EZJR_REG_ROM_LOAD_SRAM_MAP = 0x1);

    memcpy(_SRAM, rom_header, 0x200);

    // Jump to work ram
    call_from_wram();
}