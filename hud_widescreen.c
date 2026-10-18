/*
 * SpiderManFix: scoped player-HUD 16:9 correction, stand-alone hook module.
 *
 * Based on the tested player_hud(2).gdb prototype from Spider-Man 2001.
 * Does NOT patch the executable on disk and does NOT require GDB.
 *
 * IMPORTANT:
 *   This module is an integration component, NOT a replacement for the
 *   original SpiderManFix proxy DLL. Integrate with the existing
 *   early-loading and SHA-256 verification.
 *
 * Architecture: Windows x86 (32-bit) ONLY.
 * The hooks preserve general-purpose registers, EFLAGS, and x87 state.
 * Build the module with x87 floating point (-mfpmath=387 -mno-sse).
 * On scope entry, an original CALL return address is replaced with a
 * private cleanup trampoline. The cleanup trampoline restores the
 * exact saved return address while preserving EAX, flags, and ESP,
 * regardless of cdecl vs stdcall callee cleanup.
 *
 * Assumptions inherited from the verified GDB prototype:
 * - EXE image base 0x00400000.
 * - Site instructions are CALL rel32 (E8 ...).
 * - Call arguments at original pre-CALL ESP are float values.
 * - Expected offsets for sprite/rectangle x: +0,+20,+40,+60.
 * - Expected offsets for bar x/width and glyph x/width: +0,+8.
 *
 * Additional runtime protection:
 * - caller MUST have verified the SHA-256 of this exact executable;
 * - all 16 CALL sites and two auxiliary signatures are checked before
 *   writing anything;
 * - failed hook installation attempts to restore already patched bytes;
 * - width <= 4:3 remains unchanged.
 */

#include "hud_widescreen.h"
#include <stdint.h>
#include <string.h>

#if !defined(_WIN32) || !defined(_M_IX86) && !defined(__i386__)
#error "Spider-Man 2001 HUD hook is supported only for 32-bit Windows x86."
#endif

#define HUD_EXE_BASE       ((uintptr_t)0x00400000u)
#define HUD_SCREEN_W      ((volatile const int *)0x00568154u)
#define HUD_SCREEN_H      ((volatile const int *)0x00568158u)
#define HUD_TRAMPOLINE_SZ 64u
#define HUD_SCOPE_LIMIT   32u
/* 32-bit FSAVE/FRSTOR protects the complete legacy x87 context. */
#define HUD_X87_STACK_BYTES 108u
#define HUD_RETURN_OFFSET   (HUD_X87_STACK_BYTES + 36u)
#define HUD_FIRSTARG_OFFSET (HUD_X87_STACK_BYTES + 40u)

enum HookKind {
    HOOK_SPRITE = 1,
    HOOK_GLYPH = 2,
    HOOK_BAR = 3,
    HOOK_RECT = 4,
    SCOPE_TEXT = 5,
    SCOPE_BAR = 6,
    SCOPE_RECT = 7
};

typedef struct HudScope {
    uint32_t kind;
    uintptr_t original_return;
} HudScope;

typedef struct HudThread {
    unsigned depth;
    HudScope stack[HUD_SCOPE_LIMIT];
} HudThread;

typedef struct HudSite {
    uintptr_t address;
    uint32_t kind;
    BYTE saved[5];
    uintptr_t original_target;
    uintptr_t expected_target;
    BYTE *stub;
    BOOL patched;
} HudSite;

/* PATCH CALL site inventory and expected targets, verified by pre17.
 * Six sprites, one Webcart text scope, one glyph, six bar/rect scopes,
 * and two scoped draw sinks.
 */
