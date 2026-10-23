/* F12 experimental native Shell_MainMenu Quit activation.
 * Verified against SpideyPC.exe SHA-256:
 * 0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
 *
 * Prior fixes hooked 0x004A1A26 / 0x004A202F, part of a secondary menu
 * path that NEVER runs in the idle title menu (user Windows logs #464/#468).
 * The REAL Shell_MainMenu at 0x00493990 has a per-frame tick CALL at
 * 0x00493DD6 -> 0x00505720. Its normal item-activation branch begins
 * at 0x004941D5 and reads the selected slot at [ESP+0x40].
 *
 * Original native menu table at 0x00552AA8:
 *   index 3 -> text "quit" -> action 0x12 (18).
 *
 * On F12, use a short-lived one-shot request. In the native Shell_MainMenu
 * game-thread frame CALL wrapper, finish the ORIGINAL tick, set the normal
 * selected item local to 3, and return to the game's own selection/cleanup
 * branch 0x004941D5. This is identical in effect to selecting "quit" on
 * the real menu. NEVER post WM_CLOSE or call native resource destructors.
 *
 * Opening cinematics now use the game's two native Bink frame-tick
 * CALL sites. An opt-in F12 request makes those ticks return "finished",
 * so the ORIGINAL 0x470871 BinkClose path runs in the video-owning
 * game thread. All four sequential movie calls take this same path.
 * After the four movies, the original transition cleanup is called
 * on the game thread, and ONLY THEN the pending intro Quit flag is
 * forwarded to the native engine's top-level Quit check at 0x455E50.
 * The real interactive menu F12 path is unchanged. No WM_CLOSE, no
 * process kill, no direct BinkClose.
 *
 * Comic screens remain unsupported. An additional strictly opt-in 3D
 * proof-of-concept waits for native 3D owner cleanup and its top-level
 * request-7 post-cleanup call BEFORE choosing the original engine's
 * final shutdown branch at 0x45620c. No off-thread game mutation.
 * Exact code bytes MUST match before any hook is installed.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "quick_exit_f12.h"

#define SPIDER_NATIVE_F12_POST ((BYTE *)(uintptr_t)0x00515373u)
#define SPIDER_SHELL_MENU_TICK ((BYTE *)(uintptr_t)0x00493DD6u)
/* SHA-verified original Shell_MainMenu frame sync, AFTER input polling.
 * At 0x49415B (conditionally) and 0x49416F (unconditionally),
 * the engine calls 0x4E5D60(1), which busy-waits for a native clock tick.
 * Read-only duration wrappers are enabled in debug F12+Log mode only. */
#define SPIDER_MENU_WAIT_COND_SITE ((BYTE *)(uintptr_t)0x0049415Bu)
#define SPIDER_MENU_WAIT_ALWAYS_SITE ((BYTE *)(uintptr_t)0x0049416Fu)
/* Exact ORIGINAL Shell_MainMenu selection/cleanup path. Before the
 * native menu returns a selected action (including New Game), it
 * calls the SAME one-tick busy-wait at 0x494209. The first-menu
 * native menu selection wait is independently measured at this callsite.
 * Instrument it READ-ONLY to test a newly reported 3–4s stall
 * on selecting New Game, before the subsequent submenu appears. */
#define SPIDER_MENU_WAIT_EXIT_SITE ((BYTE *)(uintptr_t)0x00494209u)
#define SPIDER_NATIVE_CLOCK_WAIT ((void (__cdecl *)(DWORD))(uintptr_t)0x004E5D60u)
#define SPIDER_SHELL_MENU_QUIT_RETURN ((const BYTE *)(uintptr_t)0x004941D5u)
#define SPIDER_SHELL_MENU_ITEMS ((const BYTE *)(uintptr_t)0x00552AA8u)
#define SPIDER_NATIVE_MENU_TICK ((void (__cdecl *)(void))(uintptr_t)0x00505720u)
#define SPIDER_INTRO_TICK_CALL_1 ((BYTE *)(uintptr_t)0x0047081Du)
#define SPIDER_INTRO_TICK_CALL_2 ((BYTE *)(uintptr_t)0x0047085Fu)
#define SPIDER_NATIVE_BINK_FRAME_TICK ((BYTE (__cdecl *)(void))(uintptr_t)0x0050B5A0u)
#define SPIDER_GAME_BINK_ACTIVE ((const volatile DWORD *)(uintptr_t)0x006151F8u)
/* Game-owned Bink pointer slots (already observed by focus_media_pause.c).
 * Observe pointer PRESENCE only; do not dereference handles, call Bink
 * functions, or equate a non-null handle with active movie playback. */
#define SPIDER_BINK_VIDEO_HANDLE ((const volatile DWORD *)(uintptr_t)0x00AC0BA4u)
#define SPIDER_BINK_AUDIO_HANDLE ((const volatile DWORD *)(uintptr_t)0x00AC1020u)
/* The original top-level 0x455C90 startup calls 0x470750 four
 * times, then 0x443AD0 at 0x455D23 for intro transition cleanup.
 * The original Quit-byte test at 0x455E50 is downstream BUT NOT
 * unconditional: 0x455DC8 / 0x455DD0 branches can bypass it.
 * Preserve the original Quit byte and, with the separately opted-in
 * intro-still hook, carry the SAME F12 intent through the native
 * timed/input-wait pages to real Shell_MainMenu Quit. Do not send
 * the request to an early Shell_MainMenu splash callback. */
#define SPIDER_AFTER_INTRO_CLEANUP_CALL ((BYTE *)(uintptr_t)0x00455D23u)
#define SPIDER_AFTER_INTRO_CLEANUP ((void (__cdecl *)(int))(uintptr_t)0x00443AD0u)
#define SPIDER_GAME_QUIT_FLAG ((volatile BYTE *)(uintptr_t)0x0060CF88u)
#define SPIDER_TOPLEVEL_NATIVE_QUIT_TEST ((const BYTE *)(uintptr_t)0x00455E50u)
/* SHA-verified game menu-dispatch values for PASSIVE startup tracing:
 * 0x493996 reads native menu context presence at 0x682948.
 * 0x4A1FC3 reads a byte at 0x6828D0 before dispatching a menu.
 * 0x4A2012 reads original raw dispatcher state 0x68293C.
 * 0x4A1FEC reads transition state 0x54D38C.
 * NEVER dereference pointers, infer static-screen ownership from a
 * single value, or write any of these game-controlled addresses. */
#define SPIDER_TRACE_MENU_CONTEXT ((const volatile DWORD *)(uintptr_t)0x00682948u)
#define SPIDER_TRACE_MENU_STAGE_BYTE ((const volatile BYTE *)(uintptr_t)0x006828D0u)
#define SPIDER_TRACE_DISPATCH_RAW ((const volatile DWORD *)(uintptr_t)0x0068293Cu)
#define SPIDER_TRACE_TRANSITION_RAW ((const volatile DWORD *)(uintptr_t)0x0054D38Cu)
/* SHA-matched original 0x4A1930 (timed loop) and 0x4A17A0
 * (input wait) both increment DWORD 0x60CFA8 on native frame ticks.
 * 0x4A1953 sets byte 0x6A7785 on first entered timed page;
 * 0x4A19D6 and 0x4A18CB read the game clock at 0x6B4CA0.
 * These are PASSIVE raw scalar samples, not stable ownership flags. */
#define SPIDER_TRACE_STILL_FRAME_COUNT ((const volatile DWORD *)(uintptr_t)0x0060CFA8u)
#define SPIDER_TRACE_TIMED_PAGE_ONCE ((const volatile BYTE *)(uintptr_t)0x006A7785u)
#define SPIDER_TRACE_NATIVE_CLOCK_RAW ((const volatile DWORD *)(uintptr_t)0x006B4CA0u)
/* SHA-verified native first-still timer loop at 0x4A1930:
 * 0x4A19D6 reads clock and sets EDI=clock+0xB4.
 * 0x4A1A26 MOV EAX,[clock] (five bytes), then
 * 0x4A1A2B CMP EAX,EDI / 0x4A1A2D JA native cleanup 0x4A1A36.
 * Replace ONLY this second clock read with a CALL in Debug F12TestMode.
 * Never modify the shared native clock, duration, page objects or SEH. */
#define SPIDER_INTRO_TIMED_CLOCK_SITE ((BYTE *)(uintptr_t)0x004A1A26u)
/* Exact original L1COV.bmp New Game comic on SHA-matched SpideyPC.exe:
 * 0x42796D pushes "L1COV.bmp" and invokes game-owned image preparation.
 * 0x427983 loads ESI=initial game clock; 0x4279B8 MOV EDX,[0x6B4CA0]
 * (6 bytes) followed by SUB EDX,ESI / CMP EDX,0x12C.
 * The original JAE at 0x4279C6 leads to 0x4279F0, pushing Bink ID 4
 * after the complete comic loop returns via its normal branch.
 * Hook ONLY the clock READ inside that one specific original loop:
 * supply a virtual elapsed 300-tick delta in EDX after user F12.
 * NEVER change shared clock, script interpreter, object pointers,
 * or native opcodes. Other game covers retain original timers. */
#define SPIDER_NEWGAME_COMIC_CLOCK_SITE ((BYTE *)(uintptr_t)0x004279B8u)
#define SPIDER_NEWGAME_L1COV_OWNER ((const BYTE *)(uintptr_t)0x0042796Du)
#define SPIDER_NEWGAME_MOVIE4_OPCODE ((const BYTE *)(uintptr_t)0x004279F9u)
/* Original 0x4A17A0 input-wait page CALL: 0x4A1896: call 0x50C180.
 * Original arguments: (0x40010,1,1), expected return VA 0x4A189B.
 * Its regular true-result branch at 0x4A18AE performs all original
 * page-specific cleanup and restores SEH before calling real menu. */
#define SPIDER_INTRO_STILL_INPUT_CALL ((BYTE *)(uintptr_t)0x004A1896u)
#define SPIDER_INTRO_STILL_NATIVE_INPUT ((BYTE (__cdecl *)(DWORD,DWORD,DWORD))(uintptr_t)0x0050C180u)
/* Top-level dispatch state is observed, never written, from the already
 * installed IDirectDraw7::Flip hook. These are fixed BSS addresses of the
 * SHA-gated 2001 executable; mode labels require runtime correlation. */
#define SPIDER_ENGINE_MODE ((const volatile DWORD *)(uintptr_t)0x0060CFA4u)
#define SPIDER_ENGINE_ASYNC_STATE ((const volatile DWORD *)(uintptr_t)0x006B4C9Cu)
#define SPIDER_ENGINE_TASK ((const volatile DWORD *)(uintptr_t)0x0054A510u)
/* Native per-frame 0x455400 calls 0x47bc40 at 0x455549 when the
 * gameplay pause context at 0x682950 is nonzero. Within 0x47bc40 the
 * pause option object at 0x682968 holds the real user selection:
 *   0 retry; 1 quit to training; 2 quit to main menu.
 * Observe ONLY pointer presence from the existing Flip hook.
 * Never dereference or mutate these game-owned COM/menu objects. */
#define SPIDER_PAUSE_CONTEXT ((const volatile DWORD *)(uintptr_t)0x00682950u)
#define SPIDER_PAUSE_OPTION_MENU ((const volatile DWORD *)(uintptr_t)0x00682968u)
#define SPIDER_PAUSE_RESTART_MENU ((const volatile DWORD *)(uintptr_t)0x00682960u)
/* Original gameplay update CALL at 0x455549 (the only control-flow gate
 * to the game-owned pause option handler 0x47BC40). Its 5-byte encoding
 * is checked before a PASSIVE wrapper is installed. No synthetic menu
 * item selection, no input injection, no native cleanup from our thread. */
#define SPIDER_NATIVE_PAUSE_CALL ((BYTE *)(uintptr_t)0x00455549u)
#define SPIDER_NATIVE_PAUSE_HANDLER ((void (__cdecl *)(void))(uintptr_t)0x0047BC40u)
/* Directly preceding native input/game tick at 0x45553B:
 *  E8 B0 B9 FE FF -> 0x440EF0.
 *  The called function contains a distinct native writer of request 7
 *  (0x44108A), guarded by two engine flags and a >0x78 timer delta.
 *  Intercept the CALL only to determine which 3D owner writes request 7.
 *  It remains unmodified and executed exactly once on the game thread. */
#define SPIDER_NATIVE_GAME_INPUT_CALL ((BYTE *)(uintptr_t)0x0045553Bu)
#define SPIDER_NATIVE_GAME_INPUT_UPDATE ((void (__cdecl *)(void))(uintptr_t)0x00440EF0u)
/* SHA-matched original 3D input calls inside native 0x440EF0.
 * These belong to the gameplay AND 3D scripted intro update, so none
 * of them alone can identify Stan Lee, the second 3D cinematic or a
 * playable level. Interpose only READ-ONLY after 3D New Game entry.
 * All five originally CALL 0x50C180 with (mask,1,1). */
/* Unlike 0x50C180 masks 0x40040 / 0x10010, the original x86
 * Space scancode is DIK_SPACE 0x39. The game calls 0x50A650(0x39,0)
 * twice INSIDE the early 0x455400 update, guarded by native bit 0x100
 * in 0x02E0988C. The two CALLs form a game-owned release/press gate,
 * not proof of either cinematic's identity. Capture PASSIVELY first. */
#define SPIDER_NATIVE_3D_SPACE_CALL_1 ((BYTE *)(uintptr_t)0x00455427u)
#define SPIDER_NATIVE_3D_SPACE_CALL_2 ((BYTE *)(uintptr_t)0x00455444u)
#define SPIDER_NATIVE_3D_SPACE_POLL ((DWORD (__cdecl *)(DWORD,DWORD))(uintptr_t)0x0050A650u)
#define SPIDER_NATIVE_CUTSCENE_INPUT_FLAGS ((const volatile DWORD *)(uintptr_t)0x02E0988Cu)
/* The 0x455427/0x455444 Space gate is *not used* during either
 * intro in Wine run 20261023-151958: native bit 0x100 remained
 * clear throughout. Probe the CENTRAL original 0x50A650 keyboard
 * helper instead, at its ONLY two 0x501CB0 READ CALL sites:
 * 0x50A65D (event/edge mode=1) and 0x50A674 (level mode=0).
 * The caller address remains on the original unmodified x86 stack.
 * These are READ-ONLY wrappers, not a synthetic keyboard shortcut. */
#define SPIDER_NATIVE_KEY_READ ((DWORD (__cdecl *)(DWORD))(uintptr_t)0x00501CB0u)
/* Original buffered DirectInput key-state storage (x86 SHA-matched):
 * 0x501CB0 reads 0x6B792C+(DIK & 0xff); DIK_SPACE=0x39.
 * 0x501C87 writes 0xFF on keydown; 0x501C8F 0x80 on keyup.
 * DO NOT WRITE THIS GAME-OWNED KEYBOARD BUFFER. */
#define SPIDER_NATIVE_RAW_SPACE_BYTE ((const volatile BYTE *)(uintptr_t)0x006B7965u)
/* Original 0x505720 at 0x5058A5 passes action bit 0x10 to
 * 0x479540 at state struct 0x661200. 0x479540 independently
 * tracks held, just-pressed and early-press states plus counters.
 * This is the game-owned ACTION state, NOT a proven cutscene flag.
 * Passive telemetry samples it AFTER original 0x440EF0 returns. */
#define SPIDER_NATIVE_SPACE_ACTION_STATE ((const volatile BYTE *)(uintptr_t)0x00661200u)
#define SPIDER_NATIVE_SPACE_ACTION_WORDS ((const volatile DWORD *)(uintptr_t)0x00661200u)
/* Additional original game-owned *candidate* scene context:
 * 0x455582 reads a controller pointer at 0x56F3B8 and, if nonnull,
 * compares its DWORD at +0x2A0 to 0xD. 0x4DE890/0x4DE840
 * manage a native dispatch-linked list head at 0x6B4708.
 * Neither is PROVEN to indicate gameplay readiness. Observe only. */
#define SPIDER_NATIVE_3D_CONTROLLER_OBJECT ((const volatile DWORD *)(uintptr_t)0x0056F3B8u)
#define SPIDER_NATIVE_3D_DISPATCH_HEAD ((const volatile DWORD *)(uintptr_t)0x006B4708u)
#define SPIDER_NATIVE_KEYBOARD_DEVICE ((const volatile DWORD *)(uintptr_t)0x006B7A5Cu)
#define SPIDER_NATIVE_KEY_EDGE_CALL ((BYTE *)(uintptr_t)0x0050A65Du)
#define SPIDER_NATIVE_KEY_LEVEL_CALL ((BYTE *)(uintptr_t)0x0050A674u)
#define SPIDER_NATIVE_3D_INPUT_SITES_COUNT 5
#define SPIDER_NATIVE_3D_INPUT ((BYTE (__cdecl *)(DWORD,DWORD,DWORD))(uintptr_t)0x0050C180u)
static BYTE *const spider_native_3d_input_sites[SPIDER_NATIVE_3D_INPUT_SITES_COUNT]={
  (BYTE *)(uintptr_t)0x00440F34u,
  (BYTE *)(uintptr_t)0x00440F77u,
  (BYTE *)(uintptr_t)0x00440F8Eu,
  (BYTE *)(uintptr_t)0x00440FAFu,
  (BYTE *)(uintptr_t)0x00441606u
};
#define SPIDER_NATIVE_GAME_INPUT_TIMER_ACTIVE ((const volatile DWORD *)(uintptr_t)0x005FAED8u)
#define SPIDER_NATIVE_GAME_INPUT_FLAG_1 ((const volatile BYTE *)(uintptr_t)0x006611F0u)
#define SPIDER_NATIVE_GAME_INPUT_FLAG_2 ((const volatile BYTE *)(uintptr_t)0x006611E0u)
/* Native 3D loop at 0x455A8B calls 0x455400 each frame, and the
 * immediately following 0x455A90 CMP mode_request,0 branches to
 * 0x455BF3 for owner-owned cleanup. At 0x455BF9 it calls 0x4302D0.
 * Wrap THAT existing cleanup CALL only to confirm native owner exit.
 * The original function executes once, on the original game thread. */
#define SPIDER_NATIVE_GAME_TICK_COUNTER ((const volatile DWORD *)(uintptr_t)0x006B4CA0u)
#define SPIDER_NATIVE_GAME_INPUT_TIMER_START ((const volatile DWORD *)(uintptr_t)0x005FAEDCu)
#define SPIDER_NATIVE_GAME_CLEANUP_CALL ((BYTE *)(uintptr_t)0x00455BF9u)
#define SPIDER_NATIVE_GAME_CLEANUP ((void (__cdecl *)(void))(uintptr_t)0x004302D0u)
/* At top-level native request-7 dispatch 0x456185, AFTER owner-specific
 * cleanup, the controller calls original 0x48A820 at 0x456198 then
 * checks 0x2E0988C bit 2 and signed 0x568FB0. Neither is a universal
 * Quit flag. This probe simply records which of those original
 * conditions will return to startup loop or finish program cleanup. */
#define SPIDER_NATIVE_CONTROLLER7_FINISH_CALL ((BYTE *)(uintptr_t)0x00456198u)
#define SPIDER_NATIVE_CONTROLLER7_FINISH ((void (__cdecl *)(void))(uintptr_t)0x0048A820u)
#define SPIDER_NATIVE_CONTROLLER7_EXIT_FLAGS ((const volatile BYTE *)(uintptr_t)0x02E0988Cu)
#define SPIDER_NATIVE_CONTROLLER7_EXIT_STATUS ((const volatile LONG *)(uintptr_t)0x00568FB0u)

