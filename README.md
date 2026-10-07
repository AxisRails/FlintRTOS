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
| Boot + UART console + AArch64 port | **runs on real Raspberry Pi 4 hardware** ✅ (EL2→EL1, UART, GIC-400, generic timer) |
| List Management (`list.c`) | implemented + host-tested + running on HW |
| Queue Management (`queue.c`) + mutex/semaphores | **validated on HW** ✅ — queue-backed mutex enforces correct mutual exclusion on a shared counter across three interleaved tasks (no lost updates) |
| Task Management (`tasks.c`) + context switch | **validated on HW** ✅ — preemptive priority scheduling (3 levels, higher-priority task preempts on wake), `vTaskDelay` blocking, delayed-list wakeup, and context switch all confirmed with correct timing |
| Stack-overflow detection (`configCHECK_FOR_STACK_OVERFLOW`) | **validated on HW** ✅ — each stack painted and its guard checked on every context switch |
| MemMang `heap_4` | implemented + host-tested + running on HW |
| Software timers / event groups / stream+message buffers / co-routines | headers + config-gated skeletons |
| lwIP 2.2.0 (TCP/IP, IPv4+IPv6, NO_SYS) | vendored + FlintRTOS port; compiles & links into firmware; GENET MAC driver validation next |

> **Hardware bring-up complete (minimal kernel).** The `make LWIP=0` image boots on a real
> Raspberry Pi 4 Model B and runs the two-task preemptive demo: task A every 500 ms and task B
> every 1000 ms, driven by the 1 kHz generic-timer tick, with a verified context switch. The build
> environment here still has no emulator (QEMU packages are blocked), so images are compiled and
> linked here and flashed to an SD card for on-target validation.
>
> Bring-up notes captured on real silicon: (1) the GPU firmware routes the **mini-UART** (UART1),
> not the PL011, to the GPIO14/15 header pins when no device-tree `disable-bt` overlay is present,
> so the console driver writes to both UARTs; (2) `config.txt` must **not** carry a `dtoverlay`
> line unless the overlay files are on the card, or `start4.elf` halts before loading the kernel;
> (3) interrupts must stay masked from timer setup until the first task's `eret`, or an early tick
> corrupts the first task's saved stack pointer. See `sdcard/README-BRINGUP.md`.

## Networking — lwIP (manifest 3)

**lwIP 2.2.0** (BSD-3-Clause) is vendored under `third_party/lwip/` (pristine upstream `src/`) with
the FlintRTOS port under `port/lwip/`. It builds into the firmware by default:

```sh
make            # LWIP=1: kernel8.img with the TCP/IP stack (~130 KB)
make LWIP=0     # stack compiled out (~9 KB) - pay-for-what-you-use
```

Integration status:

- **Mode:** `NO_SYS=1` (raw/callback API), **IPv4 + IPv6**, UDP + TCP, ARP/Ethernet, ICMP, DHCP.
  This mode needs no OS mutexes/mailboxes, so it runs on the current FlintRTOS core.
- **Port** (`port/lwip/`): `arch/cc.h` (types, endianness, diag→UART, PRNG), `lwipopts.h`,
  `sys_arch.c` (`sys_now` from the tick, lightweight IRQ protection), `lwip_libc.c`
  (freestanding libc: `mem*/str*/snprintf/...`), and freestanding header shims
  (`string.h`, `stdlib.h`, `stdio.h`, `inttypes.h`, `ctype.h`, `errno.h`).
- **Demo** (`demo/rpi4/net_demo.c`, gated by `configUSE_LWIP`): a network task brings lwIP up at
  `192.168.1.50`, opens a **UDP echo server on port 7**, and pumps the stack (poll RX +
  `sys_check_timeouts`).
- **Verified:** the whole stack **compiles and links** into `kernel8.img` for AArch64 (all 39 lwIP
  core files + the port). Not executed here (no emulator in the build env).
### GENET Ethernet driver (BCM2711)