static HudSite g_sites[] = {
    {0x00465D08u, HOOK_SPRITE, {0}, 0, 0x00507910u, NULL, FALSE},
    {0x00465F46u, HOOK_SPRITE, {0}, 0, 0x00507910u, NULL, FALSE},
    {0x00466176u, HOOK_SPRITE, {0}, 0, 0x00507910u, NULL, FALSE},
    {0x004663AAu, HOOK_SPRITE, {0}, 0, 0x00507910u, NULL, FALSE},
    {0x004665D1u, HOOK_SPRITE, {0}, 0, 0x00507910u, NULL, FALSE},
    {0x004667F6u, HOOK_SPRITE, {0}, 0, 0x00507910u, NULL, FALSE},
    {0x00465A83u, SCOPE_TEXT, {0}, 0, 0x00458700u, NULL, FALSE},
    {0x005068CBu, HOOK_GLYPH, {0}, 0, 0x00507470u, NULL, FALSE},
    {0x0046687Bu, SCOPE_RECT, {0}, 0, 0x00462FB0u, NULL, FALSE},
    {0x004668D1u, SCOPE_BAR, {0}, 0, 0x00462D60u, NULL, FALSE},
    {0x00466931u, SCOPE_RECT, {0}, 0, 0x00462FB0u, NULL, FALSE},
    {0x004669C7u, SCOPE_RECT, {0}, 0, 0x00462FB0u, NULL, FALSE},
    {0x00466A1Bu, SCOPE_BAR, {0}, 0, 0x00462D60u, NULL, FALSE},
    {0x00466A65u, SCOPE_BAR, {0}, 0, 0x00462D60u, NULL, FALSE},
    {0x00462EB3u, HOOK_BAR, {0}, 0, 0x00507470u, NULL, FALSE},
    {0x004631A4u, HOOK_RECT, {0}, 0, 0x00507910u, NULL, FALSE}
};

#define HUD_COUNT (sizeof(g_sites) / sizeof(g_sites[0]))

static DWORD g_tls = TLS_OUT_OF_INDEXES;
static BYTE *g_code = NULL;
static uintptr_t g_cleanup = 0;
/* 0=not installed, 1=full HUD correction, 2=passive HUD scopes only. */
static volatile LONG g_installed = 0;
static volatile LONG g_scope_entries = 0;

static HudThread *hud_get_thread(BOOL create)
{
    HudThread *p;
    if (g_tls == TLS_OUT_OF_INDEXES)
        return NULL;
    p = (HudThread *)TlsGetValue(g_tls);
    if (p || !create)
        return p;

    p = (HudThread *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*p));
    if (!p)
        return NULL;
    if (!TlsSetValue(g_tls, p)) {
        HeapFree(GetProcessHeap(), 0, p);
        return NULL;
    }
    return p;
}

void SpiderHudWidescreen_ThreadDetach(void)
{
    HudThread *p = hud_get_thread(FALSE);
    if (!p)
        return;
    TlsSetValue(g_tls, NULL);
    HeapFree(GetProcessHeap(), 0, p);
}

static float hud_alpha(void)
{
    int width = *HUD_SCREEN_W;
    int height = *HUD_SCREEN_H;

    /* Fails safe while resolution is not initialized. */
    if (width < 320 || width > 16384 || height < 200 || height > 16384)
        return 1.0f;
    if ((int64_t)width * 3 <= (int64_t)height * 4)
        return 1.0f;
    return (4.0f * (float)height) / (3.0f * (float)width);
}

/* Called via machine-code thunk with cdecl params:
 *   transform(kind, pointer_to_first_original_call_argument).
 * Do not change floating point / integer register state in the thunk
 * without pushad/pushfd; the surrounding generated thunk saves both.
 */
static void __cdecl hud_transform(uint32_t kind, float *args)
{
    float a = hud_alpha();
    HudThread *t;

    if (a >= 1.0f)
        return;

    if (kind == HOOK_SPRITE) {
        /* Four sprite corner x-values, 20 bytes apart. */
        args[0] *= a;
        args[5] *= a;
        args[10] *= a;
        args[15] *= a;
        return;
    }

    t = hud_get_thread(FALSE);
    if (!t || !t->depth)
        return;

    kind = (kind == HOOK_GLYPH && t->stack[t->depth - 1].kind == SCOPE_TEXT)
               ? HOOK_GLYPH
               : (kind == HOOK_BAR && t->stack[t->depth - 1].kind == SCOPE_BAR)
               ? HOOK_BAR
               : (kind == HOOK_RECT && t->stack[t->depth - 1].kind == SCOPE_RECT)
               ? HOOK_RECT
               : 0;

    if (kind == HOOK_GLYPH || kind == HOOK_BAR) {
        args[0] *= a; /* x */
        args[2] *= a; /* width */
    } else if (kind == HOOK_RECT) {
        args[0] *= a;
        args[5] *= a;
        args[10] *= a;
        args[15] *= a;
    }
}