static const BYTE native_f12_original[6]={0xFF,0x15,0x70,0xB1,0x53,0x00};
static const BYTE native_f12_bypass[6]={0x83,0xC4,0x10,0x90,0x90,0x90};
static const BYTE shell_menu_tick_original[5]={0xE8,0x45,0x19,0x07,0x00};
static const BYTE shell_menu_quit_return_signature[5]={0x6A,0x00,0x68,0x00,0x20};
static const BYTE intro_tick_call_original_1[5]={0xE8,0x7E,0xAD,0x09,0x00};
static const BYTE intro_tick_call_original_2[5]={0xE8,0x3C,0xAD,0x09,0x00};
static const BYTE post_intro_cleanup_original[5]={0xE8,0xA8,0xDD,0xFE,0xFF};
static const BYTE intro_still_input_original[5]={0xE8,0xE5,0xA8,0x06,0x00};
static const BYTE intro_timed_clock_original[5]={0xA1,0xA0,0x4C,0x6B,0x00};
/* SHA-verified New Game L1 cover clock read, cover owner, movie #4. */
static const BYTE newgame_comic_clock_original[6]={0x8B,0x15,0xA0,0x4C,0x6B,0x00};
static const BYTE newgame_l1cov_owner_signature[5]={0x68,0x14,0x92,0x54,0x00};
static const BYTE newgame_movie4_opcode_signature[7]={0x6A,0x04,0xE8,0x50,0x8D,0x04,0x00};
static const BYTE menu_wait_cond_original[5]={0xE8,0x00,0x1C,0x05,0x00};
static const BYTE menu_wait_always_original[5]={0xE8,0xEC,0x1B,0x05,0x00};
static const BYTE menu_wait_exit_original[5]={0xE8,0x52,0x1B,0x05,0x00};
static const BYTE top_level_native_quit_test_original[6]={0x38,0x1D,0x88,0xCF,0x60,0x00};
static const BYTE native_pause_call_original[5]={0xE8,0xF2,0x66,0x02,0x00};
static const BYTE native_game_input_call_original[5]={0xE8,0xB0,0xB9,0xFE,0xFF};
static const BYTE spider_native_3d_space_original_1[5]={0xE8,0x24,0x52,0x0B,0x00};
static const BYTE spider_native_3d_space_original_2[5]={0xE8,0x07,0x52,0x0B,0x00};
/* 0x50A650 -> 0x501CB0, in both exact original x86 branches. */
static const BYTE spider_native_key_edge_original[5]={0xE8,0x4E,0x76,0xFF,0xFF};
static const BYTE spider_native_key_level_original[5]={0xE8,0x37,0x76,0xFF,0xFF};
/* Exact original 2001 EXE CALL rel32 -> 0x50C180 at five 3D sites. */
static const BYTE spider_native_3d_input_original[SPIDER_NATIVE_3D_INPUT_SITES_COUNT][5]={
  {0xE8,0x47,0xB2,0x0C,0x00}, /* 0x440F34: 0x40040 */
  {0xE8,0x04,0xB2,0x0C,0x00}, /* 0x440F77: 0x40000/0x40040 */
  {0xE8,0xED,0xB1,0x0C,0x00}, /* 0x440F8E: 0x10010 */
  {0xE8,0xCC,0xB1,0x0C,0x00}, /* 0x440FAF: 0x20220 */
  {0xE8,0x75,0xAB,0x0C,0x00}  /* 0x441606: 0x100 */
};
static const BYTE native_game_cleanup_call_original[5]={0xE8,0xD2,0xA6,0xFD,0xFF};
static const BYTE native_controller7_finish_call_original[5]={0xE8,0x83,0x46,0x03,0x00};

static volatile LONG enabled=0;
static volatile LONG pending=0;
static volatile LONG delivered=0;
static volatile LONG menu_frame_count=0;
/* Read-only per-menu-tick timing: existing game-owned 0x493DD6 hook only. */
static volatile LONG debug_menu_tick_entry_ms=0;
static volatile LONG debug_menu_last_exit_ms=0;
static volatile LONG last_menu_frame_ms=0;
static volatile LONG last_movie_frame_ms=0;
/* Diagnostic-only timestamp for correlation with the auto-timed still. */
static volatile LONG last_comic_candidate_ms=0;
/* The request persists across the four native movie calls,
 * and is delivered to the top-level Quit path after transition cleanup. */
static volatile LONG intro_quit_pending=0;
static volatile LONG intro_tick_shortcircuited=0;
static volatile LONG intro_native_quit_forwarded=0;
/* Limit automatic movie->Quit handoff to the FOUR startup clips,
 * never to later Bink videos during comics or gameplay. */
static volatile LONG real_main_menu_seen=0;
static volatile LONG pause_native_invocations=0;
static volatile LONG pause_native_requested7=0;
static volatile LONG input_native_invocations=0;
static volatile LONG input_native_requested7=0;
static volatile LONG native_game_cleanup_count=0;
static volatile LONG native_controller7_count=0;
/* Strict opt-in: [System] ExperimentalQuickExitF12In3D=1. F12 watcher
 * only records intent. The *native game thread* performs the state
 * transitions at a game-input owner frame and after owner cleanup. */
static volatile LONG experimental_game3d_enabled=0;
/* Optional, strictly read-only parent return-address trace: never enabled
 * by regular F12 or normal Debug Log alone. One sample per process only. */
static volatile LONG parent_trace_enabled=0;
/* Extra opt-in, read-only snapshots while starting and entering menus.
 * No additional executable hooks, game state writes or F12 authority. */
static volatile LONG static_screen_trace_enabled=0;
static volatile LONG post_intro_cleanup_seen=0;
/* Separate experimental intro-still F12 route. 0 idle, 1 pending,
 * 2 native input-wait finished, 3 original Shell_MainMenu Quit. */
static volatile LONG intro_still_opt_in=0;
/* [Debug] F12TestMode=1: F12 pre-menu -> NORMAL main menu, in 3D ->
 * existing native controller-7 final exit. OFF by default. The older
 * F12 Quit behavior remains intact when this debug mode is OFF. */
static volatile LONG debug_f12_test_mode=0;
static volatile LONG intro_still_exit_state=0;
static volatile LONG intro_still_request_ms=0;
static volatile LONG intro_still_hook_installed=0;
/* Debug mode only. Clock read hook leaves all game-owned state untouched. */
static volatile LONG intro_timed_clock_hook_installed=0;
static volatile LONG debug_timed_accelerated=0;
static volatile LONG debug_menu_wait_hooks_installed=0;
static volatile LONG debug_menu_wait_exit_hook_installed=0;
/* New Game single-F12 experiment (F12TestMode only):
 * 0 unknown/unseen, 1 exact movie-4 owner observed, 2 movie-4 native
 * completion requested/observed, 3 game-thread post-video 3D reached.
 * State 3 is NOT proof that EITHER of the TWO 3D introduction scenes
 * has ended. In the original game SPACE skips Stan Lee's narrated
 * 3D introduction, and another SPACE skips the following 3D cinematic.
 * Both must end before gameplay F12 EXIT becomes permissible, but a
 * reliable native "playable" gate has NOT yet been mapped. */
static volatile LONG debug_newgame_progress=0;
static volatile LONG debug_newgame_3d_first_ms=0;
/* Native New Game L1COV timer hook. No new INI option.
 * 0/1 installed; fastforward_observed records that the original
 * 0x4279C6 branch was made eligible without modifying the clock. */
static volatile LONG debug_newgame_comic_clock_hook_installed=0;
static volatile LONG debug_newgame_comic_fastforward_observed=0;
/* Passive count of physical SPACE key-down edges after the original
 * movie-4 -> 3D owner transition. This NEVER unlocks gameplay F12 exit. */
static volatile LONG debug_newgame_3d_space_edges=0;
/* Optional exact CALL-site probes, always pass through original input.
 * Used for mapping SPACE-controlled scripted scene transitions. */
static volatile LONG debug_newgame_native_input_probes_installed=0;
static volatile LONG debug_newgame_space_gate_probes_installed=0;
static volatile LONG debug_newgame_lowlevel_key_probes_installed=0;
static volatile LONG debug_newgame_raw_key_reads=0;
static volatile LONG debug_newgame_raw_space_reads=0;
static volatile LONG last_native_game_input_ms=0;
static volatile LONG last_game3d_flip_ms=0;
/* 0 idle, 1 requested, 2 native request 7 dispatched, 3 final exit. */
static volatile LONG game3d_exit_state=0;
static volatile LONG game3d_cleanup_baseline=0;
static volatile LONG game3d_cleanup_confirmed=0;
/* Separate experimental New Game movie #4 F12 request, disabled normally.
 * 0 idle; 1 movie armed; 2 native Bink stop; 3 controller-7 queued;
 * 4 comic F12 deferred until this exact original movie owner begins.
 * The comic path does not touch its unknown native scene owner. */
static volatile LONG postmovie_opt_in=0;
static volatile LONG comic_exit_opt_in=0;
static volatile LONG postmovie_state=0;
static volatile LONG postmovie_request_ms=0;
static PVOID volatile game_hwnd=NULL;
static void (*logger)(const char *)=NULL;

static void say(const char *msg) {
  if(logger)logger(msg);
}

int SpiderQuickExitF12_Enabled(void) {
  return InterlockedCompareExchange(&enabled,0,0)==1;
}

void SpiderQuickExitF12_AttachWindow(HWND hwnd) {
  InterlockedExchangePointer(&game_hwnd,(PVOID)hwnd);
}

void SpiderQuickExitF12_ObserveMovieFrame(void) {
  static volatile LONG post_shell_media_frames=0;
  LONG n;
  DWORD now=GetTickCount();
  /* This callback denotes a successful recognized DirectDraw movie BLT,
   * not necessarily any Bink frame playing via another presentation path. */
  InterlockedExchange(&last_movie_frame_ms,(LONG)now);
  if(!SpiderQuickExitF12_Enabled() ||
     InterlockedCompareExchange(&real_main_menu_seen,0,0)!=1)return;
  n=InterlockedIncrement(&post_shell_media_frames);
  if(n<=4 || n%120==0) {
    DWORD comic_ms=(DWORD)InterlockedCompareExchange(&last_comic_candidate_ms,0,0);
    char line[250];
    snprintf(line,sizeof(line),
      "QUICK-EXIT-F12: POST-SHELL-MEDIA-BLT frame=%ld bink_flag=%lu video_handle_present=%lu native_3d_owner_calls=%ld ms_since_comic=%lu [PRESENTATION-ONLY]",
      (long)n,(unsigned long)(*SPIDER_GAME_BINK_ACTIVE!=0u),
      (unsigned long)(*SPIDER_BINK_VIDEO_HANDLE!=0u),
      (long)InterlockedCompareExchange(&input_native_invocations,0,0),
      (unsigned long)(comic_ms?(DWORD)(now-comic_ms):0u));
    say(line);
  }
}

/* Passive comic-quad observation from the ALREADY installed D3D7 draw
 * wrapper after the known native rendering signature is confirmed.
 * Graphics activity is NOT proof that the comic page's native owner is
 * active or that it is safe to leave the scene. This cannot initiate
 * F12, modify a game-global, inspect COM internals, or free a resource.
 * The classifier in the caller remains opt-in to ComicNativeAspect. */
void SpiderQuickExitF12_ObserveComicRender(LONG flip_index, DWORD caller_rva,
                                          DWORD texture_w, DWORD texture_h) {
  static volatile LONG candidate_draws=0;
  LONG n,owner_calls,last_owner,menu_seen;
  DWORD now,owner_age;
  char line[384];
  if(!SpiderQuickExitF12_Enabled()) return;
  n=InterlockedIncrement(&candidate_draws);
  if(n>8 && n%120!=0) return;
  now=GetTickCount();
  InterlockedExchange(&last_comic_candidate_ms,(LONG)now);
  owner_calls=InterlockedCompareExchange(&input_native_invocations,0,0);
  last_owner=InterlockedCompareExchange(&last_native_game_input_ms,0,0);
  owner_age=last_owner ? (DWORD)(now-(DWORD)last_owner) : 0u;
  menu_seen=InterlockedCompareExchange(&real_main_menu_seen,0,0);
  snprintf(line,sizeof(line),
    "QUICK-EXIT-F12: COMIC-RENDER-CANDIDATE draw=%ld flip=%ld caller_rva=0x%08lX texture=%lux%lu mode_request=%lu bink_active=%lu menu_seen=%ld native_3d_owner_calls=%ld native_3d_owner_age_ms=%lu [RENDER-ONLY; comic owner UNKNOWN]",
    (long)n,(long)flip_index,(unsigned long)caller_rva,
    (unsigned long)texture_w,(unsigned long)texture_h,
    (unsigned long)*SPIDER_ENGINE_MODE,(unsigned long)*SPIDER_GAME_BINK_ACTIVE,
    (long)menu_seen,(long)owner_calls,(unsigned long)owner_age);
  say(line);
}

/* Read-only stage discovery on the existing game rendering thread.
 * No new native code hook, object access, VTable edits, or control writes.
 * Throttle to mode/quit/async transitions and a five-second heartbeat.
 * Useful for correlating static startup screens, comic mode and 3D with
 * real top-level switch 0x455F24. Opt-in QuickExitF12=1 only. */
void SpiderQuickExitF12_ObserveEngineState(LONG flip_index,
                                          LONG renderer_menu) {
  static LONG previous_mode=-1,previous_quit=-1;
  static LONG previous_async_active=-1,previous_renderer_menu=-1;
  static LONG previous_pause_context=-1,previous_pause_options=-1;
  static LONG previous_pause_restart=-1;
  static LONG previous_bink_flag=-1,previous_video_handle=-1;
  static LONG previous_native_owner_seen=-1;
  static DWORD last_snapshot=0;
  DWORD now,mode,async,task,menu_last,movie_last;
  LONG quit,menu_seen,menu_recent,movie_recent,async_active;
  LONG pause_context,pause_options,pause_restart;
  LONG bink_flag,video_handle,audio_handle,native_owner_seen;
  LONG intro_requested,forwarded;
  char line[620];
  if(!SpiderQuickExitF12_Enabled())return;
  now=GetTickCount();
  if(renderer_menu==0)
    InterlockedExchange(&last_game3d_flip_ms,(LONG)now);
  mode=*SPIDER_ENGINE_MODE;
  async=*SPIDER_ENGINE_ASYNC_STATE;
  async_active=(async!=0u)?1:0;
  task=*SPIDER_ENGINE_TASK;
  quit=(LONG)*SPIDER_GAME_QUIT_FLAG;
  pause_context=(*SPIDER_PAUSE_CONTEXT!=0u);
  pause_options=(*SPIDER_PAUSE_OPTION_MENU!=0u);
  pause_restart=(*SPIDER_PAUSE_RESTART_MENU!=0u);
  bink_flag=(*SPIDER_GAME_BINK_ACTIVE!=0u);
  video_handle=(*SPIDER_BINK_VIDEO_HANDLE!=0u);
  audio_handle=(*SPIDER_BINK_AUDIO_HANDLE!=0u);
  native_owner_seen=InterlockedCompareExchange(&input_native_invocations,0,0)>0 ? 1:0;
  /* A first-menu tick is NOT proof of the interactive title menu:
   * Shell_MainMenu is also called around the two startup stills.
   * Sample only original scalar globals (and pointer PRESENCE) to
   * map their differing contexts before considering any F12 change.
   * Opt-in logs for <=90 s from the first rendered Flip, at most once
   * per 1500 ms, with no heavy per-frame work or extra native hooks. */
  if(InterlockedCompareExchange(&static_screen_trace_enabled,0,0)==1) {
    static DWORD start_ms=0,last_ms=0;
    static LONG samples=0;
    if(!start_ms)start_ms=now;
    if((DWORD)(now-start_ms)<=90000u &&
       (!last_ms || (DWORD)(now-last_ms)>=1500u) && samples<61) {
      DWORD menu_context=(*SPIDER_TRACE_MENU_CONTEXT!=0u);
      DWORD raw_dispatch=*SPIDER_TRACE_DISPATCH_RAW;
      DWORD raw_transition=*SPIDER_TRACE_TRANSITION_RAW;
      DWORD raw_stage=*SPIDER_TRACE_MENU_STAGE_BYTE;
      DWORD still_frames=*SPIDER_TRACE_STILL_FRAME_COUNT;
      DWORD timed_page_once=*SPIDER_TRACE_TIMED_PAGE_ONCE;
      DWORD game_clock=*SPIDER_TRACE_NATIVE_CLOCK_RAW;
      LONG ticks=InterlockedCompareExchange(&menu_frame_count,0,0);
      LONG cleanup=InterlockedCompareExchange(&post_intro_cleanup_seen,0,0);
      char snapshot[590];
      last_ms=now;
      samples++;
      snprintf(snapshot,sizeof(snapshot),
        "QUICK-EXIT-F12: STARTUP-STATIC-PROBE flip=%ld since_first_flip_ms=%lu intro_cleanup_seen=%ld real_menu_tick_count=%ld renderer2d=%ld mode_request=%lu bink_active=%ld video_handle_present=%ld menu_ctx_present=%lu menu_stage_byte=%lu dispatcher_raw=%lu transition_raw=%lu still_frame_count=%lu timed_page_once=%lu native_clock_raw=%lu quit_byte=%ld [READ-ONLY; SCREEN ID UNKNOWN]",
        (long)flip_index,(unsigned long)(DWORD)(now-start_ms),
        (long)cleanup,(long)ticks,(long)renderer_menu,
        (unsigned long)mode,(long)bink_flag,(long)video_handle,
        (unsigned long)menu_context,(unsigned long)raw_stage,
        (unsigned long)raw_dispatch,(unsigned long)raw_transition,
        (unsigned long)still_frames,(unsigned long)timed_page_once,
        (unsigned long)game_clock,(long)quit);
      say(snapshot);
    }
  }
  if(previous_mode==(LONG)mode && previous_quit==quit &&
     previous_async_active==async_active &&
     previous_renderer_menu==renderer_menu &&
     previous_pause_context==pause_context &&
     previous_pause_options==pause_options &&
     previous_pause_restart==pause_restart &&
     previous_bink_flag==bink_flag &&
     previous_video_handle==video_handle &&
     previous_native_owner_seen==native_owner_seen &&
     last_snapshot && (DWORD)(now-last_snapshot)<5000u)return;
  previous_mode=(LONG)mode;
  previous_quit=quit;
  previous_async_active=async_active;
  previous_renderer_menu=renderer_menu;
  previous_pause_context=pause_context;
  previous_pause_options=pause_options;
  previous_pause_restart=pause_restart;
  previous_bink_flag=bink_flag;
  previous_video_handle=video_handle;
  previous_native_owner_seen=native_owner_seen;
  last_snapshot=now;
  menu_seen=InterlockedCompareExchange(&real_main_menu_seen,0,0);
  menu_last=(DWORD)InterlockedCompareExchange(&last_menu_frame_ms,0,0);
  movie_last=(DWORD)InterlockedCompareExchange(&last_movie_frame_ms,0,0);
  menu_recent=menu_last && (DWORD)(now-menu_last)<400u ? 1:0;
  movie_recent=movie_last && (DWORD)(now-movie_last)<750u ? 1:0;
  intro_requested=InterlockedCompareExchange(&intro_quit_pending,0,0);
  forwarded=InterlockedCompareExchange(&intro_native_quit_forwarded,0,0);
  /* The mode is a NEXT DISPATCH request: often zero during active
   * Bink, 2D and 3D. The async DWORD is a two-part countdown, NOT a
   * mode identifier; never emit 60 log lines just because it ticks. */
  snprintf(line,sizeof(line),
    "QUICK-EXIT-F12: ENGINE-STATE flip=%ld renderer2d=%ld mode_request=%lu countdown=%lu countdown_active=%ld task=%lu quit=%ld pause_ctx=%ld pause_options=%ld pause_restart=%ld menu_recent=%ld movie_recent=%ld menu_seen=%ld intro_request=%ld intro_forwarded=%ld bink_flag=%ld video_handle_present=%ld audio_handle_present=%ld native_3d_owner_seen=%ld",
    (long)flip_index,(long)renderer_menu,(unsigned long)mode,
    (unsigned long)async,(long)async_active,(unsigned long)task,
    (long)quit,(long)pause_context,(long)pause_options,
    (long)pause_restart,(long)menu_recent,(long)movie_recent,(long)menu_seen,
    (long)intro_requested,(long)forwarded,
    (long)bink_flag,(long)video_handle,(long)audio_handle,(long)native_owner_seen);
  say(line);
}

