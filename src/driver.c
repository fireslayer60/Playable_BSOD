#include "driver.h"
#include "rasterizer.h"
#include "input.h"
#include "game.h"

#if defined(_KERNEL_MODE)
static DRIVER_CONTEXT g_Context = { 0 };

// Set to 1 by whichever bugcheck callback runs first, so the game is played at
// most once per crash (reason callbacks can be invoked more than once, and we
// register two callback types).
static volatile LONG g_GameStarted = 0;

ULONG64 Platform_GetTscHz(void)
{
    return g_Context.TscHz;
}

// ============================================================================
// Registry overrides: HKLM\System\CurrentControlSet\Services\<Driver>\Parameters
//   PhysicalBase    REG_QWORD  framebuffer physical address
//   Width, Height   REG_DWORD  display mode (setup script writes the current mode)
//   Stride          REG_DWORD  pixels per scanline (defaults to Width)
//   FramebufferSize REG_DWORD  bytes to map (defaults to max(8 MiB, mode size))
// ============================================================================
typedef struct _FB_REGISTRY_PARAMS {
    ULONG64 PhysicalBase;
    ULONG   Width;
    ULONG   Height;
    ULONG   Stride;
    ULONG   FramebufferSize;
} FB_REGISTRY_PARAMS, *PFB_REGISTRY_PARAMS;

static VOID QueryRegistryDword(HANDLE Key, PCWSTR Name, PULONG Out)
{
    UCHAR buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG64)];
    PKEY_VALUE_PARTIAL_INFORMATION info = (PKEY_VALUE_PARTIAL_INFORMATION)buf;
    ULONG resultLength = 0;
    UNICODE_STRING valName;

    RtlInitUnicodeString(&valName, Name);
    if (NT_SUCCESS(ZwQueryValueKey(Key, &valName, KeyValuePartialInformation, info, sizeof(buf), &resultLength))) {
        if (info->Type == REG_DWORD && info->DataLength >= sizeof(ULONG)) {
            *Out = *(PULONG)info->Data;
        }
    }
}

static VOID QueryRegistryFramebufferParameters(
    _In_ PUNICODE_STRING RegistryPath,
    _Out_ PFB_REGISTRY_PARAMS Params
)
{
    RtlZeroMemory(Params, sizeof(*Params));
    if (!RegistryPath || !RegistryPath->Buffer) return;

    OBJECT_ATTRIBUTES objAttr;
    HANDLE keyHandle = NULL;
    UNICODE_STRING paramsPath;
    WCHAR buffer[256];

    RtlInitEmptyUnicodeString(&paramsPath, buffer, sizeof(buffer));
    RtlCopyUnicodeString(&paramsPath, RegistryPath);
    if (!NT_SUCCESS(RtlAppendUnicodeToString(&paramsPath, L"\\Parameters"))) return;

    InitializeObjectAttributes(&objAttr, &paramsPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    if (!NT_SUCCESS(ZwOpenKey(&keyHandle, KEY_READ, &objAttr))) return;

    {
        UCHAR buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG64)];
        PKEY_VALUE_PARTIAL_INFORMATION info = (PKEY_VALUE_PARTIAL_INFORMATION)buf;
        ULONG resultLength = 0;
        UNICODE_STRING valName;

        RtlInitUnicodeString(&valName, L"PhysicalBase");
        if (NT_SUCCESS(ZwQueryValueKey(keyHandle, &valName, KeyValuePartialInformation, info, sizeof(buf), &resultLength))) {
            if (info->Type == REG_QWORD && info->DataLength >= sizeof(ULONG64)) {
                Params->PhysicalBase = *(PULONG64)info->Data;
            }
        }
    }

    QueryRegistryDword(keyHandle, L"Width", &Params->Width);
    QueryRegistryDword(keyHandle, L"Height", &Params->Height);
    QueryRegistryDword(keyHandle, L"Stride", &Params->Stride);
    QueryRegistryDword(keyHandle, L"FramebufferSize", &Params->FramebufferSize);

    ZwClose(keyHandle);
}

