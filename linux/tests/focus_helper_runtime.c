/* Real native-X11 integration regression under Xvfb.
 * Exercises the packaged Linux observer against actual XGetInputFocus
 * windows, not just string matching. No Wine or game installation needed.
 * legluondunet assisted by ChatGPT (OpenAI)
 */
#define _POSIX_C_SOURCE 200809L
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void delay_ms(long ms) {
    struct timespec d = {ms/1000, (ms%1000)*1000000L};
    nanosleep(&d, NULL);
}
static int read_state(const char *file, const char *wanted) {
    FILE *f = fopen(file,"r");
    char line[96];
    int valid = f && fgets(line,sizeof(line),f) && strcmp(line,wanted)==0;
    if (f) fclose(f);
    return valid;
}
static int await_state(const char *path, const char *expected) {
    int i;
    for (i=0;i<80;i++) {
        if (read_state(path,expected)) return 0;
        delay_ms(50);
    }
    fprintf(stderr,"TIMEOUT waiting for state %s\n",expected);
    return 1;
}
static int await_removed(const char *path) {
    int i;
    for(i=0;i<80;i++) {
        if(access(path,F_OK)!=0) return 0;
        delay_ms(50);
    }
    fprintf(stderr,"TIMEOUT removing state file: %s\n",path);
    return 1;
}
static pid_t start_helper(const char *helper, const char *game) {
    pid_t pid=fork();
    if(pid==0) {
        execl(helper,helper,game,(char *)NULL);
        _exit(127);
    }
    return pid;
}
static void focus_window(Display *d, Window w) {
    XSetInputFocus(d,w,RevertToParent,CurrentTime);
    XSync(d,False);
    delay_ms(40);
}
static int wait_helper(pid_t pid) {
    int status,i;
    for(i=0;i<80;i++) {
        pid_t got=waitpid(pid,&status,WNOHANG);
        if(got==pid) return WIFEXITED(status)?WEXITSTATUS(status):3;
        if(got<0) return 4;
        delay_ms(50);
    }
    fprintf(stderr,"TIMEOUT waiting for helper exit\n");
    return 5;
}
int main(int argc,char **argv) {
    char dir[]="/tmp/spiderman-focus-test-XXXXXX";
    char game_exe[PATH_MAX],state_file[PATH_MAX];
    Display *d;
    Window root,game_window,other_window,unknown_window;
    Atom wm_pid;
    unsigned long value;
    pid_t game_pid=-1,helper_pid=-1;
    FILE *file;
    int failures=0,status;
    if(argc!=2) {
        fprintf(stderr,"usage: %s /path/to/spiderman-focus-helper\n",argv[0]);
        return 2;
    }
    if(!mkdtemp(dir)) { perror("mkdtemp"); return 2; }
    snprintf(game_exe,sizeof(game_exe),"%s/SpideyPC.exe",dir);
    snprintf(state_file,sizeof(state_file),"%s/SpiderMan2k1Fix.x11-focus",dir);
    file=fopen(game_exe,"w");
    if(!file) { perror("fopen"); return 2; }
    fputs("mock game binary\n",file);
    fclose(file);
    d=XOpenDisplay(NULL);
    if(!d) { fprintf(stderr,"ERROR: no X11 display\n"); return 2; }
    root=DefaultRootWindow(d);
    wm_pid=XInternAtom(d,"_NET_WM_PID",False);
    game_window=XCreateSimpleWindow(d,root,10,10,180,100,0,0,0);
    other_window=XCreateSimpleWindow(d,root,220,10,180,100,0,0,0);
    unknown_window=XCreateSimpleWindow(d,root,430,10,180,100,0,0,0);
    XMapWindow(d,game_window);
    XMapWindow(d,other_window);
    XMapWindow(d,unknown_window);
    XSync(d,False);
    game_pid=fork();
    if(game_pid<0) { perror("fork"); return 2; }
    if(game_pid==0) {
        if(prctl(PR_SET_NAME,"SpideyPC.exe",0,0,0)) _exit(3);
        for(;;) pause();
    }
    value=(unsigned long)game_pid;
    XChangeProperty(d,game_window,wm_pid,XA_CARDINAL,32,PropModeReplace,
                    (const unsigned char *)&value,1);
    value=(unsigned long)getpid();
    XChangeProperty(d,other_window,wm_pid,XA_CARDINAL,32,PropModeReplace,
                    (const unsigned char *)&value,1);
    XSync(d,False);
    focus_window(d,game_window);

    /* Wayland must be rejected even if an XWayland DISPLAY exists. */
    setenv("XDG_SESSION_TYPE","wayland",1);
    setenv("WAYLAND_DISPLAY","wayland-0",1);
    helper_pid=start_helper(argv[1],game_exe);
    status=wait_helper(helper_pid);
    if(status!=2) {
        fprintf(stderr,"ERROR: Wayland helper returned %d, expected 2\n",status);
        ++failures;
    }
    if(access(state_file,F_OK)==0) {
        fprintf(stderr,"ERROR: wrote a focus state under Wayland\n");
        ++failures;
    }
    unsetenv("WAYLAND_DISPLAY");
    setenv("XDG_SESSION_TYPE","x11",1);
    helper_pid=start_helper(argv[1],game_exe);
    failures+=await_state(state_file,"SPIDEY_X11_FOCUS_V1 GAME\n");
    focus_window(d,other_window);
    failures+=await_state(state_file,"SPIDEY_X11_FOCUS_V1 OTHER\n");
    focus_window(d,unknown_window);
    failures+=await_state(state_file,"SPIDEY_X11_FOCUS_V1 UNKNOWN\n");
    if(kill(helper_pid,SIGTERM)) { perror("SIGTERM"); ++failures; }
    status=wait_helper(helper_pid);
    if(status) {
        fprintf(stderr,"ERROR: helper SIGTERM returned %d\n",status);
        ++failures;
    }
    failures+=await_removed(state_file);

    /* Restart helper; test automatic cleanup when the game closes. */
    focus_window(d,game_window);
    helper_pid=start_helper(argv[1],game_exe);
    failures+=await_state(state_file,"SPIDEY_X11_FOCUS_V1 GAME\n");
    kill(game_pid,SIGTERM);
    waitpid(game_pid,NULL,0);
    status=wait_helper(helper_pid);
    if(status) {
        fprintf(stderr,"ERROR: helper game-exit returned %d\n",status);
        ++failures;
    }
    failures+=await_removed(state_file);
    XDestroyWindow(d,game_window);
    XDestroyWindow(d,other_window);
    XDestroyWindow(d,unknown_window);
    XCloseDisplay(d);
    unlink(game_exe);
    rmdir(dir);
    if(failures) {
        fprintf(stderr,"Native X11 regression: %d failed checks\n",failures);
        return 1;
    }
    puts("PASS native X11: Wayland rejection, GAME/OTHER/UNKNOWN, "
         "SIGTERM cleanup, game-exit cleanup");
    return 0;
}
