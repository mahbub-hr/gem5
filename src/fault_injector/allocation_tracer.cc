#include "fault_injector/allocation_tracer.hh"

#include <sstream>

#include "base/loader/object_file.hh"
#include "base/logging.hh"
#include "base/output.hh"
#include "cpu/base.hh"
#include "cpu/reg_class.hh"
#include "cpu/thread_context.hh"
#include "sim/process.hh"
#include "sim/sim_exit.hh"

namespace gem5
{
namespace
{
constexpr RegIndex arm64FlatArg0Idx = 0;
constexpr RegIndex arm64FlatArg1Idx = 1;
constexpr RegIndex arm64FlatLinkIdx = 33;

std::string
hex(Addr value)
{
    std::ostringstream ss;
    ss << "\"0x" << std::hex << value << "\"";
    return ss.str();
}

std::string
quoted(const std::string &raw)
{
    std::string out = "\"";
    for (char c : raw) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    out += '"';
    return out;
}
} // namespace

AllocationTracer::AllocationTracer(const AllocationTracerParams &p)
    : ProbeListenerObject(p),
      cpu(p.cpu),
      traceFile(p.trace_file),
      functionNames(p.functions),
      prevX0(0),
      prevX1(0),
      prevX30(0)
{
    if (!cpu) {
        fatal("AllocationTracer: the 'cpu' param is required.\n");
    }
}

void
AllocationTracer::startup()
{
    ThreadContext *tc = cpu->getContext(0);
    Process *p = tc->getProcessPtr();
    if (!p || !p->objFile) {
        fatal("AllocationTracer: no process object file.\n");
    }
    if (p->objFile->getArch() != loader::Arm64) {
        fatal("AllocationTracer: only AArch64 is supported.\n");
    }
    for (size_t i = 0; i < functionNames.size(); i++) {
        auto it = p->objFile->symtab().find(functionNames[i]);
        if (it == p->objFile->symtab().end()) {
            fatal("AllocationTracer: symbol '%s' not found.\n",
                  functionNames[i]);
        }
        entryToFunction[it->address()] = i;
    }
    trace.open(simout.resolve(traceFile), std::ios::out | std::ios::trunc);
    if (!trace.is_open()) {
        fatal("AllocationTracer: cannot open '%s'.\n", traceFile);
    }
    listening = true;
    registerExitCallback([this] { writeEnd(); });
}

void
AllocationTracer::regProbeListeners()
{
    typedef ProbeListenerArg<AllocationTracer, Addr> AllocationListener;
    connectListener<AllocationListener>(this, "RetiredInstsPC",
                                        &AllocationTracer::checkPc);
}

void
AllocationTracer::checkPc(const Addr &pc)
{
    if (!listening) {
        return;
    }
    ThreadContext *tc = cpu->getContext(0);
    BaseISA *isa = tc->getIsaPtr();
    const RegClass *intRC = isa->regClasses().at(IntRegClass);
    auto entry = entryToFunction.find(pc);
    if (entry != entryToFunction.end()) {
        PendingCall call;
        call.function = entry->second;
        call.arg0 = prevX0;
        call.arg1 = prevX1;
        call.lr = prevX30;
        call.entryInst = tc->getCurrentInstCount();
        stack.push_back(call);
    } else if (!stack.empty() && pc == stack.back().lr) {
        PendingCall call = stack.back();
        stack.pop_back();
        writeEvent(call, prevX0, tc->getCurrentInstCount());
    }
    prevX0 = tc->getReg((*intRC)[arm64FlatArg0Idx]);
    prevX1 = tc->getReg((*intRC)[arm64FlatArg1Idx]);
    prevX30 = tc->getReg((*intRC)[arm64FlatLinkIdx]);
}

void
AllocationTracer::writeEvent(const PendingCall &call, Addr ret,
                             uint64_t returnInst)
{
    const std::string &name = functionNames[call.function];
    std::ostringstream s;
    s << "{\"seq\": " << sequence++ << ", \"fn\": " << quoted(name)
      << ", \"caller_pc\": " << hex(call.lr) << ", \"args\": ["
      << hex(call.arg0);
    if (name == "calloc" || name == "realloc") {
        s << ", " << hex(call.arg1);
    }
    s << "], \"ret\": ";
    if (name == "free") {
        s << "null";
    } else {
        s << hex(ret);
    }
    s << ", \"inst_count_entry\": " << call.entryInst
      << ", \"inst_count_return\": " << returnInst << "}";
    trace << s.str() << '\n';
    trace.flush();
}

void
AllocationTracer::writeEnd()
{
    if (!trace.is_open()) {
        return;
    }
    ThreadContext *tc = cpu->getContext(0);
    trace << "{\"end\": true, \"sim_insts\": " << tc->getCurrentInstCount()
          << "}\n";
    trace.flush();
    trace.close();
}

} // namespace gem5
