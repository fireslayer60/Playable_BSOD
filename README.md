# Playable BSOD 🎮💀

An authentic Ring 0 Windows Kernel driver that intercepts `KeBugCheckEx` and runs an 8-bit boss fight directly on the blue screen before the OS can halt.

## Repository Layout
- `docs/ARCHITECTURE.md`: Low-level architectural specification (IRQL, UEFI GOP, PS/2 I/O).
- `src/driver.c`: KMDF/WDM driver entry point and bugcheck callback hook.
- `src/driver.h`: Framebuffer configuration and hardware port mappings.
- `vm/setup_vm.ps1`: Automated guest VM configuration script for test-signing and WinDbg.

## Development Prerequisites
1. Windows Driver Kit (WDK) 10 / Visual Studio with MSVC v143+.
2. Hyper-V, VMware, or QEMU guest running Windows 11.
3. WinDbg (Host debugger connected via KDNET).
