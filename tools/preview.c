#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "driver.h"
#include "input.h"
#include "game.h"

static FRAMEBUFFER_CONFIG g_Fb = { 0 };
static BITMAPINFO g_Bmi = { 0 };
static INPUT_STATE g_Input = { 0 };
static GAME_STATE  g_Game = { 0 };

static BOOL SaveBitmapFile(const char *filename, PFRAMEBUFFER_CONFIG Fb)
{
    FILE *f = NULL;
    fopen_s(&f, filename, "wb");
    if (!f) {
        printf("[ERROR] Could not open %s for writing.\n", filename);
        return FALSE;
    }

    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER bih;

    ZeroMemory(&bfh, sizeof(bfh));
    ZeroMemory(&bih, sizeof(bih));

    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + (DWORD)Fb->BufferSize;

    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = (LONG)Fb->Width;
    bih.biHeight = -(LONG)Fb->Height; // Negative height for top-down raster order
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = (DWORD)Fb->BufferSize;

    fwrite(&bfh, sizeof(bfh), 1, f);
    fwrite(&bih, sizeof(bih), 1, f);
    fwrite(Fb->VirtualBase, Fb->BufferSize, 1, f);
    fclose(f);

    printf("[PlayableBSOD] Successfully exported preview image: %s\n", filename);
    return TRUE;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        SetDIBitsToDevice(
            hdc,
            0, 0,
            g_Fb.Width, g_Fb.Height,
            0, 0,
            0, g_Fb.Height,
            g_Fb.VirtualBase,
            &g_Bmi,
            DIB_RGB_COLORS
        );

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        } else if (wParam == 'R' || wParam == 'r') {
            Game_Init(&g_Game);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int main(int argc, char **argv)
{
    BOOL bmpOnly = FALSE;
    if (argc > 1 && strcmp(argv[1], "--bmp-only") == 0) {
        bmpOnly = TRUE;
    }

    printf("===================================================================\n");
    printf("   PLAYABLE BSOD - PHASE 5 THE BOSS BATTLE & GAME LOOP PREVIEW     \n");
    printf("===================================================================\n\n");

    // 1. Configure the simulated framebuffer
    g_Fb.Width = DEFAULT_FB_WIDTH;   // 1024
    g_Fb.Height = DEFAULT_FB_HEIGHT; // 768
    g_Fb.PixelsPerScanLine = DEFAULT_FB_WIDTH;
    g_Fb.BitsPerPixel = DEFAULT_FB_BPP; // 32 bpp
    g_Fb.PhysicalBase = 0xF8000000ULL; // Simulated GOP VRAM base
    g_Fb.BufferSize = (SIZE_T)g_Fb.Height * g_Fb.PixelsPerScanLine * (g_Fb.BitsPerPixel / 8);

    g_Fb.VirtualBase = malloc(g_Fb.BufferSize);
    if (!g_Fb.VirtualBase) {
        printf("[ERROR] Failed to allocate simulated framebuffer memory.\n");
        return 1;
    }

    // 2. Initialize input engine and game state
    Input_Init(&g_Input);
    Game_Init(&g_Game);

    // 3. Render static backdrop once
    RenderStaticBackdrop(&g_Fb);

    // 4. Initial dynamic frame render
    RenderDynamicFrame(&g_Fb, &g_Input, (ULONG)g_Game.PlayerX);
    Game_RenderArena(&g_Fb, &g_Game, &g_Input);

    // 5. If headless / BMP only mode, export and quit
    if (bmpOnly) {
        SaveBitmapFile("playable_bsod_preview.bmp", &g_Fb);
        printf("[PlayableBSOD] Headless mode: preview image generated successfully.\n");
        free(g_Fb.VirtualBase);
        return 0;
    }

    // 6. Set up DIB bitmap info for native window rendering
    ZeroMemory(&g_Bmi, sizeof(g_Bmi));
    g_Bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_Bmi.bmiHeader.biWidth = (LONG)g_Fb.Width;
    g_Bmi.bmiHeader.biHeight = -(LONG)g_Fb.Height; // Top-down
    g_Bmi.bmiHeader.biPlanes = 1;
    g_Bmi.bmiHeader.biBitCount = 32;
    g_Bmi.bmiHeader.biCompression = BI_RGB;

    // 7. Create native Win32 window
    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "PlayableBSODPreviewWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExA(&wc);

    RECT wr = { 0, 0, (LONG)g_Fb.Width, (LONG)g_Fb.Height };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "Playable BSOD :: Phase 5 Bare-Metal Boss Fight (Arrows/A/D = Move, Space = Fire, R = Restart, Esc = Exit)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        NULL, NULL, wc.hInstance, NULL
    );

    if (!hwnd) {
        printf("[ERROR] Failed to create preview window.\n");
        free(g_Fb.VirtualBase);
        return 1;
    }

    printf("[PlayableBSOD] Boss Battle is ONLINE!\n");
    printf("               - Steer Ship : [LEFT ARROW] / [A] and [RIGHT ARROW] / [D]\n");
    printf("               - Fire Laser : [SPACEBAR]\n");
    printf("               - Restart    : [R]\n");
    printf("               - Exit       : [ESCAPE] or Close Window\n\n");

    HDC hdc = GetDC(hwnd);

    // 8. 60 FPS Game Loop
    BOOL running = TRUE;
    while (running) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = FALSE;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (!running) break;

        // Poll keyboard input
        Input_Poll(&g_Input);

        if (g_Input.Exit) {
            running = FALSE;
            break;
        }

        // Run game logic (physics, AI, collisions)
        Game_Update(&g_Game, &g_Input);

        // Render dynamic telemetry & battle arena
        RenderDynamicFrame(&g_Fb, &g_Input, (ULONG)g_Game.PlayerX);
        Game_RenderArena(&g_Fb, &g_Game, &g_Input);

        // Blit to screen
        SetDIBitsToDevice(
            hdc,
            0, 0,
            g_Fb.Width, g_Fb.Height,
            0, 0,
            0, g_Fb.Height,
            g_Fb.VirtualBase,
            &g_Bmi,
            DIB_RGB_COLORS
        );

        // Cap to ~60 FPS
        Sleep(16);
    }

    ReleaseDC(hwnd, hdc);

    // Save final frame
    SaveBitmapFile("playable_bsod_preview.bmp", &g_Fb);

    free(g_Fb.VirtualBase);
    printf("[PlayableBSOD] Boss battle preview closed cleanly.\n");
    return 0;
}