/* Called by scope-entry thunk; returns either the trampoline
 * address (scope registered) or the original return address
 * (failure -> original game function remains unaffected).
 */
static uintptr_t __cdecl hud_scope_enter(uint32_t kind, uintptr_t original_return)
{
    HudThread *t = hud_get_thread(TRUE);
    if (!t || t->depth >= HUD_SCOPE_LIMIT || !g_cleanup)
        return original_return;
    t->stack[t->depth].kind = kind;
    t->stack[t->depth].original_return = original_return;
    ++t->depth;
    InterlockedIncrement(&g_scope_entries);
    return g_cleanup;
}

/* Called only from the cleanup trampoline whose scope was registered.
 * Returning 0 would indicate a corrupted scope and MUST NOT be used
 * as a branch target; this should be impossible without memory damage.
 */
static uintptr_t __cdecl hud_scope_leave(void)
{
    HudThread *t = hud_get_thread(FALSE);
    uintptr_t ret;
    if (!t || !t->depth)
        TerminateProcess(GetCurrentProcess(), 0xE17u);
    ret = t->stack[--t->depth].original_return;
    return ret;
}

typedef struct Emitter {
    BYTE *begin;
    BYTE *p;
    BYTE *end;
} Emitter;

static void put8(Emitter *e, BYTE b)
{
    if (e->p < e->end)
        *e->p++ = b;
}

static void put32(Emitter *e, uintptr_t x)
{
    uint32_t v = (uint32_t)x;
    put8(e, (BYTE)v);
    put8(e, (BYTE)(v >> 8));
    put8(e, (BYTE)(v >> 16));
    put8(e, (BYTE)(v >> 24));
}

static void emit_mov_eax_imm(Emitter *e, uintptr_t value)
{
    put8(e, 0xB8u);
    put32(e, value);
}
static void emit_call_eax(Emitter *e)
{
    put8(e, 0xFFu); put8(e, 0xD0u);
}
static void emit_jump_rel32(Emitter *e, uintptr_t target)
{
    uintptr_t from = (uintptr_t)e->p;
    put8(e, 0xE9u);
    put32(e, (uint32_t)(target - (from + 5u)));
}

/* Preserve the x87 register stack/control word used by old D3D7 code.
 * FNSAVE stores 108 bytes and resets x87; FRSTOR restores it exactly.
 * Stub callbacks are built with -mno-sse so their FP operations are x87.
 */
static void emit_save_x87(Emitter *e)
{
    /* sub esp, 108 */
    put8(e, 0x81u); put8(e, 0xECu); put32(e, HUD_X87_STACK_BYTES);
    /* fnsave [esp] */
    put8(e, 0xDDu); put8(e, 0x34u); put8(e, 0x24u);
}
static void emit_restore_x87(Emitter *e)
{
    /* frstor [esp] */
    put8(e, 0xDDu); put8(e, 0x24u); put8(e, 0x24u);
    /* add esp, 108 */
    put8(e, 0x81u); put8(e, 0xC4u); put32(e, HUD_X87_STACK_BYTES);
}
static void emit_mov_eax_espdisp32(Emitter *e, uintptr_t offset)
{
    put8(e, 0x8Bu); put8(e, 0x84u); put8(e, 0x24u); put32(e, offset);
}
static void emit_mov_espdisp32_eax(Emitter *e, uintptr_t offset)
{
    put8(e, 0x89u); put8(e, 0x84u); put8(e, 0x24u); put32(e, offset);
}
static void emit_lea_eax_espdisp32(Emitter *e, uintptr_t offset)
{
    put8(e, 0x8Du); put8(e, 0x84u); put8(e, 0x24u); put32(e, offset);
}

