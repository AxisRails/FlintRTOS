#!/bin/sh
# FlintRTOS - local CI: the same gates as .github/workflows/ci.yml.
#
#   tools/ci.sh            firmware builds + host tests + MISRA ratchet
#   tools/ci.sh --quick    skip the MQTT end-to-end tests (no broker needed)
#
# Needs: clang, ld.lld, llvm-objcopy, cmake, a host C/C++ compiler, cppcheck,
# python3; mosquitto + mosquitto-clients for the MQTT tests. Works on Linux
# or WSL. The TAP-based lwIP MQTT test runs only as root (it creates a
# network interface); otherwise it is skipped.
set -u
cd "$(dirname "$0")/.."
QUICK=0
[ "${1:-}" = "--quick" ] && QUICK=1
fail=0
step() { printf '\n==== %s ====\n' "$1"; }
ok()   { echo "PASS: $1"; }
bad()  { echo "FAIL: $1"; fail=1; }

# 1. Firmware, all three configurations, zero warnings allowed.
for cfg in "LWIP=0" "LWIP=1" "LWIP_OS=1"; do
    step "firmware make $cfg"
    rm -rf build
    if make $cfg > build.log 2>&1; then
        w=$(grep -c 'warning:' build.log || true)
        if [ "$w" -eq 0 ]; then ok "make $cfg ($(tail -1 build.log))"; else bad "make $cfg: $w warning(s)"; grep 'warning:' build.log | head; fi
    else
        bad "make $cfg"; tail -20 build.log
    fi
done
rm -f build.log
# Leave the default image built.
rm -rf build && make LWIP=1 > /dev/null 2>&1 || true

# 2. Host tests (kernel units, safety, PTP simulation, MQTT end-to-end).
step "host tests (ctest)"
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release > /dev/null && cmake --build build-host -j > /dev/null || bad "host build"
EXCLUDE=""
if [ $QUICK -eq 1 ]; then
    EXCLUDE="mqtt_e2e"
elif [ "$(id -u)" -ne 0 ]; then
    EXCLUDE="mqtt_e2e_lwip_tap"
    echo "(not root: skipping mqtt_e2e_lwip_tap)"
fi
if [ -n "$EXCLUDE" ]; then
    (cd build-host && ctest --output-on-failure -E "$EXCLUDE") && ok "ctest" || bad "ctest"
else
    (cd build-host && ctest --output-on-failure) && ok "ctest" || bad "ctest"
fi

# 3. MISRA ratchet on the TCB.
step "MISRA (cppcheck addon, ratchet vs tools/misra/baseline.txt)"
tools/misra/run_misra.sh && ok "MISRA" || bad "MISRA regression"

echo
if [ $fail -eq 0 ]; then echo "CI: ALL GATES PASSED"; else echo "CI: FAILED"; fi
exit $fail