// ============================================================================
// Display adapter discovery (load time, PASSIVE_LEVEL)
//
// Uses HalGetBusDataByOffset rather than raw 0xCF8/0xCFC port I/O: the HAL
// serialises config-space access, so we can't race the OS's own PCI accesses.
// ============================================================================
#define PCI_VENDOR_VMWARE      0x15AD
#define PCI_DEVICE_VMWARE_SVGA2 0x0405
#define PCI_VENDOR_QEMU        0x1234
#define PCI_DEVICE_QEMU_STDVGA 0x1111
#define PCI_VENDOR_VBOX        0x80EE
#define PCI_DEVICE_VBOX_VGA    0xBEEF

#define PCI_SCAN_MAX_BUS       8

// Decode memory BAR i (handles 64-bit BARs). Returns 0 for I/O or empty BARs.
static ULONG64 DecodeMemoryBar(_In_ const PCI_COMMON_HEADER *Hdr, ULONG Index, _Out_ PULONG Consumed)
{
    ULONG bar = Hdr->u.type0.BaseAddresses[Index];
    *Consumed = 1;

    if (bar & 0x1) return 0; // I/O space BAR

    ULONG64 base = bar & ~0xFUL;
    if (((bar >> 1) & 0x3) == 0x2 && Index + 1 < PCI_TYPE0_ADDRESSES) {
        base |= ((ULONG64)Hdr->u.type0.BaseAddresses[Index + 1]) << 32;
        *Consumed = 2;
    }
    return base;
}

static BOOLEAN DetectDisplayAdapter(
    _Out_ PDISPLAY_ADAPTER_INFO Info,
    _Out_ PULONG64 FramebufferBase
)
{
    RtlZeroMemory(Info, sizeof(*Info));
    *FramebufferBase = 0;

    for (ULONG bus = 0; bus < PCI_SCAN_MAX_BUS; bus++) {
        for (ULONG dev = 0; dev < PCI_MAX_DEVICES; dev++) {
            for (ULONG fn = 0; fn < PCI_MAX_FUNCTION; fn++) {
                PCI_SLOT_NUMBER slot;
                PCI_COMMON_HEADER hdr;

                slot.u.AsULONG = 0;
                slot.u.bits.DeviceNumber = dev;
                slot.u.bits.FunctionNumber = fn;

                ULONG got = HalGetBusDataByOffset(PCIConfiguration, bus, slot.u.AsULONG, &hdr, 0, sizeof(hdr));
                if (got == 0) {
                    // Bus does not exist
                    goto next_bus;
                }
                if (got < sizeof(hdr) || hdr.VendorID == PCI_INVALID_VENDORID || hdr.VendorID == 0) {
                    if (fn == 0) break; // No device in this slot
                    continue;
                }

                if (hdr.BaseClass == PCI_CLASS_DISPLAY_CTLR &&
                    (hdr.HeaderType & ~PCI_MULTIFUNCTION) == PCI_DEVICE_TYPE) {

                    BOOLEAN bar0IsIo = (hdr.u.type0.BaseAddresses[0] & 0x1) != 0;

                    Info->Found = TRUE;
                    Info->VendorId = hdr.VendorID;
                    Info->DeviceId = hdr.DeviceID;
                    Info->Bus = bus;
                    Info->Slot = slot.u.AsULONG;

                    // VMware SVGA II (VMware, VirtualBox VMSVGA) and VirtualBox VBoxSVGA:
                    // BAR0 = I/O index/value ports, BAR1 = framebuffer.
                    if ((hdr.VendorID == PCI_VENDOR_VMWARE && hdr.DeviceID == PCI_DEVICE_VMWARE_SVGA2) ||
                        (hdr.VendorID == PCI_VENDOR_VBOX && hdr.DeviceID == PCI_DEVICE_VBOX_VGA && bar0IsIo)) {
                        Info->SvgaIoBase = hdr.u.type0.BaseAddresses[0] & ~0x3UL;
                    }

                    // Bochs VBE DISPI (QEMU std-vga, VirtualBox VBoxVGA/VBoxSVGA)
                    if ((hdr.VendorID == PCI_VENDOR_QEMU && hdr.DeviceID == PCI_DEVICE_QEMU_STDVGA) ||
                        hdr.VendorID == PCI_VENDOR_VBOX) {
                        Info->HasBochsVbe = TRUE;
                    }

                    // Framebuffer = first prefetchable memory BAR, else first memory BAR
                    ULONG64 firstMem = 0;
                    for (ULONG i = 0; i < PCI_TYPE0_ADDRESSES; ) {
                        ULONG used = 1;
                        ULONG64 base = DecodeMemoryBar(&hdr, i, &used);
                        if (base != 0) {
                            if (hdr.u.type0.BaseAddresses[i] & 0x8) { // prefetchable
                                *FramebufferBase = base;
                                break;
                            }
                            if (firstMem == 0) firstMem = base;
                        }
                        i += used;
                    }
                    if (*FramebufferBase == 0) *FramebufferBase = firstMem;

                    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
                        "[PlayableBSOD] Display adapter %04X:%04X at bus %u slot 0x%X, FB=0x%llX, SVGA io=0x%X, VBE=%u\n",
                        hdr.VendorID, hdr.DeviceID, bus, slot.u.AsULONG, *FramebufferBase,
                        Info->SvgaIoBase, Info->HasBochsVbe);
                    return TRUE;
                }

                // Only probe functions 1-7 of multi-function devices
                if (fn == 0 && !(hdr.HeaderType & PCI_MULTIFUNCTION)) break;
            }
        }
    next_bus:
        ;
    }
    return FALSE;
}

