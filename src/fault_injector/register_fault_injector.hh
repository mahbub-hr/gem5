#ifndef __REGISTER_FAULT_INJECTOR_HH__
#define __REGISTER_FAULT_INJECTOR_HH__

#include <cstdint>
#include <ostream>
#include <vector>

#include "cpu/base.hh"
#include "fault_injector/fault_injector.hh"
#include "params/RegisterFaultInjector.hh"

namespace gem5
{
class RegisterFaultInjector : public BaseFaultInjector
{
  public:
    RegisterFaultInjector(const RegisterFaultInjectorParams &p);

  protected:
    bool applyFault(size_t i) override;
    void writePointLocation(std::ostream &s, size_t i) const override;

  private:
    BaseCPU *targetCpu;
    std::vector<uint32_t> registers;
    std::vector<uint32_t> byteIndices;
    std::vector<uint32_t> byteMasks;
};
} // namespace gem5

#endif // __REGISTER_FAULT_INJECTOR_HH__
