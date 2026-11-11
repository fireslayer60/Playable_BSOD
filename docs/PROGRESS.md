# Playable BSOD — Project Progress & Session Continuity Log

> **Source of Truth Protocol**: At the start of every new session, inspect this file first to resume immediately without losing context.

---

## 1. Project Objective
An authentic Ring 0 Windows Kernel Driver (`playable_bsod.sys`) that intercepts unrecoverable operating system bugchecks (`KeBugCheckEx`) and runs an interactive 8-bit boss fight directly on the blue screen before the OS halts or resets.

---

## 2. Environment & Toolchain State
- **Host OS**: Windows 11 x64 (Hardware Virtualization Active)
- **IDE / Build Tools**: Visual Studio Community 2022 v17.14 (MSVC v143 `14.44.35207`)
- **Windows SDK**: `10.0.26100.0`
- **Windows Driver Kit (WDK)**: `10.0.26100.6584` (Installed & Verified)
- **Project Root**: `C:/Users/krish/Projects/playable-bsod`
- **Active Binary**: `bin/x64/Release/playable_bsod.sys`

---

## 3. The 5 Modular Phases (ALL PHASES COMPLETED 🎉)

| Phase | Description | Status | Deliverables |
| :--- | :--- | :--- | :--- |
| **Phase 1** | Build Pipeline & Driver Skeleton | **Completed** | `playable_bsod.vcxproj`, `build.bat`, clean build of `playable_bsod.sys` |
| **Phase 2** | BugCheck Hooking & Safe Interception | **Completed** | `KeRegisterBugCheckReasonCallback`, `KbCallbackSecondaryDumpData`, IRQL 31 safety, `NonPagedPoolNx` |
| **Phase 3** | Bare-Metal VRAM & Software Rasterizer | **Completed** | UEFI GOP mapping (`MmMapIoSpaceEx`), 32-bit ARGB rasterizer, 8x8 font, sprite engine |
| **Phase 4** | Hardware Port I/O & Keyboard Polling | **Completed** | Intel 8042 controller polling (`__inbyte(0x60)` / `__inbyte(0x64)`), Set 1 scancodes, live steering |
| **Phase 5** | The Boss Fight & Bare-Metal Game Loop | **Completed** | Fixed 60fps loop via `__rdtsc()`, boss AI state machine, projectile physics, Port 0x64 reset |

---

## 4. Phase 2 Details: BugCheck Hooking & Safe Interception (Completed)
- **Mechanism**: Registered via `KeRegisterBugCheckReasonCallback` with reason `KbCallbackSecondaryDumpData`.
- **IRQL 31 Safety Contract**:
  - The callback executes at `HIGH_LEVEL` (IRQL 31) where thread scheduling, page faults, and passive APIs are fatal.
  - All driver code and global state (`DRIVER_CONTEXT`) reside in non-paged memory (`POOL_NX_OPTIN=1`).
  - Safe deregistration in `DriverUnload` via `KeDeregisterBugCheckReasonCallback` prevents orphan callback panic.

---

## 5. Phase 3 Details: Bare-Metal VRAM & Software Rasterizer (Completed)
- **VRAM Framebuffer Mapping**:
  - Linear UEFI GOP physical video memory mapped via `MmMapIoSpaceEx` with `PAGE_READWRITE | PAGE_WRITECOMBINE` (for high-speed burst writes), falling back to `PAGE_NOCACHE` if rejected.
  - Default aperture: `0xF8000000ULL` (Hyper-V Gen2 VM / standard GOP default), 1024x768x32.
  - Supports dynamic parameter overrides via registry (`RegistryPath\Parameters`: `PhysicalBase`, `Width`, `Height`).
- **Software Rasterizer Primitives (`src/rasterizer.h`)**:
  - **Memory Math**: 2D-to-1D mapping: $\text{Offset} = (Y \times \text{Stride}) + X$
  - `Rasterizer_PlotPixel`: Safe bounds-checked 32-bit ARGB pixel plotting.
  - `Rasterizer_FillRect`: Fast solid rectangle filling.
  - `Rasterizer_DrawRect`: Outlined/wireframe rectangle with customizable border thickness.
  - `Rasterizer_DrawLine`: Bresenham's integer-only line algorithm (zero floating-point, zero FPU state dependency).
  - `Rasterizer_Clear`: Framebuffer clearing.
  - `Rasterizer_DrawChar` & `Rasterizer_DrawCharScaled`: 8x8 bitmap font rendering for full ASCII 32–126 with arbitrary integer scaling.
  - `Rasterizer_DrawString` & `Rasterizer_DrawStringScaled`: Multi-line text blitting.
  - `Rasterizer_DrawInt` & `Rasterizer_DrawHex`: Zero-CRT, zero-allocation number and hex formatters for kernel diagnostic readouts.
  - `Rasterizer_DrawProgressBar`: Graphical health and shield bars with colored fills and borders.
  - `Rasterizer_DrawSprite`: 32-bit ARGB multi-color sprite blitter with alpha transparency and scale multipliers.
  - `Rasterizer_DrawBitmap16x16`: 1-bit monochrome bitmap blitting.