// ============================================================================
// Live display mode read (crash time, HIGH_LEVEL)
//
// The mode may have changed since load (window resize, resolution change), so
// we ask the virtual GPU directly. Port I/O is legal at any IRQL, and every
// other processor is frozen, so nothing else is touching these registers.
// ============================================================================
#define SVGA_INDEX_PORT        0x0
#define SVGA_VALUE_PORT        0x1
#define SVGA_REG_ENABLE        1
#define SVGA_REG_WIDTH         2
#define SVGA_REG_HEIGHT        3
#define SVGA_REG_BITS_PER_PIXEL 7
#define SVGA_REG_BYTES_PER_LINE 12
#define SVGA_REG_FB_OFFSET     14

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF
#define VBE_DISPI_INDEX_ID     0x0
#define VBE_DISPI_INDEX_XRES   0x1
#define VBE_DISPI_INDEX_YRES   0x2
#define VBE_DISPI_INDEX_BPP    0x3
#define VBE_DISPI_INDEX_ENABLE 0x4
#define VBE_DISPI_INDEX_VIRT_WIDTH 0x6
#define VBE_DISPI_INDEX_X_OFFSET   0x8
#define VBE_DISPI_INDEX_Y_OFFSET   0x9

static ULONG SvgaReadReg(ULONG IoBase, ULONG Index)
{
    __outdword((USHORT)(IoBase + SVGA_INDEX_PORT), Index);
    return __indword((USHORT)(IoBase + SVGA_VALUE_PORT));
}

static USHORT VbeReadReg(USHORT Index)
{
    __outword(VBE_DISPI_IOPORT_INDEX, Index);
    return __inword(VBE_DISPI_IOPORT_DATA);
}

// Point the framebuffer descriptor at a new mode, if it fits inside what we mapped.
static BOOLEAN Framebuffer_ApplyMode(
    _Inout_ PFRAMEBUFFER_CONFIG Fb,
    ULONG Width,
    ULONG Height,
    ULONG StridePixels,
    SIZE_T OffsetBytes,
    FB_MODE_SOURCE Source
)
{
    if (!Fb->MappedBase) return FALSE;
    if (Width < 320 || Height < 200 || Width > 16384 || Height > 16384) return FALSE;
    if (StridePixels < Width || StridePixels > 16384) return FALSE;
    if (OffsetBytes >= Fb->MappedSize) return FALSE;

    SIZE_T rowBytes = (SIZE_T)StridePixels * 4;
    SIZE_T available = Fb->MappedSize - OffsetBytes;
    if ((SIZE_T)Height * rowBytes > available) {
        // Mode is bigger than our mapping: draw only the rows we can reach.
        Height = (ULONG)(available / rowBytes);
        if (Height < 200) return FALSE;
    }

    Fb->VirtualBase = (PUCHAR)Fb->MappedBase + OffsetBytes;
    Fb->Width = Width;
    Fb->Height = Height;
    Fb->PixelsPerScanLine = StridePixels;
    Fb->BitsPerPixel = 32;
    Fb->BufferSize = (SIZE_T)Height * rowBytes;
    Fb->ModeSource = Source;
    Framebuffer_CenterCanvas(Fb);
    return TRUE;
}

