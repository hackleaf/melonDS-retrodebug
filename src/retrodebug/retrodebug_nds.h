/*
 * retrodebug_nds.h
 * NDS-specific retrodebug data structures for misc breakpoint events.
 */

#ifndef RETRODEBUG_NDS_H
#define RETRODEBUG_NDS_H

#include <stdint.h>

/* DMA transfer event data, passed via RD_EVENT_MISC */
typedef struct rd_nds_dma {
    uint8_t cpu;       /* 0 = ARM9, 1 = ARM7 */
    uint8_t channel;   /* DMA channel 0-3 */
    uint32_t source;   /* Source address */
    uint32_t dest;     /* Destination address */
    uint32_t length;   /* Transfer length in units */
    uint8_t mode;      /* Start mode (immediate, vblank, hblank, etc.) */
} rd_nds_dma;

/* IPC FIFO event data */
typedef struct rd_nds_ipc {
    uint8_t sender;    /* 0 = ARM9, 1 = ARM7 */
    uint32_t value;    /* FIFO value */
} rd_nds_ipc;

#endif /* RETRODEBUG_NDS_H */
