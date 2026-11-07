#!/bin/sh
# FlintRTOS - MISRA check of the TCB (kernel, port, BSP) with a ratchet.
#
#   tools/misra/run_misra.sh            check against tools/misra/baseline.txt
#   tools/misra/run_misra.sh --update   accept the current counts as the baseline
#
# Uses cppcheck's MISRA addon (MISRA C:2012 incl. amendments - the closest
# open checker to MISRA C:2023; rules carried over keep their numbers).
# The project has known, documented deviations (docs/misra-deviations.md), so
# the gate is a ratchet: the build fails if any rule's violation count goes UP
# or a new rule appears; when counts go down, run --update to lock that in.
# Rule texts are copyrighted: put your licensed copy at tools/misra/rules.txt
# (see cppcheck's misra.py --rule-texts) to get full messages; without it the
# report still names each rule.
set -eu
cd "$(dirname "$0")/../.."

BASELINE=tools/misra/baseline.txt
OUT=${MISRA_OUT:-build/misra}
mkdir -p "$OUT"

TCB_FILES="
kernel/tasks.c kernel/queue.c kernel/list.c kernel/flint_libc.c
kernel/flint_safety.c kernel/flint_timing.c
kernel/cap.c kernel/msgtag.c kernel/prio_bitmap.c
portable/LLVM_AArch64/port.c portable/LLVM_AArch64/safety_port.c
portable/MemMang/heap_4.c
bsp/rpi4/uart.c bsp/rpi4/mailbox.c bsp/rpi4/pm_watchdog.c bsp/rpi4/genet.c
"

RULES_ARG=""
if [ -f tools/misra/rules.txt ]; then
    printf '{"script":"misra.py","args":["--rule-texts=%s"]}\n' "$(pwd)/tools/misra/rules.txt" > "$OUT/misra.json"
    RULES_ARG="--addon=$OUT/misra.json"
else
    RULES_ARG="--addon=misra"
fi

# shellcheck disable=SC2086
cppcheck --enable=style $RULES_ARG --addon-python="$(command -v python3)" \
    --std=c11 --platform=unix64 -q --max-configs=1 \
    -DconfigUSE_LWIP=1 -DFLINT_SCHED_TRACE=0 \
    -Iinclude/FlintRTOS -Iinclude -Ibsp/rpi4 -Iportable/LLVM_AArch64 -Idemo/rpi4 \
    --template='{file}:{line}: {id}: {message}' \
    $TCB_FILES 2> "$OUT/findings.txt" || true

grep -o 'misra-c2012-[0-9.]*' "$OUT/findings.txt" | sort | uniq -c \
    | awk '{print $2, $1}' | sort -V > "$OUT/counts.txt" || true
total=$(awk '{s+=$2} END {print s+0}' "$OUT/counts.txt")

if [ "${1:-}" = "--update" ]; then
    cp "$OUT/counts.txt" "$BASELINE"
    echo "MISRA baseline updated: $total findings across $(wc -l < "$BASELINE") rules"
    exit 0
fi

if [ ! -f "$BASELINE" ]; then
    echo "no $BASELINE - run with --update first" >&2
    exit 2
fi

status=0
improved=0
# Rules whose count rose, or that are new.
while read -r rule n; do
    base=$(awk -v r="$rule" '$1==r {print $2}' "$BASELINE")
    base=${base:-0}
    if [ "$n" -gt "$base" ]; then
        echo "MISRA REGRESSION: $rule  $base -> $n"
        grep -- "$rule" "$OUT/findings.txt" | sed 's/^/    /' | head -20
        status=1
    elif [ "$n" -lt "$base" ]; then
        improved=1
    fi
done < "$OUT/counts.txt"
# Rules that disappeared entirely count as improvements.
while read -r rule base; do
    grep -q "^$rule " "$OUT/counts.txt" || improved=1
done < "$BASELINE"

btotal=$(awk '{s+=$2} END {print s+0}' "$BASELINE")
echo "MISRA: $total findings (baseline $btotal). Full list: $OUT/findings.txt"
if [ $status -eq 0 ] && [ $improved -eq 1 ]; then
    echo "MISRA: fewer findings than the baseline - run tools/misra/run_misra.sh --update to lock it in"
fi
exit $status
