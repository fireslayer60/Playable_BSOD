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

#include "driver.h"
#include "rasterizer.h"
#include "input.h"

// ============================================================================
// Game Constants & Configuration
// ============================================================================
#define MAX_PLAYER_LASERS   12
#define MAX_BOSS_PLASMAS    24

#define ARENA_MIN_X         90
#define ARENA_MAX_X         890
#define ARENA_MIN_Y         370
#define ARENA_MAX_Y         620

#define PLAYER_START_X      494
#define PLAYER_START_Y      560
#define PLAYER_WIDTH        32
#define PLAYER_HEIGHT       32

#define BOSS_START_X        474
#define BOSS_START_Y        390
#define BOSS_WIDTH          72
#define BOSS_HEIGHT         72

typedef enum _GAME_STATUS {
    GAME_STATUS_PLAYING = 0,
    GAME_STATUS_VICTORY = 1,
    GAME_STATUS_DEFEAT  = 2
} GAME_STATUS;

// Laser / Projectile descriptor
typedef struct _PROJECTILE {
    BOOLEAN Active;
    LONG    X;
    LONG    Y;
    LONG    Vx;
    LONG    Vy;
} PROJECTILE, *PPROJECTILE;

// Master Game State
typedef struct _GAME_STATE {
    GAME_STATUS Status;
    ULONG       Score;
    ULONG       FrameCount;
    ULONG       GameOverTimer; // Countdown ticks before system reboot

    // Player
    LONG        PlayerX;
    LONG        PlayerY;
    LONG        PlayerShields; // 0 to 100
    LONG        PlayerEnergy;  // 0 to 100
    ULONG       FireCooldown;

    // Boss
    LONG        BossX;
    LONG        BossY;
    LONG        BossVx;
    LONG        BossHealth;    // 0 to 100
    ULONG       BossPhase;     // 1 = Normal, 2 = Enraged
    ULONG       BossAttackCooldown;

    // Bullets
    PROJECTILE  PlayerLasers[MAX_PLAYER_LASERS];
    PROJECTILE  BossPlasmas[MAX_BOSS_PLASMAS];

    // High precision timing
    ULONG64     LastTsc;
} GAME_STATE, *PGAME_STATE;

// ============================================================================
// Hardware Reset via Intel 8042 Keyboard Controller
// ============================================================================
static __inline VOID Hardware_ResetSystem(void)
{
#if defined(_KERNEL_MODE)
    // Command 0xFE sent to Port 0x64 pulses the CPU reset line
    for (ULONG i = 0; i < 10000; i++) {
        if ((__inbyte(KBD_STATUS_PORT) & KBD_STATUS_IN_FULL) == 0) break;
    }
    __outbyte(KBD_STATUS_PORT, 0xFE);
    KeStallExecutionProcessor(50000);

    // Fallback for machines without an 8042 (e.g. Hyper-V Gen2, many modern
    // laptops): PCI Reset Control Register 0xCF9. Write SYS_RST first, then
    // SYS_RST|RST_CPU (0x06) to trigger a hard reset.
    __outbyte(0xCF9, 0x02);
    KeStallExecutionProcessor(10);
    __outbyte(0xCF9, 0x06);
    KeStallExecutionProcessor(50000);

    // Halt CPU if reset is delayed
    for (;;) {
        __halt();
    }
#endif
}

// ============================================================================
// Game Logic Functions
// ============================================================================

static __inline VOID Game_Init(_Out_ PGAME_STATE Game)
{
    if (!Game) return;

    Game->Status = GAME_STATUS_PLAYING;
    Game->Score = 0;
    Game->FrameCount = 0;
    Game->GameOverTimer = 0;

    Game->PlayerX = PLAYER_START_X;
    Game->PlayerY = PLAYER_START_Y;
    Game->PlayerShields = 100;
    Game->PlayerEnergy = 100;
    Game->FireCooldown = 0;

    Game->BossX = BOSS_START_X;
    Game->BossY = BOSS_START_Y;
    Game->BossVx = 3;
    Game->BossHealth = 100;
    Game->BossPhase = 1;
    Game->BossAttackCooldown = 45;

    for (ULONG i = 0; i < MAX_PLAYER_LASERS; i++) {
        Game->PlayerLasers[i].Active = FALSE;
    }
    for (ULONG i = 0; i < MAX_BOSS_PLASMAS; i++) {
        Game->BossPlasmas[i].Active = FALSE;
    }

#if defined(_KERNEL_MODE)
    Game->LastTsc = __rdtsc();
#else
    Game->LastTsc = 0;
#endif
}