`bsp/rpi4/genet.c` implements the RPi4 GENET v5 MAC: MDIO/PHY, MAC setup, and TX/RX via the DMA
rings (default queue). It's wired into the netif (`flint_low_level_output` → `genet_send`,
`flint_netif_poll` → `genet_recv`). **Build-verified; hardware bring-up pending** — validate MDIO PHY
access and the RX/TX descriptor indices on a real Pi (register map follows the published GENET v5
layout; runs with MMU/caches off so DMA is coherent).

### Blocking primitives (semaphores/mutex)

`tasks.c` now has event-list blocking (`vTaskPlaceOnEventList` / `xTaskRemoveFromEventList`), and
`queue.c` blocks with timeouts. `semphr.h` adds binary/counting semaphores and a mutex over the queue.
Host-tested (167 checks).

### OS mode + BSD sockets + coreMQTT

With `make LWIP_OS=1` the stack switches to `NO_SYS=0`: lwIP's `sys_arch` (`port/lwip/sys_arch.c`) maps
semaphores/mailboxes/threads onto FlintRTOS, enabling **netconn + BSD sockets**, and **coreMQTT
v2.3.1** (vendored under `third_party/coreMQTT/`) runs over a lwIP-sockets transport
(`port/mqtt/transport_lwip.c`). `demo/rpi4/net_demo_os.c` brings up the tcpip thread, DHCP, and an
MQTT publish to `flint/status`. The full chain **compiles and links** into a 193 KB image.

### PTP / IEEE 1588 (time sync)

FlintRTOS runs an **IEEE 1588-2008 ordinary-clock slave** (`port/ptp/`) in the
default NO_SYS networking image. It keeps ptpd's OS-independent data model
(`third_party/ptpd/`: wire structures, constants, time arithmetic) and adds:

| Piece | File | What it does |
|---|---|---|
| Protocol engine | `ptp_core.c` | E2E Sync/Follow_Up/Delay_Req/Delay_Resp, correctionField, BMC dataset comparison over Announce with receipt timeout, median delay filter, step-epoch guard, UTC display |
| Servo | `ptp_servo.c` | integer PI (linuxptp-style UNLOCKED → STEPPED → LOCKED), frequency estimate from two samples, outlier rejection |
| Clock | `ptp_clock.c` | piecewise-linear clock on CNTPCT (54 MHz, 18.5 ns) with phase step + ppb frequency adjust, no floating point |
| NO_SYS transport | `ptp_lwip_raw.c` | raw UDP 319/320, IGMP join 224.0.1.129, **driver timestamps** |
| OS transport | `ptp_slave.c` | same engine over BSD sockets (socket-level timestamps) |

**Timestamping.** The Pi 4B's GENET MAC and BCM54213PE PHY have no IEEE 1588
unit, so timestamps are software, taken as close to the wire as possible: RX in
the GENET `RXDMA_MBDONE` interrupt (GIC SPI 157 → INTID 189), TX at the DMA
doorbell. The GIC is split into two priority bands: the scheduler tick sits in
the *kernel* band, which the console lock masks via `GICC_PMR`; the GENET
timestamp ISR sits in a higher *timestamp* band that nothing but short DAIF
critical sections can delay, so printing never skews a timestamp.

**Verified:** `tests/ptp_sim.c` closes the loop between the real engine/servo/
clock and a simulated two-step ptp4l master (oscillator error up to 450 ppm,
8–20 µs timestamp jitter). It locks within 3 Syncs and holds the true offset
to under 5 µs worst-case at 8 µs jitter (`ctest`: `ptp_sim_*`). Hardware run
against ptp4l: see `sdcard/README-BRINGUP.md`.

### MQTT device client (coreMQTT over lwIP raw TCP)

The default image runs a **coreMQTT v2.3.1** client inside the same NO_SYS
network task as DHCP and PTP, connecting to `configMQTT_BROKER_HOST`
(default `test.mosquitto.org:1883`, set in `demo/rpi4/FlintRTOSConfig.h`).
`<id>` is `flint-` + the board serial read from the VideoCore mailbox, which
also supplies the factory MAC address for the netif.