static VOID Framebuffer_RefreshLiveMode(_Inout_ PFRAMEBUFFER_CONFIG Fb, _In_ PDISPLAY_ADAPTER_INFO Adapter)
{
    if (!Adapter->Found) return;

    // 1. VMware SVGA II register interface (only meaningful while SVGA mode is enabled)
    if (Adapter->SvgaIoBase != 0) {
        ULONG enable = SvgaReadReg(Adapter->SvgaIoBase, SVGA_REG_ENABLE);
        if (enable & 0x1) {
            ULONG w   = SvgaReadReg(Adapter->SvgaIoBase, SVGA_REG_WIDTH);
            ULONG h   = SvgaReadReg(Adapter->SvgaIoBase, SVGA_REG_HEIGHT);
            ULONG bpp = SvgaReadReg(Adapter->SvgaIoBase, SVGA_REG_BITS_PER_PIXEL);
            ULONG bpl = SvgaReadReg(Adapter->SvgaIoBase, SVGA_REG_BYTES_PER_LINE);
            ULONG off = SvgaReadReg(Adapter->SvgaIoBase, SVGA_REG_FB_OFFSET);
            if (bpp == 32 && (bpl % 4) == 0 &&
                Framebuffer_ApplyMode(Fb, w, h, bpl / 4, off, FbModeVmwareSvga)) {
                return;
            }
        }
    }

    // 2. Bochs VBE DISPI interface
    if (Adapter->HasBochsVbe) {
        USHORT id = VbeReadReg(VBE_DISPI_INDEX_ID);
        if ((id & 0xFFF0) == 0xB0C0) {
            USHORT enable = VbeReadReg(VBE_DISPI_INDEX_ENABLE);
            if (enable & 0x1) {
                ULONG w     = VbeReadReg(VBE_DISPI_INDEX_XRES);
                ULONG h     = VbeReadReg(VBE_DISPI_INDEX_YRES);
                ULONG bpp   = VbeReadReg(VBE_DISPI_INDEX_BPP);
                ULONG virtW = VbeReadReg(VBE_DISPI_INDEX_VIRT_WIDTH);
                ULONG xOff  = VbeReadReg(VBE_DISPI_INDEX_X_OFFSET);
                ULONG yOff  = VbeReadReg(VBE_DISPI_INDEX_Y_OFFSET);
                if (virtW < w) virtW = w;
                if (bpp == 32) {
                    SIZE_T off = (((SIZE_T)yOff * virtW) + xOff) * 4;
                    Framebuffer_ApplyMode(Fb, w, h, virtW, off, FbModeBochsVbe);
                }
            }
        }
    }
    // Otherwise keep the load-time (registry/default) mode.
}