/* Diagnostic wrapper for the EXACT ORIGINAL pause-owner call.
 * The executable's game update only calls 0x47BC40 when native
 * [0x682950] is set. A DirectDraw Flip snapshot may miss its entire
 * lifecycle if game-owned pause objects are created/released between
 * frames, so observe entry and return on the GAME THREAD instead.
 *
 * NEVER mutate selection, request code, pause objects or the HWND.
 * Always run the original handler once with the same no-arg ABI.
 * The caller ignores EAX and calls 0x458940 immediately afterward. */
static void __cdecl spider_observe_native_pause_handler(void) {
  LONG n=InterlockedIncrement(&pause_native_invocations);
  DWORD before_mode=*SPIDER_ENGINE_MODE;
  DWORD before_ctx=*SPIDER_PAUSE_CONTEXT;
  DWORD before_opts=*SPIDER_PAUSE_OPTION_MENU;
  DWORD after_mode,after_ctx,after_opts;
  char line[240];
  if(n==1) {
    snprintf(line,sizeof(line),
      "QUICK-EXIT-F12: NATIVE-PAUSE owner ENTER n=%ld ctx=%lu options=%lu request=%lu",
      (long)n,(unsigned long)(before_ctx!=0u),
      (unsigned long)(before_opts!=0u),(unsigned long)before_mode);
    say(line);
  }
  SPIDER_NATIVE_PAUSE_HANDLER();
  after_mode=*SPIDER_ENGINE_MODE;
  after_ctx=*SPIDER_PAUSE_CONTEXT;
  after_opts=*SPIDER_PAUSE_OPTION_MENU;
  if(after_mode==7u && before_mode!=7u) {
    InterlockedIncrement(&pause_native_requested7);
    snprintf(line,sizeof(line),
      "QUICK-EXIT-F12: NATIVE-PAUSE owner EXIT n=%ld native request 7 from pause; ctx=%lu options=%lu",
      (long)n,(unsigned long)(after_ctx!=0u),
      (unsigned long)(after_opts!=0u));
    say(line);
  } else if(before_ctx!=0u && after_ctx==0u) {
    snprintf(line,sizeof(line),
      "QUICK-EXIT-F12: NATIVE-PAUSE owner EXIT n=%ld context cleared; request=%lu options=%lu",
      (long)n,(unsigned long)after_mode,
      (unsigned long)(after_opts!=0u));
    say(line);
  }
}

/* Native game-input owner wrapper at 0x45553B:
 * The original 0x440EF0 executes exactly once with its original ABI,
 * before the game tests pause context at 0x455540. EAX is overwritten
 * by the original MOV at 0x455540. Snapshot its return value BEFORE
 * the separately opted-in F12 request-7 write on the same game thread.
 * This distinguishes a real owner-produced transition from F12's
 * post-call controller request without claiming its internal writer. */
static void __cdecl spider_observe_native_game_input(void) {
  LONG n=InterlockedIncrement(&input_native_invocations);
  DWORD before_request=*SPIDER_ENGINE_MODE;
  DWORD before_timer=*SPIDER_NATIVE_GAME_INPUT_TIMER_ACTIVE;
  BYTE before_1=*SPIDER_NATIVE_GAME_INPUT_FLAG_1;
  BYTE before_2=*SPIDER_NATIVE_GAME_INPUT_FLAG_2;
  DWORD after_request,native_after_request,after_timer,elapsed_ticks;
  BYTE after_1,after_2;
  static LONG last_flags=-1,last_armed=-1;
  LONG current_flags,current_armed;
  char line[290];
  if(n==1) {
    snprintf(line,sizeof(line),
      "QUICK-EXIT-F12: NATIVE-GAME-INPUT owner ENTER n=%ld request=%lu timer_active=%lu flag1=%u flag2=%u",
      (long)n,(unsigned long)before_request,
      (unsigned long)before_timer,(unsigned)before_1,(unsigned)before_2);
    say(line);
  }
  /* Native owner is guaranteed to be on the actual 3D game thread.
   * Do not request controller action 7 from the F12 polling thread.
   * The original 0x455a90 checks the request after its frame update
   * returns, then follows the native 0x455bf3 3D cleanup branch. */
  InterlockedExchange(&last_native_game_input_ms,(LONG)GetTickCount());
  SPIDER_NATIVE_GAME_INPUT_UPDATE();
  after_request=*SPIDER_ENGINE_MODE;
  native_after_request=after_request; /* immutable pre-F12 snapshot */
  /* This stage is shared with an older, separately opted-in *exit*
   * prototype. Debug F12 must NOT enter its request-7 process shutdown.
   * Both original Bink teardown and script update have already run. */
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     InterlockedCompareExchange(&postmovie_state,0,2)==2) {
    InterlockedCompareExchange(&debug_newgame_progress,2,1);
    say("QUICK-EXIT-F12: DEBUG-NEWGAME-MOVIE original Bink/script returned; continuing to native 3D, NO controller-7 EXIT");
  }
  /* Positive observation of the exact movie owner means the following
   * 3D session is potentially Stan Lee's narration even if the final
   * original Bink tick was missed (e.g. the Bink movie ended via Enter).
   * The following TWO 3D scenes instead use SPACE for native skipping.
   * Never rely on observing native_result==0 to arm the safety guard. */
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     *SPIDER_GAME_BINK_ACTIVE==0u && *SPIDER_BINK_VIDEO_HANDLE==0u) {
    LONG phase=InterlockedCompareExchange(&debug_newgame_progress,0,0);
    if((phase==1 || phase==2) &&
       InterlockedCompareExchange(&debug_newgame_progress,3,phase)==phase) {
      InterlockedExchange(&debug_newgame_3d_first_ms,(LONG)GetTickCount());
      say("QUICK-EXIT-F12: DEBUG-NEWGAME-3D native input owner reached after verified movie-4 owner; INTRO vs GAMEPLAY UNKNOWN; F12 exit BLOCKED until positively identified [READ-ONLY]");
    }
    /* Extra fail-closed protection for a request queued on an exact
     * scene-boundary frame: never forward an old exit to controller 7. */
    if(InterlockedCompareExchange(&debug_newgame_progress,0,0)>=1 &&
       InterlockedCompareExchange(&game3d_exit_state,0,1)==1)
      say("QUICK-EXIT-F12: DEBUG-NEWGAME-3D cancelled pending EXIT request at intro boundary [NO CONTROLLER 7]");
  }
  /* Passive 3D phase trace. The physical SPACE key skips BOTH native
   * 3D intro scenes (Stan Lee narration, then another 3D cinematic);
   * Enter belongs to the preceding Bink movie. Observe the native
   * input owner and physical SPACE state, but DO NOT inject any key,
   * infer which scene is active, or treat SPACE edges as proof of
   * playable gameplay. The independent polling thread logs key edges
   * even if a native 3D input-owner call is temporarily absent. */
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     InterlockedCompareExchange(&debug_newgame_progress,0,0)==3) {
    /* The original 0x505720 frame tick calls 0x50A190 at 0x505738.
     * That converter scans the 8-byte game-owned (action,DIK) table
     * at 0x568398, terminated by action 0x8000. The original
     * 0x501CB0 reads it makes at 0x50A231/0x50A258 are outside the
     * earlier 0x50A650 read probes. Snapshot the LIVE mapping once
     * in game-thread 3D, without patching any extra native code. */
    static LONG keymap_logged=0;
    if(!keymap_logged) {
      const volatile DWORD *keymap=(const volatile DWORD *)(uintptr_t)0x00568398u;
      DWORD space_actions=0,space_bindings=0;
      char mapline[256];
      int i;
      keymap_logged=1;
      for(i=0;i<32;++i) {
        DWORD action=keymap[2*i],dik=keymap[2*i+1];
        if(action==0x8000u)break;
        if((dik&0xffu)==0x39u) {
          space_actions|=action;
          ++space_bindings;
        }
      }
      snprintf(mapline,sizeof(mapline),
        "QUICK-EXIT-F12: DEBUG-3D-KEYMAP action_table=0x00568398 entries=%d space_bindings=%lu space_action_mask=0x%08lX [ORIGINAL LIVE MAP; READ-ONLY; SCENE UNKNOWN]",
        i,(unsigned long)space_bindings,(unsigned long)space_actions);
      say(mapline);
    }
    static LONG last_space=-1;
    static DWORD last_sample=0;
    static LONG sample_count=0;
    DWORD now=GetTickCount();
    DWORD start=(DWORD)InterlockedCompareExchange(&debug_newgame_3d_first_ms,0,0);
    LONG space=(GetAsyncKeyState(VK_SPACE)&0x8000)!=0;
    LONG edges=InterlockedCompareExchange(&debug_newgame_3d_space_edges,0,0);
    BYTE native_space=*SPIDER_NATIVE_RAW_SPACE_BYTE;
    static LONG previous_native_space=-1;
    static LONG byte_logs=0;
    /* Record game-owned DirectInput byte transitions, not a visual
     * or timed estimate of which cinematic is currently playing. */
    if(previous_native_space!=(LONG)native_space &&
       byte_logs<45 &&
       (!start || (DWORD)(now-start)<90000u)) {
      char inputline[340];
      snprintf(inputline,sizeof(inputline),
        "QUICK-EXIT-F12: DEBUG-3D-DINPUT-SPACE-BYTE previous=%ld current=0x%02X physical_space=%ld edges=%ld keyboard_device=%lu raw_helper_reads=%ld raw_space_helper_reads=%ld since_3d_ms=%lu [ORIGINAL BUFFER READ-ONLY; SCENE UNKNOWN]",
        (long)previous_native_space,(unsigned)native_space,
        (long)space,(long)edges,
        (unsigned long)(*SPIDER_NATIVE_KEYBOARD_DEVICE!=0u),
        (long)InterlockedCompareExchange(&debug_newgame_raw_key_reads,0,0),
        (long)InterlockedCompareExchange(&debug_newgame_raw_space_reads,0,0),
        (unsigned long)(start?(DWORD)(now-start):0u));
      say(inputline);
      byte_logs++;
    }
    previous_native_space=(LONG)native_space;
    /* Original per-frame 0x505720/0x479540 action 0x10 state:
     * p[0] is held, p[1] is new press, p[2] early press window;
     * counters +4/+8/+12 are the original held/released frames.
     * These are still present while cinematic input is being
     * handled; they do NOT authorize F12 EXIT or scene skip. */
    {
      const volatile BYTE *p=SPIDER_NATIVE_SPACE_ACTION_STATE;
      DWORD status=(DWORD)p[0]|((DWORD)p[1]<<8)|((DWORD)p[2]<<16);
      static DWORD prior_status=0xffffffffu;
      static DWORD last_action_log=0;
      static LONG action_logs=0;
      if((status!=prior_status ||
          (space && (!last_action_log ||
                     (DWORD)(now-last_action_log)>=1300u))) &&
         action_logs<55 && (!start || (DWORD)(now-start)<90000u)) {
        char actionline[380];
        snprintf(actionline,sizeof(actionline),
          "QUICK-EXIT-F12: DEBUG-3D-SPACE-ACTION action=0x00000010 held=%u just_pressed=%u early=%u frames_held=%lu frames_released=%lu frames_total=%lu dik_raw=0x%02X physical_space=%ld edges=%ld since_3d_ms=%lu [NATIVE 0x661200; ORIGINAL 0x479540; READ-ONLY; SCENE NOT IDENTIFIED]",
          (unsigned)p[0],(unsigned)p[1],(unsigned)p[2],
          (unsigned long)SPIDER_NATIVE_SPACE_ACTION_WORDS[1],
          (unsigned long)SPIDER_NATIVE_SPACE_ACTION_WORDS[2],
          (unsigned long)SPIDER_NATIVE_SPACE_ACTION_WORDS[3],
          (unsigned)native_space,(long)space,(long)edges,
          (unsigned long)(start?(DWORD)(now-start):0u));
        say(actionline);
        action_logs++;
        last_action_log=now;
      }
      prior_status=status;
    }
    /* Search for the ORIGINAL SCENE OWNER / playable boundary.
     * DirectInput and native action 0x10 are identical for BOTH 3D
     * cinematic skips and the user's confirmed gameplay jump.
     * Compare game-owned controller state and transient dispatch
     * head to physical Space across all THREE events instead.
     * Never treat nonzero pointers or a particular state as a
     * cutscene/exit permission until independently established.
     */
    {
      DWORD controller=*SPIDER_NATIVE_3D_CONTROLLER_OBJECT;
      DWORD dispatch=*SPIDER_NATIVE_3D_DISPATCH_HEAD;
      DWORD scene_flags=*SPIDER_NATIVE_CUTSCENE_INPUT_FLAGS;
      DWORD ctrl_state=controller?
        ((const volatile DWORD *)(uintptr_t)controller)[0x2a0u/4u]:
        0xffffffffu;
      static DWORD previous_controller=0xffffffffu;
      static DWORD previous_state=0xffffffffu;
      static DWORD previous_dispatch=0xffffffffu;
      static DWORD previous_flags=0xffffffffu;
      static LONG previous_physical=-1;
      static DWORD last_context_log=0;
      static LONG context_logs=0;
      LONG changed=controller!=previous_controller ||
          ctrl_state!=previous_state ||
          dispatch!=previous_dispatch ||
          scene_flags!=previous_flags ||
          space!=previous_physical;
      if((changed || !last_context_log ||
          (DWORD)(now-last_context_log)>=2000u) &&
         context_logs<75 && (!start || (DWORD)(now-start)<90000u)) {
        char ctxline[450];
        snprintf(ctxline,sizeof(ctxline),
          "QUICK-EXIT-F12: DEBUG-3D-SCENE-CONTEXT controller_ptr=0x%08lX controller_state=0x%08lX dispatch_head=0x%08lX flags=0x%08lX space_held=%ld edges=%ld action_held=%u action_pressed=%u since_3d_ms=%lu [ORIGINAL 0x455582 / 0x4DE890; READ-ONLY; PLAYABLE NOT PROVEN]",
          (unsigned long)controller,(unsigned long)ctrl_state,
          (unsigned long)dispatch,(unsigned long)scene_flags,
          (long)space,(long)edges,
          (unsigned)SPIDER_NATIVE_SPACE_ACTION_STATE[0],
          (unsigned)SPIDER_NATIVE_SPACE_ACTION_STATE[1],
          (unsigned long)(start?(DWORD)(now-start):0u));
        say(ctxline);
        ++context_logs;
        last_context_log=now;
      }
      previous_controller=controller;
      previous_state=ctrl_state;
      previous_dispatch=dispatch;
      previous_flags=scene_flags;
      previous_physical=space;
    }
    if((!start || (DWORD)(now-start)<90000u) &&
       (last_space!=space || sample_count<6 ||
        (!last_sample || (DWORD)(now-last_sample)>=1500u)) &&
       sample_count<75) {
      char trace[370];
      snprintf(trace,sizeof(trace),
        "QUICK-EXIT-F12: DEBUG-NEWGAME-3D-PHASE sample=%ld since_3d_ms=%lu space_down=%ld space_edges=%ld mode=%lu task=%lu async=%lu flag1=%u flag2=%u native_timer=%lu pause_ctx=%lu movie_mode=%lu phase_obj=%lu script_slot=%lu event_raw=%lu space_gate_bit100=%lu [READ-ONLY; PLAYABLE NOT VERIFIED]",
        (long)(sample_count+1),(unsigned long)(start?(DWORD)(now-start):0u),
        (long)space,(long)edges,(unsigned long)*SPIDER_ENGINE_MODE,
        (unsigned long)*SPIDER_ENGINE_TASK,(unsigned long)*SPIDER_ENGINE_ASYNC_STATE,
        (unsigned)*SPIDER_NATIVE_GAME_INPUT_FLAG_1,
        (unsigned)*SPIDER_NATIVE_GAME_INPUT_FLAG_2,
        (unsigned long)*SPIDER_NATIVE_GAME_INPUT_TIMER_ACTIVE,
        (unsigned long)(*SPIDER_PAUSE_CONTEXT!=0u),
        (unsigned long)*(const volatile DWORD *)(uintptr_t)0x005FAECCu,
        (unsigned long)(*(const volatile DWORD *)(uintptr_t)0x005FAED0u!=0u),
        (unsigned long)(*(const volatile DWORD *)(uintptr_t)0x006B468Cu!=0u),
        (unsigned long)*(const volatile DWORD *)(uintptr_t)0x0066126Cu,
        (unsigned long)((*SPIDER_NATIVE_CUTSCENE_INPUT_FLAGS&0x100u)!=0u));
      say(trace);
      sample_count++;
      last_sample=now;
    }
    last_space=space;
  }
  /* 0x45553B is reached only AFTER the 0x45551D game-owned script
   * dispatcher returned and after the native 0x440EF0 tick finished.
   * Allow original native cleanup (0x455BF9, controller 7) to run,
   * exactly as on the previously user-validated 3D path. */
  if(InterlockedCompareExchange(&postmovie_state,0,0)==2) {
    DWORD started=(DWORD)InterlockedCompareExchange(&postmovie_request_ms,0,0);
    if(SpiderQuickExitF12_Enabled() &&
       InterlockedCompareExchange(&postmovie_opt_in,0,0)==1 &&
       InterlockedCompareExchange(&experimental_game3d_enabled,0,0)==1 &&
       InterlockedCompareExchange(&real_main_menu_seen,0,0)==1 &&
       before_request==0u && after_request==0u &&
       *SPIDER_GAME_BINK_ACTIVE==0u && *SPIDER_BINK_VIDEO_HANDLE==0u &&
       started && (DWORD)(GetTickCount()-started)<15000u &&
       InterlockedCompareExchange(&game3d_exit_state,2,0)==0) {
      InterlockedExchange(&game3d_cleanup_baseline,
        InterlockedCompareExchange(&native_game_cleanup_count,0,0));
      InterlockedExchange(&game3d_cleanup_confirmed,0);
      *(volatile DWORD *)(uintptr_t)0x0060CFA4u=7u;
      after_request=7u;
      InterlockedExchange(&postmovie_state,3);
      say("QUICK-EXIT-F12: POSTMENU-MOVIE original Bink + script returned; native input checkpoint complete; request 7 for original cleanup [EXPERIMENTAL]");
    } else {
      InterlockedExchange(&postmovie_state,0);
      say("QUICK-EXIT-F12: POSTMENU-MOVIE handoff declined; gameplay F12 still available");
    }
  }
  if(InterlockedCompareExchange(&game3d_exit_state,2,1)==1) {
    if(InterlockedCompareExchange(&experimental_game3d_enabled,0,0)==1 &&
       before_request==0u && after_request==0u &&
       *SPIDER_GAME_BINK_ACTIVE==0u) {
      InterlockedExchange(&game3d_cleanup_baseline,
        InterlockedCompareExchange(&native_game_cleanup_count,0,0));
      InterlockedExchange(&game3d_cleanup_confirmed,0);
      *(volatile DWORD *)(uintptr_t)0x0060CFA4u=7u;
      after_request=7u;
      say("QUICK-EXIT-F12: 3D game-thread frame completed; native request 7 queued for original 0x455BF3 cleanup");
    } else {
      InterlockedExchange(&game3d_exit_state,0);
      say("QUICK-EXIT-F12: 3D request cancelled: owner has changed or Bink active");
    }
  }
  after_timer=*SPIDER_NATIVE_GAME_INPUT_TIMER_ACTIVE;
  after_1=*SPIDER_NATIVE_GAME_INPUT_FLAG_1;
  after_2=*SPIDER_NATIVE_GAME_INPUT_FLAG_2;
  elapsed_ticks=(DWORD)(*SPIDER_NATIVE_GAME_TICK_COUNTER -
                        *SPIDER_NATIVE_GAME_INPUT_TIMER_START);
  current_flags=(LONG)((after_1!=0u?1u:0u) |
                       (after_2!=0u?2u:0u));
  current_armed=(after_timer!=0u)?1:0;
  if(last_flags!=current_flags || last_armed!=current_armed) {
    /* Only a short fixed number of phase transitions, not 60 FPS
     * timer spam: show the edge that arms the original 120-tick exit. */
    if(n<5000 && (current_armed || (last_armed==1 && !current_armed))) {
      snprintf(line,sizeof(line),
        "QUICK-EXIT-F12: NATIVE-GAME-INPUT flags n=%ld flags=%ld armed=%ld elapsed=%lu",
        (long)n,(long)current_flags,(long)current_armed,
        (unsigned long)elapsed_ticks);
      say(line);
    }
    last_flags=current_flags;
    last_armed=current_armed;
  }
  if(native_after_request==7u && before_request!=7u) {
    InterlockedIncrement(&input_native_requested7);
    snprintf(line,sizeof(line),
      "QUICK-EXIT-F12: NATIVE-GAME-INPUT owner EXIT n=%ld NATIVE REQUEST 7 observed after 0x440EF0 (internal writer unconfirmed); timer_before=%lu timer_after=%lu flag_before=%u,%u flag_after=%u,%u elapsed=%lu",
      (long)n,(unsigned long)before_timer,(unsigned long)after_timer,
      (unsigned)before_1,(unsigned)before_2,
      (unsigned)after_1,(unsigned)after_2,(unsigned long)elapsed_ticks);
    say(line);
  }
}

