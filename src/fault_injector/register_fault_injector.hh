#ifndef __REGISTER_FAULT_INJECTOR_HH__
#define __REGISTER_FAULT_INJECTOR_HH__

#include <cstdint>
#include <ostream>
#include <vector>

#include "base/types.hh"
#include "cpu/base.hh"
#include "fault_injector/fault_injector.hh"
#include "params/RegisterFaultInjector.hh"

namespace gem5
{
class RegisterFaultInjector : public BaseFaultInjector
{
  public:
    RegisterFaultInjector(const RegisterFaultInjectorParams &p);
    void rebindCpu(BaseCPU *newCpu) override;

  protected:
    bool applyFault(size_t i, ResolvedSite &site) override;
    void writePointLocation(std::ostream &s, size_t i) const override;
    bool armActivationWatch(size_t pointIndex) override;
    void suspendTargetWatches(bool suspend) override;

  private:
    BaseCPU *targetCpu;
    std::vector<uint32_t> registers;
    std::vector<uint32_t> byteIndices;
    std::vector<uint32_t> byteMasks;
    int pcRegisterIndex;
    bool pcFlipOriginalValid = false;
    Tick pcFlipTick = 0;
    Addr pcFlipOriginal = 0;
    bool lastPointWasPc = false;
    Addr lastPointOriginalPc = 0;
    Addr lastPointCorruptedPc = 0;
};
} // namespace gem5

#endif // __REGISTER_FAULT_INJECTOR_HH__
