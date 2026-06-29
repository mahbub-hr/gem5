#include "fault_injector/register_fault_injector.hh"

#include "arch/generic/isa.hh"
#include "cpu/reg_class.hh"
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
      byteMasks(p.target_byte_masks)
{
    if (!targetCpu) {
        fatal("RegisterFaultInjector: target_object is not a BaseCPU!");
    }
}

bool
RegisterFaultInjector::applyFault(size_t i)
{
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
} // namespace gem5