/* PASSIVE, exact-site 3D input diagnostics for the user's original
 * SHA-matched SpideyPC.exe. This wrapper executes the original
 * 0x50C180 ONCE, returns the identical BYTE result and observes only
 * scalar engine state + key presence. It does not pretend that
 * VK_SPACE necessarily corresponds to any given native input MASK.
 *
 * In particular, no F12 request is emitted to native controller 7
 * from this routine. We need positive proof of the two separate
 * scripted 3D scenes AND actual playable state before any F12 SPACE
 * skip injection or gameplay EXIT can be enabled. */
static BYTE __cdecl spider_probe_3d_native_input(
    DWORD mask,DWORD arg1,DWORD arg2) {
  BYTE original=SPIDER_NATIVE_3D_INPUT(mask,arg1,arg2);
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     InterlockedCompareExchange(&debug_newgame_progress,0,0)==3 &&
     InterlockedCompareExchange(&debug_newgame_native_input_probes_installed,0,0)==1) {
    const uintptr_t *frame=(const uintptr_t *)__builtin_frame_address(0);
    DWORD return_va=(DWORD)frame[1],now=GetTickCount();
    static LONG last_result[SPIDER_NATIVE_3D_INPUT_SITES_COUNT]={-1,-1,-1,-1,-1};
    static LONG last_space[SPIDER_NATIVE_3D_INPUT_SITES_COUNT]={-1,-1,-1,-1,-1};
    static LONG last_mode[SPIDER_NATIVE_3D_INPUT_SITES_COUNT]={-1,-1,-1,-1,-1};
    static DWORD last_emit[SPIDER_NATIVE_3D_INPUT_SITES_COUNT]={0,0,0,0,0};
    static LONG total=0;
    int i;
    for(i=0;i<SPIDER_NATIVE_3D_INPUT_SITES_COUNT;++i) {
      if(return_va==(DWORD)(uintptr_t)spider_native_3d_input_sites[i]+5u) {
        LONG space=(GetAsyncKeyState(VK_SPACE)&0x8000)!=0;
        LONG movie_mode=*(const volatile LONG *)(uintptr_t)0x005FAECCu;
        DWORD first=(DWORD)InterlockedCompareExchange(&debug_newgame_3d_first_ms,0,0);
        DWORD duration=first?(DWORD)(now-first):0u;
        LONG changed=last_result[i]!=(LONG)(original!=0u) ||
                     last_space[i]!=space || last_mode[i]!=movie_mode;
        if(duration<=120000u && total<100 &&
           (total<5 || changed ||
            (space && (!last_emit[i] || (DWORD)(now-last_emit[i])>=750u)) ||
            (!last_emit[i] || (DWORD)(now-last_emit[i])>=4000u))) {
          char line[490];
          snprintf(line,sizeof(line),
            "QUICK-EXIT-F12: DEBUG-3D-NATIVE-INPUT site=0x%08lX mask=0x%08lX args=%lu,%lu result=%u space_down=%ld space_edges=%ld phase_state=%ld phase_object=%lu script_slots_active=%lu event_raw=%lu flag1=%u flag2=%u since_3d_ms=%lu [ORIGINAL CALLED; READ-ONLY; SCENE NOT IDENTIFIED]",
            (unsigned long)(return_va-5u),(unsigned long)mask,
            (unsigned long)arg1,(unsigned long)arg2,
            (unsigned)original,(long)space,
            (long)InterlockedCompareExchange(&debug_newgame_3d_space_edges,0,0),
            (long)movie_mode,
            (unsigned long)(*(const volatile DWORD *)(uintptr_t)0x005FAED0u!=0u),
            (unsigned long)(*(const volatile DWORD *)(uintptr_t)0x006B468Cu!=0u),
            (unsigned long)*(const volatile DWORD *)(uintptr_t)0x0066126Cu,
            (unsigned)*SPIDER_NATIVE_GAME_INPUT_FLAG_1,
            (unsigned)*SPIDER_NATIVE_GAME_INPUT_FLAG_2,
            (unsigned long)duration);
          say(line);
          ++total;
          last_emit[i]=now;
        }
        last_result[i]=(LONG)(original!=0u);
        last_space[i]=space;
        last_mode[i]=movie_mode;
        break;
      }
    }
  }
  return original;
}

/* The original 3D update at 0x455400 has its OWN direct Space key
 * 0x39 polling, independently of the 0x50C180 mask calls probed
 * above. The first original 0x455427 CALL is inside a key-release
 * loop, the second original 0x455444 CALL waits for a key press;
 * the loops additionally test game-owned 0x02E0988C bit 0x100.
 * Observe only: preserving the *exact* original input return means
 * the game remains responsible for real Space and scene transitions.
 */
static DWORD __cdecl spider_probe_3d_space_gate(
    DWORD dik,DWORD input_mode) {
  DWORD original=SPIDER_NATIVE_3D_SPACE_POLL(dik,input_mode);
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     InterlockedCompareExchange(&debug_newgame_progress,0,0)==3 &&
     InterlockedCompareExchange(&debug_newgame_space_gate_probes_installed,0,0)==1) {
    const uintptr_t *frame=(const uintptr_t *)__builtin_frame_address(0);
    DWORD ret=(DWORD)frame[1],site=ret-5u,now=GetTickCount();
    if((site==0x00455427u || site==0x00455444u) &&
       dik==0x39u && input_mode==0u) {
      static volatile LONG samples=0;
      static LONG previous_result[2]={-1,-1};
      static LONG previous_physical[2]={-1,-1};
      static DWORD previous_emit[2]={0,0};
      int i=(site==0x00455427u)?0:1;
      LONG physical=(GetAsyncKeyState(VK_SPACE)&0x8000)!=0;
      DWORD native_flags=*SPIDER_NATIVE_CUTSCENE_INPUT_FLAGS;
      LONG changed=previous_result[i]!=(LONG)(original!=0u) ||
                   previous_physical[i]!=physical;
      if(InterlockedCompareExchange(&samples,0,0)<80 &&
         (changed || (!previous_emit[i] || (DWORD)(now-previous_emit[i])>=2000u))) {
        char line[350];
        DWORD since=(DWORD)InterlockedCompareExchange(&debug_newgame_3d_first_ms,0,0);
        LONG n=InterlockedIncrement(&samples);
        if(n<=80) {
          snprintf(line,sizeof(line),
            "QUICK-EXIT-F12: DEBUG-3D-SPACE-GATE site=0x%08lX dik=0x%02lX input_mode=%lu original_result=%lu physical_space=%ld native_flags=0x%08lX bit100=%lu edges=%ld since_3d_ms=%lu [READ-ONLY; ORIGINAL POLL RESULT; SCENE UNKNOWN]",
            (unsigned long)site,(unsigned long)dik,(unsigned long)input_mode,
            (unsigned long)original,(long)physical,(unsigned long)native_flags,
            (unsigned long)((native_flags&0x100u)!=0u),
            (long)InterlockedCompareExchange(&debug_newgame_3d_space_edges,0,0),
            (unsigned long)(since?(DWORD)(now-since):0u));
          say(line);
        }
        previous_emit[i]=now;
      }
      previous_result[i]=(LONG)(original!=0u);
      previous_physical[i]=physical;
    }
  }
  return original;
}

/* Original 0x50A650 DOES NOT adjust ESP or save EBP before
 * 0x50A65D/0x50A674. For a cdecl one-argument wrapped call,
 * [our EBP+4]  = return inside 0x50A650 (0x50A662/0x50A679),
 * [our EBP+8]  = pushed DIK argument from 0x50A650,
 * [our EBP+12] = ORIGINAL call return VA into 0x50A650,
 * [our EBP+16] = DIK from 0x50A650's ORIGINAL caller,
 * [our EBP+20] = input mode from that caller.
 * Inspect only scalars, never rewrite or dereference unknown callers.
 * The original raw 0x501CB0 return is passed through unaltered.
 */
static DWORD __cdecl spider_probe_native_raw_key(DWORD dik) {
  DWORD result=SPIDER_NATIVE_KEY_READ(dik);
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     InterlockedCompareExchange(&debug_newgame_progress,0,0)==3 &&
     InterlockedCompareExchange(&debug_newgame_lowlevel_key_probes_installed,0,0)==1) {
    InterlockedIncrement(&debug_newgame_raw_key_reads);
    if(dik==0x39u)
      InterlockedIncrement(&debug_newgame_raw_space_reads);
  }
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     InterlockedCompareExchange(&debug_newgame_progress,0,0)==3 &&
     InterlockedCompareExchange(&debug_newgame_lowlevel_key_probes_installed,0,0)==1 &&
     dik==0x39u) {
    const uintptr_t *frame=(const uintptr_t *)__builtin_frame_address(0);
    DWORD inner=(DWORD)frame[1],caller=(DWORD)frame[3];
    DWORD caller_key=(DWORD)frame[4],caller_mode=(DWORD)frame[5];
    if((inner==0x0050A662u || inner==0x0050A679u) &&
       caller_key==dik && (caller_mode==0u || caller_mode==1u)) {
      static volatile LONG total=0;
      static LONG last_code[2]={-1,-1};
      static LONG last_physical[2]={-1,-1};
      static DWORD last_emitted[2]={0,0};
      int index=inner==0x0050A662u?1:0;
      LONG physical=(GetAsyncKeyState(VK_SPACE)&0x8000)!=0;
      DWORD now=GetTickCount();
      LONG code=(LONG)(result&0xffu);
      LONG changed=last_code[index]!=code || last_physical[index]!=physical;
      if(InterlockedCompareExchange(&total,0,0)<100 &&
         (changed || !last_emitted[index] ||
          (DWORD)(now-last_emitted[index])>=3000u)) {
        LONG n=InterlockedIncrement(&total);
        if(n<=100) {
          DWORD start=(DWORD)InterlockedCompareExchange(&debug_newgame_3d_first_ms,0,0);
          char line[410];
          snprintf(line,sizeof(line),
            "QUICK-EXIT-F12: DEBUG-3D-RAW-SPACE origin=0x%08lX internal=0x%08lX dik=0x%02lX mode=%lu raw_code=0x%02lX physical_space=%ld edges=%ld bit100=%lu since_3d_ms=%lu [READ-ONLY; ORIGINAL RAW KEY RESULT; SCENE UNKNOWN]",
            (unsigned long)caller,(unsigned long)(inner-5u),
            (unsigned long)dik,(unsigned long)caller_mode,
            (unsigned long)(result&0xffu),(long)physical,
            (long)InterlockedCompareExchange(&debug_newgame_3d_space_edges,0,0),
            (unsigned long)((*SPIDER_NATIVE_CUTSCENE_INPUT_FLAGS&0x100u)!=0u),
            (unsigned long)(start?(DWORD)(now-start):0u));
          say(line);
        }
        last_emitted[index]=now;
      }
      last_code[index]=code;
      last_physical[index]=physical;
    }
  }
  return result;
}

/* PASSIVE owner cleanup checkpoint for the real gameplay path.
 * 0x455A90 compares mode_request to zero immediately after calling
 * 0x455400; a pending request skips to 0x455BF3.
 * 0x455BF9 then calls 0x4302D0. Interpose precisely at this CALL
 * to prove the original game thread reaches cleanup before returning
 * to the top-level controller. No F12 action or global write here. */
static void __cdecl spider_observe_native_game_cleanup(void) {
  LONG n=InterlockedIncrement(&native_game_cleanup_count);
  DWORD request=*SPIDER_ENGINE_MODE;
  char line[190];
  snprintf(line,sizeof(line),
    "QUICK-EXIT-F12: NATIVE-3D-CLEANUP begin n=%ld request=%lu",
    (long)n,(unsigned long)request);
  say(line);
  SPIDER_NATIVE_GAME_CLEANUP();
  if(InterlockedCompareExchange(&game3d_exit_state,0,0)==2 &&
     request==7u &&
     n>InterlockedCompareExchange(&game3d_cleanup_baseline,0,0)) {
    /* Only the successful return of the REAL game-owned 0x4302d0
     * validates the initial 3D cleanup checkpoint. */
    InterlockedExchange(&game3d_cleanup_confirmed,1);
    say("QUICK-EXIT-F12: 3D native cleanup checkpoint returned; awaiting controller-7 finalization");
  }
  snprintf(line,sizeof(line),
    "QUICK-EXIT-F12: NATIVE-3D-CLEANUP completed n=%ld request=%lu",
    (long)n,(unsigned long)*SPIDER_ENGINE_MODE);
  say(line);
}

/* Native controller request-7 checkpoint after all prior owner
 * cleanup calls. The original 0x48A820 runs once, and the code's
 * original test at 0x45619D will execute unmodified afterward.
 * Merely record whether native bit-4 or signed status would send the
 * controller to its COMMON final cleanup 0x45620C, or back to 0x455D98.
 * No exit request, native global modification, or menu traversal. */
static void __cdecl spider_observe_controller7_finish(void) {
  LONG n=InterlockedIncrement(&native_controller7_count);
  DWORD request=*SPIDER_ENGINE_MODE;
  BYTE before_bit=*SPIDER_NATIVE_CONTROLLER7_EXIT_FLAGS;
  LONG before_status=*SPIDER_NATIVE_CONTROLLER7_EXIT_STATUS;
  BYTE after_bit;
  LONG after_status;
  char line[240];
  if(request==7u) {
    snprintf(line,sizeof(line),
      "QUICK-EXIT-F12: CONTROLLER-7 native teardown ENTER n=%ld bit4=%u status=%ld 3D-checkpoint=%ld",
      (long)n,(unsigned)((before_bit&4u)!=0u),
      (long)before_status,
      (long)InterlockedCompareExchange(&native_game_cleanup_count,0,0));
    say(line);
  }
  SPIDER_NATIVE_CONTROLLER7_FINISH();
  after_bit=*SPIDER_NATIVE_CONTROLLER7_EXIT_FLAGS;
  after_status=*SPIDER_NATIVE_CONTROLLER7_EXIT_STATUS;
  /* This owner-safe site follows the original complete 3D loop exit,
   * original 0x443ad0(0) transition cleanup AND original 0x48a820.
   * The immediately following unchanged original game instruction
   * 0x45619d tests bit 4, and jumps to true native FINAL cleanup
   * 0x45620c if set. Preserve all unrelated flag bits. This never
   * fires for normal user-initiated "quit to main menu". */
  if(request==7u &&
     InterlockedCompareExchange(&experimental_game3d_enabled,0,0)==1 &&
     InterlockedCompareExchange(&game3d_exit_state,0,0)==2 &&
     InterlockedCompareExchange(&game3d_cleanup_confirmed,0,0)==1 &&
     InterlockedCompareExchange(&native_game_cleanup_count,0,0)>
       InterlockedCompareExchange(&game3d_cleanup_baseline,0,0)) {
    if((after_bit&4u)==0u && after_status<0) {
      *(volatile BYTE *)(uintptr_t)0x02E0988Cu=(BYTE)(after_bit|4u);
      after_bit=*SPIDER_NATIVE_CONTROLLER7_EXIT_FLAGS;
      say("QUICK-EXIT-F12: 3D owner+controller cleanup complete; engine native final-exit flag set at 0x45619D");
    } else {
      say("QUICK-EXIT-F12: 3D native final cleanup already selected by original engine");
    }
    InterlockedExchange(&game3d_exit_state,3);
  }
  if(request==7u) {
    const char *outcome=((after_bit&4u)!=0u||after_status>=0)
                         ?"NATIVE-FINAL-CLEANUP":"RETURN-TO-CONTROLLER";
    snprintf(line,sizeof(line),
      "QUICK-EXIT-F12: CONTROLLER-7 native teardown COMPLETED n=%ld bit4=%u status=%ld outcome=%s",
      (long)n,(unsigned)((after_bit&4u)!=0u),
      (long)after_status,outcome);
    say(line);
    /* Measure the interval to the last game-owned shutdown checkpoint.
     * These timestamps are observational, not evidence of what color
     * was displayed on the DirectDraw surface. */
    if(InterlockedCompareExchange(&postmovie_state,0,0)==3) {
      DWORD started=(DWORD)InterlockedCompareExchange(&postmovie_request_ms,0,0);
      if(started) {
        snprintf(line,sizeof(line),
          "QUICK-EXIT-F12: POSTMENU-MOVIE FINAL-TEARDOWN-CHECKPOINT elapsed_since_F12_ms=%lu [NO PRESENT/COLOR CLAIM]",
          (unsigned long)(DWORD)(GetTickCount()-started));
        say(line);
      }
    }
  }
}

/* A static whitelist from the SHA-verified, unmodified SpideyPC.exe:
 * original direct CALL 0x470750 addresses + 5 (return addresses).
 * This is not a general stack unwinder.  Only use for the exact two
 * native tick CALL sites at 0x47081D/0x47085F while their standard
 * x86 call stack is still active. No game memory is written. */
static int spider_f12_known_bink_parent_return(DWORD ret_addr) {
  static const DWORD known[]={
    0x00427A00u,0x00427A16u,0x00427AA5u,0x00427AC1u,
    0x00427AD7u,0x00427AEDu,0x00427B03u,0x00427B94u,
    0x00427BB0u,0x00427BC6u,0x00427C57u,0x00427C73u,
    0x00427C89u,0x00427C9Fu,0x00427CB5u,0x00427CCBu,
    0x00427CEBu,0x00427D01u,0x00427D17u,0x00427D37u,
    0x00427D4Du,0x00427D63u,0x00427D79u,0x00427D8Fu,
    0x00443BECu,0x00455CF6u,0x00455D03u,0x00455D11u,
    0x00455D1Fu,0x0049D659u
  };
  unsigned i;
  for(i=0;i<sizeof(known)/sizeof(known[0]);i++)
    if(ret_addr==known[i])return 1;
  return 0;
}

