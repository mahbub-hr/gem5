#!/bin/bash
export GEM5="/work/host/gem5"

${GEM5}/build/X86/gem5.opt \
    --debug-flags=Faults,Cache \
    --debug-file=fi_trace.out \
    ${GEM5}/configs/fault_injector/fi_cfg.py