| Topic | When | Payload |
|---|---|---|
| `flint/<id>/status` | on connect, retained | `{"state":"online","ip":...,"uptime_s":...}`; Last Will `{"state":"offline"}` |
| `flint/<id>/ptp` | 1 Hz | `{"state":"LOCKED","offset_ns":...,"delay_ns":...,"freq_ppb":...,"utc":...}` |
| `flint/<id>/stats` | 10 s | uptime, heap free/min, GENET IRQ count, per-task priority and stack high-water |
| `flint/<id>/cmd` | subscribed | `ping`, `uptime`, `stats`, `ptp`, `led on|off|toggle` (green ACT LED), `help` |
| `flint/<id>/reply` | per command | JSON answer |

| Piece | File |
|---|---|
| Application (topics, commands, reconnect with back-off 2 s → 60 s) | `port/mqtt/flint_mqtt.c` |
| Raw-TCP transport: DNS, connect, RX ring with lwIP flow control, send back-pressure | `port/mqtt/transport_raw.c` |
| Transport / platform seams (same app runs on host and Pi) | `port/mqtt/mqtt_transport.h`, `mqtt_platform.h` |
| Pi platform: LED (GPIO42), PTP + RTOS JSON, network pump | `demo/rpi4/mqtt_platform.c` |
| Bounded JSON writer | `port/mqtt/jsonw.c` |
| Task snapshot API (stack high-water from the painted stack) | `uxTaskGetSnapshot()` in `kernel/tasks.c` |
| Board MAC + serial | `bsp/rpi4/mailbox.c` |

NO_SYS lwIP only moves when its owner polls it, while coreMQTT calls block
(waiting for CONNACK, a full send window). Whenever the transport waits it
calls `fm_plat_pump()` → `net_pump()`: one iteration of the network loop, so
DHCP, ARP, TCP, PTP and the UDP echo keep running underneath a blocking MQTT
call.

**Verified (`ctest`):** `mqtt_e2e_posix` runs the app over POSIX sockets and
`mqtt_e2e_lwip_tap` runs the app + `transport_raw.c` + coreMQTT + lwIP NO_SYS
over a Linux TAP interface. Both run against a real mosquitto broker and check
the retained status, telemetry, all 9 command replies and the Last Will on
connection loss. Reconnect after a broker restart was also checked.

> The public test broker has no authentication: anyone who knows the topic
> can read it and send commands (the worst a command can do is toggle the
> LED). TLS + credentials is the next step (mbedTLS under `transport_raw.c`).

Build matrix (all verified to link, zero warnings):

| Build | Image | Contents |
|---|---|---|
| `make LWIP=0` | ~18 KB | kernel + scheduler + priority-inversion demo |
| `make` | ~195 KB | + lwIP NO_SYS + GENET (DHCP, UDP echo) + PTP slave (driver timestamps) + MQTT device client |
| `make LWIP_OS=1` | ~223 KB | + netconn/sockets + coreMQTT + PTP slave (socket timestamps) |

**Hardware status:** GENET + lwIP DHCP validated on a Pi 4B (1000BASE-T).
**Next:** PTP and MQTT validation on hardware; TLS (mbedTLS) under the MQTT
transport; OS-mode bring-up.

## Other ecosystem libraries (manifest 3 — roadmap)

`configUSE_CLI / CORE_MQTT / CORE_HTTP / PTP` switches are reserved. FlintRTOS-CLI (over UART),
coreMQTT/coreHTTP (over lwIP + TLS), and IEEE-1588 PTP follow the same vendor-and-port pattern once
the GENET driver and OS-mode sockets are in place.

## Coding standard

TCB (kernel + HAL) in **MISRA C:2023**; non-TCB (services, tools, tests) in **MISRA C++:2023**.
See `docs/CODING-STANDARD.md`.
