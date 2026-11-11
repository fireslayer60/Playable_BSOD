# Playable BSOD: Technical Architecture

## 1. Execution model

`playable_bsod.sys` is a WDM kernel driver with no device objects. All of its work happens in two phases:

| Phase | Context | What happens |
| :--- | :--- | :--- |
| **Load** (`DriverEntry`) | PASSIVE_LEVEL, normal kernel | Calibrate the TSC, find and map the framebuffer, allocate render buffers, register bugcheck callbacks |
| **Crash** (bugcheck callback) | HIGH_LEVEL (IRQL 31), interrupts off, other CPUs frozen | Read the live display mode, run the game loop, reset the machine |

Everything the crash phase needs is prepared during load, because at IRQL 31 you can't allocate memory, map I/O space, take a page fault, wait, or call almost any kernel API. During the crash phase the driver only reads and writes memory it already owns, executes `IN`/`OUT` port instructions, and reads the TSC.

## 2. Bugcheck interception

Two callbacks are registered, and an interlocked flag (`g_GameStarted`) makes sure the game runs at most once per crash:

- **`KeRegisterBugCheckCallback`** (classic). Invoked for every bugcheck, regardless of crash-dump settings.
- **`KeRegisterBugCheckReasonCallback` with `KbCallbackSecondaryDumpData`.** Invoked while the crash dump is written, and only if a dump is configured. Reason callbacks can be invoked more than once, which the guard handles. The callback leaves `OutBuffer` untouched, so no data is added to the dump.

On the first VirtualBox test the **SecondaryDumpData** callback fired first. That's why Windows' "100% complete" dump-progress text can appear over the arena once the game hands control back.

## 3. Display

### 3.1 Finding the framebuffer (load time)
`DetectDisplayAdapter` walks PCI buses 0–7 with **`HalGetBusDataByOffset`**. The HAL serializes config-space access, unlike raw `0xCF8/0xCFC` port I/O, which could race the OS. It looks for the first display-class device (base class `0x03`):

- The framebuffer is the first **prefetchable memory BAR**, else the first memory BAR. 64-bit BARs are decoded.
- **VMware SVGA II** (`15AD:0405`, also VirtualBox VMSVGA) and **VirtualBox VBoxSVGA** (`80EE:BEEF` with an I/O BAR0) record their index/value I/O port base.
- **QEMU std-vga** (`1234:1111`) and VirtualBox adapters are flagged as having **Bochs VBE** DISPI ports (`0x1CE`/`0x1CF`).

Registry overrides under `HKLM\SYSTEM\CurrentControlSet\Services\playable_bsod\Parameters`:

| Value | Type | Meaning |
| :--- | :--- | :--- |
| `PhysicalBase` | REG_QWORD | Framebuffer physical address (skips PCI detection) |
| `Width`, `Height` | REG_DWORD | Display mode. The install scripts write the current mode from `Win32_VideoController` |
| `Stride` | REG_DWORD | Pixels per scanline (default: `Width`) |
| `FramebufferSize` | REG_DWORD | Bytes to map |

The driver maps `max(8 MiB, mode size, FramebufferSize)` bytes (capped at 64 MiB) with `MmMapIoSpaceEx(PAGE_READWRITE | PAGE_WRITECOMBINE)`, falling back to `PAGE_NOCACHE`. The mapping can't grow later, so it leaves headroom for resolution changes after load.

### 3.2 Live mode detection (crash time)
`Framebuffer_RefreshLiveMode` asks the virtual GPU for its current mode. Port I/O is legal at any IRQL, and no other CPU is running.

1. **VMware SVGA II:** if `SVGA_REG_ENABLE` is set, read `WIDTH`, `HEIGHT`, `BITS_PER_PIXEL`, `BYTES_PER_LINE`, `FB_OFFSET`.
2. **Bochs VBE:** if the ID register reads `0xB0Cx` and VBE is enabled, read `XRES`, `YRES`, `BPP`, `VIRT_WIDTH`, `X/Y_OFFSET`.
3. Otherwise keep the registry/default mode.

A mode is accepted only at 32 bpp and only if it fits inside the mapping. If it's taller than the mapping, only the reachable rows are drawn. The panel's **MODE SOURCE** line shows which path was used.

### 3.3 Canvas and rendering
All UI is laid out on a fixed **1024×768 logical canvas**, centred on the real screen (`OriginX/OriginY`). Rasterizer primitives come in two flavours:

- `*Phys` primitives take real screen coordinates and are the only code that touches video memory (clipped to the real `Width`/`Height`).
- Logical primitives add the canvas origin. Text, sprites, lines, progress bars and rectangles are built on these.

**Back buffer + shadow.** The game draws into a canvas-sized buffer in non-paged RAM. `Render_Present` compares it against a shadow copy of what's on screen and writes only changed pixels to VRAM. A simulated 600-frame fight averaged about 4,100 changed pixels per frame (about 16 KB), versus about 1.36 MB per frame when the arena and panel were cleared and redrawn directly. If the buffers couldn't be allocated, the game draws straight to VRAM.

Pixels are 32-bit `0xAARRGGBB` values (BGRX in memory, the standard GOP/VBE layout). Sprites use alpha `0` as transparency.

## 4. Input: Intel 8042 polling

USB HID stacks are dead at IRQL 31, but VMs (and some PCs) emulate the legacy 8042 keyboard controller:

- Port `0x64` status: bit 0 means a byte is waiting, bit 5 means it came from the PS/2 mouse (AUX). Mouse bytes are read and discarded so they can't be mistaken for scancodes. A status of `0xFF` means no controller (floating bus).
- Port `0x60` data is fed into a **Scan Code Set 1** state machine (make/break, `0xE0` extended prefix for the arrow keys).
- Each frame drains up to 16 queued bytes. Between frames the controller is polled every ~0.25 ms, since each port read is a VM exit, so tight-loop polling just burns hypervisor time.

## 5. Timing

`CalibrateTscHz` measures TSC ticks against `KeQueryPerformanceCounter` over 50 ms at DISPATCH_LEVEL, with sanity bounds of 100 MHz–20 GHz and a 3 GHz fallback. The game loop targets `TscHz / 60` cycles per frame, spins with `_mm_pause()`, and resyncs if it falls more than four frames behind.

## 6. Game

`game.h` holds a zero-allocation state machine: fixed pools of 12 player lasers and 24 boss plasma orbs, AABB collisions, and a two-phase boss (enrages at 50% integrity: faster patrol, 3-way spread). Victory or defeat starts a 4-second countdown; ESC aborts immediately. Both end in `Hardware_ResetSystem`:

1. 8042 command `0xFE` (pulse the CPU reset line), then
2. PCI Reset Control Register `0xCF9` (`0x02`, then `0x06`) for machines without an 8042, then
3. `HLT` forever.

## 7. Desktop preview

Compiled without `_KERNEL_MODE`, the same `driver.c`/`rasterizer.h`/`game.h` build into `preview.exe`. The framebuffer is a `malloc`'d buffer blitted with `SetDIBitsToDevice`, and input comes from `GetAsyncKeyState`. `--res` and `--stride` simulate other display modes.

## 8. Safety and containment

- Test only in a disposable VM with test signing enabled (`vm/setup_vm.ps1`).
- The signing script creates a **per-developer** self-signed certificate. No certificate or private key is checked in.
- `DriverUnload` deregisters both callbacks and unmaps/frees everything, so `sc stop playable_bsod` disarms the driver cleanly.
