#pragma once

#if defined(_KERNEL_MODE)
#include <ntddk.h>
#include <intrin.h>
#else
#include <windows.h>
#ifndef BOOLEAN
typedef BYTE BOOLEAN;
#endif
#endif

// ============================================================================
// Intel 8042 Keyboard Controller Hardware Ports & Flags
// ============================================================================
#define KBD_DATA_PORT       0x60    // Read: Scancode buffer / Write: Controller commands
#define KBD_STATUS_PORT     0x64    // Read: 8-bit status register / Write: Command register

// Status Register Bits
#define KBD_STATUS_OUT_FULL 0x01    // Bit 0: Output buffer full (1 = byte ready to read in 0x60)
#define KBD_STATUS_IN_FULL  0x02    // Bit 1: Input buffer full (1 = controller busy receiving)
#define KBD_STATUS_AUX_DATA 0x20    // Bit 5: Byte in 0x60 came from the PS/2 mouse (AUX), not the keyboard

// ============================================================================
// PS/2 Scancode Set 1 Definitions
// ============================================================================
#define SCANCODE_EXTENDED       0xE0    // Prefix for extended keys (Arrows, Home, End, etc.)

// Make codes (Key Pressed)
#define SCANCODE_MAKE_ESC       0x01
#define SCANCODE_MAKE_W         0x11
#define SCANCODE_MAKE_A         0x1E
#define SCANCODE_MAKE_S         0x1F
#define SCANCODE_MAKE_D         0x20
#define SCANCODE_MAKE_ENTER     0x1C
#define SCANCODE_MAKE_SPACE     0x39

// Extended Make codes (following 0xE0)
#define SCANCODE_MAKE_UP_ARROW    0x48
#define SCANCODE_MAKE_DOWN_ARROW  0x50
#define SCANCODE_MAKE_LEFT_ARROW  0x4B
#define SCANCODE_MAKE_RIGHT_ARROW 0x4D

// Break mask (Key Released): bit 7 is set (scancode | 0x80)
#define SCANCODE_BREAK_MASK     0x80

// ============================================================================
// Input State Structure
// ============================================================================
typedef struct _INPUT_STATE {
    BOOLEAN Left;           // Left Arrow or 'A' pressed
    BOOLEAN Right;          // Right Arrow or 'D' pressed
    BOOLEAN Up;             // Up Arrow or 'W' pressed
    BOOLEAN Down;           // Down Arrow or 'S' pressed
    BOOLEAN Fire;           // Spacebar pressed
    BOOLEAN Exit;           // Escape key pressed
    BOOLEAN Enter;          // Enter key pressed
    UCHAR   LastScancode;   // Raw byte read from Port 0x60
    BOOLEAN ExtendedPrefix; // Internal flag: previous byte was 0xE0
    ULONG   PollCount;      // Total number of hardware polls performed
    ULONG   EventsProcessed;// Number of scancode events captured
} INPUT_STATE, *PINPUT_STATE;

// ============================================================================
// Hardware Port I/O Wrappers
// ============================================================================

// Low-level port input intrinsic
static __inline UCHAR Hardware_InByte(USHORT port)
{
#if defined(_KERNEL_MODE)
    // CPU assembly instruction: IN AL, DX
    return __inbyte(port);
#else
    // Simulated user-mode port read (not directly executable in Ring 3)
    UNREFERENCED_PARAMETER(port);
    return 0;
#endif
}

// Check if keyboard controller has a byte ready in Port 0x60.
// Bytes from the PS/2 mouse share the same output buffer; they are read and
// discarded here so mouse movement can never be mistaken for a keystroke
// (a mouse packet byte of 0x01 would otherwise look like ESC and quit the game).
static __inline BOOLEAN Hardware_IsKeyAvailable(void)
{
#if defined(_KERNEL_MODE)
    for (ULONG i = 0; i < 32; i++) {
        UCHAR status = Hardware_InByte(KBD_STATUS_PORT);
        if (status == 0xFF) return FALSE;                 // No 8042 present (floating bus)
        if ((status & KBD_STATUS_OUT_FULL) == 0) return FALSE;
        if ((status & KBD_STATUS_AUX_DATA) == 0) return TRUE; // Keyboard byte waiting
        Hardware_InByte(KBD_DATA_PORT);                   // Discard mouse byte
    }
    return FALSE;
#else
    return FALSE;
#endif
}

// Drain any stale keystrokes buffered prior to BugCheck crash
static __inline VOID Input_Flush(void)
{
#if defined(_KERNEL_MODE)
    // Read up to 64 bytes or until output buffer is empty
    for (ULONG i = 0; i < 64; i++) {
        if (!Hardware_IsKeyAvailable()) break;
        Hardware_InByte(KBD_DATA_PORT);
    }
#endif
}

// Initialize clean input state
static __inline VOID Input_Init(_Out_ PINPUT_STATE State)
{
    if (!State) return;
    State->Left = FALSE;
    State->Right = FALSE;
    State->Up = FALSE;
    State->Down = FALSE;
    State->Fire = FALSE;
    State->Exit = FALSE;
    State->Enter = FALSE;
    State->LastScancode = 0;
    State->ExtendedPrefix = FALSE;
    State->PollCount = 0;
    State->EventsProcessed = 0;
}

