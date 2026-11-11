# Playable BSOD: Project Roadmap & Architecture Plan

```
 ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
 │     PHASE 1     │     │     PHASE 2     │     │     PHASE 3     │     │     PHASE 4     │     │     PHASE 5     │
 │  Build Pipeline │ ──► │ BugCheck Hook   │ ──► │ Bare-Metal VRAM │ ──► │  PS/2 Port I/O  │ ──► │ The Boss Fight  │
 │  & Driver Entry │     │ & Crash Catch   │     │ & Rasterizer    │     │  Input Engine   │     │ & Game Loop     │
 └─────────────────┘     └─────────────────┘     └─────────────────┘     └─────────────────┘     └─────────────────┘
```

## Phase 1: The Build Pipeline & Driver Skeleton
- **Objective:** Create `playable_bsod.vcxproj` and compile `driver.c` into a valid `playable_bsod.sys` binary.
- **Key Learnings & Concepts:**
  - How C code turns into a `.sys` file without standard runtime libraries (no `main()`, no `printf()`, no `malloc()`).
  - How Windows loads drivers into kernel space using the Service Control Manager (`sc.exe`).
  - What `DriverEntry` and `DriverUnload` actually do.

## Phase 2: BugCheck Hooking & Safe Interception
- **Objective:** Register `KeRegisterBugCheckReasonCallback` and log a confirmation via `DbgPrintEx`.
- **Key Learnings & Concepts:**
  - IRQL (Interrupt Request Levels): Why IRQL 31 (`HIGH_LEVEL`) is a "frozen world" where threads, the OS scheduler, and paging are completely dead.
  - Why memory must strictly be in Non-Paged Pool (`NonPagedPoolNx`)—and what happens (instant double-fault) if your code triggers a page fault during a crash.

## Phase 3: Bare-Metal VRAM & Software Rasterizer
- **Objective:** Map the UEFI GOP physical video memory into kernel space and write a primitive software renderer (pixel plotting, rectangles, and an 8×8 bitmap font table).
- **Key Learnings & Concepts:**
  - How physical video RAM works and how `MmMapIoSpaceEx` gives us a virtual window to the monitor.
  - The math of a 2D framebuffer: mapping an (X, Y) coordinate to a 1D memory offset:
    $$\text{Offset} = (Y \times \text{Stride}) + (X \times 4)$$
  - How to draw text and shapes purely by setting raw 32-bit ARGB values in memory.

## Phase 4: Hardware Port I/O & Keyboard Polling
- **Objective:** Direct port polling routines using CPU intrinsics `__inbyte(0x64)` and `__inbyte(0x60)` to capture keystrokes.
- **Key Learnings & Concepts:**
  - CPU Memory Space vs. CPU I/O Space (the `IN` and `OUT` assembly instructions).
  - How the legacy Intel 8042 keyboard controller works and why it remains alive when modern USB drivers freeze.
  - Reading raw make/break scancodes to track key presses (Arrow keys, Space).

## Phase 5: The Bare-Metal Game Loop & Boss Battle
- **Objective:** A fixed-rate game loop driven by `__rdtsc()`, drawing the player ship, boss sprite, bullet patterns, and handling collisions.
- **Key Learnings & Concepts:**
  - High-precision CPU cycle counting with `__rdtsc()` when OS timers and threads don't exist.
  - Designing a zero-allocation, bare-metal state machine.
  - Triggering a hardware reset or clean shutdown once the game concludes.
