# FlintRTOS — Raspberry Pi 4 first boot (minimal `LWIP=0` image)

This folder is the SD-card kit for the **first on-hardware bring-up**. It boots the
minimal two-task demo (no networking) so a boot problem is debuggable in isolation.
Once you see tasks A and B ticking, we climb to the networking images.

## What's in this folder

| File | Role | Source |
|---|---|---|
| `kernel8.img` | FlintRTOS minimal firmware (8,899 bytes, `make LWIP=0`) | built here ✅ |
| `config.txt` | Pi boot configuration (64-bit, UART on GPIO14/15) | built here ✅ |
| `start4.elf`, `fixup4.dat` | Pi GPU boot firmware | **you supply** (see below) |

`start4.elf` / `fixup4.dat` are Raspberry Pi's proprietary GPU firmware. I can't
download binary blobs into your folder from here, so you provide them — it's a
30-second step and you very likely already have them.

## Step 1 — get the two firmware blobs (`start4.elf`, `fixup4.dat`)

**Easiest:** if this SD card ever ran Raspberry Pi OS, both files are *already* on
its FAT32 boot partition. Just leave them there and only copy in `kernel8.img` and
`config.txt` (Step 2). Done.

**Otherwise**, download them once from the official firmware repo:
<https://github.com/raspberrypi/firmware/tree/master/boot>
Click `start4.elf` → **Download raw file**; same for `fixup4.dat`. (These two are
all a Pi 4 needs — `bootcode.bin` is only for Pi 3 and earlier.) Drop them next to
`kernel8.img`.

## Step 2 — write the SD card (FAT32 boot partition)

Copy onto the boot partition (overwrite `config.txt`/`kernel8.img` if present):

```
config.txt
kernel8.img
start4.elf      <- from Step 1
fixup4.dat      <- from Step 1
```

If the card already has a Raspberry Pi OS boot partition, the simplest thing is to
**delete `kernel.img`/`kernel8.img` and `cmdline.txt`**, drop in our `kernel8.img`,
and replace `config.txt` with ours. FlintRTOS doesn't read `cmdline.txt`.

## Step 3 — wire the serial console (required)

FlintRTOS drives **no HDMI** — all output is on the PL011 UART. Use a **3.3 V
USB-to-serial adapter** (FTDI / CP2102 / CH340). **Do not use 5 V or the Pi's HDMI.**

| USB-serial pin | Pi 4 GPIO header pin | Signal |
|---|---|---|
| GND | pin 6 (or 9/14/…) | ground |
| RXD | pin 8  = GPIO14 | Pi **TXD** → adapter RX |
| TXD | pin 10 = GPIO15 | Pi **RXD** ← adapter TX |

Cross TX/RX (adapter RX ← Pi TX). **Do not connect the adapter's VCC/5 V** — power
the Pi from its normal USB-C supply.

Open a terminal at **115200 baud, 8N1, no flow control**:
- Windows: PuTTY or `tio`/`plink` on the adapter's COM port.
- The COM port shows up in Device Manager under *Ports (COM & LPT)* when you plug
  the adapter in.

## Step 4 — power on. Expected output

```
========================================
  FlintRTOS  -  Raspberry Pi 4 (AArch64)
  lean . secure . safe
========================================
[main] creating tasks...
[main] starting scheduler with 2 task(s)...
[A] tick 1  (t=1)
    [B] tock 1
[A] tick 2  (t=2)
    [B] tock 2
...
```

Tasks A and B alternating = **boot, UART, MMU-off EL1 execution, the ARM
generic-timer tick, and the context switch all work.** That's the whole minimal
kernel validated on real silicon.

## If nothing appears

1. **Blank console** — 90% of the time it's TX/RX swapped or wrong baud. Swap the
   two data wires; confirm 115200 8N1. Confirm GND is connected.
2. **Pi doesn't boot at all (no green ACT LED activity)** — `start4.elf`/`fixup4.dat`
   missing or `arm_64bit=1` not set. Re-check Step 1/2.
3. **Garbage characters** — baud mismatch, or `dtoverlay=disable-bt` missing from
   `config.txt` (then UART0 is on the BT modem, not the GPIO pins). Use our
   `config.txt` as-is.
4. **Banner prints, then stops** — boot + UART are good but the scheduler/timer
   didn't advance; capture the exact last line and tell me — that isolates it to the
   context switch or the timer IRQ, which is exactly what this minimal image is for.

## After it boots

Reply with what you see. Next steps once minimal is green:
- `make` (144 KB, lwIP NO_SYS + GENET) → validate the **GENET Ethernet driver**
  (the one hardware-unvalidated seam) with the UDP echo on port 7.
- `make LWIP_OS=1` (202 KB) → sockets + coreMQTT publish + PTP slave.
