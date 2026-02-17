#!/bin/bash
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <config file relative to script path.>"
    exit 1
fi

export GEM5="/work/host/gem5"

${GEM5}/build/X86/gem5.opt \
    --debug-flags=FI,Cache,Exec \
    --debug-file=fi_trace.out \
    ${GEM5}/configs/fault_injector/${1}
