#include "driver.h"
#include "rasterizer.h"
#include "input.h"
#include "game.h"

#if defined(_KERNEL_MODE)
static DRIVER_CONTEXT g_Context = { 0 };

// Query optional registry overrides from HKLM\System\CurrentControlSet\Services\<Driver>\Parameters
static VOID QueryRegistryFramebufferParameters(
    _In_ PUNICODE_STRING RegistryPath,
    _Inout_ PFRAMEBUFFER_CONFIG Fb
)
{
    if (!RegistryPath || !RegistryPath->Buffer) return;

    OBJECT_ATTRIBUTES objAttr;
    HANDLE keyHandle = NULL;
    UNICODE_STRING paramsPath;
    WCHAR buffer[256];

    RtlInitEmptyUnicodeString(&paramsPath, buffer, sizeof(buffer));
    RtlCopyUnicodeString(&paramsPath, RegistryPath);
    RtlAppendUnicodeToString(&paramsPath, L"\\Parameters");

    InitializeObjectAttributes(
        &objAttr,
        &paramsPath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
    );

    NTSTATUS status = ZwOpenKey(&keyHandle, KEY_READ, &objAttr);
    if (NT_SUCCESS(status)) {
        UCHAR kvInfoBuf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG64)];
        PKEY_VALUE_PARTIAL_INFORMATION kvInfo = (PKEY_VALUE_PARTIAL_INFORMATION)kvInfoBuf;
        ULONG resultLength = 0;
        UNICODE_STRING valName;

        // Check for PhysicalBase override (REG_QWORD)
        RtlInitUnicodeString(&valName, L"PhysicalBase");
        if (NT_SUCCESS(ZwQueryValueKey(keyHandle, &valName, KeyValuePartialInformation, kvInfo, sizeof(kvInfoBuf), &resultLength))) {
            if (kvInfo->Type == REG_QWORD && kvInfo->DataLength >= sizeof(ULONG64)) {
                Fb->PhysicalBase = *(PULONG64)kvInfo->Data;
            }
        }

        // Check for Width override (REG_DWORD)
        RtlInitUnicodeString(&valName, L"Width");
        if (NT_SUCCESS(ZwQueryValueKey(keyHandle, &valName, KeyValuePartialInformation, kvInfo, sizeof(kvInfoBuf), &resultLength))) {
            if (kvInfo->Type == REG_DWORD && kvInfo->DataLength >= sizeof(ULONG)) {
                Fb->Width = *(PULONG)kvInfo->Data;
            }
        }

        // Check for Height override (REG_DWORD)
        RtlInitUnicodeString(&valName, L"Height");
        if (NT_SUCCESS(ZwQueryValueKey(keyHandle, &valName, KeyValuePartialInformation, kvInfo, sizeof(kvInfoBuf), &resultLength))) {
            if (kvInfo->Type == REG_DWORD && kvInfo->DataLength >= sizeof(ULONG)) {
                Fb->Height = *(PULONG)kvInfo->Data;
            }
        }

        ZwClose(keyHandle);
    }
}

// Auto-probe physical GPU aperture via PCI Configuration Mechanism #1 (Ports 0xCF8 / 0xCFC)
static ULONG64 DetectPciDisplayAperture(void)
{
    for (ULONG dev = 0; dev < 32; dev++) {
        ULONG addrClass = 0x80000000UL | (dev << 11) | 0x08;
        __outdword(0xCF8, addrClass);
        ULONG classData = __indword(0xCFC);
        UCHAR baseClass = (UCHAR)(classData >> 24);

        if (baseClass == 0x03) { // 0x03 = PCI Display Controller (VGA/SVGA/GOP)
            // Inspect BAR 0 and BAR 1 (VBoxSVGA, VMware SVGA, QEMU, Hyper-V)
            for (ULONG barOffset = 0x10; barOffset <= 0x14; barOffset += 4) {
                ULONG addrBar = 0x80000000UL | (dev << 11) | barOffset;
                __outdword(0xCF8, addrBar);
                ULONG barVal = __indword(0xCFC);

                if ((barVal & 0x01) == 0 && (barVal & ~0xFUL) != 0) {
                    return (ULONG64)(barVal & ~0xFUL);
                }
            }
        }
    }
    return 0;
}