/* Cleanup sees ESP after original CALLEE's return, independent of RET N.
 * Original ESP=S: push eax; pushfd; pushad stores duplicate EAX at S-4.
 * After FNSAVE, its slot is [esp + 108 + 36]. Replace that slot with
 * the original CALL return address, restore x87/flags/regs, then RET.
 * This preserves the original EAX via POPAD and restores original ESP.
 */
static BOOL make_cleanup(BYTE *dst)
{
    Emitter e = {dst, dst, dst + HUD_TRAMPOLINE_SZ};
    put8(&e, 0x50u); put8(&e, 0x9Cu); put8(&e, 0x60u);
    emit_save_x87(&e);
    emit_mov_eax_imm(&e, (uintptr_t)hud_scope_leave);
    emit_call_eax(&e);
    emit_mov_espdisp32_eax(&e, HUD_RETURN_OFFSET);
    emit_restore_x87(&e);
    put8(&e, 0x61u); put8(&e, 0x9Du); put8(&e, 0xC3u);
    return e.p < e.end;
}

static BOOL make_hook(HudSite *site)
{
    Emitter e;
    BYTE *dst = site->stub;
    e.begin = e.p = dst;
    e.end = dst + HUD_TRAMPOLINE_SZ;

    put8(&e, 0x9Cu);  /* pushfd */
    put8(&e, 0x60u);  /* pushad */
    emit_save_x87(&e);

    if (site->kind == SCOPE_TEXT ||
        site->kind == SCOPE_BAR  ||
        site->kind == SCOPE_RECT) {
        /* The original CALL return address sits at ESP+144. */
        emit_mov_eax_espdisp32(&e, HUD_RETURN_OFFSET);
        put8(&e, 0x50u); /* push original_return */
        put8(&e, 0x68u); put32(&e, site->kind);
        emit_mov_eax_imm(&e, (uintptr_t)hud_scope_enter);
        emit_call_eax(&e);
        put8(&e, 0x83u); put8(&e, 0xC4u); put8(&e, 0x08u);
        /* Replace CALL return with cleanup trampoline when scoped. */
        emit_mov_espdisp32_eax(&e, HUD_RETURN_OFFSET);
    } else {
        /* ESP+148 points to original first floating-point argument. */
        emit_lea_eax_espdisp32(&e, HUD_FIRSTARG_OFFSET);
        put8(&e, 0x50u); /* push float* */
        put8(&e, 0x68u); put32(&e, site->kind);
        emit_mov_eax_imm(&e, (uintptr_t)hud_transform);
        emit_call_eax(&e);
        put8(&e, 0x83u); put8(&e, 0xC4u); put8(&e, 0x08u);
    }

    emit_restore_x87(&e);
    put8(&e, 0x61u); /* popad */
    put8(&e, 0x9Du); /* popfd */
    emit_jump_rel32(&e, site->original_target);
    return e.p < e.end;
}

static BOOL write_code(void *location, const void *buffer, size_t count)
{
    DWORD old_protect, unused_protect;
    if (!VirtualProtect(location, count, PAGE_EXECUTE_READWRITE, &old_protect))
        return FALSE;
    memcpy(location, buffer, count);
    FlushInstructionCache(GetCurrentProcess(), location, count);
    VirtualProtect(location, count, old_protect, &unused_protect);
    return TRUE;
}

LONG SpiderHudWidescreen_TakeScopeEntryCount(void)
{
    return InterlockedExchange(&g_scope_entries, 0);
}

BOOL SpiderHudWidescreen_InScope(void)
{
    HudThread *t;
    if (!InterlockedCompareExchange(&g_installed, 0, 0))
        return FALSE;
    t = hud_get_thread(FALSE);
    return t && t->depth > 0;
}

static BOOL site_is_scope(const HudSite *site)
{
    return site->kind == SCOPE_TEXT || site->kind == SCOPE_BAR ||
           site->kind == SCOPE_RECT;
}

BOOL SpiderHudWidescreen_Installed(void)
{
    return InterlockedCompareExchange(&g_installed, 0, 0) == 1;
}

