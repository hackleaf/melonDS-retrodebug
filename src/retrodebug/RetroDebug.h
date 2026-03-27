/*
 * RetroDebug.h
 * retrodebug.h integration for melonDS.
 *
 * Exposes the NDS system (ARM9 + ARM7 CPUs, memory regions) through the
 * retrodebug interface for use by debugging frontends.
 */

#ifndef MELONDS_RETRODEBUG_H
#define MELONDS_RETRODEBUG_H

#include "retrodebug.h"
#include "retrodebug_nds.h"

namespace melonDS
{

class NDS;

class RetroDebug
{
public:
    explicit RetroDebug(NDS& nds);
    ~RetroDebug();

    /// Called by the frontend to set up the debug interface.
    /// Fills in core_api_version, system, subscribe, unsubscribe.
    void SetDebugger(rd_DebuggerIf* dbg_if);

    /// Called before each ARM9 instruction in interpreter mode.
    /// Returns true if the core should halt (can_halt=true path).
    bool ARM9ExecutionHook(uint32_t pc);

    /// Called before each ARM7 instruction in interpreter mode.
    /// Returns true if the core should halt (can_halt=true path).
    bool ARM7ExecutionHook(uint32_t pc);

    /// Called on ARM9/ARM7 IRQ entry.
    void InterruptHook(int cpu_num, unsigned kind, uint32_t return_addr, uint32_t vector_addr);

    /// Called on memory writes (for watchpoint support).
    void MemoryWriteHook(int cpu_num, uint32_t addr, uint32_t value, unsigned size);

    /// Called on memory reads (for watchpoint support).
    void MemoryReadHook(int cpu_num, uint32_t addr, uint32_t value, unsigned size);

    /// Returns true if any execution subscriptions are active for ARM9.
    bool HasARM9ExecSub() const { return HasARM9ExecSub_; }

    /// Returns true if any execution subscriptions are active for ARM7.
    bool HasARM7ExecSub() const { return HasARM7ExecSub_; }

    /// Returns true if any memory write watchpoints are active.
    bool HasWriteSub() const { return HasWriteSub_; }

    /// Returns true if any memory read watchpoints are active.
    bool HasReadSub() const { return HasReadSub_; }

    /// Returns true if any subscriptions are active at all.
    bool IsActive() const { return DebuggerIf_ != nullptr; }

private:
    NDS& NDS_;
    rd_DebuggerIf* DebuggerIf_ = nullptr;

    // rd_System and components
    rd_System System_;
    rd_Cpu CpuARM9_;
    rd_Cpu CpuARM7_;
    rd_Memory MemARM9_;     // ARM9 address space (4GB virtual)
    rd_Memory MemARM7_;     // ARM7 address space (4GB virtual)
    rd_Memory MemMainRAM_;  // Main RAM (4MB NDS / 16MB DSi)
    rd_Memory MemSharedWRAM_; // Shared WRAM (32KB)
    rd_Memory MemARM7WRAM_; // ARM7 private WRAM (64KB)
    rd_Memory MemVRAM_;     // VRAM (flat composite, 656KB)
    rd_Memory MemPalette_;  // Palette RAM (2KB)
    rd_Memory MemOAM_;      // OAM (2KB)

    // Pointers for rd_System arrays
    rd_Cpu const* CpuPtrs_[2];
    rd_Memory const* ExtraRegionPtrs_[8];

    // Misc breakpoints
    rd_MiscBreakpoint BpDMA_;
    rd_MiscBreakpoint BpIPC_;
    rd_MiscBreakpoint const* SysBreakpoints_[2];

    // Subscription management
    static constexpr int MaxSubs = 16;
    struct SubSlot {
        bool active = false;
        rd_Subscription sub;
        rd_SubscriptionID id;
        int call_depth = 0;
    };
    SubSlot Subs_[MaxSubs];
    rd_SubscriptionID NextID_ = 1;

    // Fast-path flags
    bool HasARM9ExecSub_ = false;
    bool HasARM9BroadSub_ = false;
    bool HasARM7ExecSub_ = false;
    bool HasARM7BroadSub_ = false;
    bool HasWriteSub_ = false;
    bool HasReadSub_ = false;
    bool HasDMASub_ = false;
    bool HasIPCSub_ = false;

    // Static callbacks (bridge to instance methods)
    static rd_SubscriptionID SubscribeCB(rd_Subscription const* sub);
    static void UnsubscribeCB(rd_SubscriptionID id);

    // Instance methods
    rd_SubscriptionID Subscribe(rd_Subscription const* sub);
    void Unsubscribe(rd_SubscriptionID id);
    void RecomputeSubState();

    // Memory peek/poke callbacks
    static uint64_t ARM9Peek(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t* out, bool side_effects);
    static uint64_t ARM9Poke(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t const* buf);
    static uint64_t ARM7Peek(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t* out, bool side_effects);
    static uint64_t ARM7Poke(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t const* buf);
    static uint64_t MainRAMPeek(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t* out, bool side_effects);
    static uint64_t MainRAMPoke(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t const* buf);
    static uint64_t SharedWRAMPeek(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t* out, bool side_effects);
    static uint64_t SharedWRAMPoke(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t const* buf);
    static uint64_t ARM7WRAMPeek(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t* out, bool side_effects);
    static uint64_t ARM7WRAMPoke(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t const* buf);
    static uint64_t VRAMPeek(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t* out, bool side_effects);
    static uint64_t VRAMPoke(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t const* buf);
    static uint64_t PalettePeek(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t* out, bool side_effects);
    static uint64_t PalettePoke(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t const* buf);
    static uint64_t OAMPeek(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t* out, bool side_effects);
    static uint64_t OAMPoke(rd_Memory const* self, uint64_t addr, uint64_t size, uint8_t const* buf);

    // ARM9 memory map
    static unsigned ARM9MapCount(rd_Memory const* self);
    static void ARM9Map(rd_Memory const* self, rd_MemoryMap* out);

    // ARM7 memory map
    static unsigned ARM7MapCount(rd_Memory const* self);
    static void ARM7Map(rd_Memory const* self, rd_MemoryMap* out);

    // CPU register access
    static uint64_t ARM9GetReg(rd_Cpu const* self, unsigned reg);
    static int ARM9SetReg(rd_Cpu const* self, unsigned reg, uint64_t value);
    static uint64_t ARM7GetReg(rd_Cpu const* self, unsigned reg);
    static int ARM7SetReg(rd_Cpu const* self, unsigned reg, uint64_t value);

    // Content info
    static int GetContentInfo(char* outbuff, int outsize);

    // Global instance pointer (for static callbacks)
    static RetroDebug* Instance_;
};

} // namespace melonDS

#endif // MELONDS_RETRODEBUG_H