NTSTATUS InitializeFramebuffer(
    _Inout_ PFRAMEBUFFER_CONFIG Fb,
    _In_opt_ PUNICODE_STRING RegistryPath
)
{
    Fb->PhysicalBase = 0;
    Fb->Width = DEFAULT_FB_WIDTH;
    Fb->Height = DEFAULT_FB_HEIGHT;
    Fb->BitsPerPixel = DEFAULT_FB_BPP;

    // 1. Check for optional registry overrides
    if (RegistryPath) {
        QueryRegistryFramebufferParameters(RegistryPath, Fb);
    }

    // 2. If no registry override, query PCI configuration space directly from hardware
    if (Fb->PhysicalBase == 0) {
        Fb->PhysicalBase = DetectPciDisplayAperture();
    }

    // 3. Fallback defaults if PCI scan returned zero
    if (Fb->PhysicalBase == 0) {
        Fb->PhysicalBase = 0xF8000000ULL; // Standard Hyper-V Gen2 default
    }

    Fb->PixelsPerScanLine = Fb->Width;
    Fb->BufferSize = (SIZE_T)Fb->Height * Fb->PixelsPerScanLine * (Fb->BitsPerPixel / 8);

    PHYSICAL_ADDRESS fbPhys;
    fbPhys.QuadPart = (LONGLONG)Fb->PhysicalBase;

    Fb->VirtualBase = MmMapIoSpaceEx(fbPhys, Fb->BufferSize, PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (!Fb->VirtualBase) {
        Fb->VirtualBase = MmMapIoSpaceEx(fbPhys, Fb->BufferSize, PAGE_READWRITE | PAGE_NOCACHE);
    }

    if (Fb->VirtualBase == NULL) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "[PlayableBSOD] Error: Could not map physical VRAM aperture at 0x%llX\n",
            Fb->PhysicalBase);
        return STATUS_UNSUCCESSFUL;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "[PlayableBSOD] Framebuffer mapped: VA=0x%p, PA=0x%llX, Res=%ux%u, Stride=%u\n",
        Fb->VirtualBase, Fb->PhysicalBase, Fb->Width, Fb->Height, Fb->PixelsPerScanLine);

    return STATUS_SUCCESS;
}

VOID CleanupFramebuffer(PFRAMEBUFFER_CONFIG Fb)
{
    if (Fb && Fb->VirtualBase) {
        MmUnmapIoSpace(Fb->VirtualBase, Fb->BufferSize);
        Fb->VirtualBase = NULL;
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[PlayableBSOD] Framebuffer unmapped successfully\n");
    }
}

VOID BugCheckCallbackRoutine(
    IN KBUGCHECK_CALLBACK_REASON Reason,
    IN struct _KBUGCHECK_REASON_CALLBACK_RECORD *Record,
    IN OUT PVOID ReasonSpecificData,
    IN ULONG ReasonSpecificDataLength
)
{
    UNREFERENCED_PARAMETER(Reason);
    UNREFERENCED_PARAMETER(Record);
    UNREFERENCED_PARAMETER(ReasonSpecificData);
    UNREFERENCED_PARAMETER(ReasonSpecificDataLength);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
        "[PlayableBSOD] BugCheck callback intercepted at IRQL HIGH_LEVEL (31)\n");

    // Check if framebuffer base is mapped
    if (g_Context.Framebuffer.VirtualBase != NULL) {
        ExecuteBareMetalGameLoop(&g_Context.Framebuffer);
    }
}
#endif // _KERNEL_MODE

// ============================================================================
// PHASE 5: THE BOSS FIGHT & BARE-METAL GAME LOOP
// ============================================================================