// Spawn dual lasers from player ship
static __inline VOID Game_SpawnPlayerLaser(_Inout_ PGAME_STATE Game)
{
    // Find two free laser slots
    LONG spawned = 0;
    for (ULONG i = 0; i < MAX_PLAYER_LASERS && spawned < 2; i++) {
        if (!Game->PlayerLasers[i].Active) {
            Game->PlayerLasers[i].Active = TRUE;
            Game->PlayerLasers[i].X = (spawned == 0) ? (Game->PlayerX + 6) : (Game->PlayerX + 18);
            Game->PlayerLasers[i].Y = Game->PlayerY - 10;
            Game->PlayerLasers[i].Vx = 0;
            Game->PlayerLasers[i].Vy = -14;
            spawned++;
        }
    }
}

// Spawn boss attack plasma wave
static __inline VOID Game_SpawnBossAttack(_Inout_ PGAME_STATE Game)
{
    if (Game->BossPhase == 1) {
        // Phase 1: Dual downward plasma orbs
        LONG spawned = 0;
        for (ULONG i = 0; i < MAX_BOSS_PLASMAS && spawned < 2; i++) {
            if (!Game->BossPlasmas[i].Active) {
                Game->BossPlasmas[i].Active = TRUE;
                Game->BossPlasmas[i].X = (spawned == 0) ? (Game->BossX + 12) : (Game->BossX + 44);
                Game->BossPlasmas[i].Y = Game->BossY + 60;
                Game->BossPlasmas[i].Vx = 0;
                Game->BossPlasmas[i].Vy = 5;
                spawned++;
            }
        }
    } else {
        // Phase 2 (Enraged): 3-Way Angled Spread Attack!
        LONG angles[3] = { -2, 0, 2 };
        LONG spawned = 0;
        for (ULONG i = 0; i < MAX_BOSS_PLASMAS && spawned < 3; i++) {
            if (!Game->BossPlasmas[i].Active) {
                Game->BossPlasmas[i].Active = TRUE;
                Game->BossPlasmas[i].X = Game->BossX + 28;
                Game->BossPlasmas[i].Y = Game->BossY + 60;
                Game->BossPlasmas[i].Vx = angles[spawned];
                Game->BossPlasmas[i].Vy = 6;
                spawned++;
            }
        }
    }
}

// Update game physics and AI for one frame
static __inline VOID Game_Update(
    _Inout_ PGAME_STATE Game,
    _In_    PINPUT_STATE Input
)
{
    if (!Game || !Input) return;

    Game->FrameCount++;

    // 1. If Game Over (Victory or Defeat), tick countdown
    if (Game->Status != GAME_STATUS_PLAYING) {
        if (Game->GameOverTimer > 0) {
            Game->GameOverTimer--;
            if (Game->GameOverTimer == 0) {
                Hardware_ResetSystem();
            }
        }
        return;
    }

    // 2. Player Movement
    if (Input->Left && Game->PlayerX > ARENA_MIN_X) {
        Game->PlayerX -= 6;
    }
    if (Input->Right && Game->PlayerX < (ARENA_MAX_X - PLAYER_WIDTH)) {
        Game->PlayerX += 6;
    }

    // 3. Player Firing & Cooldown
    if (Input->Fire && Game->FireCooldown == 0) {
        Game_SpawnPlayerLaser(Game);
        Game->FireCooldown = 7; // ~8.5 shots per second
    }
    if (Game->FireCooldown > 0) {
        Game->FireCooldown--;
    }

    // 4. Update Player Lasers & Collision with Boss
    for (ULONG i = 0; i < MAX_PLAYER_LASERS; i++) {
        if (Game->PlayerLasers[i].Active) {
            Game->PlayerLasers[i].Y += Game->PlayerLasers[i].Vy;

            // Screen boundary check
            if (Game->PlayerLasers[i].Y < ARENA_MIN_Y) {
                Game->PlayerLasers[i].Active = FALSE;
                continue;
            }

            // Hit test with Boss hitbox (72x72)
            LONG lx = Game->PlayerLasers[i].X;
            LONG ly = Game->PlayerLasers[i].Y;
            if (lx >= (Game->BossX + 4) && lx <= (Game->BossX + BOSS_WIDTH - 4) &&
                ly >= Game->BossY && ly <= (Game->BossY + BOSS_HEIGHT - 8)) {
                
                Game->PlayerLasers[i].Active = FALSE;
                Game->Score += 100;
                
                // Damage boss
                if (Game->BossHealth > 2) {
                    Game->BossHealth -= 2;
                } else {
                    Game->BossHealth = 0;
                    Game->Status = GAME_STATUS_VICTORY;
                    Game->Score += 5000;
                    Game->GameOverTimer = 240; // ~4 seconds before hardware reset
                }

                // Check for Phase 2 Enrage Transition at 50% HP
                if (Game->BossHealth <= 50 && Game->BossPhase == 1) {
                    Game->BossPhase = 2;
                    Game->BossVx = (Game->BossVx > 0) ? 5 : -5; // Speed boost
                }
            }
        }
    }

    // 5. Boss AI Movement Patrol
    Game->BossX += Game->BossVx;
    if (Game->BossX <= (ARENA_MIN_X + 20)) {
        Game->BossX = ARENA_MIN_X + 20;
        Game->BossVx = -Game->BossVx;
    } else if (Game->BossX >= (ARENA_MAX_X - BOSS_WIDTH - 20)) {
        Game->BossX = ARENA_MAX_X - BOSS_WIDTH - 20;
        Game->BossVx = -Game->BossVx;
    }

    // 6. Boss Attack Waves
    if (Game->BossAttackCooldown > 0) {
        Game->BossAttackCooldown--;
    } else {
        Game_SpawnBossAttack(Game);
        Game->BossAttackCooldown = (Game->BossPhase == 1) ? 42 : 24;
    }

    // 7. Update Boss Plasmas & Collision with Player
    for (ULONG i = 0; i < MAX_BOSS_PLASMAS; i++) {
        if (Game->BossPlasmas[i].Active) {
            Game->BossPlasmas[i].X += Game->BossPlasmas[i].Vx;
            Game->BossPlasmas[i].Y += Game->BossPlasmas[i].Vy;

            // Boundary check
            if (Game->BossPlasmas[i].Y > ARENA_MAX_Y ||
                Game->BossPlasmas[i].X < ARENA_MIN_X ||
                Game->BossPlasmas[i].X > ARENA_MAX_X) {
                Game->BossPlasmas[i].Active = FALSE;
                continue;
            }

            // Hit test with Player hitbox (32x32)
            LONG px = Game->BossPlasmas[i].X;
            LONG py = Game->BossPlasmas[i].Y;
            if (px + 12 >= Game->PlayerX && px <= (Game->PlayerX + PLAYER_WIDTH) &&
                py + 12 >= Game->PlayerY && py <= (Game->PlayerY + PLAYER_HEIGHT)) {
                
                Game->BossPlasmas[i].Active = FALSE;
                
                // Damage player shields
                if (Game->PlayerShields > 10) {
                    Game->PlayerShields -= 10;
                } else {
                    Game->PlayerShields = 0;
                    Game->Status = GAME_STATUS_DEFEAT;
                    Game->GameOverTimer = 240; // ~4 seconds before hardware reset
                }
            }
        }
    }
}

