#pragma once

#if defined(_KERNEL_MODE)
#include <ntddk.h>
#else
#include <windows.h>
#ifndef BOOLEAN
typedef BYTE BOOLEAN;
#endif
#endif

#define DRIVER_TAG 'dosB'

#include "input.h"

// Default framebuffer parameters (used when neither live hardware detection
// nor the registry provide a mode)
#define DEFAULT_FB_WIDTH   1024
#define DEFAULT_FB_HEIGHT  768
#define DEFAULT_FB_BPP     32

// All game/UI drawing happens on a fixed logical canvas of this size. The
// canvas is centred on the real screen via FRAMEBUFFER_CONFIG.OriginX/Y, so the
// layout works at any resolution >= 1024x768 (smaller screens are clipped).
#define CANVAS_WIDTH       1024
#define CANVAS_HEIGHT      768

// Minimum amount of the framebuffer BAR mapped at load time. 8 MiB covers
// 1920x1080x32. The mapping cannot be grown at crash time (MmMapIoSpaceEx is
// PASSIVE/DISPATCH only), so larger modes need the FramebufferSize registry value.
#define MIN_FB_MAP_SIZE    (8u * 1024u * 1024u)
#define MAX_FB_MAP_SIZE    (64u * 1024u * 1024u)

// Where the active display mode came from (shown on the diagnostics panel)
typedef enum _FB_MODE_SOURCE {
    FbModeDefault = 0,       // Hardcoded 1024x768 fallback
    FbModeRegistry,          // Services\playable_bsod\Parameters (written by setup script)
    FbModeVmwareSvga,        // Live read from VMware SVGA II registers (VMware, VirtualBox VMSVGA/VBoxSVGA)
    FbModeBochsVbe,          // Live read from Bochs VBE DISPI registers (QEMU std-vga, VirtualBox VBoxVGA)
    FbModePreview            // User-mode desktop preview
} FB_MODE_SOURCE;

// Framebuffer descriptor structure
typedef struct _FRAMEBUFFER_CONFIG {
    PVOID   VirtualBase;       // Start of visible screen (may be offset into the mapping)
    ULONG64 PhysicalBase;      // Physical address of the mapped aperture
    ULONG   Width;             // Visible width in pixels
    ULONG   Height;            // Visible height in pixels
    ULONG   PixelsPerScanLine; // Stride in pixels (>= Width)
    ULONG   BitsPerPixel;
    SIZE_T  BufferSize;        // Bytes of the visible screen (Height * stride * 4)

    // Logical canvas placement (see CANVAS_WIDTH/HEIGHT)
    ULONG   OriginX;
    ULONG   OriginY;

    // Mapping bookkeeping
    PVOID   MappedBase;        // What MmMapIoSpaceEx returned (needed for unmap)
    SIZE_T  MappedSize;        // Bytes actually mapped
    BOOLEAN WriteCombined;     // TRUE if the WC mapping succeeded (else uncached)
    FB_MODE_SOURCE ModeSource;
} FRAMEBUFFER_CONFIG, *PFRAMEBUFFER_CONFIG;

// Recompute OriginX/OriginY so the logical canvas is centred on the screen
static __inline VOID Framebuffer_CenterCanvas(PFRAMEBUFFER_CONFIG Fb)
{
    Fb->OriginX = (Fb->Width  > CANVAS_WIDTH)  ? (Fb->Width  - CANVAS_WIDTH)  / 2 : 0;
    Fb->OriginY = (Fb->Height > CANVAS_HEIGHT) ? (Fb->Height - CANVAS_HEIGHT) / 2 : 0;
}

#if defined(_KERNEL_MODE)

// Display adapter identity captured at load time (PCI scan via the HAL)
typedef struct _DISPLAY_ADAPTER_INFO {
    BOOLEAN Found;
    USHORT  VendorId;
    USHORT  DeviceId;
    ULONG   Bus;
    ULONG   Slot;
    ULONG   SvgaIoBase;        // VMware SVGA II index/value port base (0 if not SVGA)
    BOOLEAN HasBochsVbe;       // Bochs VBE DISPI ports 0x1CE/0x1CF expected
} DISPLAY_ADAPTER_INFO, *PDISPLAY_ADAPTER_INFO;

// Driver global context
typedef struct _DRIVER_CONTEXT {
    // Reason callback: runs during crash-dump writing (only if a dump is configured)
    KBUGCHECK_REASON_CALLBACK_RECORD ReasonRecord;
    BOOLEAN ReasonRegistered;
    // Classic callback: runs on every bugcheck, independent of dump settings
    KBUGCHECK_CALLBACK_RECORD        LegacyRecord;
    BOOLEAN LegacyRegistered;

    FRAMEBUFFER_CONFIG   Framebuffer;
    DISPLAY_ADAPTER_INFO Adapter;
    ULONG64              TscHz;          // Calibrated time-stamp counter frequency
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

// Function prototypes
DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD     DriverUnload;

NTSTATUS InitializeFramebuffer(PFRAMEBUFFER_CONFIG Fb, PUNICODE_STRING RegistryPath);
VOID CleanupFramebuffer(PFRAMEBUFFER_CONFIG Fb);

VOID BugCheckReasonCallbackRoutine(
    IN KBUGCHECK_CALLBACK_REASON Reason,
    IN struct _KBUGCHECK_REASON_CALLBACK_RECORD *Record,
    IN OUT PVOID ReasonSpecificData,
    IN ULONG ReasonSpecificDataLength
);

VOID BugCheckLegacyCallbackRoutine(
    IN PVOID Buffer,
    IN ULONG Length
);
#endif

// Clock speed shown on the diagnostics panel (0 = unknown / preview)
ULONG64 Platform_GetTscHz(void);

VOID RenderStaticBackdrop(PFRAMEBUFFER_CONFIG Fb, const char *TrapSource);
VOID RenderDynamicFrame(PFRAMEBUFFER_CONFIG Fb, PINPUT_STATE Input, ULONG PlayerX);
VOID ExecuteBareMetalGameLoop(PFRAMEBUFFER_CONFIG Fb, const char *TrapSource);
