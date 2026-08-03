#ifndef __FAULT_INJECTOR_HH__
#define __FAULT_INJECTOR_HH__

#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "base/types.hh"
#include "params/BaseFaultInjector.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"

namespace gem5
{
class BaseCPU;
class ThreadContext;
class Process;
class EmulationPageTable;
class Packet;

class BaseFaultInjector : public SimObject
{
  public:
    BaseFaultInjector(const BaseFaultInjectorParams &p);
    void startup() override;
    void rebindCpu(BaseCPU *newCpu);

  protected:
    struct AppliedRecord
    {
        size_t index;
        Tick tick;
        bool landed;
    };

    struct ResolvedSite
    {
        bool landed = false;
        std::string kind;
        bool hasPaddr = false;
        Addr paddr = 0;
        bool hasVaddr = false;
        Addr vaddr = 0;
        const char *vaddrReason = nullptr;

        Tick injectTick = 0;
        bool hasInjectPc = false;
        Addr injectPc = 0;
        std::string injectSymbol;
        std::string injectDisasm;
        bool hasInjectInst = false;
        Counter injectInst = 0;

        bool watched = false;
        bool live = false;
        bool activated = false;
        Tick activationTick = 0;
        bool hasActivationPc = false;
        Addr activationPc = 0;
        std::string activationSymbol;
        std::string activationDisasm;
        bool hasActivationInst = false;
        Counter activationInst = 0;
        bool hasActivationVaddr = false;
        Addr activationVaddr = 0;
        unsigned activationSize = 0;
        uint64_t reads = 0;
        uint64_t writes = 0;
        const char *endReason = nullptr;
        bool hasEndTick = false;
        Tick endTick = 0;
    };

    std::vector<Tick> injectionSchedule;
    std::map<Tick, std::vector<size_t>> tickToPoints;
    std::vector<Tick> distinctTicks;
    size_t currentTickIndex = 0;

    std::vector<Counter> instSchedule;
    std::map<Counter, std::vector<size_t>> instToPoints;
    std::vector<Counter> distinctInsts;
    size_t currentInstIndex = 0;
    bool instMode = false;
    BaseCPU *cpu = nullptr;
    ThreadContext *armedTc = nullptr;

    int flipsApplied = 0;
    std::string resultFile;
    std::string domainName;
    std::string targetComponent;
    std::string selectionMode;
    std::vector<AppliedRecord> appliedLog;

    static const unsigned maxDecodeChunks = 8;

    bool resolveFaults;
    bool trackActivation;
    std::vector<ResolvedSite> resolvedSites;

    virtual void
    selectLocations(Tick t, std::vector<size_t> &pts)
    {}
    virtual bool applyFault(size_t pointIndex, ResolvedSite &site) = 0;
    virtual void writePointLocation(std::ostream &s, size_t i) const = 0;
    virtual void
    afterTick(Tick t)
    {}
    virtual bool
    armActivationWatch(size_t pointIndex)
    { return false; }

    bool resolveVirtualAddress(Addr paddr, Addr &vaddr) const;
    void writeResolvedSite(std::ostream &s, size_t i) const;

    void captureInjectionContext(ResolvedSite &site) const;
    void noteSiteAccess(size_t pointIndex, Packet *pkt);
    void noteSiteEviction(size_t pointIndex, bool dataPreserved);

    bool currentPc(Addr &pc) const;
    bool currentInstCount(Counter &count) const;
    bool disassembleAt(Addr pc, std::string &text) const;
    static bool lookupSymbol(Addr addr, std::string &symbol);
    static std::string jsonEscape(const std::string &raw);

  private:
    void processEvent();
    void processInstEvent();
    void armInstEvent(Counter target);
    void applyPointsAt(const std::vector<size_t> &pts, Tick logTick);
    void writeInjectionContext(std::ostream &s, size_t i) const;
    void writeActivation(std::ostream &s, size_t i) const;
    void writeResult();
    bool resultWritten = false;
    EventFunctionWrapper injectEvent;
    EventFunctionWrapper injectInstEvent;
};
} // namespace gem5

#endif // __FAULT_INJECTOR_HH__