// Render dynamic battle arena, projectiles, and status overlays
static __inline VOID Game_RenderArena(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ PGAME_STATE         Game,
    _In_ PINPUT_STATE        Input
)
{
    if (!Fb || !Fb->VirtualBase || !Game) return;

    // 1. Clear battle arena
    Rasterizer_FillRect(Fb, 81, 366, 858, 273, COLOR_BLACK);

    // 2. Boss HUD Bar
    if (Game->BossPhase == 1) {
        Rasterizer_DrawString(Fb, 100, 375, "TARGET: BUGCHECK OVERLORD [NORMAL PHASE]", COLOR_YELLOW, COLOR_TRANSPARENT);
    } else {
        Rasterizer_DrawString(Fb, 100, 375, "TARGET: BUGCHECK OVERLORD [ENRAGED PHASE!]", COLOR_RED, COLOR_TRANSPARENT);
    }

    Rasterizer_DrawString(Fb, 640, 375, "INTEGRITY: ", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 730, 375, Game->BossHealth, (Game->BossHealth > 50) ? COLOR_GREEN : COLOR_RED, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 760, 375, "%", COLOR_WHITE, COLOR_TRANSPARENT);

    ULONG bossBarColor = (Game->BossHealth > 50) ? COLOR_GOLD : COLOR_RED;
    Rasterizer_DrawProgressBar(Fb, 100, 390, 820, 12, (ULONG)Game->BossHealth, 100, bossBarColor, COLOR_DARK_RED, COLOR_WHITE);

    // 3. Boss Sprite
    if (Game->Status != GAME_STATUS_VICTORY || (Game->FrameCount % 8 < 4)) {
        Rasterizer_DrawSprite(Fb, (ULONG)Game->BossX, (ULONG)Game->BossY + 20, 24, 24, g_SpriteBoss24x24, 3);
    }

    // 4. Render Active Boss Plasma Orbs
    for (ULONG i = 0; i < MAX_BOSS_PLASMAS; i++) {
        if (Game->BossPlasmas[i].Active) {
            Rasterizer_DrawSprite(Fb, (ULONG)Game->BossPlasmas[i].X, (ULONG)Game->BossPlasmas[i].Y, 8, 8, g_SpriteBossPlasma8x8, 2);
        }
    }

    // 5. Render Active Player Lasers
    for (ULONG i = 0; i < MAX_PLAYER_LASERS; i++) {
        if (Game->PlayerLasers[i].Active) {
            Rasterizer_DrawSprite(Fb, (ULONG)Game->PlayerLasers[i].X, (ULONG)Game->PlayerLasers[i].Y, 4, 8, g_SpritePlayerLaser4x8, 2);
            Rasterizer_DrawLine(Fb, Game->PlayerLasers[i].X + 3, Game->PlayerLasers[i].Y + 16,
                                     Game->PlayerLasers[i].X + 3, Game->PlayerLasers[i].Y + 28, COLOR_BLUE);
        }
    }

    // 6. Player Ship
    if (Game->Status != GAME_STATUS_DEFEAT || (Game->FrameCount % 8 < 4)) {
        Rasterizer_DrawSprite(Fb, (ULONG)Game->PlayerX, (ULONG)Game->PlayerY, 16, 16, g_SpritePlayerShip16x16, 2);
    }

    // 7. Player HUD Footer (Shields & Score)
    Rasterizer_DrawBitmap16x16(Fb, 100, 608, g_BitmapShield16x16, 1, COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 122, 612, "SHIELDS: ", COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 185, 612, Game->PlayerShields, COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawString(Fb, 210, 612, "%", COLOR_CYAN, COLOR_TRANSPARENT);
    Rasterizer_DrawProgressBar(Fb, 230, 612, 240, 10, (ULONG)Game->PlayerShields, 100, COLOR_CYAN, COLOR_DARK_CYAN, COLOR_WHITE);

    Rasterizer_DrawString(Fb, 560, 612, "SCORE: ", COLOR_YELLOW, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 615, 612, (LONG)Game->Score, COLOR_YELLOW, COLOR_TRANSPARENT);

    Rasterizer_DrawString(Fb, 730, 612, "PHASE: ", COLOR_WHITE, COLOR_TRANSPARENT);
    Rasterizer_DrawInt(Fb, 785, 612, (LONG)Game->BossPhase, (Game->BossPhase == 1) ? COLOR_GREEN : COLOR_RED, COLOR_TRANSPARENT);

    // ========================================================================
    // 8. VICTORY / DEFEAT RETRO OVERLAY BANNERS
    // ========================================================================
    if (Game->Status == GAME_STATUS_VICTORY) {
        Rasterizer_FillRect(Fb, 150, 440, 720, 100, COLOR_BLACK);
        Rasterizer_DrawRect(Fb, 146, 436, 728, 108, 3, COLOR_GOLD);
        Rasterizer_DrawRect(Fb, 150, 440, 720, 100, 1, COLOR_GREEN);

        Rasterizer_DrawStringScaled(Fb, 175, 450, "*** SYSTEM RESTORED: BUGCHECK DEFEATED! ***", 1, COLOR_GREEN, COLOR_TRANSPARENT);
        Rasterizer_DrawStringScaled(Fb, 210, 470, "THE KERNEL HAS RECOVERED FROM UNRECOVERABLE STATE", 1, COLOR_WHITE, COLOR_TRANSPARENT);

        Rasterizer_DrawString(Fb, 260, 500, "SYSTEM HARDWARE REBOOT IN: ", COLOR_YELLOW, COLOR_TRANSPARENT);
        Rasterizer_DrawInt(Fb, 480, 500, (LONG)(Game->GameOverTimer / 60) + 1, COLOR_YELLOW, COLOR_TRANSPARENT);
        Rasterizer_DrawString(Fb, 500, 500, "SECONDS (PORT 0x64 RESET)", COLOR_YELLOW, COLOR_TRANSPARENT);
    } else if (Game->Status == GAME_STATUS_DEFEAT) {
        Rasterizer_FillRect(Fb, 150, 440, 720, 100, COLOR_BLACK);
        Rasterizer_DrawRect(Fb, 146, 436, 728, 108, 3, COLOR_RED);
        Rasterizer_DrawRect(Fb, 150, 440, 720, 100, 1, COLOR_DARK_RED);

        Rasterizer_DrawStringScaled(Fb, 190, 450, "*** CRITICAL FAILURE: KERNEL PANIC FATAL ***", 1, COLOR_RED, COLOR_TRANSPARENT);
        Rasterizer_DrawStringScaled(Fb, 230, 470, "SHIELDS DEPLETED. SYSTEM CANNOT RESUME EXECUTION", 1, COLOR_WHITE, COLOR_TRANSPARENT);

        Rasterizer_DrawString(Fb, 260, 500, "HARDWARE RESET IN: ", COLOR_YELLOW, COLOR_TRANSPARENT);
        Rasterizer_DrawInt(Fb, 410, 500, (LONG)(Game->GameOverTimer / 60) + 1, COLOR_YELLOW, COLOR_TRANSPARENT);
        Rasterizer_DrawString(Fb, 430, 500, "SECONDS (PRESS ESC TO ABORT)", COLOR_YELLOW, COLOR_TRANSPARENT);
    }
}