// Process a single raw scancode byte into the Input State
static __inline VOID Input_ProcessScancode(
    _Inout_ PINPUT_STATE State,
    _In_ UCHAR scancode
)
{
    if (!State) return;

    State->LastScancode = scancode;
    State->EventsProcessed++;

    // 1. Extended prefix check
    if (scancode == SCANCODE_EXTENDED) {
        State->ExtendedPrefix = TRUE;
        return;
    }

    BOOLEAN isBreak = (scancode & SCANCODE_BREAK_MASK) != 0;
    UCHAR baseCode = scancode & ~SCANCODE_BREAK_MASK;

    // 2. Handle Extended Keys (Arrow keys)
    if (State->ExtendedPrefix) {
        State->ExtendedPrefix = FALSE;

        switch (baseCode) {
        case SCANCODE_MAKE_LEFT_ARROW:
            State->Left = !isBreak;
            break;
        case SCANCODE_MAKE_RIGHT_ARROW:
            State->Right = !isBreak;
            break;
        case SCANCODE_MAKE_UP_ARROW:
            State->Up = !isBreak;
            break;
        case SCANCODE_MAKE_DOWN_ARROW:
            State->Down = !isBreak;
            break;
        }
        return;
    }

    // 3. Handle Standard Keys (WASD, Space, Esc, Enter)
    switch (baseCode) {
    case SCANCODE_MAKE_A:
        State->Left = !isBreak;
        break;
    case SCANCODE_MAKE_D:
        State->Right = !isBreak;
        break;
    case SCANCODE_MAKE_W:
        State->Up = !isBreak;
        break;
    case SCANCODE_MAKE_S:
        State->Down = !isBreak;
        break;
    case SCANCODE_MAKE_SPACE:
        State->Fire = !isBreak;
        break;
    case SCANCODE_MAKE_ESC:
        State->Exit = !isBreak;
        break;
    case SCANCODE_MAKE_ENTER:
        State->Enter = !isBreak;
        break;
    }
}

// Non-blocking poll of Port 0x64 and 0x60
// Returns TRUE if a new scancode was read and processed
static __inline BOOLEAN Input_Poll(_Inout_ PINPUT_STATE State)
{
    if (!State) return FALSE;

    State->PollCount++;

#if defined(_KERNEL_MODE)
    if (Hardware_IsKeyAvailable()) {
        UCHAR scancode = Hardware_InByte(KBD_DATA_PORT);
        Input_ProcessScancode(State, scancode);
        return TRUE;
    }
    return FALSE;
#else
    // User-mode preview simulation: use Win32 GetAsyncKeyState
    BOOLEAN stateChanged = FALSE;

    BOOLEAN leftPressed = (GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState('A') & 0x8000);
    BOOLEAN rightPressed = (GetAsyncKeyState(VK_RIGHT) & 0x8000) || (GetAsyncKeyState('D') & 0x8000);
    BOOLEAN upPressed = (GetAsyncKeyState(VK_UP) & 0x8000) || (GetAsyncKeyState('W') & 0x8000);
    BOOLEAN downPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000) || (GetAsyncKeyState('S') & 0x8000);
    BOOLEAN firePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    BOOLEAN exitPressed = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    BOOLEAN enterPressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;

    if (leftPressed != State->Left) {
        State->Left = leftPressed;
        State->LastScancode = leftPressed ? SCANCODE_MAKE_LEFT_ARROW : (SCANCODE_MAKE_LEFT_ARROW | SCANCODE_BREAK_MASK);
        stateChanged = TRUE;
    }
    if (rightPressed != State->Right) {
        State->Right = rightPressed;
        State->LastScancode = rightPressed ? SCANCODE_MAKE_RIGHT_ARROW : (SCANCODE_MAKE_RIGHT_ARROW | SCANCODE_BREAK_MASK);
        stateChanged = TRUE;
    }
    if (upPressed != State->Up) {
        State->Up = upPressed;
        State->LastScancode = upPressed ? SCANCODE_MAKE_UP_ARROW : (SCANCODE_MAKE_UP_ARROW | SCANCODE_BREAK_MASK);
        stateChanged = TRUE;
    }
    if (downPressed != State->Down) {
        State->Down = downPressed;
        State->LastScancode = downPressed ? SCANCODE_MAKE_DOWN_ARROW : (SCANCODE_MAKE_DOWN_ARROW | SCANCODE_BREAK_MASK);
        stateChanged = TRUE;
    }
    if (firePressed != State->Fire) {
        State->Fire = firePressed;
        State->LastScancode = firePressed ? SCANCODE_MAKE_SPACE : (SCANCODE_MAKE_SPACE | SCANCODE_BREAK_MASK);
        stateChanged = TRUE;
    }
    if (exitPressed != State->Exit) {
        State->Exit = exitPressed;
        State->LastScancode = exitPressed ? SCANCODE_MAKE_ESC : (SCANCODE_MAKE_ESC | SCANCODE_BREAK_MASK);
        stateChanged = TRUE;
    }
    if (enterPressed != State->Enter) {
        State->Enter = enterPressed;
        State->LastScancode = enterPressed ? SCANCODE_MAKE_ENTER : (SCANCODE_MAKE_ENTER | SCANCODE_BREAK_MASK);
        stateChanged = TRUE;
    }

    if (stateChanged) {
        State->EventsProcessed++;
    }

    return stateChanged;
#endif
}