// ============================================================================
// Framebuffer mapping (load time)
// ============================================================================
NTSTATUS InitializeFramebuffer(
    _Inout_ PFRAMEBUFFER_CONFIG Fb,
    _In_opt_ PUNICODE_STRING RegistryPath
)
{
    FB_REGISTRY_PARAMS reg;
    ULONG64 pciBase = 0;

    RtlZeroMemory(Fb, sizeof(*Fb));
    Fb->Width = DEFAULT_FB_WIDTH;
    Fb->Height = DEFAULT_FB_HEIGHT;
    Fb->BitsPerPixel = DEFAULT_FB_BPP;
    Fb->ModeSource = FbModeDefault;

    // 1. Registry overrides (setup script records the current resolution here)
    QueryRegistryFramebufferParameters(RegistryPath, &reg);
    if (reg.Width >= 320 && reg.Height >= 200 && reg.Width <= 16384 && reg.Height <= 16384) {
        Fb->Width = reg.Width;
        Fb->Height = reg.Height;
        Fb->ModeSource = FbModeRegistry;
    }
    Fb->PixelsPerScanLine = (reg.Stride >= Fb->Width && reg.Stride <= 16384) ? reg.Stride : Fb->Width;

    // 2. Find the display adapter and its framebuffer BAR
    DetectDisplayAdapter(&g_Context.Adapter, &pciBase);

    if (reg.PhysicalBase != 0) {
        Fb->PhysicalBase = reg.PhysicalBase;
    } else if (pciBase != 0) {
        Fb->PhysicalBase = pciBase;
    } else {
        Fb->PhysicalBase = 0xF8000000ULL; // Hyper-V legacy default (see docs: Gen2 has no PS/2 input)
    }

    // 3. Decide how much to map. We can't map more at crash time, so leave
    //    headroom for the user changing resolution after the driver loads.
    SIZE_T modeBytes = (SIZE_T)Fb->Height * Fb->PixelsPerScanLine * 4;
    SIZE_T mapSize = MIN_FB_MAP_SIZE;
    if (modeBytes > mapSize) mapSize = modeBytes;
    if (reg.FramebufferSize > mapSize) mapSize = reg.FramebufferSize;
    if (mapSize > MAX_FB_MAP_SIZE) mapSize = MAX_FB_MAP_SIZE;
    if (mapSize < modeBytes) {
        // Registry mode larger than we're willing to map: fall back to defaults
        Fb->Width = DEFAULT_FB_WIDTH;
        Fb->Height = DEFAULT_FB_HEIGHT;
        Fb->PixelsPerScanLine = DEFAULT_FB_WIDTH;
        Fb->ModeSource = FbModeDefault;
        modeBytes = (SIZE_T)Fb->Height * Fb->PixelsPerScanLine * 4;
    }

    PHYSICAL_ADDRESS fbPhys;
    fbPhys.QuadPart = (LONGLONG)Fb->PhysicalBase;

    // Try the generous mapping first, then just the current mode.
    SIZE_T attempts[2] = { mapSize, modeBytes };
    for (ULONG a = 0; a < 2 && Fb->MappedBase == NULL; a++) {
        if (a == 1 && attempts[1] == attempts[0]) break;
        Fb->MappedBase = MmMapIoSpaceEx(fbPhys, attempts[a], PAGE_READWRITE | PAGE_WRITECOMBINE);
        if (Fb->MappedBase) {
            Fb->WriteCombined = TRUE;
        } else {
            Fb->MappedBase = MmMapIoSpaceEx(fbPhys, attempts[a], PAGE_READWRITE | PAGE_NOCACHE);
        }
        if (Fb->MappedBase) Fb->MappedSize = attempts[a];
    }

    if (Fb->MappedBase == NULL) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "[PlayableBSOD] Error: Could not map physical VRAM aperture at 0x%llX\n", Fb->PhysicalBase);
        return STATUS_UNSUCCESSFUL;
    }

    Fb->VirtualBase = Fb->MappedBase;
    Fb->BufferSize = modeBytes;
    Framebuffer_CenterCanvas(Fb);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "[PlayableBSOD] Framebuffer mapped: VA=0x%p, PA=0x%llX, Mapped=%Iu bytes (%s), Mode=%ux%u stride %u\n",
        Fb->MappedBase, Fb->PhysicalBase, Fb->MappedSize, Fb->WriteCombined ? "WC" : "UC",
        Fb->Width, Fb->Height, Fb->PixelsPerScanLine);

    return STATUS_SUCCESS;
}

VOID CleanupFramebuffer(PFRAMEBUFFER_CONFIG Fb)
{
    if (Fb && Fb->MappedBase) {
        MmUnmapIoSpace(Fb->MappedBase, Fb->MappedSize);
        Fb->MappedBase = NULL;
        Fb->VirtualBase = NULL;
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[PlayableBSOD] Framebuffer unmapped successfully\n");
    }
}