/* Collect only once, only if explicitly requested with a debug-only
 * environment switch. GCC's __builtin_frame_address(0) forces the
 * current x86 wrapper frame. On these two *verified native CALL sites*
 * the original 0x470750 has push esi (0x470750) and push ebx
 * (0x470809) before calling this wrapper; both are still stacked.
 *
 *   frame[1] = native tick CALL return (must be 0x470822/0x470864)
 *   frame[2] = game owner's saved EBX
 *   frame[3] = game owner's saved ESI
 *   frame[4] = direct caller return address of native 0x470750
 *   frame[5] = REUSED stack scratch, not the original video ID:
 *              owner 0x4707E4 overwrites its first-argument slot.
 *   frame[6] = allow-skip (preserved original second argument)
 *
 * If the first return address differs, DO NOT inspect deeper stack.
 * Even a whitelisted parent identifies only a callsite, NOT authority
 * to initiate a native F12 game-exit/cleanup from within this wrapper. */
static void spider_f12_trace_native_bink_parent(const uintptr_t *frame) {
  DWORD tick_return=(DWORD)frame[1];
  DWORD parent_return,allow_skip;
  char line[260];
  if(tick_return!=0x00470822u && tick_return!=0x00470864u) {
    snprintf(line,sizeof(line),
      "QUICK-EXIT-F12: POST-SHELL-BINK-PARENT unsupported native wrapper return=0x%08lX [FRAME-READ-ONLY; NOT IDENTIFIED]",
      (unsigned long)tick_return);
    say(line);
    return;
  }
  parent_return=(DWORD)frame[4];
  /* The original first argument at frame[5] was already OVERWRITTEN
   * by 0x4707E4 before the tick callback; video IDs must instead
   * be inferred from the SHA-verified native callsite, if constant. */
  allow_skip=(DWORD)frame[6];
  snprintf(line,sizeof(line),
    "QUICK-EXIT-F12: POST-SHELL-BINK-PARENT native_tick_return=0x%08lX parent_return=0x%08lX known_original_site=%d allow_skip=%lu [FRAME-READ-ONLY; ID MUST COME FROM CALLSITE; NO EXIT AUTHORITY]",
    (unsigned long)tick_return,(unsigned long)parent_return,
    spider_f12_known_bink_parent_return(parent_return),
    (unsigned long)allow_skip);
  say(line);
}

/* Native BinkFrame tick wrapper, called by the game thread at the two
 * original 0x470750 video-loop sites. False makes the owning caller
 * branch to 0x470871 and invoke original BinkClose, audio reset and
 * movie-active clearing. Never call BinkClose or access Bink handles in
 * this hook, and never destroy HWND while the video loop is alive. */
static BYTE __cdecl spider_f12_native_intro_tick(void) {
  static volatile LONG after_shell_ticks=0;
  static volatile LONG after_shell_previous_result=-1;
  static volatile LONG after_shell_previous_space=0;
  static DWORD after_shell_last_log_ms=0;
  BYTE native_result;
  LONG post_shell=0,n=0;
  if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1) {
    if(InterlockedCompareExchange(&intro_tick_shortcircuited,1,0)==0)
      say("QUICK-EXIT-F12: native Bink tick returned finished; original game video cleanup will run");
    return 0;
  }
  /* Identify New Game movie #4 from its EXACT original native stack,
   * whether or not F12 was pressed. This is positive cinematic-stage
   * evidence: the 3D intro must not be mistaken for playable gameplay.
   * The native owner and stack signature are already Windows-proven. */
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     InterlockedCompareExchange(&real_main_menu_seen,0,0)==1 &&
     *SPIDER_GAME_BINK_ACTIVE!=0u && *SPIDER_BINK_VIDEO_HANDLE!=0u) {
    const uintptr_t *frame=(const uintptr_t *)__builtin_frame_address(0);
    DWORD return_address=(DWORD)frame[1];
    if((return_address==0x00470822u || return_address==0x00470864u) &&
       (DWORD)frame[4]==0x00427A00u && (DWORD)frame[6]==1u &&
       InterlockedCompareExchange(&debug_newgame_progress,1,0)==0)
      say("QUICK-EXIT-F12: DEBUG-NEWGAME-MOVIE original movie-4 owner identified (0x4279FB); future 3D F12 is guarded until playable intro boundary proven");
  }
  /* Movie 4's owner was proven by Windows caller trace and original
   * EXE: call 0x4279FB, return 0x427A00, allow-skip argument 1.
   * Verify the current native tick CALL return before deeper stack reads.
   * A returned zero follows game-owned 0x470871 Bink teardown and
   * normal scene-script continuation; it does NOT exit immediately. */
  {
    LONG stage=InterlockedCompareExchange(&postmovie_state,0,0);
    if(stage==1 || stage==4) {
      DWORD started=(DWORD)InterlockedCompareExchange(&postmovie_request_ms,0,0);
      DWORD max_age=stage==4 ? 12000u : 5000u;
      if(!started || (DWORD)(GetTickCount()-started)>max_age) {
        if(InterlockedCompareExchange(&postmovie_state,0,stage)==stage)
          say("QUICK-EXIT-F12: POSTMENU-MOVIE or deferred COMIC request expired; original playback untouched");
      } else if(SpiderQuickExitF12_Enabled() &&
                (InterlockedCompareExchange(&postmovie_opt_in,0,0)==1 ||
                 InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) &&
                (stage!=4 ||
                 InterlockedCompareExchange(&comic_exit_opt_in,0,0)==1 ||
                 InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) &&
                InterlockedCompareExchange(&real_main_menu_seen,0,0)==1 &&
                InterlockedCompareExchange(&input_native_invocations,0,0)==0 &&
                *SPIDER_GAME_BINK_ACTIVE!=0u &&
                *SPIDER_BINK_VIDEO_HANDLE!=0u) {
        const uintptr_t *frame=(const uintptr_t *)__builtin_frame_address(0);
        DWORD tick_return=(DWORD)frame[1];
        if((tick_return==0x00470822u || tick_return==0x00470864u) &&
           (DWORD)frame[4]==0x00427A00u &&
           (DWORD)frame[6]==1u &&
           InterlockedCompareExchange(&postmovie_state,2,stage)==stage) {
          char line[240];
          snprintf(line,sizeof(line),
            "QUICK-EXIT-F12: POSTMENU-MOVIE verified native movie owner 0x4279FB; original Bink completion and 0x470871 teardown follows; source=%s elapsed_since_F12_ms=%lu",
            stage==4 ? "DEFERRED-COMIC" : "CINEMATIC",
            (unsigned long)(DWORD)(GetTickCount()-started));
          say(line);
          if(stage==4 &&
             InterlockedCompareExchange(&debug_newgame_comic_fastforward_observed,0,0)==1)
            say("QUICK-EXIT-F12: DEBUG-NEWGAME-COMIC-FINISHED native L1COV.bmp 300-tick threshold satisfied at 0x4279B8; original 0x4279C6 JAE and Bink owner #4 reached [GLOBAL CLOCK UNMODIFIED]");
          if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1)
            InterlockedCompareExchange(&debug_newgame_progress,2,1);
          return 0;
        }
      }
    }
  }
  /* The existing native 0x470750 Bink loop is also reached after
   * New Game. Keep the exact original tick result and call it ONCE.
   * The sampled space-key state is only correlation evidence, not proof
   * of the game input event that ended the movie. */
  post_shell=SpiderQuickExitF12_Enabled() &&
             InterlockedCompareExchange(&real_main_menu_seen,0,0)==1;
  if(post_shell) {
    n=InterlockedIncrement(&after_shell_ticks);
    if(n==1) {
      say("QUICK-EXIT-F12: POST-SHELL-BINK-TICK-CALLSITE original 0x470750 tick observed [OWNER-IDENTITY NOT PROVEN]");
      if(InterlockedCompareExchange(&parent_trace_enabled,0,0)==1)
        spider_f12_trace_native_bink_parent(
          (const uintptr_t *)__builtin_frame_address(0));
    }
  }
  native_result=SPIDER_NATIVE_BINK_FRAME_TICK();
  /* Even a manual Enter Bink movie-skip must retain a "scripted 3D may
   * follow" guard. No F12 skip request is needed for this observation. */
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     native_result==0 &&
     InterlockedCompareExchange(&debug_newgame_progress,0,0)==1 &&
     *SPIDER_GAME_BINK_ACTIVE!=0u) {
    const uintptr_t *frame=(const uintptr_t *)__builtin_frame_address(0);
    DWORD return_address=(DWORD)frame[1];
    if((return_address==0x00470822u || return_address==0x00470864u) &&
       (DWORD)frame[4]==0x00427A00u && (DWORD)frame[6]==1u &&
       InterlockedCompareExchange(&debug_newgame_progress,2,1)==1)
      say("QUICK-EXIT-F12: DEBUG-NEWGAME-MOVIE original movie-4 finished (possibly via Enter); awaiting 3D owner [READ-ONLY]");
  }
  if(post_shell) {
    LONG old_result=InterlockedExchange(&after_shell_previous_result,(LONG)native_result);
    /* The native loop called its Bink tick >880,000 times in ~16s on
     * Windows. Do NOT poll the physical key on every tight-loop call or
     * emit a line every 120 calls (~7,300 lines per cinematic).
     * Compare the unchanged result every tick, but sample the keyboard
     * at most once per 128 calls and report changes / 1s heartbeat. */
    if(n<=4 || n%128==0 || native_result!=(BYTE)old_result) {
      LONG space_down=(GetAsyncKeyState(VK_SPACE)&0x8000)!=0;
      LONG old_space=InterlockedExchange(&after_shell_previous_space,space_down);
      DWORD now=GetTickCount();
      if(n<=4 || native_result!=(BYTE)old_result ||
         (space_down && !old_space) ||
         !after_shell_last_log_ms ||
         (DWORD)(now-after_shell_last_log_ms)>=1000u) {
        DWORD comic_ms=(DWORD)InterlockedCompareExchange(&last_comic_candidate_ms,0,0);
        char line[320];
        after_shell_last_log_ms=now;
        snprintf(line,sizeof(line),
          "QUICK-EXIT-F12: POST-SHELL-BINK-TICK-RESULT n=%ld native_result=%u previous_result=%ld space_down_sampled=%ld bink_flag=%lu video_handle_present=%lu native_3d_owner_seen=%lu ms_since_comic=%lu [READ-ONLY; SKIP-REASON UNKNOWN]",
          (long)n,(unsigned)native_result,(long)old_result,(long)space_down,
          (unsigned long)(*SPIDER_GAME_BINK_ACTIVE!=0u),
          (unsigned long)(*SPIDER_BINK_VIDEO_HANDLE!=0u),
          (unsigned long)(InterlockedCompareExchange(&input_native_invocations,0,0)>0),
          (unsigned long)(comic_ms?(DWORD)(now-comic_ms):0u));
        say(line);
      }
    }
  }
  return native_result;
}

/* Wrapper on the ORIGINAL GAME THREAD, immediately after the four
 * startup movies. Calls original 0x443AD0 native transition cleanup
 * once. The game MAY skip its 0x455E50 Quit test; the opt-in fallback
 * carries the opening F12 to original static input cleanup and THEN
 * real Shell_MainMenu native Quit (not an early splash callback).
 * When 0x455E50 is reached, original 0x4561F2 -> 0x45620C
 * still provides its standard final shutdown. */
static void __cdecl spider_after_intro_cleanup(int mode) {
  SPIDER_AFTER_INTRO_CLEANUP(mode);
  if(InterlockedCompareExchange(&post_intro_cleanup_seen,1,0)==0 &&
     InterlockedCompareExchange(&static_screen_trace_enabled,0,0)==1)
    say("QUICK-EXIT-F12: STARTUP-STATIC-PROBE original four-movie transition cleanup 0x455D23 completed [READ-ONLY]");
  if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1 &&
     InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) {
    /* The initial F12 is a menu SKIP, not a Quit request. The original
     * four Bink owners and 0x443AD0 have already cleaned up. Leave the
     * game Quit byte untouched. Complete the native timed still normally,
     * then finish input-wait via its verified 0x4A18AE native teardown. */
    if(InterlockedCompareExchange(&intro_still_opt_in,0,0)==1 &&
       InterlockedCompareExchange(&intro_still_hook_installed,0,0)==1 &&
       InterlockedCompareExchange(&real_main_menu_seen,0,0)==0 &&
       InterlockedCompareExchange(&intro_still_exit_state,0,0)==0) {
      InterlockedExchange(&intro_still_request_ms,(LONG)GetTickCount());
      if(InterlockedCompareExchange(&intro_still_exit_state,1,0)==0)
        say("QUICK-EXIT-F12: DEBUG-MENU-SKIP opening Bink cleanup completed; F12 routed into original timed/input-wait pages (NO QUIT)");
    }
    /* The native 0x470750 Bink tick is shared by the FOUR opening
     * films AND the New Game cinematic. The startup request was
     * consumed after the original 0x443AD0 transition cleanup above:
     * clear it NOW or the first tick of New Game movie #4 will still
     * return 0 and instantly skip that unrelated cinematic.
     * The separate intro_still_exit_state=1 latch persists to the
     * exact original 0x4A1896 input-wait owner, so menu navigation
     * still completes without another F12. In legacy Quit mode the
     * original pending flag stays unchanged. */
    InterlockedExchange(&intro_quit_pending,0);
    InterlockedExchange(&intro_tick_shortcircuited,0);
    say("QUICK-EXIT-F12: DEBUG-MENU-SKIP opening Bink request CONSUMED at native post-intro cleanup; movie #4 tick restored for future New Game playback");
  } else if(InterlockedCompareExchange(&intro_quit_pending,0,0)==1) {
    /* Original x86 0x455D23 -> 0x455E50 is NOT unconditional:
     * branches at 0x455DC8 / 0x455DD0 may skip the 0x455E50
     * Quit-byte comparison entirely. Windows #589 showed quit=1
     * yet the engine continued to the first still. Preserve the
     * original flag for paths that do reach its native check, AND
     * carry this SAME F12 into the already validated input-wait
     * page cleanup when that separate option is enabled. */
    *SPIDER_GAME_QUIT_FLAG=1u;
    InterlockedExchange(&intro_native_quit_forwarded,1);
    say("QUICK-EXIT-F12: four native movie calls completed; original transition cleanup finished; Quit byte forwarded (top-level 0x455E50 may be bypassed by startup branches)");
    if(SpiderQuickExitF12_Enabled() &&
       InterlockedCompareExchange(&intro_still_opt_in,0,0)==1 &&
       InterlockedCompareExchange(&intro_still_hook_installed,0,0)==1 &&
       InterlockedCompareExchange(&real_main_menu_seen,0,0)==0 &&
       InterlockedCompareExchange(&intro_still_exit_state,0,0)==0) {
      /* Do NOT trigger a second synthetic F12 or skip native page
       * lifetime. The timed still finishes normally and the original
       * 0x4A1896 input-wait call takes native TRUE -> 0x4A18AE
       * page cleanup -> real Shell_MainMenu Quit action 18.
       * Timeout handling is shared with manual F12 on either still. */
      InterlockedExchange(&intro_still_request_ms,(LONG)GetTickCount());
      if(InterlockedCompareExchange(&intro_still_exit_state,1,0)==0)
        say("QUICK-EXIT-F12: INTRO-MOVIE single F12 carried into original timed/input-wait still cleanup; no second keypress required [EXPERIMENTAL]");
    }
  }
}

/* Executes instead of the exact ORIGINAL 0x4A1A26 MOV EAX,[0x6B4CA0].
 * This is the game's own first-still clock comparison, immediately before
 * CMP EAX,EDI / JA normal page cleanup at 0x4A1A36.
 *
 * In debug mode ONLY, when a verified pre-menu F12 is pending and the
 * timed page actually entered, return the already computed deadline EDI+1.
 * The game then takes its OWN timeout branch, releases its page, restores
 * SEH and moves to the input-wait page. Nothing writes the shared clock.
 *
 * Naked x86 wrapper: EDI is the original native loop's deadline register.
 * No prologue may clobber it. All caller registers except EAX are intact;
 * CMP immediately after this call overwrites condition flags anyway. */
__attribute__((naked,used)) static DWORD spider_f12_debug_timed_clock(void) {
  __asm__ __volatile__(
    "movl 0x006B4CA0, %eax\n\t"
    "cmpl $1, _debug_f12_test_mode\n\t"
    "jne 1f\n\t"
    "cmpl $1, _intro_still_exit_state\n\t"
    "jne 1f\n\t"
    "cmpl $1, _post_intro_cleanup_seen\n\t"
    "jne 1f\n\t"
    "cmpl $1, _intro_still_hook_installed\n\t"
    "jne 1f\n\t"
    "cmpl $0, _real_main_menu_seen\n\t"
    "jne 1f\n\t"
    "cmpb $1, 0x006A7785\n\t"
    "jne 1f\n\t"
    "cmpl $0, 0x006151F8\n\t"
    "jne 1f\n\t"
    "cmpl $0, 0x00AC0BA4\n\t"
    "jne 1f\n\t"
    "cmpl $-1, %edi\n\t"
    "je 1f\n\t"
    "leal 1(%edi), %eax\n\t"
    "movl $1, _debug_timed_accelerated\n\t"
    "1:\n\t"
    "ret\n\t"
  );
}

/* The L1COV.bmp NEW GAME timed image belongs to native 0x427910,
 * not the separate PRE-MENU timed still at 0x4A1930.
 * The overwritten 6-byte instruction is MOV EDX,[0x6B4CA0] at
 * 0x4279B8. ESI holds the original comic-entry clock tick.
 *
 * A verified focused F12 on the comic latches postmovie_state=4.
 * On the game thread, return synthetic local time EDX=ESI+300,
 * causing original SUB EDX,ESI / CMP 0x12C / JAE 0x4279F0 to run.
 * As with ordinary expiration the original loop first calls 0x505720,
 * polls original input, and leaves normally; Bink #4 then starts.
 * All original registers EXCEPT EDX are untouched, no clock write,
 * no forced jump, no bypass of game-owned cleanup. This hook is
 * installed only after original bytes and owner signatures match. */
__attribute__((naked,used)) static void spider_f12_debug_comic_clock(void) {
  __asm__ __volatile__(
    "movl 0x006B4CA0, %edx\n\t"
    "cmpl $1, _debug_f12_test_mode\n\t"
    "jne 1f\n\t"
    "cmpl $1, _debug_newgame_comic_clock_hook_installed\n\t"
    "jne 1f\n\t"
    "cmpl $4, _postmovie_state\n\t"
    "jne 1f\n\t"
    "cmpl $1, _real_main_menu_seen\n\t"
    "jne 1f\n\t"
    "cmpl $0, _debug_newgame_progress\n\t"
    "jne 1f\n\t"
    "cmpl $0, 0x006151F8\n\t"
    "jne 1f\n\t"
    "cmpl $0, 0x00AC0BA4\n\t"
    "jne 1f\n\t"
    "leal 0x12c(%esi), %edx\n\t"
    "movl $1, _debug_newgame_comic_fastforward_observed\n\t"
    "1:\n\t"
    "ret\n\t"
  );
}

/* Wrapper for ORIGINAL 0x4A1896 input check, on the game's own
 * input-wait scene thread. Never free the game-owned page directly,
 * jump across its exception frame or post a synthetic Enter event.
 * A verified F12 request may return the same TRUE completion as the
 * original input, letting 0x4A18AE clean objects and restore SEH. */