VOID RenderStaticBackdrop(PFRAMEBUFFER_CONFIG Fb)
{
    if (!Fb || !Fb->VirtualBase) return;

    // 1. Clear background to dark space canvas
    Rasterizer_Clear(Fb, COLOR_NAVY);

    // 2. Render deterministic retro starfield
    ULONG seed = 0x1337BEEF;
    for (ULONG i = 0; i < 150; i++) {
        seed = seed * 1103515245 + 12345;
        ULONG sx = seed % Fb->Width;
        seed = seed * 1103515245 + 12345;
        ULONG sy = seed % Fb->Height;
        seed = seed * 1103515245 + 12345;
        ULONG starColor = (seed % 4 == 0) ? COLOR_CYAN : ((seed % 4 == 1) ? COLOR_YELLOW : COLOR_WHITE);
        Rasterizer_PlotPixel(Fb, sx, sy, starColor);
    }

    // 3. Draw Main Retro Game Cabinet Frame & Window Borders
    Rasterizer_FillRect(Fb, 64, 44, 896, 680, COLOR_BLACK);
    Rasterizer_DrawRect(Fb, 60, 40, 896, 680, 2, COLOR_BSOD_BLUE);
    Rasterizer_DrawRect(Fb, 64, 44, 888, 672, 1, COLOR_CYAN);

    // 4. Arcade Header Banner
    Rasterizer_FillRect(Fb, 66, 46, 884, 48, COLOR_BLACK);
    Rasterizer_DrawLine(Fb, 66, 94, 950, 94, COLOR_CYAN);
    Rasterizer_DrawStringScaled(Fb, 84, 58, "PLAYABLE BSOD :: BARE-METAL BOSS BATTLE", 2, COLOR_YELLOW, COLOR_TRANSPARENT);

    // 5. Left Panel: Kernel Diagnostics & VRAM Architecture Readout
    Rasterizer_FillRect(Fb, 80, 110, 390, 240, COLOR_BLACK);
    Rasterizer_DrawRect(Fb, 80, 110, 390, 240, 1, COLOR_DARK_GRAY);

    Rasterizer_DrawString(Fb, 96, 122, "[ SYSTEM DIAGNOSTICS & MEMORY MAP ]", COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawLine(Fb, 96, 134, 450, 134, COLOR_DARK_CYAN);

    Rasterizer_DrawString(Fb, 96, 146, "TRAP STATE     :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 240, 146, "KeBugCheckEx CAUGHT", COLOR_GREEN, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 166, "IRQL LEVEL     :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 240, 166, "31 (HIGH_LEVEL)", COLOR_YELLOW, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 186, "VRAM PHYS BASE :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawHex(Fb, 240, 186, Fb->PhysicalBase, 8, COLOR_GREEN, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 206, "VRAM VIRT BASE :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawHex(Fb, 240, 206, (ULONG64)Fb->VirtualBase, 16, COLOR_GREEN, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 226, "RESOLUTION     :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 240, 226, Fb->Width, COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 275, 226, "x", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 290, 226, Fb->Height, COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 325, 226, "(32 BPP ARGB)", COLOR_GRAY, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 246, "PIXEL STRIDE   :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 240, 246, Fb->PixelsPerScanLine, COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 280, 246, "px / scanline", COLOR_GRAY, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 266, "MEMORY ATTR    :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 240, 266, "MmWriteCombined", COLOR_GREEN, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 286, "TIMING ENGINE  :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 240, 286, "__rdtsc() (CYCLE SYNC)", COLOR_YELLOW, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 310, "CRT RUNTIME    :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 240, 310, "NONE (ZERO DEPS)", COLOR_MAGENTA, COLOR_TRANSPARENT);

    // 6. Right Panel Container Outline
    Rasterizer_FillRect(Fb, 490, 110, 450, 240, COLOR_BLACK);
    Rasterizer_DrawRect(Fb, 490, 110, 450, 240, 1, COLOR_DARK_GRAY);

    // 7. Center Battle Arena Outer Frame
    Rasterizer_DrawRect(Fb, 80, 365, 860, 275, 1, COLOR_DARK_BLUE);

    // 8. Bottom Status & Controls Bar
    Rasterizer_FillRect(Fb, 66, 650, 884, 64, COLOR_BLACK);
    Rasterizer_DrawLine(Fb, 66, 650, 950, 650, COLOR_BSOD_BLUE);

    Rasterizer_DrawString(Fb, 84, 662,
        "[ PHASE 5 ACTIVE ]: Fixed 60 FPS loop via __rdtsc(), Boss AI, Hitboxes, and Port 0x64 Reset.",
        COLOR_GREEN, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 84, 682,
        "[ CONTROLS       ]: [LEFT / A] = Steer | [RIGHT / D] = Steer | [SPACE] = Fire | [ESC] = Abort.",
        COLOR_YELLOW, COLOR_TRANSPARENT);
}

VOID RenderDynamicFrame(PFRAMEBUFFER_CONFIG Fb, PINPUT_STATE Input, ULONG PlayerX)
{
    if (!Fb || !Fb->VirtualBase || !Input) return;
    UNREFERENCED_PARAMETER(PlayerX);

    // ========================================================================
    // UPDATE RIGHT PANEL: LIVE INTEL 8042 HARDWARE PORT I/O TELEMETRY
    // ========================================================================
    Rasterizer_FillRect(Fb, 491, 111, 448, 238, COLOR_BLACK);

    Rasterizer_DrawString(Fb, 506, 122, "[ INTEL 8042 KEYBOARD PORT I/O MONITOR ]", COLOR_YELLOW, COLOR_TRANSPARENT);
    Rasterizer_DrawLine(Fb, 506, 134, 920, 134, COLOR_GOLD);

    Rasterizer_DrawString(Fb, 506, 146, "PORT 0x64 STATUS :", COLOR_WHITE, COLOR_TRANSPARENT);
    UCHAR statusByte = Hardware_InByte(KBD_STATUS_PORT);
    Rasterizer_DrawHex(Fb, 660, 146, statusByte, 2, COLOR_GREEN, COLOR_TRANSPARENT);
    if (statusByte & KBD_STATUS_OUT_FULL) {
        Rasterizer_DrawString(Fb, 720, 146, "(DATA READY)", COLOR_GREEN, COLOR_TRANSPARENT);
    } else {
        Rasterizer_DrawString(Fb, 720, 146, "(BUFFER IDLE)", COLOR_GRAY, COLOR_TRANSPARENT);
    }

    Rasterizer_DrawString(Fb, 506, 166, "PORT 0x60 DATA   :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawHex(Fb, 660, 166, Input->LastScancode, 2, COLOR_CYAN, COLOR_TRANSPARENT);
    if (Input->LastScancode & SCANCODE_BREAK_MASK) {
        Rasterizer_DrawString(Fb, 720, 166, "(KEY BREAK / UP)", COLOR_GRAY, COLOR_TRANSPARENT);
    } else if (Input->LastScancode != 0) {
        Rasterizer_DrawString(Fb, 720, 166, "(KEY MAKE / DOWN)", COLOR_YELLOW, COLOR_TRANSPARENT);
    }

    Rasterizer_DrawString(Fb, 506, 186, "CONTROLLER TYPE  :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 660, 186, "LEGACY 8042 PS/2 SET 1", COLOR_GREEN, COLOR_TRANSPARENT);

    // Active Key States
    Rasterizer_DrawString(Fb, 506, 212, "STEER LEFT  [A/<-] :", COLOR_WHITE, COLOR_TRANSPARENT);
    if (Input->Left) {
        Rasterizer_DrawString(Fb, 670, 212, "[ PRESSED ]", COLOR_GREEN, COLOR_BLACK);
    } else {
        Rasterizer_DrawString(Fb, 670, 212, "[   ---   ]", COLOR_DARK_GRAY, COLOR_BLACK);
    }

    Rasterizer_DrawString(Fb, 506, 232, "STEER RIGHT [D/->] :", COLOR_WHITE, COLOR_TRANSPARENT);
    if (Input->Right) {
        Rasterizer_DrawString(Fb, 670, 232, "[ PRESSED ]", COLOR_GREEN, COLOR_BLACK);
    } else {
        Rasterizer_DrawString(Fb, 670, 232, "[   ---   ]", COLOR_DARK_GRAY, COLOR_BLACK);
    }

    Rasterizer_DrawString(Fb, 506, 252, "FIRE LASER  [SPACE]:", COLOR_WHITE, COLOR_TRANSPARENT);
    if (Input->Fire) {
        Rasterizer_DrawString(Fb, 670, 252, "[ FIRING! ]", COLOR_YELLOW, COLOR_BLACK);
    } else {
        Rasterizer_DrawString(Fb, 670, 252, "[   ---   ]", COLOR_DARK_GRAY, COLOR_BLACK);
    }

    Rasterizer_DrawString(Fb, 506, 276, "POLL CYCLES      :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 670, 276, (LONG)Input->PollCount, COLOR_CYAN, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 506, 296, "EVENTS CAPTURED  :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 670, 296, (LONG)Input->EventsProcessed, COLOR_GREEN, COLOR_TRANSPARENT);
}

VOID ExecuteBareMetalGameLoop(PFRAMEBUFFER_CONFIG Fb)
{
    if (!Fb || !Fb->VirtualBase) return;

    // 1. Render static arcade background, borders, and diagnostics
    RenderStaticBackdrop(Fb);

    // 2. Initialize hardware port I/O keyboard controller
    INPUT_STATE input;
    Input_Init(&input);
    Input_Flush();

    // 3. Initialize Game State Machine
    GAME_STATE game;
    Game_Init(&game);

#if defined(_KERNEL_MODE)
    // Target ~60 FPS (approx 50,000,000 CPU cycles per frame on a 3.0 GHz CPU)
    const ULONG64 cyclesPerFrame = 50000000ULL;
    ULONG64 lastTsc = __rdtsc();

    while (!input.Exit) {
        Input_Poll(&input);

        Game_Update(&game, &input);

        RenderDynamicFrame(Fb, &input, (ULONG)game.PlayerX);
        Game_RenderArena(Fb, &game, &input);

        // Hardware CPU Time-Stamp Counter frame rate throttle
        while ((__rdtsc() - lastTsc) < cyclesPerFrame) {
            _mm_pause();
        }
        lastTsc = __rdtsc();
    }
#else
    // Headless / single-frame user mode fallback
    RenderDynamicFrame(Fb, &input, (ULONG)game.PlayerX);
    Game_RenderArena(Fb, &game, &input);
#endif
}

#if defined(_KERNEL_MODE)
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[PlayableBSOD] DriverEntry Initializing\n");

    DriverObject->DriverUnload = DriverUnload;

    // Initialize and map Framebuffer VRAM (with optional registry parameter overrides)
    InitializeFramebuffer(&g_Context.Framebuffer, RegistryPath);

    // Initialize BugCheck callback record
    KeInitializeCallbackRecord(&g_Context.CallbackRecord);

    BOOLEAN status = KeRegisterBugCheckReasonCallback(
        &g_Context.CallbackRecord,
        BugCheckCallbackRoutine,
        KbCallbackSecondaryDumpData,
        (PUCHAR)"PlayableBSOD"
    );

    if (!status) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[PlayableBSOD] Failed to register BugCheck callback\n");
        CleanupFramebuffer(&g_Context.Framebuffer);
        return STATUS_UNSUCCESSFUL;
    }

    g_Context.CallbackRegistered = TRUE;
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[PlayableBSOD] BugCheck callback registered successfully\n");

    return STATUS_SUCCESS;
}

VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[PlayableBSOD] Driver Unloading\n");

    if (g_Context.CallbackRegistered) {
        KeDeregisterBugCheckReasonCallback(&g_Context.CallbackRecord);
        g_Context.CallbackRegistered = FALSE;
    }

    CleanupFramebuffer(&g_Context.Framebuffer);
}
#endif // _KERNEL_MODE