// ============================================================================
// TSC calibration (load time)
//
// Measures the time-stamp counter against the performance counter for ~50 ms
// so the game runs at 60 FPS regardless of CPU clock speed.
// ============================================================================
#define TSC_FALLBACK_HZ 3000000000ULL

static ULONG64 CalibrateTscHz(void)
{
    LARGE_INTEGER qpcFreq, q0, q1;
    ULONG64 t0, t1;
    KIRQL oldIrql;

    // Avoid being preempted mid-measurement
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    q0 = KeQueryPerformanceCounter(&qpcFreq);
    t0 = __rdtsc();
    for (ULONG i = 0; i < 1000; i++) {
        KeStallExecutionProcessor(50); // 1000 x 50 us = 50 ms
    }
    q1 = KeQueryPerformanceCounter(NULL);
    t1 = __rdtsc();
    KeLowerIrql(oldIrql);

    ULONG64 dq = (ULONG64)(q1.QuadPart - q0.QuadPart);
    if (qpcFreq.QuadPart <= 0 || dq == 0 || t1 <= t0) return TSC_FALLBACK_HZ;

    ULONG64 hz = ((t1 - t0) * (ULONG64)qpcFreq.QuadPart) / dq;
    if (hz < 100000000ULL || hz > 20000000000ULL) return TSC_FALLBACK_HZ; // sanity: 100 MHz - 20 GHz
    return hz;
}

// ============================================================================
// Bugcheck callbacks (HIGH_LEVEL)
// ============================================================================
static VOID RunGameOnce(const char *TrapSource)
{
    if (InterlockedCompareExchange(&g_GameStarted, 1, 0) != 0) {
        return; // Already played during this crash
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
        "[PlayableBSOD] BugCheck intercepted via %s\n", TrapSource);

    if (g_Context.Framebuffer.MappedBase == NULL) return;

    Framebuffer_RefreshLiveMode(&g_Context.Framebuffer, &g_Context.Adapter);
    ExecuteBareMetalGameLoop(&g_Context.Framebuffer, TrapSource);
}

// Runs while the crash dump is being written (only if a dump is configured).
VOID BugCheckReasonCallbackRoutine(
    IN KBUGCHECK_CALLBACK_REASON Reason,
    IN struct _KBUGCHECK_REASON_CALLBACK_RECORD *Record,
    IN OUT PVOID ReasonSpecificData,
    IN ULONG ReasonSpecificDataLength
)
{
    UNREFERENCED_PARAMETER(Record);
    UNREFERENCED_PARAMETER(ReasonSpecificData);
    UNREFERENCED_PARAMETER(ReasonSpecificDataLength);

    if (Reason != KbCallbackSecondaryDumpData) return;
    // We leave OutBuffer untouched, so no secondary data is added to the dump.
    RunGameOnce("SecondaryDumpData CB");
}

// Classic callback: invoked on every bugcheck regardless of dump settings.
VOID BugCheckLegacyCallbackRoutine(
    IN PVOID Buffer,
    IN ULONG Length
)
{
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);
    RunGameOnce("KeBugCheckCallback");
}
#else
// User-mode preview: no calibrated TSC
ULONG64 Platform_GetTscHz(void)
{
    return 0;
}
#endif // _KERNEL_MODE

// ============================================================================
// PHASE 5: THE BOSS FIGHT & BARE-METAL GAME LOOP
// ============================================================================

static const char *ModeSourceName(FB_MODE_SOURCE Source)
{
    switch (Source) {
    case FbModeRegistry:   return "REGISTRY (SETUP SCRIPT)";
    case FbModeVmwareSvga: return "LIVE: VMWARE SVGA II";
    case FbModeBochsVbe:   return "LIVE: BOCHS VBE";
    case FbModePreview:    return "DESKTOP PREVIEW";
    default:               return "DEFAULT 1024x768";
    }
}