static BYTE __cdecl spider_f12_intro_still_native_input(
    DWORD mask,DWORD arg1,DWORD arg2) {
  BYTE original=SPIDER_INTRO_STILL_NATIVE_INPUT(mask,arg1,arg2);
  if(InterlockedCompareExchange(&intro_still_exit_state,0,0)!=1)
    return original;
  {
    const uintptr_t *frame=(const uintptr_t *)__builtin_frame_address(0);
    DWORD started=(DWORD)InterlockedCompareExchange(&intro_still_request_ms,0,0);
    DWORD age=started ? (DWORD)(GetTickCount()-started) : 0xFFFFFFFFu;
    if(SpiderQuickExitF12_Enabled() &&
       InterlockedCompareExchange(&intro_still_opt_in,0,0)==1 &&
       InterlockedCompareExchange(&post_intro_cleanup_seen,0,0)==1 &&
       InterlockedCompareExchange(&real_main_menu_seen,0,0)==0 &&
       *SPIDER_GAME_BINK_ACTIVE==0u &&
       *SPIDER_BINK_VIDEO_HANDLE==0u &&
       *SPIDER_TRACE_MENU_CONTEXT!=0u &&
       *SPIDER_TRACE_TIMED_PAGE_ONCE==1u &&
       *SPIDER_ENGINE_MODE==0u &&
       (DWORD)frame[1]==0x004A189Bu &&
       mask==0x40010u && arg1==1u && arg2==1u &&
       age<=15000u &&
       InterlockedCompareExchange(&intro_still_exit_state,2,1)==1) {
      /* Only the original REAL Shell_MainMenu frame may consume this
       * request; it follows this page's own resource/SEH teardown. */
      if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) {
        if(InterlockedCompareExchange(&debug_timed_accelerated,0,1)==1)
          say("QUICK-EXIT-F12: DEBUG-MENU-SKIP first timed-page deadline accelerated via 0x4A1A26; original 0x4A1A36 cleanup completed [NO CLOCK WRITE]");
        /* Do not queue real-menu Quit. The original input-wait owner
         * destroys its resources before original Shell_MainMenu runs. */
        say("QUICK-EXIT-F12: DEBUG-MENU-SKIP original 0x4A1896 input-wait TRUE completion; 0x4A18AE page cleanup, main menu allowed (NO QUIT)");
      } else {
        InterlockedExchange(&delivered,0);
        InterlockedExchange(&pending,1);
        say("QUICK-EXIT-F12: INTRO-STILL original input-wait owner 0x4A1896 verified; normal TRUE completion for 0x4A18AE native page cleanup; menu Quit queued [EXPERIMENTAL]");
      }
      return 1;
    }
  }
  return original;
}

/* Returns nonzero ONLY when a recent, explicitly requested F12 action
 * may be delivered inside the ACTUAL Shell_MainMenu frame loop. This
 * never mutates engine state or executes resource cleanup itself.
 * Executed on the original game thread after the game's normal tick. */
/* Called on the original menu thread from the already installed 0x493DD6
 * call wrapper, immediately BEFORE the real 0x505720 menu tick.
 * Never touches original DirectInput, scheduler, or game-owned state. */
void __cdecl SpiderQuickExitF12_MarkMenuTickEntry(void) {
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     InterlockedCompareExchange(&intro_still_exit_state,0,0)>=2 &&
     InterlockedCompareExchange(&menu_frame_count,0,0)<65)
    InterlockedExchange(&debug_menu_tick_entry_ms,(LONG)GetTickCount());
}

int __cdecl SpiderQuickExitF12_ConsumeRealMenuRequest(void) {
  LONG n=InterlockedIncrement(&menu_frame_count);
  InterlockedExchange(&last_menu_frame_ms,(LONG)GetTickCount());
  InterlockedExchange(&real_main_menu_seen,1);
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     InterlockedCompareExchange(&intro_still_exit_state,0,0)>=2 &&
     n<=65) {
    DWORD exit_ms=GetTickCount();
    DWORD entry_ms=(DWORD)InterlockedCompareExchange(&debug_menu_tick_entry_ms,0,0);
    DWORD prev_ms=(DWORD)InterlockedExchange(&debug_menu_last_exit_ms,(LONG)exit_ms);
    DWORD native_ms=entry_ms?(DWORD)(exit_ms-entry_ms):0u;
    DWORD between_ms=(prev_ms && entry_ms)?(DWORD)(entry_ms-prev_ms):0u;
    if(n<=20 || n%10==0 || native_ms>=100u || between_ms>=100u) {
      char line[280];
      snprintf(line,sizeof(line),
        "QUICK-EXIT-F12: DEBUG-MENU-TICK-TIMING frame=%ld since_last_exit_ms=%lu native_0x505720_ms=%lu wallclock_ms=%lu [READ-ONLY]",
        (long)n,(unsigned long)between_ms,(unsigned long)native_ms,(unsigned long)exit_ms);
      say(line);
    }
  }
  if(n==1)
    say("QUICK-EXIT-F12: REAL Shell_MainMenu frame callback observed (0x493DD6)");
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) {
    /* READ-ONLY: original Shell_MainMenu draws a frame and calls its
     * normal input tick at 0x493DD6, but at 0x493DDB it tests
     * [0x54D38C] and skips the game's own menu input handling
     * while nonzero. Identify whether that gate explains the
     * observed several-second visible-but-unresponsive interval.
     * Also correlate the game's active-input flag [0x6B78F0]
     * with the separate DI8 retry installation at Flip #120.
     * No new EXE hook, DirectInput call or game-state write. */
    if(InterlockedCompareExchange(&intro_still_exit_state,3,2)==2)
      say("QUICK-EXIT-F12: DEBUG-MENU-SKIP real Shell_MainMenu reached, input-wait page cleaned, no native Quit action");
    if(InterlockedCompareExchange(&intro_still_exit_state,0,0)==3 &&
       n<=210) {
      static DWORD first_tick_ms=0;
      static LONG previous_transition=-1,previous_active=-1;
      DWORD now=GetTickCount();
      DWORD transition=*SPIDER_TRACE_TRANSITION_RAW;
      DWORD active=*(const volatile DWORD *)(uintptr_t)0x006B78F0u;
      DWORD since_request=(DWORD)InterlockedCompareExchange(&intro_still_request_ms,0,0);
      int change=(previous_transition!=(LONG)transition ||
                  previous_active!=(LONG)active);
      if(!first_tick_ms)first_tick_ms=now;
      if(change || n==1 || (n<=150 && n%15==0) || n==210) {
        char status[320];
        snprintf(status,sizeof(status),
          "QUICK-EXIT-F12: DEBUG-MENU-INPUT-PROBE frame=%ld ms_since_first_menu=%lu ms_since_intro_F12_latch=%lu native_transition=0x%08lX native_menu_input_branch=%s game_input_active=%lu menu_ctx=%lu native_clock=%lu [READ-ONLY; NO DEVICE-HR CLAIM]",
          (long)n,(unsigned long)(DWORD)(now-first_tick_ms),
          (unsigned long)(since_request?(DWORD)(now-since_request):0u),
          (unsigned long)transition,transition?"GATED":"ELIGIBLE",
          (unsigned long)active,
          (unsigned long)(*SPIDER_TRACE_MENU_CONTEXT!=0u),
          (unsigned long)*SPIDER_TRACE_NATIVE_CLOCK_RAW);
        say(status);
      }
      previous_transition=(LONG)transition;
      previous_active=(LONG)active;
    }
    return 0;
  }
  /* Intentionally do NOT consume intro_quit_pending here: this callback
   * also runs around the two pre-menu static images. The intro exit
   * goes through the top-level native Quit check (0x455E50), after
   * game-owned movie and transition cleanup, not via an early menu. */
  if(InterlockedCompareExchange(&pending,0,1)==1) {
    InterlockedExchange(&delivered,1);
    if(InterlockedCompareExchange(&intro_still_exit_state,3,2)==2)
      say("QUICK-EXIT-F12: INTRO-STILL native page owner returned; original Shell_MainMenu Quit now activated [EXPERIMENTAL]");
    say("QUICK-EXIT-F12: native Shell_MainMenu Quit activated: slot=3 action=18, native cleanup branch=0x4941D5");
    return 1;
  }
  return 0;
}

/* x86 stack at entry to the patched game CALL:
 *   [esp+0x00] = original return address 0x00493DDB
 *   [esp+0x44] = caller's native menu selected-index local
 * pushfl + pushal each preserve the caller's flags and registers and
 * relocate those to [esp+0x24] and [esp+0x68] respectively.
 * On F12, redirect just the native RETURN to the existing item-selection
 * branch. The branch releases the real menu's objects and returns the
 * original action ID via the native table. No off-thread cleanup.
 *
 * GCC x86 MinGW basic asm syntax, function is naked (no prologue).
 * Function intentionally has no C statements or compiler-generated
 * epilogue; the only end instruction is its own RET. */
__attribute__((naked,used)) static void spider_real_menu_tick_hook(void) {
  __asm__ __volatile__(
    "pushfl\n\t"
    "pushal\n\t"
    "call _SpiderQuickExitF12_MarkMenuTickEntry\n\t"
    "movl $0x00505720, %eax\n\t"
    "call *%eax\n\t"
    "call _SpiderQuickExitF12_ConsumeRealMenuRequest\n\t"
    "testl %eax, %eax\n\t"
    "jz 1f\n\t"
    "movl $3, 0x68(%esp)\n\t"
    "movl $0x004941D5, 0x24(%esp)\n\t"
    "1:\n\t"
    "popal\n\t"
    "popfl\n\t"
    "ret\n\t"
  );
}

static int write_code(BYTE *address,const BYTE *expected,const BYTE *next,
                      SIZE_T count) {
  DWORD old_protect,ignored;
  if(memcmp(address,expected,count)!=0)return 0;
  if(!VirtualProtect(address,count,PAGE_EXECUTE_READWRITE,&old_protect))
    return 0;
  memcpy(address,next,count);
  FlushInstructionCache(GetCurrentProcess(),address,count);
  VirtualProtect(address,count,old_protect,&ignored);
  return 1;
}

static void restore_original_menu_tick(void) {
  DWORD old_protect,ignored;
  if(!VirtualProtect(SPIDER_SHELL_MENU_TICK,5,
                     PAGE_EXECUTE_READWRITE,&old_protect))return;
  memcpy(SPIDER_SHELL_MENU_TICK,shell_menu_tick_original,5);
  FlushInstructionCache(GetCurrentProcess(),SPIDER_SHELL_MENU_TICK,5);
  VirtualProtect(SPIDER_SHELL_MENU_TICK,5,old_protect,&ignored);
}

static int install_real_menu_hook(void) {
  intptr_t distance=(intptr_t)(void *)spider_real_menu_tick_hook-
                    (intptr_t)(SPIDER_SHELL_MENU_TICK+5);
  int32_t rel;
  BYTE patched[5]={0xE8,0,0,0,0};
  if(distance<(intptr_t)INT32_MIN || distance>(intptr_t)INT32_MAX)return 0;
  rel=(int32_t)distance;
  memcpy(patched+1,&rel,4);
  return write_code(SPIDER_SHELL_MENU_TICK,shell_menu_tick_original,patched,5);
}

static int patch_native_call(BYTE *site,const BYTE original[5],
                             const void *replacement) {
  intptr_t delta=(intptr_t)replacement-(intptr_t)(site+5);
  int32_t rel;
  BYTE call_bytes[5]={0xE8,0,0,0,0};
  if(delta<(intptr_t)INT32_MIN || delta>(intptr_t)INT32_MAX)return 0;
  rel=(int32_t)delta;
  memcpy(call_bytes+1,&rel,4);
  return write_code(site,original,call_bytes,5);
}

static void restore_intro_tick_call(BYTE *site,const BYTE original[5]) {
  DWORD old_protect,ignored;
  if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old_protect))return;
  memcpy(site,original,5);
  FlushInstructionCache(GetCurrentProcess(),site,5);
  VirtualProtect(site,5,old_protect,&ignored);
}

static int install_intro_tick_hooks(void) {
  if(memcmp(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1,5)!=0 ||
     memcmp(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2,5)!=0)
    return 0;
  if(!patch_native_call(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1,
                        (const void *)spider_f12_native_intro_tick))
    return 0;
  if(!patch_native_call(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2,
                        (const void *)spider_f12_native_intro_tick)) {
    restore_intro_tick_call(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1);
    return 0;
  }
  return 1;
}

static int install_passive_controller7_probe(void) {
  return patch_native_call(SPIDER_NATIVE_CONTROLLER7_FINISH_CALL,
                           native_controller7_finish_call_original,
                           (const void *)spider_observe_controller7_finish);
}

static void restore_passive_controller7_probe(void) {
  restore_intro_tick_call(SPIDER_NATIVE_CONTROLLER7_FINISH_CALL,
                          native_controller7_finish_call_original);
}

static int install_passive_game_cleanup_probe(void) {
  return patch_native_call(SPIDER_NATIVE_GAME_CLEANUP_CALL,
                           native_game_cleanup_call_original,
                           (const void *)spider_observe_native_game_cleanup);
}

static void restore_passive_game_cleanup_probe(void) {
  restore_intro_tick_call(SPIDER_NATIVE_GAME_CLEANUP_CALL,
                          native_game_cleanup_call_original);
}

/* Instrument the exact Space-release/Space-press CALLs in the real
 * 0x455400 3D owner, but preserve 0x50A650's returned DWORD unchanged.
 * If only the second patch fails, restore the first one atomically.
 */
static int install_debug_3d_space_gate_probes(void) {
  if(memcmp(SPIDER_NATIVE_3D_SPACE_CALL_1,spider_native_3d_space_original_1,5)!=0 ||
     memcmp(SPIDER_NATIVE_3D_SPACE_CALL_2,spider_native_3d_space_original_2,5)!=0)
    return 0;
  if(!patch_native_call(SPIDER_NATIVE_3D_SPACE_CALL_1,
                        spider_native_3d_space_original_1,
                        (const void *)spider_probe_3d_space_gate))
    return 0;
  if(!patch_native_call(SPIDER_NATIVE_3D_SPACE_CALL_2,
                        spider_native_3d_space_original_2,
                        (const void *)spider_probe_3d_space_gate)) {
    restore_intro_tick_call(SPIDER_NATIVE_3D_SPACE_CALL_1,
                            spider_native_3d_space_original_1);
    return 0;
  }
  InterlockedExchange(&debug_newgame_space_gate_probes_installed,1);
  return 1;
}

static void restore_debug_3d_space_gate_probes(void) {
  if(InterlockedCompareExchange(&debug_newgame_space_gate_probes_installed,0,1)!=1)
    return;
  restore_intro_tick_call(SPIDER_NATIVE_3D_SPACE_CALL_1,
                          spider_native_3d_space_original_1);
  restore_intro_tick_call(SPIDER_NATIVE_3D_SPACE_CALL_2,
                          spider_native_3d_space_original_2);
}

/* Both original 0x50A650 branches perform a single 5-byte
 * CALL to 0x501CB0. Trace only DIK_SPACE=0x39 in confirmed post-Bink
 * New Game sessions. Install both READ-ONLY hooks or none.
 * The original game owns all KEY PRESS / KEY RELEASE semantics. */
static int install_debug_raw_space_probes(void) {
  if(memcmp(SPIDER_NATIVE_KEY_EDGE_CALL,spider_native_key_edge_original,5)!=0 ||
     memcmp(SPIDER_NATIVE_KEY_LEVEL_CALL,spider_native_key_level_original,5)!=0)
    return 0;
  if(!patch_native_call(SPIDER_NATIVE_KEY_EDGE_CALL,
                        spider_native_key_edge_original,
                        (const void *)spider_probe_native_raw_key))
    return 0;
  if(!patch_native_call(SPIDER_NATIVE_KEY_LEVEL_CALL,
                        spider_native_key_level_original,
                        (const void *)spider_probe_native_raw_key)) {
    restore_intro_tick_call(SPIDER_NATIVE_KEY_EDGE_CALL,
                            spider_native_key_edge_original);
    return 0;
  }
  InterlockedExchange(&debug_newgame_lowlevel_key_probes_installed,1);
  return 1;
}

static void restore_debug_raw_space_probes(void) {
  if(InterlockedCompareExchange(&debug_newgame_lowlevel_key_probes_installed,0,1)!=1)
    return;
  restore_intro_tick_call(SPIDER_NATIVE_KEY_EDGE_CALL,
                          spider_native_key_edge_original);
  restore_intro_tick_call(SPIDER_NATIVE_KEY_LEVEL_CALL,
                          spider_native_key_level_original);
}

/* Instrument all five exact original 3D input CALL sites or none.
 * This is passive runtime attribution only; original 0x50C180
 * result and stack ownership remain unchanged. No hidden fallback
 * to a synthetic SPACE or controller-7 process EXIT. */
static int install_debug_3d_native_input_probes(void) {
  int i,j;
  for(i=0;i<SPIDER_NATIVE_3D_INPUT_SITES_COUNT;++i)
    if(memcmp(spider_native_3d_input_sites[i],
              spider_native_3d_input_original[i],5)!=0)
      return 0;
  for(i=0;i<SPIDER_NATIVE_3D_INPUT_SITES_COUNT;++i)
    if(!patch_native_call(spider_native_3d_input_sites[i],
                          spider_native_3d_input_original[i],
                          (const void *)spider_probe_3d_native_input)) {
      for(j=i-1;j>=0;--j)
        restore_intro_tick_call(spider_native_3d_input_sites[j],
                                spider_native_3d_input_original[j]);
      return 0;
    }
  InterlockedExchange(&debug_newgame_native_input_probes_installed,1);
  return 1;
}

static void restore_debug_3d_native_input_probes(void) {
  int i;
  if(InterlockedCompareExchange(&debug_newgame_native_input_probes_installed,0,1)!=1)
    return;
  for(i=0;i<SPIDER_NATIVE_3D_INPUT_SITES_COUNT;++i)
    restore_intro_tick_call(spider_native_3d_input_sites[i],
                            spider_native_3d_input_original[i]);
}

static int install_passive_game_input_probe(void) {
  return patch_native_call(SPIDER_NATIVE_GAME_INPUT_CALL,
                           native_game_input_call_original,
                           (const void *)spider_observe_native_game_input);
}

static void restore_passive_game_input_probe(void) {
  restore_intro_tick_call(SPIDER_NATIVE_GAME_INPUT_CALL,
                          native_game_input_call_original);
}

static int install_passive_pause_probe(void) {
  return patch_native_call(SPIDER_NATIVE_PAUSE_CALL,
                           native_pause_call_original,
                           (const void *)spider_observe_native_pause_handler);
}

static void restore_passive_pause_probe(void) {
  restore_intro_tick_call(SPIDER_NATIVE_PAUSE_CALL,
                          native_pause_call_original);
}

static int install_post_intro_checkpoint(void) {
  return patch_native_call(SPIDER_AFTER_INTRO_CLEANUP_CALL,
                           post_intro_cleanup_original,
                           (const void *)spider_after_intro_cleanup);
}

static void restore_post_intro_checkpoint(void) {
  restore_intro_tick_call(SPIDER_AFTER_INTRO_CLEANUP_CALL,
                          post_intro_cleanup_original);
}

/* The two ORIGINAL Shell_MainMenu call sites supply one DWORD parameter
 * each, and are cdecl. These diagnostic replacements call their original
 * target exactly once, including its normal busy-wait and clock compare.
 * There is NO bypass or mutation of the native game clock or input state.
 * The same SHA-verified DLL is already running native menu thread hooks. */
static void __cdecl spider_f12_debug_menu_wait(DWORD ticks,int which) {
  DWORD before_ms=GetTickCount();
  DWORD before_clock=*SPIDER_TRACE_NATIVE_CLOCK_RAW;
  SPIDER_NATIVE_CLOCK_WAIT(ticks);
  if(which==3 &&
     InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) {
    static volatile LONG exit_wait_calls=0;
    LONG calls=InterlockedIncrement(&exit_wait_calls);
    DWORD end_ms=GetTickCount();
    DWORD end_clock=*SPIDER_TRACE_NATIVE_CLOCK_RAW;
    DWORD elapsed_ms=(DWORD)(end_ms-before_ms);
    if(calls<=8 || elapsed_ms>=100u) {
      char line[300];
      snprintf(line,sizeof(line),
        "QUICK-EXIT-F12: DEBUG-MENU-EXIT-CLOCK-WAIT site=0x00494209 call=%ld elapsed_ms=%lu wait_arg=%lu native_clock_before=%lu native_clock_after=%lu [ORIGINAL WAIT CALLED; READ-ONLY; SELECTED ACTION UNKNOWN]",
        (long)calls,(unsigned long)elapsed_ms,(unsigned long)ticks,
        (unsigned long)before_clock,(unsigned long)end_clock);
      say(line);
    }
  }
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     InterlockedCompareExchange(&intro_still_exit_state,0,0)>=2 &&
     InterlockedCompareExchange(&menu_frame_count,0,0)<=3) {
    DWORD end_ms=GetTickCount(), end_clock=*SPIDER_TRACE_NATIVE_CLOCK_RAW;
    char line[280];
    snprintf(line,sizeof(line),
      "QUICK-EXIT-F12: DEBUG-MENU-CLOCK-WAIT site=0x%08lX frame=%ld wait_arg=%lu elapsed_ms=%lu native_clock_before=%lu native_clock_after=%lu [ORIGINAL WAIT CALLED; READ-ONLY]",
      (unsigned long)(which==1?0x0049415Bu:0x0049416Fu),
      (long)InterlockedCompareExchange(&menu_frame_count,0,0),
      (unsigned long)ticks,(unsigned long)(DWORD)(end_ms-before_ms),
      (unsigned long)before_clock,(unsigned long)end_clock);
    say(line);
  }
}

