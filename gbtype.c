#include <gbdk/platform.h>
#include <stdint.h>
#include <stdio.h>
#include "ezgb.h"

#include "filesectorbuffer.h"

void main(void) NONBANKED
{
    // Wait 4 frames
    // For PAL SNES(SGB) this delay is required on startup
    for (uint8_t i = 4; i != 0; i--)
        vsync();

    HIDE_SPRITES;
    SHOW_BKG;
    // discable shadow OAM copying in VBlank
    DISABLE_VBL_TRANSFER;
    // turn on display
    DISPLAY_ON;

    EZJR_REG_SRAM_PAGE_SELECT = 0x11;
    EZGB_COMMAND_PACKET(EZJR_REG_SRAM_MAP = EZJR_SRAM_MAP_SRAM);

    EZJR_REG_SRAM_PAGE_SELECT = 0;
    EZGB_COMMAND_PACKET(EZJR_REG_SRAM_MAP = EZJR_SRAM_MAP_NONE);

    printf("Press [A] to start ROM loading\n");
    uint8_t i = waitpad(J_A | J_B);

    if (i == J_A)
    {
        execute_gb_file_load("WHICH/WHICH_~1.GBC");
    }
    else
    {
        execute_gb_file_load("Y.GBC");
    }

    // should never end here
    while (1)
    {
        vsync();
    }
}