- **Predefined Pixel Art Assets**:
  - `g_SpritePlayerShip16x16`: 16x16 retro player fighter ship.
  - `g_SpriteBoss24x24`: 24x24 "BugCheck Overlord" boss entity.
  - `g_SpritePlayerLaser4x8`: 4x8 player plasma laser.
  - `g_SpriteBossPlasma8x8`: 8x8 boss plasma attack orb.
  - `g_BitmapShield16x16`: 1-bit monochrome shield status icon.

---

## 6. Phase 4 Details: Hardware Port I/O & Keyboard Polling (Completed)
- **Direct Hardware I/O Engine (`src/input.h`)**:
  - Communicates directly with the legacy **Intel 8042 Keyboard Controller** using MSVC CPU intrinsic `__inbyte`:
    - Port `0x64`: Status register check. Bit 0 (`0x01` / `KBD_STATUS_OUT_FULL`) signals when data is waiting.
    - Port `0x60`: Data register read (scancode byte).
  - Implements `Input_Flush()`: Drains any stale keystrokes buffered before the crash occurred.
  - Implements `Input_Poll()`: Non-blocking poll routine executing at IRQL 31 without OS scheduler or USB HID stacks.
- **Scancode Set 1 State Machine**:
  - Handles Make codes (key down) and Break codes (key up, `scancode | 0x80`).
  - Supports extended keys prefixed with `0xE0` (Arrow keys).
  - Key bindings supported:
    - **Steer Left**: Left Arrow (`0xE0 0x4B`) or 'A' (`0x1E`)
    - **Steer Right**: Right Arrow (`0xE0 0x4D`) or 'D' (`0x20`)
    - **Pitch Up / Down**: Up Arrow (`0xE0 0x48`) / Down Arrow (`0xE0 0x50`) / 'W' (`0x11`) / 'S' (`0x1F`)
    - **Fire Lasers**: Spacebar (`0x39` make, `0xB9` break)
    - **Exit / Reset**: Escape key (`0x01` make)

---

## 7. Phase 5 Details: The Boss Fight & Bare-Metal Game Loop (Completed)
- **Zero-Allocation Game Engine (`src/game.h`)**:
  - Entire game state (`GAME_STATE`) resides in static non-paged pool memory with zero dynamic allocations (`malloc` or `ExAllocatePool` prohibited at IRQL 31).
  - Fixed projectile object pools (`MAX_PLAYER_LASERS = 12`, `MAX_BOSS_PLASMAS = 24`).
- **Hardware-Level Frame Rate Limiting (`__rdtsc()`)**:
  - Uses the CPU Time-Stamp Counter `__rdtsc()` to synchronize a fixed 60 FPS loop without OS timers or interrupts ($\approx 50{,}000{,}000$ cycles per frame on a 3.0 GHz CPU).
  - Employs `_mm_pause()` inside the spin loop to optimize pipeline power and prevent hyperthread starvation.
- **Boss AI State Machine**:
  - **Patrol**: Smooth horizontal hovering patrol across the arena boundaries.
  - **Phase 1 (HP > 50%)**: Normal speed (3 px/frame), fires dual downward plasma orbs every 42 frames.
  - **Phase 2 (Enraged, HP <= 50%)**: High-speed evasion (5 px/frame), fires 3-way spread plasma waves every 24 frames.
- **Axis-Aligned Bounding Box (AABB) Collisions**:
  - Player laser collision with Boss $72\times72$ hitbox: awards score (+100) and deducts Boss health.
  - Boss plasma collision with Player $32\times32$ hitbox: deducts player shields (-10).
- **Victory & Defeat End States**:
  - Defeating the Boss awards +5,000 bonus score and renders the golden `*** SYSTEM RESTORED ***` banner.
  - Shield collapse renders the crimson `*** CRITICAL FAILURE ***` banner.
- **Authentic Hardware Reset via Intel 8042**:
  - Once the game-over countdown concludes, `Hardware_ResetSystem()` writes command `0xFE` to Port `0x64`, pulsing the CPU reset line for an instant cold system reboot.
- **Interactive Desktop Preview (`preview.bat`)**:
  - Full 60 FPS interactive boss fight on the desktop with real-time ship controls, dual lasers, enrage transitions, and restart with `R`.