static void __cdecl spider_f12_debug_menu_wait_cond(DWORD ticks) {
  spider_f12_debug_menu_wait(ticks,1);
}
static void __cdecl spider_f12_debug_menu_wait_always(DWORD ticks) {
  spider_f12_debug_menu_wait(ticks,2);
}
static void __cdecl spider_f12_debug_menu_wait_exit(DWORD ticks) {
  spider_f12_debug_menu_wait(ticks,3);
}

/* The third site is exclusively diagnostic. All three native clock wait
 * sites execute the original 0x4E5D60 function without bypass. */
static void install_debug_menu_wait_exit_probe(void) {
  if(!patch_native_call(SPIDER_MENU_WAIT_EXIT_SITE,
                        menu_wait_exit_original,
                        (const void *)spider_f12_debug_menu_wait_exit)) {
    say("QUICK-EXIT-F12: DEBUG-MENU-EXIT-CLOCK-WAIT optional 0x494209 signature mismatch; original menu transition untouched");
    return;
  }
  InterlockedExchange(&debug_menu_wait_exit_hook_installed,1);
  say("QUICK-EXIT-F12: DEBUG-MENU-EXIT-CLOCK-WAIT read-only native menu selection wait at 0x494209 armed; original 0x4E5D60 still called");
}

static int install_debug_menu_wait_hooks(void) {
  if(memcmp(SPIDER_MENU_WAIT_COND_SITE,menu_wait_cond_original,5)!=0 ||
     memcmp(SPIDER_MENU_WAIT_ALWAYS_SITE,menu_wait_always_original,5)!=0)
    return 0;
  if(!patch_native_call(SPIDER_MENU_WAIT_COND_SITE,menu_wait_cond_original,
                        (const void *)spider_f12_debug_menu_wait_cond))
    return 0;
  if(!patch_native_call(SPIDER_MENU_WAIT_ALWAYS_SITE,menu_wait_always_original,
                        (const void *)spider_f12_debug_menu_wait_always)) {
    restore_intro_tick_call(SPIDER_MENU_WAIT_COND_SITE,menu_wait_cond_original);
    return 0;
  }
  InterlockedExchange(&debug_menu_wait_hooks_installed,1);
  return 1;
}

static void restore_debug_menu_wait_hooks(void) {
  if(InterlockedCompareExchange(&debug_menu_wait_exit_hook_installed,0,1)==1)
    restore_intro_tick_call(SPIDER_MENU_WAIT_EXIT_SITE,menu_wait_exit_original);
  if(InterlockedCompareExchange(&debug_menu_wait_hooks_installed,0,1)==1) {
    restore_intro_tick_call(SPIDER_MENU_WAIT_ALWAYS_SITE,menu_wait_always_original);
    restore_intro_tick_call(SPIDER_MENU_WAIT_COND_SITE,menu_wait_cond_original);
  }
}

static int install_intro_timed_clock_hook(void) {
  if(!patch_native_call(SPIDER_INTRO_TIMED_CLOCK_SITE,
                        intro_timed_clock_original,
                        (const void *)spider_f12_debug_timed_clock))
    return 0;
  InterlockedExchange(&intro_timed_clock_hook_installed,1);
  return 1;
}

static void restore_intro_timed_clock_hook(void) {
  if(InterlockedCompareExchange(&intro_timed_clock_hook_installed,0,1)==1)
    restore_intro_tick_call(SPIDER_INTRO_TIMED_CLOCK_SITE,
                            intro_timed_clock_original);
}

/* The New Game comic's clock read uses six ORIGINAL bytes, unlike
 * the five-byte pre-menu timed still. CALL rel32 + NOP is instruction-
 * boundary exact and returns to the original SUB EDX,ESI at 0x4279BE.
 * Reject any version mismatch before writing a single instruction. */
static int install_newgame_comic_clock_hook(void) {
  intptr_t delta=(intptr_t)(const void *)spider_f12_debug_comic_clock-
                 (intptr_t)(SPIDER_NEWGAME_COMIC_CLOCK_SITE+5);
  int32_t rel;
  BYTE patched[6]={0xE8,0,0,0,0,0x90};
  if(memcmp(SPIDER_NEWGAME_L1COV_OWNER,newgame_l1cov_owner_signature,5)!=0 ||
     memcmp(SPIDER_NEWGAME_MOVIE4_OPCODE,newgame_movie4_opcode_signature,7)!=0)
    return 0;
  if(delta<(intptr_t)INT32_MIN || delta>(intptr_t)INT32_MAX)return 0;
  rel=(int32_t)delta;
  memcpy(patched+1,&rel,4);
  if(!write_code(SPIDER_NEWGAME_COMIC_CLOCK_SITE,
                 newgame_comic_clock_original,patched,6))return 0;
  InterlockedExchange(&debug_newgame_comic_clock_hook_installed,1);
  return 1;
}

static void restore_newgame_comic_clock_hook(void) {
  DWORD old_protect,ignored;
  if(InterlockedCompareExchange(&debug_newgame_comic_clock_hook_installed,0,1)!=1)
    return;
  if(!VirtualProtect(SPIDER_NEWGAME_COMIC_CLOCK_SITE,6,
                     PAGE_EXECUTE_READWRITE,&old_protect))return;
  memcpy(SPIDER_NEWGAME_COMIC_CLOCK_SITE,newgame_comic_clock_original,6);
  FlushInstructionCache(GetCurrentProcess(),SPIDER_NEWGAME_COMIC_CLOCK_SITE,6);
  VirtualProtect(SPIDER_NEWGAME_COMIC_CLOCK_SITE,6,old_protect,&ignored);
}

static int install_intro_still_input_hook(void) {
  if(!patch_native_call(SPIDER_INTRO_STILL_INPUT_CALL,intro_still_input_original,
                        (const void *)spider_f12_intro_still_native_input))
    return 0;
  InterlockedExchange(&intro_still_hook_installed,1);
  return 1;
}

static void restore_intro_still_input_hook(void) {
  if(InterlockedCompareExchange(&intro_still_hook_installed,0,1)==1)
    restore_intro_tick_call(SPIDER_INTRO_STILL_INPUT_CALL,intro_still_input_original);
}

static void restore_intro_tick_hooks(void) {
  restore_intro_tick_call(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1);
  restore_intro_tick_call(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2);
}

static int suppress_unsafe_native_f12(void) {
  return write_code(SPIDER_NATIVE_F12_POST,native_f12_original,
                    native_f12_bypass,sizeof(native_f12_original));
}

static DWORD WINAPI f12_input_thread(LPVOID ignored) {
  BOOL previously_down=TRUE;
  LONG previous_3d_space=-1;
  (void)ignored;
  for(;;) {
    HWND hwnd=(HWND)InterlockedCompareExchangePointer(&game_hwnd,NULL,NULL);
    BOOL focused=hwnd && IsWindow(hwnd) && GetForegroundWindow()==hwnd;
    BOOL down=(GetAsyncKeyState(VK_F12)&0x8000)!=0;
    /* Observe each separate physical SPACE press after Bink movie #4,
     * without delivering input to the game. Unlike the first two
     * SPACE presses in the user's 2026-10-23 15:57:30 Wine test, its
     * THIRD press was a player JUMP during active gameplay, NOT a
     * third cinematic. This counter is ONLY a raw physical event
     * count; never derive cinematic count, gameplay readiness or
     * controller-7 shutdown permission from it. */
    if(focused &&
       InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
       InterlockedCompareExchange(&debug_newgame_progress,0,0)==3) {
      LONG space_down=(GetAsyncKeyState(VK_SPACE)&0x8000)!=0;
      if(previous_3d_space==0 && space_down) {
        LONG count=InterlockedIncrement(&debug_newgame_3d_space_edges);
        DWORD since=(DWORD)InterlockedCompareExchange(&debug_newgame_3d_first_ms,0,0);
        DWORD now=GetTickCount();
        if(count<=8) {
          char line[280];
          snprintf(line,sizeof(line),
            "QUICK-EXIT-F12: DEBUG-NEWGAME-3D-SPACE-EVENT edge=%ld since_3d_ms=%lu [READ-ONLY; RAW PHYSICAL PRESS ONLY; MAY BE GAMEPLAY JUMP; NO SCENE OR EXIT AUTHORITY]",
            (long)count,(unsigned long)(since?(DWORD)(now-since):0u));
          say(line);
        }
      }
      previous_3d_space=space_down;
    } else {
      previous_3d_space=-1;
    }
    /* A comic page has a finite automatic lifetime. Fail closed:
     * do not leave an F12 request armed if the expected video never
     * begins. No original game resources are modified here. */
    if((InterlockedCompareExchange(&comic_exit_opt_in,0,0)==1 ||
        InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) &&
       InterlockedCompareExchange(&postmovie_state,0,0)==4) {
      DWORD start=(DWORD)InterlockedCompareExchange(&postmovie_request_ms,0,0);
      if(!start || (DWORD)(GetTickCount()-start)>12000u) {
        if(InterlockedCompareExchange(&postmovie_state,0,4)==4)
          say("QUICK-EXIT-F12: COMIC F12 deferred intent timed out; no native movie interrupted");
      }
    }
    /* Fail closed if native input-wait or real menu never arrives. */
    if(InterlockedCompareExchange(&intro_still_opt_in,0,0)==1) {
      LONG state=InterlockedCompareExchange(&intro_still_exit_state,0,0);
      if(state==1 || state==2) {
        DWORD start=(DWORD)InterlockedCompareExchange(&intro_still_request_ms,0,0);
        if(!start || (DWORD)(GetTickCount()-start)>15000u) {
          if(InterlockedCompareExchange(&intro_still_exit_state,0,state)==state) {
            if(state==2)InterlockedExchange(&pending,0);
            say("QUICK-EXIT-F12: INTRO-STILL request timed out; no late menu Quit will be triggered");
          }
        }
      }
    }
    if(!focused) {
      previously_down=TRUE;
    } else if(!down) {
      previously_down=FALSE;
    } else if(!previously_down) {
      DWORD now=GetTickCount();
      LONG menu_time=InterlockedCompareExchange(&last_menu_frame_ms,0,0);
      LONG movie_time=InterlockedCompareExchange(&last_movie_frame_ms,0,0);
      previously_down=TRUE;
      if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) {
        if(InterlockedCompareExchange(&real_main_menu_seen,0,0)==0 &&
           InterlockedCompareExchange(&post_intro_cleanup_seen,0,0)==1 &&
           *SPIDER_GAME_BINK_ACTIVE==0u &&
           *SPIDER_BINK_VIDEO_HANDLE==0u &&
           *SPIDER_TRACE_MENU_CONTEXT!=0u &&
           *SPIDER_TRACE_TIMED_PAGE_ONCE==1u &&
           *SPIDER_ENGINE_MODE==0u) {
          /* Native first timed page finishes normally. The original
           * second-page input-wait owner then completes without Quit. */
          if(InterlockedCompareExchange(&intro_still_exit_state,0,0)==0) {
            InterlockedExchange(&intro_still_request_ms,(LONG)now);
            if(InterlockedCompareExchange(&intro_still_exit_state,1,0)==0)
              say("QUICK-EXIT-F12: DEBUG-MENU-SKIP F12 on startup still; awaiting game-owned page completion (NO QUIT)");
          }
        } else if(InterlockedCompareExchange(&real_main_menu_seen,0,0)==0 &&
                  ((movie_time && (DWORD)(now-(DWORD)movie_time)<750u) ||
                   *SPIDER_GAME_BINK_ACTIVE!=0u)) {
          if(InterlockedCompareExchange(&intro_quit_pending,1,0)==0)
            say("QUICK-EXIT-F12: DEBUG-MENU-SKIP F12 in opening Bink; native video cleanup then main menu (NO QUIT)");
        } else if(InterlockedCompareExchange(&real_main_menu_seen,0,0)==1 &&
                  InterlockedCompareExchange(&input_native_invocations,0,0)==0 &&
                  *SPIDER_ENGINE_MODE==0u &&
                  *SPIDER_GAME_BINK_ACTIVE==0u &&
                  *SPIDER_BINK_VIDEO_HANDLE==0u &&
                  InterlockedCompareExchange(&last_comic_candidate_ms,0,0)!=0 &&
                  (DWORD)(now-(DWORD)InterlockedCompareExchange(&last_comic_candidate_ms,0,0))<=6500u) {
          /* F12 during L1COV.bmp: the exact native clock-read hook
           * at 0x4279B8 (when installed) supplies a locally expired
           * tick so original 0x4279C6 cleanup/continuation runs now.
           * The one-shot request stays live for verified Bink #4.
           * A mismatched hook safely falls back to normal timeout. */
          if(InterlockedCompareExchange(&postmovie_state,0,0)==0) {
            InterlockedExchange(&postmovie_request_ms,(LONG)now);
            if(InterlockedCompareExchange(&postmovie_state,4,0)==0)
              say(InterlockedCompareExchange(&debug_newgame_comic_clock_hook_installed,0,0)==1 ?
                  "QUICK-EXIT-F12: DEBUG-NEWGAME-COMIC F12 armed; original L1COV.bmp 300-tick completion will be met at next 0x4279B8 poll, followed by native movie #4 (NO EXIT)" :
                  "QUICK-EXIT-F12: DEBUG-NEWGAME-COMIC native clock hook unavailable; fallback to original comic timer before movie #4 (NO EXIT)");
          }
        } else if(InterlockedCompareExchange(&real_main_menu_seen,0,0)==1 &&
                  InterlockedCompareExchange(&debug_newgame_progress,0,0)==1 &&
                  *SPIDER_ENGINE_MODE==0u &&
                  *SPIDER_GAME_BINK_ACTIVE!=0u &&
                  *SPIDER_BINK_VIDEO_HANDLE!=0u) {
          if(InterlockedCompareExchange(&postmovie_state,0,0)==0) {
            InterlockedExchange(&postmovie_request_ms,(LONG)now);
            if(InterlockedCompareExchange(&postmovie_state,1,0)==0)
              say("QUICK-EXIT-F12: DEBUG-NEWGAME-MOVIE F12 queued for exact native Enter-equivalent Bink termination, NO EXIT");
          }
        } else if(InterlockedCompareExchange(&debug_newgame_progress,0,0)>=1) {
          /* Movie #4 conclusively preceded this phase. TWO subsequent
           * 3D scenes can be skipped with SPACE: Stan Lee's narrated
           * introduction and the next 3D cinematic. Neither scene's
           * native end condition or playable boundary has been mapped.
           * An unclassified F12 must NEVER become a controller-7 exit.
           * Capture evidence and consume the key edge safely. */
          say("QUICK-EXIT-F12: DEBUG-NEWGAME-3D F12 consumed: TWO SPACE-skippable 3D introductions vs gameplay NOT YET VERIFIED; native EXIT BLOCKED [NO INPUT INJECTION]");
        } else if(InterlockedCompareExchange(&real_main_menu_seen,0,0)==1 &&
                  InterlockedCompareExchange(&experimental_game3d_enabled,0,0)==1 &&
                  *SPIDER_ENGINE_MODE==0u &&
                  *SPIDER_GAME_BINK_ACTIVE==0u &&
                  *SPIDER_BINK_VIDEO_HANDLE==0u &&
                  InterlockedCompareExchange(&last_native_game_input_ms,0,0)!=0 &&
                  (DWORD)(now-(DWORD)InterlockedCompareExchange(&last_native_game_input_ms,0,0))<350u &&
                  InterlockedCompareExchange(&last_game3d_flip_ms,0,0)!=0 &&
                  (DWORD)(now-(DWORD)InterlockedCompareExchange(&last_game3d_flip_ms,0,0))<350u) {
          if(InterlockedCompareExchange(&game3d_exit_state,1,0)==0)
            say("QUICK-EXIT-F12: DEBUG-3D-EXIT native gameplay exit queued; waiting for original controller-7 cleanup");
        } else {
          say("QUICK-EXIT-F12: DEBUG F12 ignored outside recognized startup/New Game stages and active 3D gameplay");
        }
      } else if(InterlockedCompareExchange(&comic_exit_opt_in,0,0)==1 &&
                InterlockedCompareExchange(&postmovie_opt_in,0,0)==1 &&
                InterlockedCompareExchange(&experimental_game3d_enabled,0,0)==1 &&
                InterlockedCompareExchange(&real_main_menu_seen,0,0)==1 &&
                InterlockedCompareExchange(&input_native_invocations,0,0)==0 &&
                *SPIDER_ENGINE_MODE==0u &&
                *SPIDER_GAME_BINK_ACTIVE==0u &&
                *SPIDER_BINK_VIDEO_HANDLE==0u &&
                InterlockedCompareExchange(&last_comic_candidate_ms,0,0)!=0 &&
                (DWORD)(now-(DWORD)InterlockedCompareExchange(
                  &last_comic_candidate_ms,0,0))<=6500u) {
        /* Comic native owner is not known. Never cut its timer short:
         * defer F12 and let it finish on its own, then interrupt
         * ONLY the exact verified New Game video #4 when it starts. */
        if(InterlockedCompareExchange(&postmovie_state,0,0)==0) {
          InterlockedExchange(&postmovie_request_ms,(LONG)now);
          if(InterlockedCompareExchange(&postmovie_state,4,0)==0)
            say("QUICK-EXIT-F12: COMIC F12 deferred until native New Game movie 4; comic continues normally [EXPERIMENTAL]");
        }
      } else if(menu_time && (DWORD)(now-(DWORD)menu_time)<=400u) {
        /* Give the previously Windows-validated main-menu path priority,
         * including a key press just after the last decoded intro frame. */
        unsigned elapsed=0;
        InterlockedExchange(&delivered,0);
        InterlockedExchange(&pending,1);
        say("QUICK-EXIT-F12: F12 queued for REAL Shell_MainMenu (0x493DD6)");
        while(elapsed<800u && InterlockedCompareExchange(&delivered,0,0)==0) {
          Sleep(20);
          elapsed+=20;
        }
        InterlockedExchange(&pending,0);
        if(InterlockedCompareExchange(&delivered,0,0)==0)
          say("QUICK-EXIT-F12: native menu frame not reached within 800ms; no exit");
      } else if(InterlockedCompareExchange(&intro_still_opt_in,0,0)==1 &&
                 InterlockedCompareExchange(&post_intro_cleanup_seen,0,0)==1 &&
                 InterlockedCompareExchange(&real_main_menu_seen,0,0)==0 &&
                 InterlockedCompareExchange(&menu_frame_count,0,0)==0 &&
                 *SPIDER_GAME_BINK_ACTIVE==0u &&
                 *SPIDER_BINK_VIDEO_HANDLE==0u &&
                 *SPIDER_TRACE_MENU_CONTEXT!=0u &&
                 *SPIDER_TRACE_TIMED_PAGE_ONCE==1u &&
                 *SPIDER_ENGINE_MODE==0u) {
        /* First static timed page runs naturally; second page ends
         * through its verified native 0x4A18AE completion path. */
        if(InterlockedCompareExchange(&intro_still_exit_state,0,0)==0) {
          InterlockedExchange(&intro_still_request_ms,(LONG)now);
          if(InterlockedCompareExchange(&intro_still_exit_state,1,0)==0)
            say("QUICK-EXIT-F12: INTRO-STILL F12 latched after original Bink cleanup; waiting for native timed page + input-wait owner [EXPERIMENTAL]");
        }
      } else if(InterlockedCompareExchange(&real_main_menu_seen,0,0)==0 &&
                ((movie_time && (DWORD)(now-(DWORD)movie_time)<750u) ||
                 *SPIDER_GAME_BINK_ACTIVE!=0u)) {
        /* Set a one-shot intent: Bink tick wrappers finish remaining
         * startup movies through game-owned cleanup; the original
         * post-movie transition then forwards native Quit before
         * the top-level engine's startup-state check. */
        if(InterlockedCompareExchange(&intro_quit_pending,1,0)==0)
           say("QUICK-EXIT-F12: intro movie exit requested; native Bink cleanup then top-level engine Quit");
      } else if(InterlockedCompareExchange(&postmovie_opt_in,0,0)==1 &&
                InterlockedCompareExchange(&experimental_game3d_enabled,0,0)==1 &&
                InterlockedCompareExchange(&real_main_menu_seen,0,0)==1 &&
                InterlockedCompareExchange(&input_native_invocations,0,0)==0 &&
                InterlockedCompareExchange(&last_comic_candidate_ms,0,0)!=0 &&
                (DWORD)(now-(DWORD)InterlockedCompareExchange(
                  &last_comic_candidate_ms,0,0))<180000u &&
                *SPIDER_GAME_BINK_ACTIVE!=0u &&
                *SPIDER_BINK_VIDEO_HANDLE!=0u) {
        /* F12 worker only queues intent: native tick wrapper will
         * verify the specific movie owner before interrupting it. */
        if(InterlockedCompareExchange(&postmovie_state,0,0)==0) {
          /* Publish timestamp BEFORE state=1: the native Bink tick
           * executes on a separate thread and may consume immediately. */
          InterlockedExchange(&postmovie_request_ms,(LONG)now);
          if(InterlockedCompareExchange(&postmovie_state,1,0)==0)
            say("QUICK-EXIT-F12: POSTMENU-MOVIE F12 intent requested; original Bink owner verification pending [EXPERIMENTAL]");
        }
      } else if(InterlockedCompareExchange(&experimental_game3d_enabled,0,0)==1 &&
                InterlockedCompareExchange(&real_main_menu_seen,0,0)==1 &&
                *SPIDER_ENGINE_MODE==0u &&
                *SPIDER_GAME_BINK_ACTIVE==0u &&
                InterlockedCompareExchange(&last_native_game_input_ms,0,0)!=0 &&
                (DWORD)(now-(DWORD)InterlockedCompareExchange(&last_native_game_input_ms,0,0))<350u &&
                InterlockedCompareExchange(&last_game3d_flip_ms,0,0)!=0 &&
                (DWORD)(now-(DWORD)InterlockedCompareExchange(&last_game3d_flip_ms,0,0))<350u) {
        /* F12 thread records ONLY the intent. Original game thread
         * must run the 3D frame, mode cleanup and top-level dispatch. */
        if(InterlockedCompareExchange(&game3d_exit_state,1,0)==0)
          say("QUICK-EXIT-F12: 3D native exit intent queued; waiting for original gameplay tick");
      } else {
        say("QUICK-EXIT-F12: REAL Shell_MainMenu, startup movie or recognized 3D owner not active; F12 safely ignored");
      }
    }
    Sleep(25);
  }
  return 0;
}

