/*
 * retrodebug.h
 * based on hcdebug.h by leiradel
 *
 * Everything starts at rd_DebuggerIf, so please see that struct first.
*/

#ifndef RETRO_DEBUG__
#define RETRO_DEBUG__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define RD_API_VERSION 1

/* Watchpoint operations */
#define RD_MEMORY_READ (1 << 0)
#define RD_MEMORY_WRITE (1 << 1)

/* Event types */
typedef enum {
    RD_EVENT_BREAKPOINT = 0,
    RD_EVENT_STEP = 1,
    RD_EVENT_INTERRUPT = 2,
    RD_EVENT_MEMORY = 3,
    RD_EVENT_REG = 4,
    RD_EVENT_MISC = 5
}
rd_EventType;

typedef int64_t rd_SubscriptionID;

typedef enum {
    RD_STEP_INTO,
    RD_STEP_INTO_SKIP_IRQ,
    RD_STEP_OVER,
    RD_STEP_OUT,
}
rd_StepMode;

typedef struct rd_MiscBreakpoint {
    struct {
        char const* description;
    }
    v1;
}
rd_MiscBreakpoint;

typedef struct rd_Memory rd_Memory;

typedef struct rd_MemoryMap {
    uint64_t base_addr;
    uint64_t size;
    rd_Memory const* source;
    uint64_t source_base_addr;
    int64_t  bank;
    uint32_t flags;
}
rd_MemoryMap;

#define RD_MEMMAP_CACHED  (1 << 0)

struct rd_Memory {
    struct {
        char const* id;
        char const* description;
        unsigned alignment;
        uint64_t size;
        rd_MiscBreakpoint const* const* break_points;
        unsigned num_break_points;
        uint64_t (*peek)(struct rd_Memory const* self, uint64_t address, uint64_t size, uint8_t* outbuff, bool side_effects);
        uint64_t (*poke)(struct rd_Memory const* self, uint64_t address, uint64_t size, uint8_t const* buff);
        unsigned (*get_memory_map_count)(struct rd_Memory const* self);
        void (*get_memory_map)(struct rd_Memory const* self, rd_MemoryMap *out);
        bool (*get_bank_address)(struct rd_Memory const* self, uint64_t address, int64_t bank, rd_MemoryMap* out);
        int (*cache_probe)(struct rd_Memory const* self, uint64_t address);
    }
    v1;
};

typedef struct rd_Cpu {
    struct {
        char const* id;
        char const* description;
        unsigned type;
        rd_Memory const* memory_region;
        rd_MiscBreakpoint const* const* break_points;
        unsigned num_break_points;
        uint64_t (*get_register)(struct rd_Cpu const* self, unsigned reg);
        int (*set_register)(struct rd_Cpu const* self, unsigned reg, uint64_t value);
        bool (*pipeline_get_delay_pc)(struct rd_Cpu const* self, unsigned delay, uint64_t* out_pc);
    }
    v1;
}
rd_Cpu;

typedef enum {
    RD_FS_OTHER     = 0,
    RD_FS_FILE      = 1,
    RD_FS_DIRECTORY = 2,
} rd_FsFileType;

#define RD_FS_READABLE  (1 << 0)
#define RD_FS_WRITABLE  (1 << 1)

typedef struct rd_FsStat {
    uint64_t size;
    rd_FsFileType type;
    uint32_t flags;
    uint64_t sector;
} rd_FsStat;

typedef struct rd_Filesystem {
    struct {
        char const *scheme;
        char const *description;
        char separator;
        int (*list)(struct rd_Filesystem const *self, const char *path, void *user_data,
                         void (*cb)(const char *name, void *user_data));
        bool (*stat)(struct rd_Filesystem const *self, const char *path,
                     rd_FsStat *out);
        uint64_t (*read)(struct rd_Filesystem const *self, const char *path,
                         uint64_t offset, uint8_t *buf, uint64_t size);
        uint64_t (*write)(struct rd_Filesystem const *self, const char *path,
                          uint64_t offset, uint8_t const *buf, uint64_t size);
    } v1;
} rd_Filesystem;

typedef struct rd_System {
    struct {
        char const* id;
        rd_Cpu const* const* cpus;
        unsigned num_cpus;
        rd_Memory const* const* memory_regions;
        unsigned num_memory_regions;
        rd_Filesystem const* const* filesystems;
        unsigned num_filesystems;
        rd_MiscBreakpoint const* const* break_points;
        unsigned num_break_points;
        int (*get_content_info)(char* outbuff, int outsize);
    }
    v1;
}
rd_System;

