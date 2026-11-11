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

// Default GOP Framebuffer parameters (fallback for standard 1024x768x32 display)
#define DEFAULT_FB_WIDTH   1024
#define DEFAULT_FB_HEIGHT  768
#define DEFAULT_FB_BPP     32

// Framebuffer descriptor structure
typedef struct _FRAMEBUFFER_CONFIG {
    PVOID   VirtualBase;
    ULONG64 PhysicalBase;
    ULONG   Width;
    ULONG   Height;
    ULONG   PixelsPerScanLine;
    ULONG   BitsPerPixel;
    SIZE_T  BufferSize;
} FRAMEBUFFER_CONFIG, *PFRAMEBUFFER_CONFIG;

#if defined(_KERNEL_MODE)
// Driver global context
typedef struct _DRIVER_CONTEXT {
    KBUGCHECK_REASON_CALLBACK_RECORD CallbackRecord;
    BOOLEAN CallbackRegistered;
    FRAMEBUFFER_CONFIG Framebuffer;
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

// Function prototypes
DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD     DriverUnload;

NTSTATUS InitializeFramebuffer(PFRAMEBUFFER_CONFIG Fb, PUNICODE_STRING RegistryPath);
VOID CleanupFramebuffer(PFRAMEBUFFER_CONFIG Fb);

VOID BugCheckCallbackRoutine(
    IN KBUGCHECK_CALLBACK_REASON Reason,
    IN struct _KBUGCHECK_REASON_CALLBACK_RECORD *Record,
    IN OUT PVOID ReasonSpecificData,
    IN ULONG ReasonSpecificDataLength
);
#endif

VOID RenderStaticBackdrop(PFRAMEBUFFER_CONFIG Fb);
VOID RenderDynamicFrame(PFRAMEBUFFER_CONFIG Fb, PINPUT_STATE Input, ULONG PlayerX);
VOID ExecuteBareMetalGameLoop(PFRAMEBUFFER_CONFIG Fb);
