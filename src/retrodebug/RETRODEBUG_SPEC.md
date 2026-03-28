# melonDS retrodebug.h Implementation Spec

## Overview

This document specifies the full integration of `retrodebug.h` into melonDS,
exposing the Nintendo DS's dual-CPU architecture (ARM9 + ARM7) to the
arret-debugger frontend.

## Status: Scaffold Complete

The following files have been created:

- `src/retrodebug/retrodebug.h` - retrodebug API header (v1, with new ARM types)
- `src/retrodebug/retrodebug_nds.h` - NDS-specific misc breakpoint data structs
- `src/retrodebug/RetroDebug.h` - C++ class header
- `src/retrodebug/RetroDebug.cpp` - Implementation (register access, memory, subscriptions, events)

## New CPU Types Added to retrodebug.h

```
RD_CPU_ARM7TDMI  = RD_MAKE_CPU_TYPE(7, 1)   // ARMv4T, NDS ARM7
RD_CPU_ARM946ES  = RD_MAKE_CPU_TYPE(8, 1)   // ARMv5TE, NDS ARM9
```

39 registers defined (`RD_ARM_R0` through `RD_ARM_SPSR_UND`):
- R0-R15 (general purpose + PC)
- CPSR
- Banked SP/LR/SPSR for USR, FIQ, IRQ, SVC, ABT, UND modes
- FIQ R8-R12 banked registers

6 interrupt types: IRQ, FIQ, SWI, PABT, DABT, UND.
CPSR flag bit masks for N, Z, C, V, I, F, T.

## What Is Implemented (scaffold)

### rd_System: "nds"
- 2 CPUs: ARM9 (primary), ARM7
- 6 extra memory regions: main RAM, shared WRAM, ARM7 WRAM, palette, OAM, VRAM
- 2 misc breakpoints: DMA transfer, IPC FIFO
- `get_content_info` (stub)

### CPUs
- `arm9` (ARM946E-S): full register get/set for all 39 registers
- `arm7` (ARM7TDMI): full register get/set for all 39 registers
- Both read from melonDS `ARM::R[16]`, `CPSR`, `R_FIQ[8]`, `R_SVC[3]`, `R_ABT[3]`, `R_IRQ[3]`, `R_UND[3]`

### Memory Regions

| ID | Description | Size | Access |
|---|---|---|---|
| `arm9` | ARM9 address space | 4 GB virtual | Via `NDS::ARM9Read8`/`ARM9Write8` |
| `arm7` | ARM7 address space | 4 GB virtual | Via `NDS::ARM7Read8`/`ARM7Write8` |
| `main` | Main RAM | 4 MB (NDS) / 16 MB (DSi) | Direct array access |
| `swram` | Shared WRAM | 32 KB | Direct array access |
| `arm7wram` | ARM7 private WRAM | 64 KB | Direct array access |
| `palette` | Palette RAM | 2 KB | Direct `GPU::Palette` access |
| `oam` | OAM | 2 KB | Direct `GPU::OAM` access |
| `vram` | VRAM (flat banks A-I) | 656 KB | Concatenated bank access |

### Memory Maps
- ARM9: 8 regions (ITCM, Main RAM, Shared WRAM, I/O, Palette, VRAM, GBA slot, BIOS)
- ARM7: 7 regions (BIOS, Main RAM, Shared WRAM, ARM7 WRAM, I/O, VRAM, GBA slot)

### Subscription/Event System
- Max 16 simultaneous subscriptions
- Supported event types: BREAKPOINT, STEP, INTERRUPT, MEMORY, MISC
- Fast-path flags to skip hook overhead when no subscriptions active
- Step modes: INTO, INTO_SKIP_IRQ, OVER, OUT (call_depth tracking)
- `can_halt = true` for execution events (breakpoint, step)

### Execution Hooks
- `ARM9ExecutionHook(pc)` - called before each ARM9 instruction
- `ARM7ExecutionHook(pc)` - called before each ARM7 instruction
- Both return `true` to request halt (can_halt=true path)

### Interrupt Hook
- `InterruptHook(cpu_num, kind, return_addr, vector_addr)`
- Updates step subscription call_depth for IRQ skip tracking

### Memory Watchpoint Hooks
- `MemoryWriteHook(cpu_num, addr, value, size)`
- `MemoryReadHook(cpu_num, addr, value, size)`