typedef struct rd_BreakpointEvent {
    rd_Cpu const* cpu;
    uint64_t address;
}
rd_BreakpointEvent;

typedef struct rd_StepEvent {
    rd_Cpu const* cpu;
    uint64_t address;
}
rd_StepEvent;

typedef struct rd_InterruptEvent {
    rd_Cpu const* cpu;
    unsigned kind;
    uint64_t return_address;
    uint64_t vector_address;
}
rd_InterruptEvent;

typedef struct rd_MemoryWatchpointEvent {
    rd_Memory const* memory;
    uint64_t address;
    uint8_t operation;
    uint8_t value;
    void* accessor;
}
rd_MemoryWatchpointEvent;

typedef struct rd_RegisterWatchpointEvent {
    rd_Cpu const* cpu;
    unsigned reg;
    uint64_t new_value;
}
rd_RegisterWatchpointEvent;

typedef struct rd_MiscBreakpointEvent {
    rd_MiscBreakpoint const* breakpoint;
    void const* data;
    size_t data_size;
}
rd_MiscBreakpointEvent;

typedef struct rd_Event {
    rd_EventType type;
    bool can_halt;
    union {
        rd_BreakpointEvent breakpoint;
        rd_StepEvent step;
        rd_InterruptEvent interrupt;
        rd_MemoryWatchpointEvent memory;
        rd_RegisterWatchpointEvent reg;
        rd_MiscBreakpointEvent misc;
    };
}
rd_Event;

typedef struct rd_BreakpointSubscription {
    rd_Cpu const* cpu;
    uint64_t address;
}
rd_BreakpointSubscription;

typedef struct rd_StepSubscription {
    rd_Cpu const* cpu;
    rd_StepMode mode;
}
rd_StepSubscription;

typedef struct rd_InterruptSubscription {
    rd_Cpu const* cpu;
    unsigned kind;
}
rd_InterruptSubscription;

typedef struct rd_MemoryWatchpointSubscription {
    rd_Memory const* memory;
    uint64_t address_range_begin;
    uint64_t address_range_end;
    uint8_t operation;
}
rd_MemoryWatchpointSubscription;

typedef struct rd_RegisterWatchpointSubscription {
    rd_Cpu const* cpu;
    unsigned reg;
}
rd_RegisterWatchpointSubscription;

typedef struct rd_MiscBreakpointSubscription {
    rd_MiscBreakpoint const* breakpoint;
}
rd_MiscBreakpointSubscription;

typedef struct rd_Subscription {
    rd_EventType type;
    union {
        rd_BreakpointSubscription breakpoint;
        rd_StepSubscription step;
        rd_InterruptSubscription interrupt;
        rd_MemoryWatchpointSubscription memory;
        rd_RegisterWatchpointSubscription reg;
        rd_MiscBreakpointSubscription misc;
    };
}
rd_Subscription;

typedef struct rd_DebuggerIf {
    unsigned const frontend_api_version;
    unsigned core_api_version;
    struct {
        rd_System const* system;
        void* const user_data;
        bool (* const handle_event)(void* frontend_user_data, rd_SubscriptionID subscription_id, rd_Event const* event);
        rd_SubscriptionID (* subscribe)(rd_Subscription const* subscription);
        void (* unsubscribe)(rd_SubscriptionID subscription_id);
    }
    v1;
}
rd_DebuggerIf;

typedef void (*rd_Set)(rd_DebuggerIf* const debugger_if);

#define RD_MAKE_CPU_TYPE(id, version) ((id) << 16 | (version))
#define RD_CPU_API_VERSION(type) ((type) & 0xffffU)

#define RD_CPU_Z80 RD_MAKE_CPU_TYPE(0, 1)
#define RD_Z80_A 0
#define RD_Z80_F 1
#define RD_Z80_BC 2
#define RD_Z80_DE 3
#define RD_Z80_HL 4
#define RD_Z80_IX 5
#define RD_Z80_IY 6
#define RD_Z80_AF2 7
#define RD_Z80_BC2 8
#define RD_Z80_DE2 9
#define RD_Z80_HL2 10
#define RD_Z80_I 11
#define RD_Z80_R 12
#define RD_Z80_SP 13
#define RD_Z80_PC 14
#define RD_Z80_IFF 15
#define RD_Z80_IM 16
#define RD_Z80_WZ 17
#define RD_Z80_NUM_REGISTERS 18
#define RD_Z80_INT 0
#define RD_Z80_NMI 1

