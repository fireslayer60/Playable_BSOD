# Playable BSOD: Technical Architecture & Execution Model

## 1. System Overview
This project implements an x64 Windows Kernel-Mode Driver that intercepts unrecoverable operating system bugchecks (`KeBugCheckEx`) and executes a self-contained, bare-metal mini-game directly within the crash state prior to machine halt or restart.

## 2. BugCheck Interception
- **Mechanism**: Registration via `KeRegisterBugCheckReasonCallback` utilizing a `KBUGCHECK_REASON_CALLBACK_RECORD`.
- **Reason Code**: `KbCallbackSecondaryDumpData` / `KbCallbackDumpAttribute`.
- **Execution Context**:
  - IRQL: `HIGH_LEVEL` (31).
  - Hardware interrupts are disabled.
  - Paging is severely restricted; all driver code, buffers, and state must reside in non-paged pool memory (`NonPagedPoolNx`).
  - OS scheduler, threading, and standard Win32/DirectX subsystems are completely inert.

## 3. Display Acquisition (Bare-Metal Framebuffer)
- Windows userland and kernel display drivers (WDDM) terminate during a bugcheck.
- We retrieve the physical address of the linear UEFI Graphics Output Protocol (GOP) framebuffer.
- Map the physical video memory into non-paged kernel virtual address space via `MmMapIoSpace` using `MmWriteCombined`.
- Implement a minimal ARGB32 software rasterizer (rendering 8x8 font glyphs and 16x16 sprite bitmaps directly to video RAM).

## 4. Hardware Input (PS/2 Port 0x60 Polling)
- Standard USB HID input stacks are non-functional at `HIGH_LEVEL`.
- Motherboards and virtual machines emulate the legacy Intel 8042 keyboard controller.
- Poll hardware I/O ports directly using compiler intrinsic assembly: `__inbyte(0x64)` (status check) and `__inbyte(0x60)` (data scancode read).
- Parse make/break scancodes for directional movement and fire actions.

## 5. Containment & Development Safety
- **Testing Isolation**: All driver binaries must strictly be tested inside an isolated Hyper-V or QEMU virtual machine.
- **Kernel Debugging**: Host-guest connection orchestrated via KDNET over UDP.
- **Test Signing**: Target guest VM configured with `bcdedit /set testsigning on`.
