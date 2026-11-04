/* Wine-only native X11 focus observer for SpiderMan2k1Fix.
 * Installed alongside the Windows DLL and automatically spawned from Wine.
 * NEVER injects keyboard events, raises a window, or touches the game.
 *
 * GPL-3.0-or-later; legluondunet assisted by ChatGPT (OpenAI).
 */
#define _POSIX_C_SOURCE 200809L
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static volatile sig_atomic_t running = 1;
static void stop_now(int signal_number) {
    (void)signal_number;
    running = 0;
}

static int read_game_comm(pid_t pid) {
    char name[128], path[64];
    FILE *f;
    if (snprintf(path, sizeof(path), "/proc/%ld/comm", (long)pid) >= (int)sizeof(path)) return 0;
    f = fopen(path, "r");
    if (!f) return 0;
    if (!fgets(name, sizeof(name), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    name[strcspn(name, "\r\n")] = '\0';
    return strcmp(name, "SpideyPC.exe") == 0;
}

/* Do not accidentally compare Firefox with the Lutris/UMU launcher.
 * Multiple game instances are ambiguous: emit UNKNOWN and fail closed. */
static pid_t find_unique_game(void) {
    DIR *proc = opendir("/proc");
    struct dirent *item;
    pid_t found = 0;
    if (!proc) return 0;
    while ((item = readdir(proc)) != NULL) {
        char *end = NULL;
        long num;
        if (item->d_name[0] < '0' || item->d_name[0] > '9') continue;
        errno = 0;
        num = strtol(item->d_name, &end, 10);
        if (errno || !end || *end || num <= 0 || num > INT_MAX) continue;
        if (!read_game_comm((pid_t)num)) continue;
        if (found) {
            closedir(proc);
            return -1;
        }
        found = (pid_t)num;
    }
    closedir(proc);
    return found;
}

/* xdotool's 'getwindowfocus getwindowpid' semantics without external tools:
 * _NET_WM_PID is usually on the toplevel, not necessarily the focused child. */
static pid_t window_pid(Display *d, Window win, Atom pid_atom) {
    unsigned depth;
    for (depth = 0; win && win != None && depth < 24; depth++) {
        Atom type = None;
        int format = 0;
        unsigned long items = 0, extra = 0;
        unsigned char *value = NULL;
        if (XGetWindowProperty(d, win, pid_atom, 0, 1, False, XA_CARDINAL,
                               &type, &format, &items, &extra, &value) == Success) {
            if (value && type == XA_CARDINAL && format == 32 && items == 1) {
                unsigned long raw = *(unsigned long *)value;
                XFree(value);
                if (raw > 0 && raw <= INT_MAX) return (pid_t)raw;
            } else if (value) XFree(value);
        }
        {
            Window root = None, parent = None, *children = NULL;
            unsigned nchildren = 0;
            if (!XQueryTree(d, win, &root, &parent, &children, &nchildren)) break;
            if (children) XFree(children);
            if (parent == None || parent == win) break;
            win = parent;
        }
    }
    return 0;
}

static int atomic_write(const char *path, const char *state) {
    char tmp[PATH_MAX];
    int fd;
    size_t length = strlen(state);
    ssize_t sent;
    if (snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", path, (long)getpid()) >= (int)sizeof(tmp))
        return -1;
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) return -1;
    sent = write(fd, state, length);
    if (close(fd) || sent != (ssize_t)length || rename(tmp, path)) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    char state_file[PATH_MAX], game_exe[PATH_MAX];
    const char *slash;
    Display *display;
    Atom pid_atom;
    int ever_seen_game = 0, failure = 0;
    struct timespec sleep_time = { 0, 200000000L };

    if (argc != 2 || !argv[1] || !*argv[1]) return 2;
    if (snprintf(game_exe, sizeof(game_exe), "%s", argv[1]) >= (int)sizeof(game_exe))
        return 2;
    slash = strrchr(game_exe, '/');
    if (!slash || slash == game_exe) return 2;
    if (snprintf(state_file, sizeof(state_file), "%.*s/SpiderMan2k1Fix.x11-focus",
                 (int)(slash-game_exe), game_exe) >= (int)sizeof(state_file))
        return 2;
    if (access(game_exe, R_OK) != 0) return 2;
    if (!getenv("DISPLAY") || !*getenv("DISPLAY")) return 2;
    /* XWayland cannot reliably observe the focus of native Wayland apps.
     * Refuse to publish misleading GAME/OTHER evidence on a Wayland
     * session even when DISPLAY points to a working XWayland server. */
    {
        const char *session = getenv("XDG_SESSION_TYPE");
        const char *wayland_socket = getenv("WAYLAND_DISPLAY");
        if (session && strcasecmp(session, "wayland") == 0) return 2;
        if ((!session || !*session) && wayland_socket && *wayland_socket)
            return 2;
    }

    signal(SIGTERM, stop_now);
    signal(SIGINT, stop_now);
    display = XOpenDisplay(NULL);
    if (!display) return 2;
    pid_atom = XInternAtom(display, "_NET_WM_PID", True);
    if (pid_atom == None) {
        XCloseDisplay(display);
        return 2;
    }

    while (running) {
        pid_t game_pid = find_unique_game();
        const char *state = "SPIDEY_X11_FOCUS_V1 UNKNOWN\n";
        if (game_pid > 0) {
            Window focused = None;
            int revert_to = 0;
            pid_t focus_pid;
            ever_seen_game = 1;
            XGetInputFocus(display, &focused, &revert_to);
            focus_pid = window_pid(display, focused, pid_atom);
            if (focus_pid > 0) {
                state = focus_pid == game_pid
                    ? "SPIDEY_X11_FOCUS_V1 GAME\n"
                    : "SPIDEY_X11_FOCUS_V1 OTHER\n";
            }
        } else if (ever_seen_game && game_pid == 0) {
            break; /* game closed; no orphan helper */
        }
        if (atomic_write(state_file, state)) {
            failure = 1;
            break; /* stale state will be rejected by the DLL */
        }
        nanosleep(&sleep_time, NULL);
    }
    unlink(state_file);
    XCloseDisplay(display);
    return failure ? 1 : 0;
}
