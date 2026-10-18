#include "fault_injector/register_fault_injector.hh"

#include <memory>

#include "arch/generic/isa.hh"
#include "arch/generic/pcstate.hh"
#include "cpu/reg_class.hh"
#include "cpu/simple_thread.hh"
#include "cpu/thread_context.hh"
#include "debug/FI.hh"

namespace gem5
{
RegisterFaultInjector::RegisterFaultInjector(
    const RegisterFaultInjectorParams &p)
    : BaseFaultInjector(p),
      targetCpu(dynamic_cast<BaseCPU *>(p.target_object)),
      registers(p.target_registers),
      byteIndices(p.target_byte_indices),
      byteMasks(p.target_byte_masks),
      pcRegisterIndex(p.pc_register_index)
{
    if (!targetCpu) {
        fatal("RegisterFaultInjector: target_object is not a BaseCPU!");
    }
    if (registers.size() != resolvedSites.size() ||
        byteIndices.size() != resolvedSites.size() ||
        byteMasks.size() != resolvedSites.size()) {
        fatal("RegisterFaultInjector: registers/byteIndices/byteMasks/sites "
              "size mismatch (%d/%d/%d/%d)\n",
              registers.size(), byteIndices.size(), byteMasks.size(),
              resolvedSites.size());
    }
}

void
RegisterFaultInjector::rebindCpu(BaseCPU *newCpu)
{
    targetCpu = newCpu;
    BaseFaultInjector::rebindCpu(newCpu);
}

bool
RegisterFaultInjector::applyFault(size_t i, ResolvedSite &site)
{
    lastPointWasPc = false;
    site.kind = "register";
    if (targetCpu->numContexts() == 0) {
        warn("RegisterFaultInjector: CPU has no thread contexts\n");
        return false;
    }
    ThreadContext *tc = targetCpu->getContext(0);
    const auto &rcs = tc->getIsaPtr()->regClasses();
    const RegClass &intRC = *rcs.at(IntRegClass);
    if (registers[i] >= intRC.numRegs()) {
        warn("RegisterFaultInjector: register index %d out of range (%d)\n",
             registers[i], intRC.numRegs());
        return false;
    }
    if (byteIndices[i] > 7 || byteMasks[i] > 0xFF) {
        warn("RegisterFaultInjector: byte index/mask out of range "
             "(byte=%d, mask=%d)\n",
             byteIndices[i], byteMasks[i]);
        return false;
    }

    if (pcRegisterIndex >= 0 &&
        registers[i] == static_cast<uint32_t>(pcRegisterIndex)) {
        site.kind = "pc";
        if (!pcFlipOriginalValid || pcFlipTick != curTick()) {
            pcFlipOriginal = tc->pcState().instAddr();
            pcFlipTick = curTick();
            pcFlipOriginalValid = true;
        }
        Addr curAddr = tc->pcState().instAddr();
        Addr newAddr = curAddr ^ (Addr(byteMasks[i]) << (byteIndices[i] * 8));
        std::unique_ptr<PCStateBase> pc(tc->pcState().clone());
        pc->set(newAddr);
        tc->pcState(*pc);
        lastPointWasPc = true;
        lastPointOriginalPc = pcFlipOriginal;
        site.hasInjectPc = true;
        site.injectPc = pcFlipOriginal;
        lastPointCorruptedPc = newAddr;
        DPRINTF(FI, "RegisterFaultInjector: pc flip byte=%d, mask=%d, "
                "old_pc=%#x, new_pc=%#x\n",
                byteIndices[i], byteMasks[i], curAddr, newAddr);
        return true;
    }

    RegId rid = intRC[registers[i]];
    RegVal v = tc->getReg(rid);
    v ^= (RegVal(byteMasks[i]) << (byteIndices[i] * 8));
    tc->setReg(rid, v);
    DPRINTF(FI, "RegisterFaultInjector: reg=%d, byte=%d, mask=%d\n",
            registers[i], byteIndices[i], byteMasks[i]);
    return true;
}

void
RegisterFaultInjector::writePointLocation(std::ostream &s, size_t i) const
{
    s << "\"register_index\": " << registers[i]
      << ", \"byte\": " << byteIndices[i] << ", \"mask\": " << byteMasks[i];
}

bool
RegisterFaultInjector::armActivationWatch(size_t i)
{
    if (pcRegisterIndex >= 0 &&
        registers[i] == static_cast<uint32_t>(pcRegisterIndex)) {
        ResolvedSite &r = resolvedSites[i];

        r.activated = true;
        r.activationKind = "pc_fetch";
        r.activationTick = curTick();
        r.hasActivationPc = true;
        r.activationPc = lastPointOriginalPc;
        r.hasActivationVaddr = true;
        r.activationVaddr = lastPointCorruptedPc;

        Counter count = 0;
        if (currentInstCount(count)) {
            r.hasActivationInst = true;
            r.activationInst = count;
        }
        Addr fp = 0;
        Addr sp = 0;
        if (currentFrameRegisters(fp, sp)) {
            r.hasActivationFrame = true;
            r.activationFp = fp;
            r.activationSp = sp;
        }

        journalActivated(i);

        r.reads = 1;
        r.live = false;
        r.endReason = "consumed_by_fetch";
        r.hasEndTick = true;
        r.endTick = curTick();
        journalWatchEnded(i);
        return true;
    }

    ThreadContext *tc = targetCpu->getContext(0);
    auto *thread = dynamic_cast<SimpleThread *>(tc);
    if (!thread) {
        return false;
    }
    const auto &rcs = tc->getIsaPtr()->regClasses();
    const RegClass &intRC = *rcs.at(IntRegClass);
    RegId flat = intRC[registers[i]].flatten(*tc->getIsaPtr());
    thread->watchReg(flat, [this, i](bool isWrite) {
        noteRegisterAccess(i, isWrite);
    });
    return true;
}

void
RegisterFaultInjector::suspendTargetWatches(bool s)
{
    if (!targetCpu || targetCpu->numContexts() == 0) {
        return;
    }
    ThreadContext *tc = targetCpu->getContext(0);
    auto *thread = dynamic_cast<SimpleThread *>(tc);
    if (thread) {
        thread->suspendRegWatches(s);
    }
}
} // namespace gem5