VOID RenderStaticBackdrop(PFRAMEBUFFER_CONFIG Fb, const char *TrapSource)
{
    if (!Fb || !Fb->VirtualBase) return;

    // 1. Clear the whole physical screen to dark space canvas
    Rasterizer_Clear(Fb, COLOR_NAVY);

    // 2. Deterministic retro starfield across the whole physical screen
    ULONG64 area = (ULONG64)Fb->Width * Fb->Height;
    ULONG starCount = (ULONG)((150ULL * area) / ((ULONG64)CANVAS_WIDTH * CANVAS_HEIGHT));
    if (starCount > 1200) starCount = 1200;

    ULONG seed = 0x1337BEEF;
    for (ULONG i = 0; i < starCount; i++) {
        seed = seed * 1103515245 + 12345;
        ULONG sx = seed % Fb->Width;
        seed = seed * 1103515245 + 12345;
        ULONG sy = seed % Fb->Height;
        seed = seed * 1103515245 + 12345;
        ULONG starColor = (seed % 4 == 0) ? COLOR_CYAN : ((seed % 4 == 1) ? COLOR_YELLOW : COLOR_WHITE);
        Rasterizer_PlotPixelPhys(Fb, sx, sy, starColor);
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

    Rasterizer_DrawString(Fb, 96, 146, "TRAP SOURCE    :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 240, 146, TrapSource ? TrapSource : "UNKNOWN", COLOR_GREEN, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 166, "IRQL LEVEL     :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 240, 166, "31 (HIGH_LEVEL)", COLOR_YELLOW, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 186, "VRAM PHYS BASE :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawHex(Fb, 240, 186, Fb->PhysicalBase, (Fb->PhysicalBase > 0xFFFFFFFFULL) ? 12 : 8, COLOR_GREEN, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 206, "VRAM VIRT BASE :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawHex(Fb, 240, 206, (ULONG64)Fb->VirtualBase, 16, COLOR_GREEN, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 226, "RESOLUTION     :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 240, 226, Fb->Width, COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, (Fb->Width >= 1000) ? 275 : 267, 226, "x", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, (Fb->Width >= 1000) ? 287 : 279, 226, Fb->Height, COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 330, 226, "(32 BPP ARGB)", COLOR_GRAY, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 246, "PIXEL STRIDE   :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 240, 246, Fb->PixelsPerScanLine, COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 280, 246, "px / scanline", COLOR_GRAY, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 266, "MEMORY ATTR    :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 240, 266, Fb->WriteCombined ? "WRITE-COMBINED" : "UNCACHED",
        Fb->WriteCombined ? COLOR_GREEN : COLOR_YELLOW, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 286, "TIMING ENGINE  :", COLOR_WHITE, COLOR_TRANSPARENT);
    {
        ULONG64 tscHz = Platform_GetTscHz();
        if (tscHz != 0) {
            Rasterizer_DrawString(Fb, 240, 286, "__rdtsc() @", COLOR_YELLOW, COLOR_TRANSPARENT);
            Rasterizer_DrawInt(Fb, 336, 286, (LONG)(tscHz / 1000000ULL), COLOR_YELLOW, COLOR_TRANSPARENT);
            Rasterizer_DrawString(Fb, 376, 286, "MHz", COLOR_YELLOW, COLOR_TRANSPARENT);
        } else {
            Rasterizer_DrawString(Fb, 240, 286, "Sleep(16) PREVIEW", COLOR_YELLOW, COLOR_TRANSPARENT);
        }
    }

    Rasterizer_DrawString(Fb, 96, 306, "MODE SOURCE    :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 240, 306, ModeSourceName(Fb->ModeSource),
        (Fb->ModeSource == FbModeDefault) ? COLOR_RED : COLOR_GREEN, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 96, 326, "CRT RUNTIME    :", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 240, 326, "NONE (ZERO DEPS)", COLOR_MAGENTA, COLOR_TRANSPARENT);

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
    if (statusByte == 0xFF) {
        // Reads of a non-existent I/O port float high: no 8042 on this machine
        Rasterizer_DrawString(Fb, 720, 146, "(NO 8042 FOUND!)", COLOR_RED, COLOR_TRANSPARENT);
    } else if (statusByte & KBD_STATUS_OUT_FULL) {
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

VOID ExecuteBareMetalGameLoop(PFRAMEBUFFER_CONFIG Fb, const char *TrapSource)
{
    if (!Fb || !Fb->VirtualBase) return;

    // 1. Render static arcade background, borders, and diagnostics
    RenderStaticBackdrop(Fb, TrapSource);

    // 2. Initialize hardware port I/O keyboard controller
    INPUT_STATE input;
    Input_Init(&input);
    Input_Flush();

    // 3. Initialize Game State Machine
    GAME_STATE game;
    Game_Init(&game);

#if defined(_KERNEL_MODE)
    // Fixed 60 FPS using the TSC frequency calibrated at load time
    ULONG64 tscHz = Platform_GetTscHz();
    const ULONG64 cyclesPerFrame = (tscHz ? tscHz : 3000000000ULL) / 60;
    ULONG64 nextFrame = __rdtsc() + cyclesPerFrame;

    while (!input.Exit) {
        // Drain every byte the controller has queued this frame, so key-ups
        // are never missed when several keys change at once.
        for (ULONG n = 0; n < 16 && Input_Poll(&input); n++) {
        }

        Game_Update(&game, &input);

        RenderDynamicFrame(Fb, &input, (ULONG)game.PlayerX);
        Game_RenderArena(Fb, &game, &input);

        // Hardware CPU Time-Stamp Counter frame rate throttle (keep polling
        // the keyboard while we wait so the 8042 buffer never overflows)
        while ((LONG64)(__rdtsc() - nextFrame) < 0) {
            if (Hardware_IsKeyAvailable()) {
                Input_Poll(&input);
            }
            _mm_pause();
        }
        nextFrame += cyclesPerFrame;
        // If rendering fell badly behind, resync instead of fast-forwarding
        if ((LONG64)(__rdtsc() - nextFrame) > (LONG64)(cyclesPerFrame * 4)) {
            nextFrame = __rdtsc() + cyclesPerFrame;
        }
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

    // 1. Measure CPU clock for frame pacing
    g_Context.TscHz = CalibrateTscHz();
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "[PlayableBSOD] TSC calibrated at %llu Hz\n", g_Context.TscHz);

    // 2. Find and map the framebuffer (registry overrides + PCI discovery)
    if (!NT_SUCCESS(InitializeFramebuffer(&g_Context.Framebuffer, RegistryPath))) {
        // Nothing to draw on: refuse to load rather than arm a useless hook
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    // 3. Register both bugcheck hooks; RunGameOnce ensures only the first plays.
    KeInitializeCallbackRecord(&g_Context.LegacyRecord);
    g_Context.LegacyRegistered = KeRegisterBugCheckCallback(
        &g_Context.LegacyRecord,
        BugCheckLegacyCallbackRoutine,
        NULL,
        0,
        (PUCHAR)"PlayableBSOD");

    KeInitializeCallbackRecord(&g_Context.ReasonRecord);
    g_Context.ReasonRegistered = KeRegisterBugCheckReasonCallback(
        &g_Context.ReasonRecord,
        BugCheckReasonCallbackRoutine,
        KbCallbackSecondaryDumpData,
        (PUCHAR)"PlayableBSOD");

    if (!g_Context.LegacyRegistered && !g_Context.ReasonRegistered) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[PlayableBSOD] Failed to register any BugCheck callback\n");
        CleanupFramebuffer(&g_Context.Framebuffer);
        return STATUS_UNSUCCESSFUL;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "[PlayableBSOD] Armed: legacy callback=%u, reason callback=%u\n",
        g_Context.LegacyRegistered, g_Context.ReasonRegistered);

    return STATUS_SUCCESS;
}

VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[PlayableBSOD] Driver Unloading\n");

    if (g_Context.LegacyRegistered) {
        KeDeregisterBugCheckCallback(&g_Context.LegacyRecord);
        g_Context.LegacyRegistered = FALSE;
    }
    if (g_Context.ReasonRegistered) {
        KeDeregisterBugCheckReasonCallback(&g_Context.ReasonRecord);
        g_Context.ReasonRegistered = FALSE;
    }

    CleanupFramebuffer(&g_Context.Framebuffer);
}
#endif // _KERNEL_MODE
