/*
 * RetroDebug.cpp
 * retrodebug.h integration for melonDS.
 *
 * Implements the retrodebug interface, exposing NDS system state
 * (ARM9 + ARM7 CPUs, memory regions, subscriptions/events) to
 * debugging frontends.
 */

#include "RetroDebug.h"
#include "../NDS.h"
#include "../GPU.h"
#include "../ARM.h"

#include <cstring>
#include <cstdio>
#include <algorithm>

namespace melonDS
{

RetroDebug* RetroDebug::Instance_ = nullptr;

// ========================================================================
// Helper: read/write ARM banked registers from melonDS ARM class
// ========================================================================

static uint64_t ReadARMRegister(ARM& arm, unsigned reg)
{
    if (reg <= RD_ARM_PC)
        return arm.R[reg];
    if (reg == RD_ARM_CPSR)
        return arm.CPSR;

    switch (reg) {
        case RD_ARM_SP_USR: return arm.R[13];
        case RD_ARM_LR_USR: return arm.R[14];
        case RD_ARM_R8_FIQ:    return arm.R_FIQ[0];
        case RD_ARM_R9_FIQ:    return arm.R_FIQ[1];
        case RD_ARM_R10_FIQ:   return arm.R_FIQ[2];
        case RD_ARM_R11_FIQ:   return arm.R_FIQ[3];
        case RD_ARM_R12_FIQ:   return arm.R_FIQ[4];
        case RD_ARM_SP_FIQ:    return arm.R_FIQ[5];
        case RD_ARM_LR_FIQ:    return arm.R_FIQ[6];
        case RD_ARM_SPSR_FIQ:  return arm.R_FIQ[7];
        case RD_ARM_SP_IRQ:    return arm.R_IRQ[0];
        case RD_ARM_LR_IRQ:    return arm.R_IRQ[1];
        case RD_ARM_SPSR_IRQ:  return arm.R_IRQ[2];
        case RD_ARM_SP_SVC:    return arm.R_SVC[0];
        case RD_ARM_LR_SVC:    return arm.R_SVC[1];
        case RD_ARM_SPSR_SVC:  return arm.R_SVC[2];
        case RD_ARM_SP_ABT:    return arm.R_ABT[0];
        case RD_ARM_LR_ABT:    return arm.R_ABT[1];
        case RD_ARM_SPSR_ABT:  return arm.R_ABT[2];
        case RD_ARM_SP_UND:    return arm.R_UND[0];
        case RD_ARM_LR_UND:    return arm.R_UND[1];
        case RD_ARM_SPSR_UND:  return arm.R_UND[2];
        default: return 0;
    }
}

static int WriteARMRegister(ARM& arm, unsigned reg, uint64_t value)
{
    u32 val = (u32)value;
    if (reg <= RD_ARM_PC) { arm.R[reg] = val; return 1; }
    if (reg == RD_ARM_CPSR) { arm.CPSR = val; return 1; }

    switch (reg) {
        case RD_ARM_SP_USR: arm.R[13] = val; return 1;
        case RD_ARM_LR_USR: arm.R[14] = val; return 1;
        case RD_ARM_R8_FIQ:    arm.R_FIQ[0] = val; return 1;
        case RD_ARM_R9_FIQ:    arm.R_FIQ[1] = val; return 1;
        case RD_ARM_R10_FIQ:   arm.R_FIQ[2] = val; return 1;
        case RD_ARM_R11_FIQ:   arm.R_FIQ[3] = val; return 1;
        case RD_ARM_R12_FIQ:   arm.R_FIQ[4] = val; return 1;
        case RD_ARM_SP_FIQ:    arm.R_FIQ[5] = val; return 1;
        case RD_ARM_LR_FIQ:    arm.R_FIQ[6] = val; return 1;
        case RD_ARM_SPSR_FIQ:  arm.R_FIQ[7] = val; return 1;
        case RD_ARM_SP_IRQ:    arm.R_IRQ[0] = val; return 1;
        case RD_ARM_LR_IRQ:    arm.R_IRQ[1] = val; return 1;
        case RD_ARM_SPSR_IRQ:  arm.R_IRQ[2] = val; return 1;
        case RD_ARM_SP_SVC:    arm.R_SVC[0] = val; return 1;
        case RD_ARM_LR_SVC:    arm.R_SVC[1] = val; return 1;
        case RD_ARM_SPSR_SVC:  arm.R_SVC[2] = val; return 1;
        case RD_ARM_SP_ABT:    arm.R_ABT[0] = val; return 1;
        case RD_ARM_LR_ABT:    arm.R_ABT[1] = val; return 1;
        case RD_ARM_SPSR_ABT:  arm.R_ABT[2] = val; return 1;
        case RD_ARM_SP_UND:    arm.R_UND[0] = val; return 1;
        case RD_ARM_LR_UND:    arm.R_UND[1] = val; return 1;
        case RD_ARM_SPSR_UND:  arm.R_UND[2] = val; return 1;
        default: return 0;
    }
}

// ========================================================================
// Constructor / Destructor
// ========================================================================

RetroDebug::RetroDebug(NDS& nds) : NDS_(nds)
{
    Instance_ = this;
    memset(Subs_, 0, sizeof(Subs_));

    BpDMA_ = { .v1 = { .description = "DMA Transfer" } };
    BpIPC_ = { .v1 = { .description = "IPC FIFO" } };
    SysBreakpoints_[0] = &BpDMA_;
    SysBreakpoints_[1] = &BpIPC_;

    MemARM9_ = { .v1 = { .id = "arm9", .description = "ARM9 Address Space", .alignment = 4, .size = 0x100000000ULL, .break_points = nullptr, .num_break_points = 0, .peek = ARM9Peek, .poke = ARM9Poke, .get_memory_map_count = ARM9MapCount, .get_memory_map = ARM9Map, .get_bank_address = nullptr, .cache_probe = nullptr }};
    MemARM7_ = { .v1 = { .id = "arm7", .description = "ARM7 Address Space", .alignment = 4, .size = 0x100000000ULL, .break_points = nullptr, .num_break_points = 0, .peek = ARM7Peek, .poke = ARM7Poke, .get_memory_map_count = ARM7MapCount, .get_memory_map = ARM7Map, .get_bank_address = nullptr, .cache_probe = nullptr }};
    MemMainRAM_ = { .v1 = { .id = "main", .description = "Main RAM", .alignment = 4, .size = 0x400000, .break_points = nullptr, .num_break_points = 0, .peek = MainRAMPeek, .poke = MainRAMPoke, .get_memory_map_count = nullptr, .get_memory_map = nullptr, .get_bank_address = nullptr, .cache_probe = nullptr }};
    MemSharedWRAM_ = { .v1 = { .id = "swram", .description = "Shared WRAM", .alignment = 4, .size = 0x8000, .break_points = nullptr, .num_break_points = 0, .peek = SharedWRAMPeek, .poke = SharedWRAMPoke, .get_memory_map_count = nullptr, .get_memory_map = nullptr, .get_bank_address = nullptr, .cache_probe = nullptr }};
    MemARM7WRAM_ = { .v1 = { .id = "arm7wram", .description = "ARM7 Private WRAM", .alignment = 4, .size = 0x10000, .break_points = nullptr, .num_break_points = 0, .peek = ARM7WRAMPeek, .poke = ARM7WRAMPoke, .get_memory_map_count = nullptr, .get_memory_map = nullptr, .get_bank_address = nullptr, .cache_probe = nullptr }};
    MemPalette_ = { .v1 = { .id = "palette", .description = "Palette RAM", .alignment = 2, .size = 2 * 1024, .break_points = nullptr, .num_break_points = 0, .peek = PalettePeek, .poke = PalettePoke, .get_memory_map_count = nullptr, .get_memory_map = nullptr, .get_bank_address = nullptr, .cache_probe = nullptr }};
    MemOAM_ = { .v1 = { .id = "oam", .description = "OAM (Object Attribute Memory)", .alignment = 4, .size = 2 * 1024, .break_points = nullptr, .num_break_points = 0, .peek = OAMPeek, .poke = OAMPoke, .get_memory_map_count = nullptr, .get_memory_map = nullptr, .get_bank_address = nullptr, .cache_probe = nullptr }};
    MemVRAM_ = { .v1 = { .id = "vram", .description = "VRAM (Video RAM)", .alignment = 2, .size = 656 * 1024, .break_points = nullptr, .num_break_points = 0, .peek = VRAMPeek, .poke = VRAMPoke, .get_memory_map_count = nullptr, .get_memory_map = nullptr, .get_bank_address = nullptr, .cache_probe = nullptr }};

    CpuARM9_ = { .v1 = { .id = "arm9", .description = "ARM946E-S (ARM9)", .type = RD_CPU_ARM946ES, .memory_region = &MemARM9_, .break_points = nullptr, .num_break_points = 0, .get_register = ARM9GetReg, .set_register = ARM9SetReg, .pipeline_get_delay_pc = nullptr }};
    CpuARM7_ = { .v1 = { .id = "arm7", .description = "ARM7TDMI (ARM7)", .type = RD_CPU_ARM7TDMI, .memory_region = &MemARM7_, .break_points = nullptr, .num_break_points = 0, .get_register = ARM7GetReg, .set_register = ARM7SetReg, .pipeline_get_delay_pc = nullptr }};

    CpuPtrs_[0] = &CpuARM9_;
    CpuPtrs_[1] = &CpuARM7_;
    ExtraRegionPtrs_[0] = &MemMainRAM_;
    ExtraRegionPtrs_[1] = &MemSharedWRAM_;
    ExtraRegionPtrs_[2] = &MemARM7WRAM_;
    ExtraRegionPtrs_[3] = &MemPalette_;
    ExtraRegionPtrs_[4] = &MemOAM_;
    ExtraRegionPtrs_[5] = &MemVRAM_;

    System_ = { .v1 = { .id = "nds", .cpus = CpuPtrs_, .num_cpus = 2, .memory_regions = ExtraRegionPtrs_, .num_memory_regions = 6, .filesystems = nullptr, .num_filesystems = 0, .break_points = SysBreakpoints_, .num_break_points = 2, .get_content_info = GetContentInfo }};
}

RetroDebug::~RetroDebug()
{
    if (Instance_ == this) Instance_ = nullptr;
}

void RetroDebug::SetDebugger(rd_DebuggerIf* dbg_if)
{
    DebuggerIf_ = dbg_if;
    if (DebuggerIf_) {
        DebuggerIf_->core_api_version = RD_API_VERSION;
        DebuggerIf_->v1.system = &System_;
        DebuggerIf_->v1.subscribe = SubscribeCB;
        DebuggerIf_->v1.unsubscribe = UnsubscribeCB;
    }
}

// ========================================================================
// Subscription management
// ========================================================================

rd_SubscriptionID RetroDebug::SubscribeCB(rd_Subscription const* sub) { if (!Instance_) return -1; return Instance_->Subscribe(sub); }
void RetroDebug::UnsubscribeCB(rd_SubscriptionID id) { if (!Instance_) return; Instance_->Unsubscribe(id); }

rd_SubscriptionID RetroDebug::Subscribe(rd_Subscription const* sub)
{
    if (sub->type == RD_EVENT_BREAKPOINT) {
        if (sub->breakpoint.cpu != &CpuARM9_ && sub->breakpoint.cpu != &CpuARM7_) return -1;
    } else if (sub->type == RD_EVENT_STEP) {
        if (sub->step.cpu != &CpuARM9_ && sub->step.cpu != &CpuARM7_) return -1;
    } else if (sub->type == RD_EVENT_INTERRUPT) {
        if (sub->interrupt.cpu != &CpuARM9_ && sub->interrupt.cpu != &CpuARM7_) return -1;
    } else if (sub->type == RD_EVENT_MEMORY) {
        if (sub->memory.memory != &MemARM9_ && sub->memory.memory != &MemARM7_) return -1;
    } else if (sub->type == RD_EVENT_MISC) {
        if (sub->misc.breakpoint != &BpDMA_ && sub->misc.breakpoint != &BpIPC_) return -1;
    } else { return -1; }

    for (int i = 0; i < MaxSubs; i++) {
        if (!Subs_[i].active) {
            Subs_[i].active = true;
            Subs_[i].sub = *sub;
            Subs_[i].id = NextID_++;
            Subs_[i].call_depth = 0;
            RecomputeSubState();
            return Subs_[i].id;
        }
    }
    return -1;
}

void RetroDebug::Unsubscribe(rd_SubscriptionID id)
{
    for (int i = 0; i < MaxSubs; i++) {
        if (Subs_[i].active && Subs_[i].id == id) { Subs_[i].active = false; RecomputeSubState(); return; }
    }
}

void RetroDebug::RecomputeSubState()
{
    HasARM9BpSub_ = HasARM9StepSub_ = HasARM7BpSub_ = HasARM7StepSub_ = false;
    HasWriteSub_ = HasReadSub_ = HasDMASub_ = HasIPCSub_ = false;

    for (int i = 0; i < MaxSubs; i++) {
        SubSlot& s = Subs_[i];
        if (!s.active) continue;
        if (s.sub.type == RD_EVENT_BREAKPOINT) {
            if (s.sub.breakpoint.cpu == &CpuARM9_) HasARM9BpSub_ = true;
            else if (s.sub.breakpoint.cpu == &CpuARM7_) HasARM7BpSub_ = true;
        } else if (s.sub.type == RD_EVENT_STEP) {
            if (s.sub.step.cpu == &CpuARM9_) HasARM9StepSub_ = true;
            else if (s.sub.step.cpu == &CpuARM7_) HasARM7StepSub_ = true;
        } else if (s.sub.type == RD_EVENT_MEMORY) {
            if (s.sub.memory.operation & RD_MEMORY_WRITE) HasWriteSub_ = true;
            if (s.sub.memory.operation & RD_MEMORY_READ) HasReadSub_ = true;
        } else if (s.sub.type == RD_EVENT_MISC) {
            if (s.sub.misc.breakpoint == &BpDMA_) HasDMASub_ = true;
            else if (s.sub.misc.breakpoint == &BpIPC_) HasIPCSub_ = true;
        }
    }
    RebuildBloom();
    UpdateHooks();
}

void RetroDebug::RebuildBloom()
{
    memset(BloomARM9_, 0, sizeof(BloomARM9_));
    memset(BloomARM7_, 0, sizeof(BloomARM7_));
    for (int i = 0; i < MaxSubs; i++) {
        SubSlot& s = Subs_[i];
        if (!s.active || s.sub.type != RD_EVENT_BREAKPOINT) continue;
        unsigned h = BloomHash((uint32_t)s.sub.breakpoint.address);
        if (s.sub.breakpoint.cpu == &CpuARM9_) { if (BloomARM9_[h] < 255) BloomARM9_[h]++; }
        else if (s.sub.breakpoint.cpu == &CpuARM7_) { if (BloomARM7_[h] < 255) BloomARM7_[h]++; }
    }
}

void RetroDebug::UpdateHooks()
{
    bool arm9_need = HasARM9BpSub_ || HasARM9StepSub_;
    NDS_.ARM9.RetroDebugHook = arm9_need ? ARM9HookTrampoline : nullptr;
    NDS_.ARM9.RetroDebugUserData = arm9_need ? this : nullptr;
    bool arm7_need = HasARM7BpSub_ || HasARM7StepSub_;
    NDS_.ARM7.RetroDebugHook = arm7_need ? ARM7HookTrampoline : nullptr;
    NDS_.ARM7.RetroDebugUserData = arm7_need ? this : nullptr;
}

bool RetroDebug::ARM9HookTrampoline(u32 pc, void* ud) { return static_cast<RetroDebug*>(ud)->CheckARM9(pc); }
bool RetroDebug::ARM7HookTrampoline(u32 pc, void* ud) { return static_cast<RetroDebug*>(ud)->CheckARM7(pc); }

// ========================================================================
// Per-instruction checks (bloom filter fast path)
// ========================================================================

bool RetroDebug::CheckARM9(uint32_t pc)
{
    bool halt = false;
    if (!HasARM9StepSub_ && !BloomARM9_[BloomHash(pc)]) return false;

    for (int i = 0; i < MaxSubs; i++) {
        SubSlot& s = Subs_[i];
        if (!s.active) continue;
        if (s.sub.type == RD_EVENT_BREAKPOINT) {
            if (s.sub.breakpoint.cpu != &CpuARM9_ || s.sub.breakpoint.address != pc) continue;
            rd_Event ev; ev.type = RD_EVENT_BREAKPOINT; ev.can_halt = true;
            ev.breakpoint.cpu = &CpuARM9_; ev.breakpoint.address = pc;
            if (DebuggerIf_->v1.handle_event(DebuggerIf_->v1.user_data, s.id, &ev)) halt = true;
        } else if (s.sub.type == RD_EVENT_STEP) {
            if (s.sub.step.cpu != &CpuARM9_) continue;
            switch (s.sub.step.mode) {
                case RD_STEP_INTO: break;
                case RD_STEP_INTO_SKIP_IRQ: if (s.call_depth > 0) continue; break;
                case RD_STEP_OVER: if (s.call_depth > 0) continue; break;
                case RD_STEP_OUT: if (s.call_depth >= 0) continue; break;
            }
            rd_Event ev; ev.type = RD_EVENT_STEP; ev.can_halt = true;
            ev.step.cpu = &CpuARM9_; ev.step.address = pc;
            if (DebuggerIf_->v1.handle_event(DebuggerIf_->v1.user_data, s.id, &ev)) halt = true;
        }
    }
    return halt;
}

bool RetroDebug::CheckARM7(uint32_t pc)
{
    bool halt = false;
    if (!HasARM7StepSub_ && !BloomARM7_[BloomHash(pc)]) return false;

    for (int i = 0; i < MaxSubs; i++) {
        SubSlot& s = Subs_[i];
        if (!s.active) continue;
        if (s.sub.type == RD_EVENT_BREAKPOINT) {
            if (s.sub.breakpoint.cpu != &CpuARM7_ || s.sub.breakpoint.address != pc) continue;
            rd_Event ev; ev.type = RD_EVENT_BREAKPOINT; ev.can_halt = true;
            ev.breakpoint.cpu = &CpuARM7_; ev.breakpoint.address = pc;
            if (DebuggerIf_->v1.handle_event(DebuggerIf_->v1.user_data, s.id, &ev)) halt = true;
        } else if (s.sub.type == RD_EVENT_STEP) {
            if (s.sub.step.cpu != &CpuARM7_) continue;
            switch (s.sub.step.mode) {
                case RD_STEP_INTO: break;
                case RD_STEP_INTO_SKIP_IRQ: if (s.call_depth > 0) continue; break;
                case RD_STEP_OVER: if (s.call_depth > 0) continue; break;
                case RD_STEP_OUT: if (s.call_depth >= 0) continue; break;
            }
            rd_Event ev; ev.type = RD_EVENT_STEP; ev.can_halt = true;
            ev.step.cpu = &CpuARM7_; ev.step.address = pc;
            if (DebuggerIf_->v1.handle_event(DebuggerIf_->v1.user_data, s.id, &ev)) halt = true;
        }
    }
    return halt;
}

void RetroDebug::InterruptHook(int cpu_num, unsigned kind, uint32_t return_addr, uint32_t vector_addr)
{
    if (!DebuggerIf_) return;
    rd_Cpu const* cpu = (cpu_num == 0) ? &CpuARM9_ : &CpuARM7_;
    for (int i = 0; i < MaxSubs; i++) {
        SubSlot& s = Subs_[i];
        if (!s.active || s.sub.type != RD_EVENT_STEP) continue;
        if (s.sub.step.cpu != cpu) continue;
        if (s.sub.step.mode == RD_STEP_INTO_SKIP_IRQ || s.sub.step.mode == RD_STEP_OVER) s.call_depth++;
    }
    for (int i = 0; i < MaxSubs; i++) {
        SubSlot& s = Subs_[i];
        if (!s.active || s.sub.type != RD_EVENT_INTERRUPT) continue;
        if (s.sub.interrupt.cpu != cpu) continue;
        rd_Event ev; ev.type = RD_EVENT_INTERRUPT; ev.can_halt = false;
        ev.interrupt.cpu = cpu; ev.interrupt.kind = kind;
        ev.interrupt.return_address = return_addr; ev.interrupt.vector_address = vector_addr;
        DebuggerIf_->v1.handle_event(DebuggerIf_->v1.user_data, s.id, &ev);
    }
}

void RetroDebug::MemoryWriteHook(int cpu_num, uint32_t addr, uint32_t value, unsigned size)
{
    if (!DebuggerIf_) return;
    rd_Memory const* mem = (cpu_num == 0) ? &MemARM9_ : &MemARM7_;
    for (int i = 0; i < MaxSubs; i++) {
        SubSlot& s = Subs_[i];
        if (!s.active || s.sub.type != RD_EVENT_MEMORY) continue;
        if (s.sub.memory.memory != mem || !(s.sub.memory.operation & RD_MEMORY_WRITE)) continue;
        if (addr + size <= s.sub.memory.address_range_begin || addr >= s.sub.memory.address_range_end) continue;
        rd_Event ev; ev.type = RD_EVENT_MEMORY; ev.can_halt = false;
        ev.memory.memory = mem; ev.memory.address = addr;
        ev.memory.operation = RD_MEMORY_WRITE; ev.memory.value = (uint8_t)(value & 0xFF); ev.memory.accessor = nullptr;
        DebuggerIf_->v1.handle_event(DebuggerIf_->v1.user_data, s.id, &ev);
    }
}

void RetroDebug::MemoryReadHook(int cpu_num, uint32_t addr, uint32_t value, unsigned size)
{
    if (!DebuggerIf_) return;
    rd_Memory const* mem = (cpu_num == 0) ? &MemARM9_ : &MemARM7_;
    for (int i = 0; i < MaxSubs; i++) {
        SubSlot& s = Subs_[i];
        if (!s.active || s.sub.type != RD_EVENT_MEMORY) continue;
        if (s.sub.memory.memory != mem || !(s.sub.memory.operation & RD_MEMORY_READ)) continue;
        if (addr + size <= s.sub.memory.address_range_begin || addr >= s.sub.memory.address_range_end) continue;
        rd_Event ev; ev.type = RD_EVENT_MEMORY; ev.can_halt = false;
        ev.memory.memory = mem; ev.memory.address = addr;
        ev.memory.operation = RD_MEMORY_READ; ev.memory.value = (uint8_t)(value & 0xFF); ev.memory.accessor = nullptr;
        DebuggerIf_->v1.handle_event(DebuggerIf_->v1.user_data, s.id, &ev);
    }
}

// ========================================================================
// CPU register access callbacks
// ========================================================================

uint64_t RetroDebug::ARM9GetReg(rd_Cpu const*, unsigned reg) { if (!Instance_) return 0; return ReadARMRegister(Instance_->NDS_.ARM9, reg); }
int RetroDebug::ARM9SetReg(rd_Cpu const*, unsigned reg, uint64_t value) { if (!Instance_) return 0; return WriteARMRegister(Instance_->NDS_.ARM9, reg, value); }
uint64_t RetroDebug::ARM7GetReg(rd_Cpu const*, unsigned reg) { if (!Instance_) return 0; return ReadARMRegister(Instance_->NDS_.ARM7, reg); }
int RetroDebug::ARM7SetReg(rd_Cpu const*, unsigned reg, uint64_t value) { if (!Instance_) return 0; return WriteARMRegister(Instance_->NDS_.ARM7, reg, value); }

// ========================================================================
// Memory peek/poke callbacks
// ========================================================================

uint64_t RetroDebug::ARM9Peek(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t* out, bool) { if (!Instance_) return 0; NDS& nds = Instance_->NDS_; for (uint64_t i = 0; i < size; i++) out[i] = nds.ARM9Read8((u32)(addr + i)); return size; }
uint64_t RetroDebug::ARM9Poke(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t const* buf) { if (!Instance_) return 0; NDS& nds = Instance_->NDS_; for (uint64_t i = 0; i < size; i++) nds.ARM9Write8((u32)(addr + i), buf[i]); return size; }
uint64_t RetroDebug::ARM7Peek(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t* out, bool) { if (!Instance_) return 0; NDS& nds = Instance_->NDS_; for (uint64_t i = 0; i < size; i++) out[i] = nds.ARM7Read8((u32)(addr + i)); return size; }
uint64_t RetroDebug::ARM7Poke(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t const* buf) { if (!Instance_) return 0; NDS& nds = Instance_->NDS_; for (uint64_t i = 0; i < size; i++) nds.ARM7Write8((u32)(addr + i), buf[i]); return size; }

uint64_t RetroDebug::MainRAMPeek(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t* out, bool) { if (!Instance_) return 0; NDS& nds = Instance_->NDS_; u32 mask = nds.MainRAMMask; for (uint64_t i = 0; i < size; i++) out[i] = nds.MainRAM[(addr + i) & mask]; return size; }
uint64_t RetroDebug::MainRAMPoke(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t const* buf) { if (!Instance_) return 0; NDS& nds = Instance_->NDS_; u32 mask = nds.MainRAMMask; for (uint64_t i = 0; i < size; i++) nds.MainRAM[(addr + i) & mask] = buf[i]; return size; }
uint64_t RetroDebug::SharedWRAMPeek(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t* out, bool) { if (!Instance_) return 0; NDS& nds = Instance_->NDS_; for (uint64_t i = 0; i < size; i++) out[i] = nds.SharedWRAM[(addr + i) & 0x7FFF]; return size; }
uint64_t RetroDebug::SharedWRAMPoke(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t const* buf) { if (!Instance_) return 0; NDS& nds = Instance_->NDS_; for (uint64_t i = 0; i < size; i++) nds.SharedWRAM[(addr + i) & 0x7FFF] = buf[i]; return size; }
uint64_t RetroDebug::ARM7WRAMPeek(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t* out, bool) { if (!Instance_) return 0; NDS& nds = Instance_->NDS_; for (uint64_t i = 0; i < size; i++) out[i] = nds.ARM7WRAM[(addr + i) & 0xFFFF]; return size; }
uint64_t RetroDebug::ARM7WRAMPoke(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t const* buf) { if (!Instance_) return 0; NDS& nds = Instance_->NDS_; for (uint64_t i = 0; i < size; i++) nds.ARM7WRAM[(addr + i) & 0xFFFF] = buf[i]; return size; }
uint64_t RetroDebug::PalettePeek(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t* out, bool) { if (!Instance_) return 0; const u8* pal = Instance_->NDS_.GPU.Palette; for (uint64_t i = 0; i < size; i++) out[i] = pal[(addr + i) & 0x7FF]; return size; }
uint64_t RetroDebug::PalettePoke(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t const* buf) { if (!Instance_) return 0; u8* pal = Instance_->NDS_.GPU.Palette; for (uint64_t i = 0; i < size; i++) pal[(addr + i) & 0x7FF] = buf[i]; return size; }
uint64_t RetroDebug::OAMPeek(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t* out, bool) { if (!Instance_) return 0; const u8* oam = Instance_->NDS_.GPU.OAM; for (uint64_t i = 0; i < size; i++) out[i] = oam[(addr + i) & 0x7FF]; return size; }
uint64_t RetroDebug::OAMPoke(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t const* buf) { if (!Instance_) return 0; u8* oam = Instance_->NDS_.GPU.OAM; for (uint64_t i = 0; i < size; i++) oam[(addr + i) & 0x7FF] = buf[i]; return size; }

uint64_t RetroDebug::VRAMPeek(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t* out, bool)
{
    if (!Instance_) return 0;
    static const u32 bs[] = { 128*1024, 128*1024, 128*1024, 128*1024, 64*1024, 16*1024, 16*1024, 32*1024, 16*1024 };
    GPU& gpu = Instance_->NDS_.GPU;
    for (uint64_t i = 0; i < size; i++) {
        u32 off = (u32)((addr + i) % (656 * 1024)), bo = 0;
        for (int b = 0; b < 9; b++) { if (off < bo + bs[b]) { out[i] = gpu.VRAM[b][off - bo]; break; } bo += bs[b]; }
    }
    return size;
}

uint64_t RetroDebug::VRAMPoke(rd_Memory const*, uint64_t addr, uint64_t size, uint8_t const* buf)
{
    if (!Instance_) return 0;
    static const u32 bs[] = { 128*1024, 128*1024, 128*1024, 128*1024, 64*1024, 16*1024, 16*1024, 32*1024, 16*1024 };
    GPU& gpu = Instance_->NDS_.GPU;
    for (uint64_t i = 0; i < size; i++) {
        u32 off = (u32)((addr + i) % (656 * 1024)), bo = 0;
        for (int b = 0; b < 9; b++) { if (off < bo + bs[b]) { gpu.VRAM[b][off - bo] = buf[i]; break; } bo += bs[b]; }
    }
    return size;
}

// ========================================================================
// Memory maps
// ========================================================================

unsigned RetroDebug::ARM9MapCount(rd_Memory const*) { return 8; }
void RetroDebug::ARM9Map(rd_Memory const*, rd_MemoryMap* out)
{
    if (!Instance_) return;
    out[0] = { 0x00000000, 0x02000000, nullptr, 0, -1, 0 };
    out[1] = { 0x02000000, 0x01000000, &Instance_->MemMainRAM_, 0, -1, 0 };
    out[2] = { 0x03000000, 0x01000000, &Instance_->MemSharedWRAM_, 0, -1, 0 };
    out[3] = { 0x04000000, 0x01000000, nullptr, 0, -1, 0 };
    out[4] = { 0x05000000, 0x01000000, &Instance_->MemPalette_, 0, -1, 0 };
    out[5] = { 0x06000000, 0x02000000, nullptr, 0, -1, 0 };
    out[6] = { 0x08000000, 0xF7000000ULL, nullptr, 0, -1, 0 };
    out[7] = { 0xFFFF0000, 0x00010000, nullptr, 0, -1, 0 };
}

unsigned RetroDebug::ARM7MapCount(rd_Memory const*) { return 7; }
void RetroDebug::ARM7Map(rd_Memory const*, rd_MemoryMap* out)
{
    if (!Instance_) return;
    out[0] = { 0x00000000, 0x02000000, nullptr, 0, -1, 0 };
    out[1] = { 0x02000000, 0x01000000, &Instance_->MemMainRAM_, 0, -1, 0 };
    out[2] = { 0x03000000, 0x00800000, &Instance_->MemSharedWRAM_, 0, -1, 0 };
    out[3] = { 0x03800000, 0x00800000, &Instance_->MemARM7WRAM_, 0, -1, 0 };
    out[4] = { 0x04000000, 0x01000000, nullptr, 0, -1, 0 };
    out[5] = { 0x06000000, 0x02000000, nullptr, 0, -1, 0 };
    out[6] = { 0x08000000, 0xF8000000ULL, nullptr, 0, -1, 0 };
}

int RetroDebug::GetContentInfo(char* outbuff, int outsize)
{
    const char* info = "Nintendo DS";
    int len = (int)strlen(info);
    if (outbuff && outsize > 0) { int n = std::min(len, outsize - 1); memcpy(outbuff, info, n); outbuff[n] = '\0'; }
    return len;
}

} // namespace melonDS