#define RD_CPU_6502 RD_MAKE_CPU_TYPE(1, 1)
#define RD_6502_A 0
#define RD_6502_X 1
#define RD_6502_Y 2
#define RD_6502_S 3
#define RD_6502_PC 4
#define RD_6502_P 5
#define RD_6502_NUM_REGISTERS 6
#define RD_6502_NMI 0
#define RD_6502_IRQ 1

#define RD_CPU_65816 RD_MAKE_CPU_TYPE(2, 1)
#define RD_65816_A 0
#define RD_65816_X 1
#define RD_65816_Y 2
#define RD_65816_S 3
#define RD_65816_PC 4
#define RD_65816_P 5
#define RD_65816_DB 6
#define RD_65816_D 7
#define RD_65816_PB 8
#define RD_65816_EMU 9
#define RD_65816_NUM_REGISTERS 10

#define RD_CPU_R3000A RD_MAKE_CPU_TYPE(3, 1)
#define RD_R3000A_R0 0
#define RD_R3000A_AT 1
#define RD_R3000A_V0 2
#define RD_R3000A_V1 3
#define RD_R3000A_A0 4
#define RD_R3000A_A1 5
#define RD_R3000A_A2 6
#define RD_R3000A_A3 7
#define RD_R3000A_T0 8
#define RD_R3000A_T1 9
#define RD_R3000A_T2 10
#define RD_R3000A_T3 11
#define RD_R3000A_T4 12
#define RD_R3000A_T5 13
#define RD_R3000A_T6 14
#define RD_R3000A_T7 15
#define RD_R3000A_S0 16
#define RD_R3000A_S1 17
#define RD_R3000A_S2 18
#define RD_R3000A_S3 19
#define RD_R3000A_S4 20
#define RD_R3000A_S5 21
#define RD_R3000A_S6 22
#define RD_R3000A_S7 23
#define RD_R3000A_T8 24
#define RD_R3000A_T9 25
#define RD_R3000A_K0 26
#define RD_R3000A_K1 27
#define RD_R3000A_GP 28
#define RD_R3000A_SP 29
#define RD_R3000A_FP 30
#define RD_R3000A_RA 31
#define RD_R3000A_PC 32
#define RD_R3000A_LO 33
#define RD_R3000A_HI 34
#define RD_R3000A_NUM_REGISTERS 35

#define RD_CPU_M68K RD_MAKE_CPU_TYPE(4, 1)
#define RD_M68K_D0 0
#define RD_M68K_D1 1
#define RD_M68K_D2 2
#define RD_M68K_D3 3
#define RD_M68K_D4 4
#define RD_M68K_D5 5
#define RD_M68K_D6 6
#define RD_M68K_D7 7
#define RD_M68K_A0 8
#define RD_M68K_A1 9
#define RD_M68K_A2 10
#define RD_M68K_A3 11
#define RD_M68K_A4 12
#define RD_M68K_A5 13
#define RD_M68K_A6 14
#define RD_M68K_A7 15
#define RD_M68K_PC 16
#define RD_M68K_SR 17
#define RD_M68K_SSP 18
#define RD_M68K_USP 19
#define RD_M68K_NUM_REGISTERS 20

#define RD_CPU_LR35902 RD_MAKE_CPU_TYPE(5, 1)
#define RD_LR35902_A 0
#define RD_LR35902_F 1
#define RD_LR35902_B 2
#define RD_LR35902_C 3
#define RD_LR35902_D 4
#define RD_LR35902_E 5
#define RD_LR35902_H 6
#define RD_LR35902_L 7
#define RD_LR35902_SP 8
#define RD_LR35902_PC 9
#define RD_LR35902_AF 10
#define RD_LR35902_BC 11
#define RD_LR35902_DE 12
#define RD_LR35902_HL 13
#define RD_LR35902_IME 14
#define RD_LR35902_NUM_REGISTERS 15
#define RD_LR35902_INT_VBLANK 0
#define RD_LR35902_INT_STAT 1
#define RD_LR35902_INT_TIMER 2
#define RD_LR35902_INT_SERIAL 3
#define RD_LR35902_INT_JOYPAD 4
#define RD_LR35902_FLAG_Z 0x80
#define RD_LR35902_FLAG_N 0x40
#define RD_LR35902_FLAG_H 0x20
#define RD_LR35902_FLAG_C 0x10

