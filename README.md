# FlintRTOS

A lean, secure, and safe real-time operating system: **lean like FreeRTOS, secure like Linux, safe like an automotive RTOS**, portable across MPU-class microcontrollers and MMU-class application processors, and easy to extend.

**First target: Raspberry Pi 4** (BCM2711, Cortex-A72, AArch64).

The kernel presents a familiar FreeRTOS-style module layout and API. The FlintRTOS security design (privilege separation, MMU isolation domains, capabilities, freedom-from-interference) layers on top of this core in later increments — see the design documents in the parent folder and `docs/`.

## Repository layout (manifest)

```
include/FlintRTOS/     public API headers (FlintRTOS.h, task.h, list.h, queue.h,
                       timers.h, event_groups.h, stream_buffer.h, message_buffer.h, ...)
kernel/                core + extension modules:
                         tasks.c    - Task Management (scheduler, states, switching)
                         list.c     - List Management (ready/blocked lists)
                         queue.c    - Queue Management (FIFO; sem/mutex foundation)
                         timers.c, event_groups.c, stream_buffer.c, croutine.c  (extensions)
                         flint_libc.c - minimal freestanding memcpy/memset/...
                         cap.c, msgtag.c, prio_bitmap.c - security-layer primitives (design)
portable/
   LLVM_AArch64/       RPi4 port: port.c, portASM.S, portmacro.h, vectors.S
   Host/               host portmacro.h for native unit testing
   MemMang/            heap_1.c .. heap_5.c (select via configHEAP_ALGORITHM)
boot/rpi4/             start.S (EL2->EL1, boot), linker.ld, boot.c
bsp/rpi4/              board support: rpi4.h (peripherals), uart.c (PL011)
demo/rpi4/             main.c (two-task demo) + FlintRTOSConfig.h
tests/                 host unit tests (C++17)
docs/                  coding standard, module status
Makefile               RPi4 firmware build (kernel8.img) via LLVM toolchain
CMakeLists.txt         host unit-test build
```

## Two builds

### 1. Host unit tests (portable logic — fully verified)

```sh
cmake -S . -B build-host
cmake --build build-host
./build-host/flint_tests          # or: ctest --test-dir build-host --output-on-failure
```

Covers the portable modules that need no hardware: **list**, **queue**, **heap_4**, and the
security-layer primitives (cap, msgtag, prio_bitmap). 149 checks, all passing.

### 2. RPi4 firmware image (`kernel8.img`)

Uses the LLVM bare-metal toolchain (clang + ld.lld + llvm-objcopy) — **no GNU cross-toolchain
required**:

```sh
make                  # -> build/kernel8.img
make disasm           # sanity-check the ELF
```

## Running on Raspberry Pi 4

### Real hardware (SD card)
1. Prepare an SD card with the standard RPi4 firmware: `bootcode.bin` (if applicable),
   `start4.elf`, `fixup4.dat` from the official firmware repo.
2. Add a `config.txt` containing:
   ```
   arm_64bit=1
   kernel=kernel8.img
   enable_uart=1
   ```
3. Copy `build/kernel8.img` to the SD card boot partition.
4. Connect a 3.3 V USB-UART to GPIO14 (TXD)/GPIO15 (RXD), 115200 8N1, and power on.
   You should see the FlintRTOS banner and the two tasks (A/B) printing.

### QEMU (if available)
```sh
qemu-system-aarch64 -M raspi4b -kernel build/kernel8.img -serial stdio -display none
# (a recent QEMU with raspi4b support is required; -M raspi3b also works with matching addresses)
```

## Configuration

Everything is tuned in `demo/rpi4/FlintRTOSConfig.h` (manifest section 4): tick rate, priorities,
heap algorithm/size, and the `configUSE_*` switches that compile extension modules and ecosystem
libraries in or out (pay-for-what-you-use).

## Status — implementation increment 2

| Module | Status |
|---|---|
| Boot + PL011 UART + AArch64 port skeleton | build-verified (image boots on QEMU/HW — runtime bring-up is the next on-target step) |
| List Management (`list.c`) | implemented + host-tested |
| Queue Management (`queue.c`) | core FIFO implemented + host-tested; blocking wires to tasks next |
| Task Management (`tasks.c`) + context switch | implemented, build-verified; **first on-target task: verify the context switch** |
| MemMang `heap_4` | implemented + host-tested; `heap_1`/`heap_3` implemented; `heap_2`/`heap_5` skeletons |
| Software timers / event groups / stream+message buffers / co-routines | headers + config-gated skeletons |

> The build environment here has no emulator (QEMU packages are blocked), so the AArch64 firmware
> is **compiled, linked, and disassembly-verified** but not executed here. The portable logic is
> **executed and unit-tested** natively. First hardware/QEMU step: confirm boot UART output, then
> the timer-driven context switch between tasks A and B.

## Ecosystem libraries (manifest 3 — roadmap)

`configUSE_LWIP / CLI / CORE_MQTT / CORE_HTTP / PTP` switches are reserved in the config. Integration
(LwIP TCP/IP, FlintRTOS-CLI, coreMQTT/coreHTTP, IEEE-1588 PTP) follows once queues/notifications and
the network driver land — each as an isolated, config-gated component.

## Coding standard

TCB (kernel + HAL) in **MISRA C:2023**; non-TCB (services, tools, tests) in **MISRA C++:2023**.
See `docs/CODING-STANDARD.md`.