void SpiderQuickExitF12_Init(void (*log_fn)(const char *)) {
  char exe[MAX_PATH],ini[MAX_PATH],val[24],trace[8],static_trace[8],movie_trial[8],comic_trial[8],*slash;
  DWORD n;
  HANDLE thread;
  logger=log_fn;
  n=GetModuleFileNameA(NULL,exe,sizeof(exe));
  if(!n || n>=sizeof(exe) || !(slash=strrchr(exe,'\\'))) {
    say("QUICK-EXIT-F12: disabled (INI path unavailable)");
    return;
  }
  if((size_t)(slash-exe)+sizeof("SpiderMan2k1Fix.ini")>sizeof(ini)) {
    say("QUICK-EXIT-F12: disabled (INI path too long)");
    return;
  }
  memcpy(ini,exe,(size_t)(slash-exe)+1);
  strcpy(ini+(slash-exe)+1,"SpiderMan2k1Fix.ini");
  GetPrivateProfileStringA("System","QuickExitF12","0",val,sizeof(val),ini);
  if(GetPrivateProfileIntA("Debug","F12TestMode",0,ini)==1) {
    /* One diagnostic switch is sufficient: retain the proven native
     * hook setup without requiring any legacy System F12 option. */
    InterlockedExchange(&debug_f12_test_mode,1);
    strcpy(val,"1");
    say("QUICK-EXIT-F12: [Debug] F12TestMode=1 — pre-menu F12 to main menu, 3D F12 native exit; overrides old F12 behavior [EXPERIMENTAL]");
  }
  memset(trace,0,sizeof(trace));
  if(GetPrivateProfileIntA("Debug","Log",0,ini)==1 &&
     GetEnvironmentVariableA("SPIDERMAN2K1FIX_F12_PARENT_TRACE",
                             trace,sizeof(trace))==1 && trace[0]=='1') {
    InterlockedExchange(&parent_trace_enabled,1);
    say("QUICK-EXIT-F12: optional passive native Bink parent trace armed (one sample after interactive menu)");
  }
  memset(static_trace,0,sizeof(static_trace));
  if(strcmp(val,"1")==0 &&
     GetPrivateProfileIntA("Debug","Log",0,ini)==1 &&
     GetEnvironmentVariableA("SPIDERMAN2K1FIX_F12_STATIC_TRACE",
                             static_trace,sizeof(static_trace))==1 &&
     static_trace[0]=='1') {
    InterlockedExchange(&static_screen_trace_enabled,1);
    say("QUICK-EXIT-F12: STARTUP-STATIC-PROBE opt-in enabled (passive, no extra hooks; 90s/61 samples max)");
  }
  /* Separate extra opt-in for risky native gameplay shutdown prototype.
   * Enabling QuickExitF12 alone retains the previous menu/Bink behavior. */
  InterlockedExchange(&experimental_game3d_enabled,
    (InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 ||
     GetPrivateProfileIntA("System","ExperimentalQuickExitF12In3D",0,ini)==1) ? 1 : 0);
  memset(movie_trial,0,sizeof(movie_trial));
  /* The explicit INI options make tested Wine installations easier
   * to configure. Keep the original one-process environment switches
   * as backwards-compatible alternatives for controlled experiments.
   * Both independent stages remain OFF by default, and neither
   * bypasses the existing QuickExitF12, 3D and Debug Log gates. */
  if(strcmp(val,"1")==0 &&
     GetPrivateProfileIntA("Debug","Log",0,ini)==1 &&
     InterlockedCompareExchange(&experimental_game3d_enabled,0,0)==1 &&
     (GetPrivateProfileIntA("System","ExperimentalQuickExitF12InNewGameMovie",0,ini)==1 ||
      (GetEnvironmentVariableA("SPIDERMAN2K1FIX_F12_POSTMENU_MOVIE_EXIT",
                               movie_trial,sizeof(movie_trial))==1 &&
       movie_trial[0]=='1'))) {
    InterlockedExchange(&postmovie_opt_in,1);
    say("QUICK-EXIT-F12: POSTMENU-MOVIE opt-in enabled for New Game Bink video 4 only [EXPERIMENTAL; NOT YET WINDOWS-VALIDATED]");
  }
  memset(comic_trial,0,sizeof(comic_trial));
  if(InterlockedCompareExchange(&postmovie_opt_in,0,0)==1 &&
     (GetPrivateProfileIntA("System","ExperimentalQuickExitF12InNewGameComic",0,ini)==1 ||
      (GetEnvironmentVariableA("SPIDERMAN2K1FIX_F12_COMIC_EXIT",
                               comic_trial,sizeof(comic_trial))==1 &&
       comic_trial[0]=='1'))) {
    InterlockedExchange(&comic_exit_opt_in,1);
    say("QUICK-EXIT-F12: COMIC F12 deferred-to-Bink experiment armed (12s timeout; comic native timing unchanged) [EXPERIMENTAL; NOT YET WINDOWS-VALIDATED]");
  }
  if(strcmp(val,"1")==0 &&
     (InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 ||
      (GetPrivateProfileIntA("Debug","Log",0,ini)==1 &&
       GetPrivateProfileIntA("System","ExperimentalQuickExitF12InIntroStills",0,ini)==1))) {
    InterlockedExchange(&intro_still_opt_in,1);
    say("QUICK-EXIT-F12: INTRO-STILL F12 opt-in enabled (exact 0x4A1896 native owner; Windows/Wine UNVALIDATED)");
  }
  /* Even with opt-in OFF, suppress the game's original unsafe
   * VK_F12 -> WM_CLOSE, which is known to orphan Bink and the process. */
  if(strcmp(val,"1")!=0) {
    if(suppress_unsafe_native_f12())
      say("QUICK-EXIT-F12: OFF; unsafe original F12 WM_CLOSE suppressed");
    else
      say("QUICK-EXIT-F12: OFF; native F12 signature mismatch");
    return;
  }
  /* Shell_MainMenu has 8 entries. Slot 3 must be "quit" action=0x12,
   * as verified from 0x00552AA8 + 3*40 + 12. Verify this as well as
   * both exact game code sites before touching ANY instruction. */
  if(memcmp(SPIDER_NATIVE_F12_POST,native_f12_original,6)!=0 ||
     memcmp(SPIDER_SHELL_MENU_TICK,shell_menu_tick_original,5)!=0 ||
     memcmp(SPIDER_INTRO_TICK_CALL_1,intro_tick_call_original_1,5)!=0 ||
     memcmp(SPIDER_INTRO_TICK_CALL_2,intro_tick_call_original_2,5)!=0 ||
     memcmp(SPIDER_AFTER_INTRO_CLEANUP_CALL,post_intro_cleanup_original,5)!=0 ||
     memcmp(SPIDER_NATIVE_PAUSE_CALL,native_pause_call_original,5)!=0 ||
     memcmp(SPIDER_NATIVE_GAME_INPUT_CALL,native_game_input_call_original,5)!=0 ||
     memcmp(SPIDER_NATIVE_GAME_CLEANUP_CALL,native_game_cleanup_call_original,5)!=0 ||
     memcmp(SPIDER_NATIVE_CONTROLLER7_FINISH_CALL,native_controller7_finish_call_original,5)!=0 ||
     memcmp(SPIDER_TOPLEVEL_NATIVE_QUIT_TEST,
            top_level_native_quit_test_original,6)!=0 ||
     memcmp(SPIDER_SHELL_MENU_QUIT_RETURN,
            shell_menu_quit_return_signature,5)!=0 ||
     *(const DWORD *)(SPIDER_SHELL_MENU_ITEMS+3*40+12)!=0x12u) {
    say("QUICK-EXIT-F12: DISABLED: real Shell_MainMenu code/table signature mismatch");
    return;
  }
  if(!install_real_menu_hook()) {
    say("QUICK-EXIT-F12: DISABLED: real Shell_MainMenu tick hook failed");
    return;
  }
  if(!install_intro_tick_hooks()) {
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: DISABLED: original Bink tick hooks could not be installed");
    return;
  }
  if(!install_post_intro_checkpoint()) {
    restore_intro_tick_hooks();
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: DISABLED: native post-intro cleanup hook could not be installed");
    return;
  }
  if(!install_passive_pause_probe()) {
    restore_post_intro_checkpoint();
    restore_intro_tick_hooks();
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: WARNING: native pause probe not installed; F12 remains disabled for safety");
    return;
  }
  if(!install_passive_game_input_probe()) {
    restore_passive_pause_probe();
    restore_post_intro_checkpoint();
    restore_intro_tick_hooks();
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: DISABLED: native game input probe signature mismatch or install failed");
    return;
  }
  if(!install_passive_game_cleanup_probe()) {
    restore_passive_game_input_probe();
    restore_passive_pause_probe();
    restore_post_intro_checkpoint();
    restore_intro_tick_hooks();
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: DISABLED: passive native 3D cleanup probe could not be installed");
    return;
  }
  if(!install_passive_controller7_probe()) {
    restore_passive_game_cleanup_probe();
    restore_passive_game_input_probe();
    restore_passive_pause_probe();
    restore_post_intro_checkpoint();
    restore_intro_tick_hooks();
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: DISABLED: native controller-7 gate probe could not be installed");
    return;
  }
  if(InterlockedCompareExchange(&intro_still_opt_in,0,0)==1 &&
     !install_intro_still_input_hook()) {
    InterlockedExchange(&intro_still_opt_in,0);
    if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) {
      /* Never fall back to legacy F12 Quit if the debug mode's
       * required native menu-skip hook is missing. Undo every
       * installed game-thread hook, preserve original F12 WM_CLOSE
       * suppression if possible, and leave this test mode OFF. */
      InterlockedExchange(&debug_f12_test_mode,0);
      InterlockedExchange(&experimental_game3d_enabled,0);
      restore_passive_controller7_probe();
      restore_passive_game_cleanup_probe();
      restore_passive_game_input_probe();
      restore_passive_pause_probe();
      restore_post_intro_checkpoint();
      restore_intro_tick_hooks();
      restore_original_menu_tick();
      (void)suppress_unsafe_native_f12();
      say("QUICK-EXIT-F12: DEBUG F12TestMode DISABLED: required native still hook unavailable; all F12 actions fail closed");
      return;
    } else
      say("QUICK-EXIT-F12: INTRO-STILL native hook failed; still prototype disabled, other modes preserved");
  }
  /* Native timer clock hook is installed ONLY for F12TestMode.
   * A missing/mismatched exact opcode cannot silently revert to an
   * old Quit path: fail closed and restore ALL hooks. */
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     !install_intro_timed_clock_hook()) {
    InterlockedExchange(&debug_f12_test_mode,0);
    InterlockedExchange(&experimental_game3d_enabled,0);
    InterlockedExchange(&intro_still_opt_in,0);
    restore_intro_still_input_hook();
    restore_passive_controller7_probe();
    restore_passive_game_cleanup_probe();
    restore_passive_game_input_probe();
    restore_passive_pause_probe();
    restore_post_intro_checkpoint();
    restore_intro_tick_hooks();
    restore_original_menu_tick();
    (void)suppress_unsafe_native_f12();
    say("QUICK-EXIT-F12: DEBUG F12TestMode DISABLED: timed-still clock signature mismatch or hook failure; all F12 actions fail closed");
    return;
  }
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1)
    say("QUICK-EXIT-F12: DEBUG first timed-page clock read 0x4A1A26 guarded hook installed; F12 timeout accelerated via native 0x4A1A36 [EXPERIMENTAL]");
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     GetPrivateProfileIntA("Debug","Log",0,ini)==1) {
    if(install_debug_menu_wait_hooks()) {
      say("QUICK-EXIT-F12: DEBUG-MENU-CLOCK-WAIT 0x49415B/0x49416F native wait callsites instrumented; original native wait always called (passive trace only)");
      if(GetPrivateProfileIntA("Debug","Log",0,ini)==1)
        install_debug_menu_wait_exit_probe();
    } else {
      say("QUICK-EXIT-F12: DEBUG-MENU-CLOCK-WAIT signature mismatch; original waits preserved (optional read-only probe disabled)");
    }
  }
  /* The comic's original L1COV.bmp owner and Bink #4 are verified by
   * the supported EXE's bytes, not inferred from drawing geometry.
   * If not exact, leave timer untouched and retain deferred fallback. */
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1) {
    if(install_newgame_comic_clock_hook())
      say("QUICK-EXIT-F12: DEBUG-NEWGAME-COMIC native L1COV.bmp 0x4279B8 clock-read hook installed (F12 local 300-tick expiry; original 0x4279C6 branch retained)");
    else
      say("QUICK-EXIT-F12: DEBUG-NEWGAME-COMIC exact owner/clock signature mismatch; native comic timer unchanged, deferred movie-4 fallback");
  }
  /* Optional diagnostic requires F12TestMode and Log, and runs only
   * from the original 3D input CALL sites. If anything mismatches,
   * leave those calls completely untouched. */
  if(InterlockedCompareExchange(&debug_f12_test_mode,0,0)==1 &&
     GetPrivateProfileIntA("Debug","Log",0,ini)==1) {
    if(install_debug_3d_native_input_probes())
      say("QUICK-EXIT-F12: DEBUG-3D-NATIVE-INPUT five original 0x50C180 3D input callsites traced (original result untouched; SPACE owner and playable boundary UNKNOWN)");
    else
      say("QUICK-EXIT-F12: DEBUG-3D-NATIVE-INPUT callsite signatures mismatch; passive 3D input probes disabled, F12 exit guard preserved");
    if(install_debug_3d_space_gate_probes())
      say("QUICK-EXIT-F12: DEBUG-3D-SPACE-GATE original 0x455427/0x455444 DIK_SPACE=0x39 and flag 0x100 gate traced (original polls unchanged; gate INACTIVE in previous Wine runs)");
    else
      say("QUICK-EXIT-F12: DEBUG-3D-SPACE-GATE exact original callsites mismatch; probes disabled, original SPACE and F12 guard preserved");
    if(install_debug_raw_space_probes())
      say("QUICK-EXIT-F12: DEBUG-3D-RAW-SPACE original 0x50A650 -> 0x501CB0 edge/level key-read sites traced (DIK_SPACE only, original raw result unchanged, origin logged)");
    else
      say("QUICK-EXIT-F12: DEBUG-3D-RAW-SPACE exact low-level 0x50A65D/0x50A674 signatures mismatch; probes disabled; F12 safety unchanged");
  }
  if(!suppress_unsafe_native_f12()) {
    restore_debug_raw_space_probes();
    restore_debug_3d_space_gate_probes();
    restore_debug_3d_native_input_probes();
    restore_newgame_comic_clock_hook();
    restore_debug_menu_wait_hooks();
    restore_intro_timed_clock_hook();
    restore_intro_still_input_hook();
    restore_passive_controller7_probe();
    restore_passive_game_cleanup_probe();
    restore_passive_game_input_probe();
    restore_passive_pause_probe();
    restore_post_intro_checkpoint();
    restore_intro_tick_hooks();
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: DISABLED: native F12 WM_CLOSE suppression failed");
    return;
  }
  InterlockedExchange(&enabled,1);
  thread=CreateThread(NULL,0,f12_input_thread,NULL,0,NULL);
  if(!thread) {
    InterlockedExchange(&enabled,0);
    restore_debug_raw_space_probes();
    restore_debug_3d_space_gate_probes();
    restore_debug_3d_native_input_probes();
    restore_newgame_comic_clock_hook();
    restore_debug_menu_wait_hooks();
    restore_intro_timed_clock_hook();
    restore_intro_still_input_hook();
    restore_passive_controller7_probe();
    restore_passive_game_cleanup_probe();
    restore_passive_game_input_probe();
    restore_passive_pause_probe();
    restore_post_intro_checkpoint();
    restore_intro_tick_hooks();
    restore_original_menu_tick();
    say("QUICK-EXIT-F12: DISABLED: F12 input thread could not start");
    return;
  }
  CloseHandle(thread);
  if(InterlockedCompareExchange(&experimental_game3d_enabled,0,0)==1)
    say("QUICK-EXIT-F12: EXPERIMENTAL 3D native exit enabled (game-thread cleanup then controller-7 final branch); not Windows-validated");
  else
    say("QUICK-EXIT-F12: menu native Quit validated; 3D native exit OFF by default (ExperimentalQuickExitF12In3D=0); comics unsupported");
}