#define RD_CPU_SH2 RD_MAKE_CPU_TYPE(6, 1)
#define RD_SH2_R0  0
#define RD_SH2_R1  1
#define RD_SH2_R2  2
#define RD_SH2_R3  3
#define RD_SH2_R4  4
#define RD_SH2_R5  5
#define RD_SH2_R6  6
#define RD_SH2_R7  7
#define RD_SH2_R8  8
#define RD_SH2_R9  9
#define RD_SH2_R10 10
#define RD_SH2_R11 11
#define RD_SH2_R12 12
#define RD_SH2_R13 13
#define RD_SH2_R14 14
#define RD_SH2_R15 15
#define RD_SH2_PC  16
#define RD_SH2_SR  17
#define RD_SH2_GBR 18
#define RD_SH2_VBR 19
#define RD_SH2_MACH 20
#define RD_SH2_MACL 21
#define RD_SH2_PR  22
#define RD_SH2_NUM_REGISTERS 23

/* ARM7TDMI - 32-bit ARM CPU (ARMv4T) used in Game Boy Advance, Nintendo DS (ARM7) */
#define RD_CPU_ARM7TDMI RD_MAKE_CPU_TYPE(7, 1)

/* ARM946E-S - 32-bit ARM CPU (ARMv5TE) used in Nintendo DS (ARM9) */
#define RD_CPU_ARM946ES RD_MAKE_CPU_TYPE(8, 1)

/* ARM register definitions (shared between ARM7TDMI and ARM946E-S) */
#define RD_ARM_R0  0
#define RD_ARM_R1  1
#define RD_ARM_R2  2
#define RD_ARM_R3  3
#define RD_ARM_R4  4
#define RD_ARM_R5  5
#define RD_ARM_R6  6
#define RD_ARM_R7  7
#define RD_ARM_R8  8
#define RD_ARM_R9  9
#define RD_ARM_R10 10
#define RD_ARM_R11 11  /* FP (frame pointer, by convention) */
#define RD_ARM_R12 12  /* IP (intra-procedure scratch) */
#define RD_ARM_SP  13  /* R13 - Stack Pointer */
#define RD_ARM_LR  14  /* R14 - Link Register */
#define RD_ARM_PC  15  /* R15 - Program Counter */
#define RD_ARM_CPSR 16 /* Current Program Status Register */

/* Banked registers: user/system mode SP and LR */
#define RD_ARM_SP_USR 17
#define RD_ARM_LR_USR 18

/* FIQ mode banked registers */
#define RD_ARM_R8_FIQ  19
#define RD_ARM_R9_FIQ  20
#define RD_ARM_R10_FIQ 21
#define RD_ARM_R11_FIQ 22
#define RD_ARM_R12_FIQ 23
#define RD_ARM_SP_FIQ  24
#define RD_ARM_LR_FIQ  25
#define RD_ARM_SPSR_FIQ 26

/* IRQ mode banked registers */
#define RD_ARM_SP_IRQ  27
#define RD_ARM_LR_IRQ  28
#define RD_ARM_SPSR_IRQ 29

/* Supervisor mode banked registers */
#define RD_ARM_SP_SVC  30
#define RD_ARM_LR_SVC  31
#define RD_ARM_SPSR_SVC 32

/* Abort mode banked registers */
#define RD_ARM_SP_ABT  33
#define RD_ARM_LR_ABT  34
#define RD_ARM_SPSR_ABT 35

/* Undefined mode banked registers */
#define RD_ARM_SP_UND  36
#define RD_ARM_LR_UND  37
#define RD_ARM_SPSR_UND 38

#define RD_ARM_NUM_REGISTERS 39

/* ARM interrupt types */
#define RD_ARM_IRQ 0
#define RD_ARM_FIQ 1
#define RD_ARM_SWI 2   /* Software interrupt */
#define RD_ARM_PABT 3  /* Prefetch abort */
#define RD_ARM_DABT 4  /* Data abort */
#define RD_ARM_UND 5   /* Undefined instruction */

/* ARM CPSR flag bits */
#define RD_ARM_FLAG_N 0x80000000  /* Negative */
#define RD_ARM_FLAG_Z 0x40000000  /* Zero */
#define RD_ARM_FLAG_C 0x20000000  /* Carry */
#define RD_ARM_FLAG_V 0x10000000  /* Overflow */
#define RD_ARM_FLAG_I 0x00000080  /* IRQ disable */
#define RD_ARM_FLAG_F 0x00000040  /* FIQ disable */
#define RD_ARM_FLAG_T 0x00000020  /* Thumb state */

#endif /* RETRO_DEBUG__ */