---

## What Must Still Be Implemented

### 1. Hook Integration into ARM Execute Loop (CRITICAL)

**Files to modify:** `src/ARM.cpp` (lines ~593-854)

The execution hooks must be called from within `ARMv5::Execute()` and
`ARMv4::Execute()`. This requires a new `CPUExecuteMode` or conditional
checks within the existing interpreter path.

**Approach B: Callback pointer on ARM class** (preferred - avoids template explosion)
```cpp
// In ARM.h:
bool (*RetroDebugHook)(uint32_t pc) = nullptr;  // set by RetroDebug

// In Execute():
if (RetroDebugHook && RetroDebugHook(R[15] - offset))
    return;
```

**Pipeline offset:** ARM9 in ARM mode: `R[15] - 8`; Thumb mode: `R[15] - 4`.
ARM7 same convention (2 instructions ahead in pipeline).

### 2. Memory Write/Read Hook Integration (HIGH PRIORITY)

**Files to modify:** `src/ARM.cpp` or `src/NDS.cpp`

Watchpoint hooks must be called on DataWrite and DataRead operations.
Since watchpoints are expensive, they should only be active when
`HasWriteSub()` or `HasReadSub()` returns true. A function pointer
approach (like the execution hook) is ideal.

### 3. IRQ Hook Integration (MEDIUM PRIORITY)

**File to modify:** `src/ARM.cpp`, specifically `ARM::TriggerIRQ()`

### 4. NDS Class Integration (CRITICAL)

**File to modify:** `src/NDS.h`, `src/NDS.cpp`

Add a `RetroDebug*` member to the NDS class.

### 5. RunFrame Dispatcher (CRITICAL)

**File to modify:** `src/NDS.cpp`, `NDS::RunFrame()`

If using the callback pointer approach, just set the hook pointers
when retrodebug is active and use the normal Interpreter path.

### 6. ARM Branch Tracking for Step-Over/Step-Out (MEDIUM PRIORITY)

ARM instruction decoding to track call/return depth:

**ARM mode (32-bit) call detection:**
- `BL`: bits [27:24] = 0b1011
- `BLX (register)`: bits [27:4] = 0x12FFF3x

**ARM mode return detection:**
- `BX LR`: `0xE12FFF1E`
- `MOV PC, LR`: `0xE1A0F00E`
- `LDMFD SP!, {..., PC}`: load-multiple with PC bit set

**Thumb mode (16-bit):**
- `BL/BLX`: two-instruction sequence
- `BX LR`: `0x4770`
- `POP {PC}`: `0xBDxx` with PC bit

### 7. Content Info from Cart Header (LOW PRIORITY)
### 8. ITCM/DTCM Memory Regions (LOW PRIORITY)
### 9. VRAM Bank-Aware Access (LOW PRIORITY)
### 10. Cache Probe for ARM9 ICache (LOW PRIORITY)
### 11. DSi Support (LOW PRIORITY)
### 12. Filesystem: Game Card (LOW PRIORITY)
### 13. CMake Build Integration (REQUIRED)

## Implementation Priority Order

1. **Hook integration into Execute loop** - without this, nothing works
2. **NDS class integration** - RetroDebug* member and RunFrame dispatch
3. **CMake build integration** - must compile
4. **Memory watchpoint hooks** - essential for debugging
5. **IRQ hook** - needed for step-over-skip-irq
6. **ARM branch tracking** - needed for step-over/step-out
7. **Content info from cart header** - quality of life
8. **ITCM/DTCM regions** - useful for ARM9 debugging
9. **VRAM bank-aware access** - useful for graphics debugging
10. **Cache probe** - niche but useful
11. **DSi support** - extends to DSi hardware
12. **NitroFS filesystem** - file browser support

## Architecture Notes

- melonDS uses C++ namespaces (`melonDS`), not C linkage
- No libretro core exists - this is a native emulator
- JIT mode cannot easily support per-instruction hooks; retrodebug
  requires interpreter mode (same constraint as GDB stub)
- The existing GDB stub (`CPUExecuteMode::InterpreterGDB`) provides a
  reference for how to hook into the execute loop
- Dual-CPU timing: ARM9 runs at 2x ARM7 clock. Both must be hookable
  independently. The frontend may subscribe to events on one or both CPUs.