static BOOL hud_install(BOOL image_verified, BOOL full_correction)
{
    unsigned i;
    BYTE *base;
    BYTE patch[5];

    if (!image_verified || (uintptr_t)GetModuleHandleW(NULL) != HUD_EXE_BASE)
        return FALSE;
    /* Once a mode is installed, refuse incompatible reinstallation. */
    {
        LONG installed = InterlockedCompareExchange(&g_installed, 0, 0);
        if (installed)
            return installed == 1 || !full_correction;
    }

    /* No modifications before ALL required signatures are verified. */
    base = (BYTE *)HUD_EXE_BASE;
    if (*(const uint16_t *)(base) != 0x5A4Du ||
        *(const BYTE *)0x00465A88u != 0x53u ||
        *(const BYTE *)0x00465B3Du != 0xA1u)
        return FALSE;

    for (i = 0; i < HUD_COUNT; ++i) {
        HudSite *site = &g_sites[i];
        int32_t displacement;
        const BYTE *src = (const BYTE *)site->address;
        if (src[0] != 0xE8u)
            return FALSE;
        memcpy(site->saved, src, 5);
        memcpy(&displacement, &src[1], sizeof(displacement));
        site->original_target = site->address + 5u + displacement;
        /* Exact per-site CALL destination for the SHA-256-pinned game.
         * A generic in-range match is insufficient for safe detours. */
        if (site->original_target != site->expected_target)
            return FALSE;
    }

    g_tls = TlsAlloc();
    if (g_tls == TLS_OUT_OF_INDEXES)
        return FALSE;

    g_code = (BYTE *)VirtualAlloc(NULL,
                (HUD_COUNT + 1u) * HUD_TRAMPOLINE_SZ,
                MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!g_code)
        goto failed;

    g_cleanup = (uintptr_t)(g_code + HUD_COUNT * HUD_TRAMPOLINE_SZ);
    if (!make_cleanup((BYTE *)g_cleanup))
        goto failed;

    for (i = 0; i < HUD_COUNT; ++i) {
        if (!full_correction && !site_is_scope(&g_sites[i]))
            continue;
        g_sites[i].stub = g_code + i * HUD_TRAMPOLINE_SZ;
        if (!make_hook(&g_sites[i]))
            goto failed;
    }
    FlushInstructionCache(GetCurrentProcess(), g_code,
                          (HUD_COUNT + 1u) * HUD_TRAMPOLINE_SZ);

    /* Apply each CALL rel32 patch. The proxy must invoke this before
     * rendering starts; concurrent runtime patching is unsupported.
     */
    for (i = 0; i < HUD_COUNT; ++i) {
        HudSite *site = &g_sites[i];
        if (!full_correction && !site_is_scope(site))
            continue;
        uint32_t rel = (uint32_t)((uintptr_t)site->stub - (site->address + 5u));
        patch[0] = 0xE8u;
        memcpy(&patch[1], &rel, 4);
        if (!write_code((void *)site->address, patch, sizeof(patch)))
            goto failed;
        site->patched = TRUE;
    }
    InterlockedExchange(&g_installed, full_correction ? 1 : 2);
    return TRUE;

failed:
    /* Safe only here, before rendering has started. */
    /* Passive mode patches only sparse site indices: restore by the
     * patched flag, never by the number of modified entries.
     */
    for (i = HUD_COUNT; i > 0; ) {
        HudSite *site = &g_sites[--i];
        if (!site->patched) continue;
        write_code((void *)site->address, site->saved, sizeof(site->saved));
        site->patched = FALSE;
    }
    if (g_code) {
        VirtualFree(g_code, 0, MEM_RELEASE);
        g_code = NULL;
    }
    g_cleanup = 0;
    if (g_tls != TLS_OUT_OF_INDEXES) {
        TlsFree(g_tls);
        g_tls = TLS_OUT_OF_INDEXES;
    }
    return FALSE;
}

BOOL SpiderHudWidescreen_Install(BOOL image_verified, BOOL config_enabled)
{
    if (!config_enabled) return TRUE;
    return hud_install(image_verified, TRUE);
}

BOOL SpiderHudWidescreen_InstallScopesOnly(BOOL image_verified)
{
    return hud_install(image_verified, FALSE);
}