#ifndef _EZGB_H_DEFINED
#define _EZGB_H_DEFINED

#include "ezflashjr.h"

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

#define EZGB_COMMAND_PACKET(x) \
    ezjr_unlock();             \
    x;                         \
    ezjr_lock()

#endif /* _EZGB_H_DEFINED */
