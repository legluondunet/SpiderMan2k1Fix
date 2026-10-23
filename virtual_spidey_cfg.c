#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <wchar.h>
#include "spidey_cfg_template.h"
#include "virtual_spidey_cfg.h"
#include "display_policy.h"
#include "ini_validation.h"
#include "ini_recovery.h"

/* Always-on virtual configuration hooks ONLY imports of verified SpideyPC.exe (never patch
 * kernel32/msvcrt globally). Unknown bytes remain byte-for-byte preserved.
 * Win32 file handles are backed by NUL, but all reads/writes/seeks are handled
 * inside this module. C stdio is bridged through tmpfile() (a private temporary
 * stream: it is NOT an on-disk Spidey.cfg). Support for other CRT entry points
 * requires observation of the actual game, not assumptions.
 */
#define CFG_SIZE 252
#define CFG_HANDLES 8
#define CFG_STREAMS 8
static BYTE cfg[CFG_SIZE];
static CRITICAL_SECTION guard;
static int ready=0,dirty=0;
/* Only block saving if automatic INI repair fails. */
static int ini_writes_permitted=1;
static char ini_file[MAX_PATH];
static unsigned policy_width=640, policy_height=480;
static unsigned captured_native_width=0, captured_native_height=0;
static int policy_is_auto=1;
static char configured_resolution[64]="Auto";
static char configured_aspect[16]="Auto";
static void (*note)(const char *);
static DWORD opens=0,reads=0,writes=0,stdio_opens=0;
static volatile LONG bootstrap_attempted=0;

typedef struct { HANDLE h; LONG pos; DWORD access; } VHandle;
typedef struct { FILE *f; int writable; } VStream;
static VHandle handles[CFG_HANDLES];
static VStream streams[CFG_STREAMS];
static void report(const char *s) { if(note) note(s); }
static int ends_in_cfg_a(const char *p) {
  size_t n;
  if(!p) return 0;
  n=strlen(p);
  if(n<10 || lstrcmpiA(p+n-10,"Spidey.cfg")!=0) return 0;
  return n==10 || p[n-11]=='/' || p[n-11]=='\\';
}
static int ends_in_cfg_w(const WCHAR *p) {
  size_t n;
  if(!p) return 0;
  n=wcslen(p);
  if(n<10 || lstrcmpiW(p+n-10,L"Spidey.cfg")!=0) return 0;
  return n==10 || p[n-11]==L'/' || p[n-11]==L'\\';
}
static DWORD get_word(DWORD off) {
  return (DWORD)cfg[off] | ((DWORD)cfg[off+1]<<8) |
         ((DWORD)cfg[off+2]<<16) | ((DWORD)cfg[off+3]<<24);
}
static void set_word(DWORD off,DWORD x) {
  cfg[off]=(BYTE)x;cfg[off+1]=(BYTE)(x>>8);
  cfg[off+2]=(BYTE)(x>>16);cfg[off+3]=(BYTE)(x>>24);
}
static int hex_digit(int c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c>='a'&&c<='f')return c-'a'+10;
  if(c>='A'&&c<='F')return c-'A'+10;
  return -1;
}
/* Separate [Keyboard] and [Joystick] binding tables match the game's two menus.
 * The eleven verified DIK keyboard fields are at 0x14..0x3C. The seven
 * joystick action BUTTON indices are at 0x50..0x68, as confirmed by the
 * factory configuration and the in-game Joystick Configuration screenshot.
 * Joystick movement values at 0x40..0x4C have not been decoded: leave them
 * untouched inside [Internal] RawHex until an isolated input test verifies
 * their semantics. All other fields in the 252-byte image remain preserved. */
static const char *action_names[11]={"Up","Down","Left","Right","Jump","Kick","Punch","Web","Aim","Zipline","Swing"};
static const DWORD action_offsets[11]={20,24,28,32,36,40,44,48,52,56,60};
static const struct { const char *name; DWORD scan; } dik_names[]={
  {"UP",0xC8},{"DOWN",0xD0},{"LEFT",0xCB},{"RIGHT",0xCD},
  {"SPACE",0x39},{"ESC",0x01},{"ENTER",0x1C},{"TAB",0x0F},
  {"BACKSPACE",0x0E},{"LSHIFT",0x2A},{"RSHIFT",0x36},
  {"LCTRL",0x1D},{"RCTRL",0x9D},{"LALT",0x38},{"RALT",0xB8},
  {"A",0x1E},{"B",0x30},{"C",0x2E},{"D",0x20},{"E",0x12},
  {"F",0x21},{"G",0x22},{"H",0x23},{"I",0x17},{"J",0x24},
  {"K",0x25},{"L",0x26},{"M",0x32},{"N",0x31},{"O",0x18},
  {"P",0x19},{"Q",0x10},{"R",0x13},{"S",0x1F},{"T",0x14},
  {"U",0x16},{"V",0x2F},{"W",0x11},{"X",0x2D},{"Y",0x15},
  {"Z",0x2C},{"1",0x02},{"2",0x03},{"3",0x04},{"4",0x05},
  {"5",0x06},{"6",0x07},{"7",0x08},{"8",0x09},{"9",0x0A},
  {"0",0x0B},{"F1",0x3B},{"F2",0x3C},{"F3",0x3D},{"F4",0x3E},
  {"F5",0x3F},{"F6",0x40},{"F7",0x41},{"F8",0x42},
  {"F9",0x43},{"F10",0x44},{"F11",0x57},{"F12",0x58},
  {"HOME",0xC7},{"END",0xCF},{"DELETE",0xD3},{"INSERT",0xD2},
  {"PAGEUP",0xC9},{"PAGEDOWN",0xD1}
};
static int decode_key(const char *name,DWORD *scan) {
  unsigned i;
  for(i=0;i<sizeof(dik_names)/sizeof(dik_names[0]);i++)
    if(!lstrcmpiA(name,dik_names[i].name)) { *scan=dik_names[i].scan;return 1; }
  /* Lossless 32-bit fallback for as-yet-unidentified action binding codes. */
  if(name[0]=='0' && (name[1]=='x' || name[1]=='X')) {
    char *end=NULL;
    unsigned long n;
    errno=0;
    n=strtoul(name+2,&end,16);
    if(!errno && end && end!=name+2 && *end=='\0' && n<=0xFFFFFFFFUL) {
      *scan=(DWORD)n;
      return 1;
    }
  }
  return 0;
}
static const char *encode_key(DWORD scan) {
  unsigned i;
  for(i=0;i<sizeof(dik_names)/sizeof(dik_names[0]);i++)
    if(scan==dik_names[i].scan) return dik_names[i].name;
  return NULL;
}
static void load_keyboard(void) {
  unsigned i; char value[64],msg[140]; DWORD code;
  for(i=0;i<11;i++) {
    /* Missing field preserves current RawHex (or factory) value. */
    GetPrivateProfileStringA("Keyboard",action_names[i],"",value,sizeof(value),ini_file);
    if(!value[0]) continue;
    if(decode_key(value,&code)) set_word(action_offsets[i],code);
    else {
      snprintf(msg,sizeof(msg),"SPIDEY-CFG: unrecognized [Keyboard] %s=%s; keeping existing binding",action_names[i],value);
      report(msg);
    }
  }
}
static int save_keyboard(void) {
  unsigned i; char raw[32]; const char *name;
  for(i=0;i<11;i++) {
    DWORD code=get_word(action_offsets[i]);
    name=encode_key(code);
    if(!name) {snprintf(raw,sizeof(raw),"0x%02lX",(unsigned long)code);name=raw;}
    if(!WritePrivateProfileStringA("Keyboard",action_names[i],name,ini_file))return 0;
  }
  return 1;
}
/* The joystick menu has separate button assignments from keyboard.
 * These DWORDs store zero-based button indexes (Jump=3, Kick=4, etc.).
 * Only seven button bindings have been verified; do not interpret the
 * four preceding 0x4000 values as button IDs or directional axis names. */
static const char *joystick_actions[7]={"Jump","Kick","Punch","Web","Aim","Zipline","Swing"};
static const DWORD joystick_offsets[7]={0x50,0x54,0x58,0x5C,0x60,0x64,0x68};
static int decode_joystick_button(const char *name,DWORD *button) {
  /* User-facing BUTTONn uses the game's zero-based button numbering.
   * The hex form remains available to preserve unknown future encodings. */
  if(name && strlen(name)>=7) {
    static const char prefix[]="BUTTON";
    unsigned i;
    int prefix_ok=1;
    for(i=0;i<6;i++) {
      char a=name[i],b=prefix[i];
      if(a>='a' && a<='z') a=(char)(a-'a'+'A');
      if(a!=b) {prefix_ok=0;break;}
    }
    if(prefix_ok) {
      char *end=NULL;
      unsigned long n;
      const char *digits=name+6;
      if(*digits<'0' || *digits>'9') return 0;
      errno=0;
      n=strtoul(digits,&end,10);
      if(!errno && end && *end=='\0' && n<=127) {
        *button=(DWORD)n;
        return 1;
      }
      return 0;
    }
  }
  if(name && name[0]=='0' && (name[1]=='x'||name[1]=='X')) {
    char *end=NULL;
    unsigned long n;
    errno=0;
    n=strtoul(name+2,&end,16);
    if(!errno && end && end!=name+2 && *end=='\0' && n<=0xFFFFFFFFUL) {
      *button=(DWORD)n;
      return 1;
    }
  }
  return 0;
}
static void load_joystick(void) {
  unsigned i; char value[64],msg[160]; DWORD button;
  for(i=0;i<7;i++) {
    GetPrivateProfileStringA("Joystick",joystick_actions[i],"",value,sizeof(value),ini_file);
    if(!value[0]) continue; /* Keep factory/RawHex binding if not specified. */
    if(decode_joystick_button(value,&button)) set_word(joystick_offsets[i],button);
    else {
      snprintf(msg,sizeof(msg),"SPIDEY-CFG: unrecognized [Joystick] %s=%s; keeping existing binding",joystick_actions[i],value);
      report(msg);
    }
  }
}
static int save_joystick(void) {
  unsigned i; char name[32];
  for(i=0;i<7;i++) {
    DWORD code=get_word(joystick_offsets[i]);
    if(code<=127) snprintf(name,sizeof(name),"BUTTON%lu",(unsigned long)code);
    else snprintf(name,sizeof(name),"0x%08lX",(unsigned long)code);
    if(!WritePrivateProfileStringA("Joystick",joystick_actions[i],name,ini_file)) return 0;
  }
  return 1;
}
/* Volume offsets and limits established by controlled in-game INI captures:
 * Music/SFX = uint32 LE at 0xEC (0..0x3FFF); movie = 0xF0 (0..255);
 * voice = 0xF4 (0..255); 0xF8 holds mono=1 / stereo=0.
 * Keep RawHex as the authoritative lossless representation: rounding a
 * percentage back to an integer must not change an untouched raw value. */
static const struct { const char *name; DWORD offset,max_value; } audio_volumes[]={
  {"MusicSFXLevel",0xEC,16383},
  {"VoiceLevel",0xF4,255},
  {"MovieLevel",0xF0,255}
};
static unsigned to_percent(DWORD raw,DWORD max_value) {
  if(raw>=max_value) return 100;
  return (unsigned)((raw*100u+max_value/2u)/max_value);
}
static DWORD from_percent(unsigned percentage,DWORD max_value) {
  return (DWORD)((percentage*max_value+50u)/100u);
}
static int read_number(const char *section,const char *key,int *number) {
  char text[48],*end;
  long value;
  GetPrivateProfileStringA(section,key,"",text,sizeof(text),ini_file);
  if(!text[0]) return 0;
  value=strtol(text,&end,10);
  while(*end==' ' || *end=='\t') end++;
  if(end==text || *end || value<0 || value>100) {
    char msg[170];
    snprintf(msg,sizeof(msg),"SPIDEY-CFG: invalid [%s] %s=%s; RawHex retained",section,key,text);
    report(msg);
    return 0;
  }
  *number=(int)value;
  return 1;
}
static void load_audio(void) {
  unsigned i;
  int pct;
  DWORD raw;
  for(i=0;i<sizeof(audio_volumes)/sizeof(audio_volumes[0]);i++) {
    if(!read_number("Audio",audio_volumes[i].name,&pct))continue;
    raw=get_word(audio_volumes[i].offset);
    /* Preserve the exact raw value when the requested percentage is already
     * its rounded display representation. Prevents one-unit drift at startup. */
    if(raw<=audio_volumes[i].max_value &&
       to_percent(raw,audio_volumes[i].max_value)==(unsigned)pct) continue;
    set_word(audio_volumes[i].offset,from_percent((unsigned)pct,audio_volumes[i].max_value));
  }
  if(read_number("Audio","Stereo",&pct)) {
    if(pct==0 || pct==1) set_word(0xF8,pct==1?0u:1u);
    else report("SPIDEY-CFG: [Audio] Stereo must be 0 or 1; RawHex retained");
  }
}
static int save_audio(void) {
  unsigned i;
  char text[24];
  for(i=0;i<sizeof(audio_volumes)/sizeof(audio_volumes[0]);i++) {
    DWORD raw=get_word(audio_volumes[i].offset);
    if(raw>audio_volumes[i].max_value) continue; /* Unknown/corrupt raw: preserve. */
    snprintf(text,sizeof(text),"%u",to_percent(raw,audio_volumes[i].max_value));
    if(!WritePrivateProfileStringA("Audio",audio_volumes[i].name,text,ini_file))return 0;
  }
  /* The game encodes stereo as 0 and mono as 1; user-facing INI is inverse. */
  if(get_word(0xF8)<=1 && !WritePrivateProfileStringA("Audio","Stereo",
       get_word(0xF8)==0?"1":"0",ini_file)) return 0;
  return 1;
}
/* Generate factory defaults. Existing files are never truncated or overwritten.
 * Repair uses a unique temporary file, then installs it after backing up the old INI. */
static int write_ini_text(HANDLE file,const char *value) {
  DWORD written=0,length=(DWORD)strlen(value);
  return WriteFile(file,value,length,&written,NULL) && written==length;
}
static int SpiderCfg_WriteDefaultIniAt(const char *ini,int allow_existing,void (*logger)(const char *)) {
  char hex[CFG_SIZE*2+1],line[180];
  DWORD error; HANDLE file;
  unsigned i;
  const char *name;
  static const char header[]=
    "; SpiderMan2k1Fix.ini - generated automatically when missing.\r\n"
    "; Changes made in the game's options are saved to this file.\r\n"
    "; Edit this file while the game is not running. Invalid files are backed up as SpiderMan2k1Fix_N.old.\r\n"
    "\r\n"
    "[Debug]\r\n"
    "; 1 = write SpiderMan2k1Fix.log (debug); 0 = no log output. Restart to apply.\r\n"
    "Log=0\r\n"
    "; F12 debug fast-test: before menu -> menu, active 3D -> native exit. OFF.\r\n"
    "; Overrides legacy [System] F12 options while enabled.\r\n"
    "F12TestMode=0\r\n"
    "; Initial-menu wait workaround follows F12TestMode automatically.\r\n"
    "; Uncomment to disable only the menu wait workaround.\r\n"
    ";F12SkipInitialMenuWait=0\r\n"
    "\r\n"
    "[System]\r\n"
    "; F12 native game-owned Quit at real menu. Default OFF.\r\n"
    "QuickExitF12=0\r\n"
    "; Experimental native 3D F12 shutdown after owner cleanup. Default OFF.\r\n"
    "ExperimentalQuickExitF12In3D=0\r\n"
    "; Experimental New Game Bink movie #4 clean F12. Needs F12, 3D and Debug Log. OFF.\r\n"
    "ExperimentalQuickExitF12InNewGameMovie=0\r\n"
    "; Experimental New Game comic F12 deferred until Bink. Needs movie option. OFF.\r\n"
    "ExperimentalQuickExitF12InNewGameComic=0\r\n"
    "; Experimental F12 on intro still screens using original native input cleanup. OFF.\r\n"
    "ExperimentalQuickExitF12InIntroStills=0\r\n"
    "\r\n"
    "[Display]\r\n"
    "; Auto = desktop monitor resolution\r\n"
    "Resolution=Auto\r\n"
    "; Auto or 4:3 for a native gaming experience\r\n"
    "AspectRatio=Auto\r\n"
    "; Brightness: 0..9. FPSLimit: 0 disables; otherwise 1..240.\r\n"
    "Brightness=4\r\n"
    "FPSLimit=30\r\n"
    "; Development flags needed to preserve the currently tested rendering behaviour.\r\n"
    "ExperimentalFOVAspect=1\r\n"
    "ExperimentalComicNativeAspect=1\r\n"
    "\r\n"
    "[HUD]\r\n"
    "; Adaptive HUD correction: enabled by default; 0 explicitly disables it.\r\n"
    "HUDWidescreenFix=1\r\n"
    "\r\n"
    "[Input]\r\n"
    "; Opt-in native DirectInput error recovery after Alt+Tab; default off.\r\n"
    "DirectInputReacquire=0\r\n"
    "; Experimental Windows 11 Ghost surrogate suppression (0=default; 1=on until exit).\r\n"
    "DisableWindowsGhosting=0\r\n"
    "; Experimental Flip-boundary pause while the game loses focus (0=off).\r\n"
    "PauseOnFocusLoss=0\r\n"
    "; Any keyboard key resumes focus pause; the wake press/release is consumed.\r\n"
    "; Show centered transparent red-outline pause message when focus pause is active.\r\n"
    "PauseOverlay=1\r\n"
    "; Dim the paused game behind the PAUSE banner; 0 disables only the shade.\r\n"
    "PauseDim=1\r\n"
    "; Black shade strength from 0 to 90 percent (default 50).\r\n"
    "PauseDimOpacity=60\r\n"
    "; Optional 16-pixel grid (0=off by default; 1=on).\r\n"
    "PauseGrid=0\r\n"
    "; Restore Windows desktop cursor on loss of game focus (1=on, 0=off).\r\n"
    "RestoreCursorOnFocusLoss=1\r\n"
    "; Experimental Bink audio/video pause on lost focus; default off.\r\n"
    "PauseBinkOnFocusLoss=0\r\n"
    "; Experimental game-owned secondary DirectSound voice pause; default off.\r\n"
    "PauseDirectSoundOnFocusLoss=0\r\n"
    "; Detailed focus diagnosis is disabled by default.\r\n"
    "ExperimentalFocusTrace=0\r\n"
    "; Entry-only DirectInput8 factory probe; requires ExperimentalFocusTrace=1.\r\n"
    "ExperimentalDI8EntryTrace=0\r\n"
    "\r\n"
    "[Audio]\r\n"
    "; All levels are 0..100 percent, with lossless raw values kept below.\r\n";
  static const char keyboard_header[]=
    "\r\n[Keyboard]\r\n"
    "; Keyboard actions use physical DirectInput scan codes.\r\n"
    "; Use names like SPACE, F, UP or numeric 0xNNNNNNNN.\r\n"
    "; Unrecognized raw binding codes are preserved as 0xNNNNNNNN.\r\n";
  static const char joystick_header[]=
    "\r\n[Joystick]\r\n"
    "; Buttons are numbered from zero, as shown in the game menu.\r\n"
    "; Joystick movement/axis parameters are preserved in [Internal] RawHex.\r\n";
  static const char internal_header[]=
    "\r\n[Internal]\r\n"
    "; Raw 252-byte Spidey.cfg image, including unidentified settings.\r\n"
    "; Do not edit RawHex manually.\r\n"
    "RawHex=";
  file=CreateFileA(ini,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE) {
    error=GetLastError();
    if((error==ERROR_FILE_EXISTS || error==ERROR_ALREADY_EXISTS) && allow_existing) return 1;
    snprintf(line,sizeof(line),"INI-AUTO: ERROR creating SpiderMan2k1Fix.ini (Win32=%lu)",(unsigned long)error);
    if(logger)logger(line);
    return 0;
  }
  if(!write_ini_text(file,header))goto write_fail;
  for(i=0;i<sizeof(audio_volumes)/sizeof(audio_volumes[0]);i++) {
    DWORD off=audio_volumes[i].offset;
    DWORD raw=(DWORD)spidey_cfg_factory[off] |
       ((DWORD)spidey_cfg_factory[off+1]<<8) |
       ((DWORD)spidey_cfg_factory[off+2]<<16) |
       ((DWORD)spidey_cfg_factory[off+3]<<24);
    snprintf(line,sizeof(line),"%s=%u\r\n",audio_volumes[i].name,to_percent(raw,audio_volumes[i].max_value));
    if(!write_ini_text(file,line))goto write_fail;
  }
  snprintf(line,sizeof(line),"; 1 = Stereo, 0 = Mono (the game's internal flag is inverted).\r\nStereo=%d\r\n",
           spidey_cfg_factory[0xF8]==0?1:0);
  if(!write_ini_text(file,line) || !write_ini_text(file,keyboard_header))goto write_fail;
  for(i=0;i<11;i++) {
    DWORD off=action_offsets[i];
    DWORD raw=(DWORD)spidey_cfg_factory[off] |
       ((DWORD)spidey_cfg_factory[off+1]<<8) |
       ((DWORD)spidey_cfg_factory[off+2]<<16) |
       ((DWORD)spidey_cfg_factory[off+3]<<24);
    name=encode_key(raw);
    if(name)snprintf(line,sizeof(line),"%s=%s\r\n",action_names[i],name);
    else snprintf(line,sizeof(line),"%s=0x%02lX\r\n",action_names[i],(unsigned long)raw);
    if(!write_ini_text(file,line))goto write_fail;
  }
  if(!write_ini_text(file,joystick_header))goto write_fail;
  for(i=0;i<7;i++) {
    DWORD off=joystick_offsets[i];
    DWORD code=(DWORD)spidey_cfg_factory[off] |
       ((DWORD)spidey_cfg_factory[off+1]<<8) |
       ((DWORD)spidey_cfg_factory[off+2]<<16) |
       ((DWORD)spidey_cfg_factory[off+3]<<24);
    if(code<=127) snprintf(line,sizeof(line),"%s=BUTTON%lu\r\n",joystick_actions[i],(unsigned long)code);
    else snprintf(line,sizeof(line),"%s=0x%08lX\r\n",joystick_actions[i],(unsigned long)code);
    if(!write_ini_text(file,line))goto write_fail;
  }
  if(!write_ini_text(file,internal_header))goto write_fail;
  for(i=0;i<CFG_SIZE;i++)sprintf(hex+i*2,"%02X",(unsigned)spidey_cfg_factory[i]);
  if(!write_ini_text(file,hex) || !write_ini_text(file,"\r\n"))goto write_fail;
  if(!FlushFileBuffers(file))goto write_fail;
  CloseHandle(file);
  if(logger)logger("INI-AUTO: generated factory SpiderMan2k1Fix configuration");
  return 1;
write_fail:
  error=GetLastError();
  CloseHandle(file);
  DeleteFileA(ini); /* Do not leave a truncated first-run configuration. */
  snprintf(line,sizeof(line),"INI-AUTO: ERROR writing SpiderMan2k1Fix.ini (Win32=%lu)",(unsigned long)error);
  if(logger)logger(line);
  return 0;
}



static int SpiderCfg_IniPath(char *ini,size_t cap) {
  char exe[MAX_PATH],*slash; DWORD n=GetModuleFileNameA(NULL,exe,sizeof(exe));
  size_t prefix;
  if(!n||n>=sizeof(exe)||!(slash=strrchr(exe,'\\')))return 0;
  prefix=(size_t)(slash-exe)+1;
  if(prefix+strlen("SpiderMan2k1Fix.ini")+1>cap)return 0;
  memcpy(ini,exe,prefix);
  strcpy(ini+prefix,"SpiderMan2k1Fix.ini");return 1;
}
int SpiderCfg_EnsureDefaultIni(void (*logger)(const char *)) {
  char ini[MAX_PATH];
  if(!SpiderCfg_IniPath(ini,sizeof(ini))) {
    if(logger)logger("INI-AUTO: ERROR cannot resolve INI path");
    return 0;
  }
  return SpiderCfg_WriteDefaultIniAt(ini,1,logger);
}


/* Normalize an invalid INI into the generator's canonical format.
 * The original is preserved at SpiderMan2k1Fix_0.old, _1.old, ... .
 * Build the replacement separately and back up BEFORE the atomic replacement.
 * Failure leaves the original INI untouched and blocks game-origin writes.
 */
static int SpiderCfg_RepairIni(const char *ini,const SpiderIniResult *check,
                             char *backup_out,size_t backup_cap,unsigned *restored,
                             void (*logger)(const char *)) {
  SpiderIniRecovered *values;
  char directory[MAX_PATH],tmp[MAX_PATH],backup[MAX_PATH],msg[260];
  char *slash;
  size_t prefix;
  unsigned i,serial;
  int ok=0;
  DWORD err;
  values=(SpiderIniRecovered *)malloc(sizeof(*values));
  if(!values)return 0;
  if(!SpiderIni_Recover(ini,values))goto done;
  slash=strrchr(ini,'\\');if(!slash)goto done;
  prefix=(size_t)(slash-ini)+1;
  if(prefix>=sizeof(directory))goto done;
  memcpy(directory,ini,prefix);directory[prefix]=0;
  /* Never overwrite an unrelated leftover temporary or an earlier backup. */
  for(serial=0;serial<10000;serial++) {
    if(snprintf(tmp,sizeof(tmp),"%sSpiderMan2k1Fix-repair_%u.tmp",directory,serial)>=(int)sizeof(tmp))goto done;
    if(GetFileAttributesA(tmp)==INVALID_FILE_ATTRIBUTES)break;
  }
  if(serial==10000)goto done;
  if(!SpiderCfg_WriteDefaultIniAt(tmp,0,logger))goto done;
  for(i=0;i<values->count;i++) {
    SpiderIniRecoveredEntry *e=&values->entries[i];
    if(!WritePrivateProfileStringA(e->section,e->key,e->value,tmp)) {
      err=GetLastError();
      snprintf(msg,sizeof(msg),"INI-REPAIR: failed writing [%s] %s, Win32=%lu",
               e->section,e->key,(unsigned long)err);
      if(logger)logger(msg);
      DeleteFileA(tmp);goto done;
    }
  }
  /* Flush Win32 profile cache before manipulating the replacement file. */
  WritePrivateProfileStringA(NULL,NULL,NULL,tmp);
  /* Validate the entire replacement, including its 504-character RawHex. */
  {
    SpiderIniResult rebuilt=SpiderIni_Validate(tmp,NULL,NULL);
    if(rebuilt.errors||rebuilt.warnings) {
      if(logger)logger("INI-REPAIR: replacement failed final validation; original retained");
      DeleteFileA(tmp);goto done;
    }
  }
  for(serial=0;serial<10000;serial++) {
    if(snprintf(backup,sizeof(backup),"%sSpiderMan2k1Fix_%u.old",directory,serial)>=(int)sizeof(backup)) {
      DeleteFileA(tmp);goto done;
    }
    if(GetFileAttributesA(backup)==INVALID_FILE_ATTRIBUTES)break;
  }
  if(serial==10000) {DeleteFileA(tmp);goto done;}
  /* CopyFile fail_if_exists protects numbered backups even against a race. */
  if(!CopyFileA(ini,backup,TRUE)) {
    err=GetLastError();
    snprintf(msg,sizeof(msg),"INI-REPAIR: original backup failed (Win32=%lu), INI unchanged",(unsigned long)err);
    if(logger)logger(msg);
    DeleteFileA(tmp);goto done;
  }
  /* Same-directory atomic replacement; original bytes are in numbered backup. */
  if(!MoveFileExA(tmp,ini,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
    err=GetLastError();
    snprintf(msg,sizeof(msg),"INI-REPAIR: replacing INI failed (Win32=%lu), backup preserved",(unsigned long)err);
    if(logger)logger(msg);
    DeleteFileA(tmp);goto done;
  }
  /* Flush any cached Win32-profile interpretation of the old INI. */
  WritePrivateProfileStringA(NULL,NULL,NULL,ini);
  if(backup_cap>strlen(backup))strcpy(backup_out,backup);
  if(restored)*restored=values->count;
  snprintf(msg,sizeof(msg),"INI-REPAIR: fixed %u error(s), %u unknown/other warning(s); recovered %u setting(s)",
           check->errors,check->warnings,values->count);
  if(logger)logger(msg);
  ok=1;
done:
  free(values);return ok;
}
static void SpiderCfg_ClearOldValidationReport(const char *ini);
void SpiderCfg_ValidateCurrentIni(void (*logger)(const char *)) {
  char ini[MAX_PATH],backup[MAX_PATH]="",summary[540];
  unsigned recovered=0;
  SpiderIniResult check,rebuilt;
  if(!SpiderCfg_IniPath(ini,sizeof(ini))) {
    ini_writes_permitted=0;
    if(logger)logger("INI-REPAIR: cannot resolve path; saving disabled");
    return;
  }
  check=SpiderIni_Validate(ini,NULL,NULL);
  if(!check.errors && !check.warnings) {
    ini_writes_permitted=1;
    SpiderCfg_ClearOldValidationReport(ini);
    if(logger)logger("INI-CHECK: valid SpiderMan2k1Fix.ini");
    return;
  }
  ini_writes_permitted=0;
  if(!SpiderCfg_RepairIni(ini,&check,backup,sizeof(backup),&recovered,logger)) {
    if(logger)logger("INI-REPAIR: FAILED; source unchanged, game INI writes disabled");
    MessageBoxA(NULL,"SpiderMan2k1Fix.ini has errors or unknown entries, but automatic repair failed.\n"
      "Your original INI has been kept. Game options will not overwrite it.\n"
      "Check write permissions and available disk space.",
      "SpiderMan2k1Fix - INI repair failed",MB_OK|MB_ICONWARNING|MB_TOPMOST);
    return;
  }
  rebuilt=SpiderIni_Validate(ini,NULL,NULL);
  ini_writes_permitted=!rebuilt.errors && !rebuilt.warnings;
  if(!ini_writes_permitted) {
    if(logger)logger("INI-REPAIR: unexpected error re-reading clean INI; saving disabled");
    return;
  }
  SpiderCfg_ClearOldValidationReport(ini);
  snprintf(summary,sizeof(summary),
    "SpiderMan2k1Fix.ini contained %u error(s) and %u unknown entry/entries.\n\n"
    "A new, clean configuration was generated.\n"
    "%u recognized setting(s) were recovered.\n\n"
    "The original file was saved as:\n%s",
    check.errors,check.warnings,recovered,backup);
  MessageBoxA(NULL,summary,"SpiderMan2k1Fix - INI repaired",MB_OK|MB_ICONINFORMATION|MB_TOPMOST);
}

/* Obsolete report files from the previous read-only validator are stale. */
static void SpiderCfg_ClearOldValidationReport(const char *ini) {
  char path[MAX_PATH];char *slash;
  if(strlen(ini)>=sizeof(path))return;
  strcpy(path,ini);slash=strrchr(path,'\\');if(!slash)return;
  if((size_t)(slash-path)+strlen("SpiderMan2k1Fix.ini-errors.txt")+2>=sizeof(path))return;
  strcpy(slash+1,"SpiderMan2k1Fix.ini-errors.txt");
  DeleteFileA(path);
}
static void apply_locked_display(void) {
  SpiderDisplayPolicy_Enforce(cfg,CFG_SIZE,policy_width,policy_height);
}
static void load_settings(void) {
  char buf[CFG_SIZE*2+8],msg[270];
  DWORD i,a,b,brightness;
  DEVMODEA mode;
  unsigned w,h;
  memcpy(cfg,spidey_cfg_factory,CFG_SIZE);
  GetPrivateProfileStringA("Internal","RawHex","",buf,sizeof(buf),ini_file);
  if(strlen(buf)==CFG_SIZE*2) {
    for(i=0;i<CFG_SIZE;i++) {
      a=hex_digit(buf[2*i]);b=hex_digit(buf[2*i+1]);
      if(a>15 || b>15) break;
      cfg[i]=(BYTE)((a<<4)|b);
    }
    if(i!=CFG_SIZE) { memcpy(cfg,spidey_cfg_factory,CFG_SIZE); report("SPIDEY-CFG: invalid RawHex; factory layout restored"); }
  } else if(buf[0]) report("SPIDEY-CFG: RawHex must contain exactly 504 hexadecimal characters; ignored");
  /* One authoritative resolution setting: [Display]. */
  GetPrivateProfileStringA("Display","Resolution","Auto",configured_resolution,sizeof(configured_resolution),ini_file);
  GetPrivateProfileStringA("Display","AspectRatio","Auto",configured_aspect,sizeof(configured_aspect),ini_file);
  policy_is_auto=!lstrcmpiA(configured_resolution,"Auto");
  memset(&mode,0,sizeof(mode));mode.dmSize=sizeof(mode);
  if(EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&mode)) {
    captured_native_width=mode.dmPelsWidth;
    captured_native_height=mode.dmPelsHeight;
  }
  if(SpiderDisplayPolicy_ResolveRuntime(configured_resolution,configured_aspect,captured_native_width,
        captured_native_height,&w,&h)) {
    policy_width=w;policy_height=h;
  } else {
    /* Do not trust stale cfg resolution when Auto monitor probing fails. */
    policy_width=640;policy_height=480;
    report("SPIDEY-CFG: WARNING Display Resolution invalid/unavailable; temporary safe 640x480 pending monitor probe");
  }
  apply_locked_display();
  load_keyboard();
  load_joystick();
  load_audio();
  /* Never let GetPrivateProfileIntA turn an invalid string into brightness=0. */
  {
    char value[48],*end=NULL;unsigned long parsed;
    GetPrivateProfileStringA("Display","Brightness","",value,sizeof(value),ini_file);
    if(value[0]) {
      errno=0;parsed=strtoul(value,&end,10);
      while(end && (*end==' '||*end=='\t'))end++;
      if(value[0]!='-' && value[0]!='+' && !errno && end!=value && *end==0 && parsed<=9)
        set_word(12,(DWORD)parsed);
      else report("SPIDEY-CFG: invalid Brightness (allowed 0..9); preserved raw value");
    }
  }
  brightness=get_word(12);
  snprintf(msg,sizeof(msg),"SPIDEY-CFG: virtual template bytes=%d authoritative [Display] Resolution=%s AspectRatio=%s -> %ux%u; color depth LOCKED=32; brightness=%lu (240 other bytes preserved)",
    CFG_SIZE,configured_resolution,configured_aspect,policy_width,policy_height,
    (unsigned long)get_word(12));
  report(msg);
}
/* Called from the existing pre-fullscreen HWND/monitor probe. For Auto this
 * corrects the initial primary-monitor fallback on multi-display systems.
 * If the game read its cfg before installation, no late hook can undo it. */
void SpiderCfg_ObserveNativeMonitor(unsigned width,unsigned height) {
  char msg[180];
  if(!ready || !policy_is_auto || width<640 || height<480 ||
     width>16384 || height>16384) return;
  EnterCriticalSection(&guard);
  {
    unsigned chosen_w,chosen_h;
    if(!SpiderDisplayPolicy_ResolveRuntime(configured_resolution,configured_aspect,
           width,height,&chosen_w,&chosen_h)) {
      LeaveCriticalSection(&guard);return;
    }
    if(policy_width==chosen_w && policy_height==chosen_h) {
      LeaveCriticalSection(&guard);return;
    }
    policy_width=chosen_w;policy_height=chosen_h;
    apply_locked_display();
    snprintf(msg,sizeof(msg),"SPIDEY-CFG: Auto monitor aspect policy resolved to %ux%u (locked 32bpp); already-read game settings cannot be retroactively changed",policy_width,policy_height);
    report(msg);
  }
  LeaveCriticalSection(&guard);
}
static int save_settings(void) {
  char hex[CFG_SIZE*2+1],value[80],msg[150];
  unsigned i;
  if(!dirty) return 1;
  if(!ini_writes_permitted) {
    report("SPIDEY-CFG: ERROR cannot save: automatic INI recovery failed; original retained");
    return 0;
  }
  /* Never let in-game changes to screen size or color depth persist. */
  apply_locked_display();
  for(i=0;i<CFG_SIZE;i++) sprintf(hex+i*2,"%02X",(unsigned)cfg[i]);
  if(!WritePrivateProfileStringA("Internal","RawHex",hex,ini_file)) goto fail;
  snprintf(value,sizeof(value),"%lu",(unsigned long)get_word(12));
  if(!WritePrivateProfileStringA("Display","Brightness",value,ini_file)) goto fail;
  if(!save_keyboard()) goto fail;
  if(!save_joystick()) goto fail;
  if(!save_audio()) goto fail;
  dirty=0;
  report("SPIDEY-CFG: game keyboard/joystick/audio/brightness saved; resolution and color depth locked by [Display] policy");
  return 1;
fail:
  snprintf(msg,sizeof(msg),"SPIDEY-CFG: ERROR saving INI Win32=%lu (data still in memory)",(unsigned long)GetLastError());
  report(msg);
  return 0;
}
static VHandle *get_handle(HANDLE h) {
  unsigned i;
  for(i=0;i<CFG_HANDLES;i++) if(handles[i].h==h && h && h!=INVALID_HANDLE_VALUE) return &handles[i];
  return NULL;
}
static VStream *get_stream(FILE *f) {
  unsigned i;
  for(i=0;i<CFG_STREAMS;i++) if(streams[i].f==f && f) return &streams[i];
  return NULL;
}
static HANDLE open_virtual(DWORD access,DWORD disposition) {
  unsigned i;
  HANDLE h;
  char msg[140];
  if(disposition==CREATE_NEW) {SetLastError(ERROR_FILE_EXISTS);return INVALID_HANDLE_VALUE;}
  EnterCriticalSection(&guard);
  for(i=0;i<CFG_HANDLES && handles[i].h;i++);
  if(i==CFG_HANDLES) { LeaveCriticalSection(&guard); SetLastError(ERROR_TOO_MANY_OPEN_FILES);return INVALID_HANDLE_VALUE; }
  /* A real, safely closeable handle, but NEVER the original Spidey.cfg. */
  h=CreateFileA("NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(h==INVALID_HANDLE_VALUE) { LeaveCriticalSection(&guard); return h; }
  handles[i].h=h;handles[i].pos=0;handles[i].access=access;
  opens++;
  snprintf(msg,sizeof(msg),"SPIDEY-CFG: virtual CreateFile intercepted #%lu access=0x%08lX disposition=%lu",
    (unsigned long)opens,(unsigned long)access,(unsigned long)disposition);
  report(msg);
  LeaveCriticalSection(&guard);
  SetLastError(disposition==OPEN_ALWAYS || disposition==CREATE_ALWAYS ? ERROR_ALREADY_EXISTS : ERROR_SUCCESS);
  return h;
}
static HANDLE WINAPI hooked_CreateFileA(LPCSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES sa,DWORD disp,DWORD attr,HANDLE tmpl) {
  (void)share;(void)sa;(void)attr;(void)tmpl;
  if(ready && ends_in_cfg_a(path)) return open_virtual(access,disp);
  return CreateFileA(path,access,share,sa,disp,attr,tmpl);
}
static HANDLE WINAPI hooked_CreateFileW(LPCWSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES sa,DWORD disp,DWORD attr,HANDLE tmpl) {
  if(ready && ends_in_cfg_w(path)) return open_virtual(access,disp);
  return CreateFileW(path,access,share,sa,disp,attr,tmpl);
}
static BOOL WINAPI hooked_ReadFile(HANDLE h,LPVOID buffer,DWORD len,LPDWORD done,LPOVERLAPPED overlapped) {
  VHandle *v;DWORD n;
  if(!ready) return ReadFile(h,buffer,len,done,overlapped);
  EnterCriticalSection(&guard);v=get_handle(h);
  if(!v) { LeaveCriticalSection(&guard);return ReadFile(h,buffer,len,done,overlapped); }
  if(overlapped || (!buffer && len)) {LeaveCriticalSection(&guard);SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
  if(v->pos<0 || v->pos>=CFG_SIZE) n=0;
  else n=CFG_SIZE-(DWORD)v->pos;
  if(n>len) n=len;
  if(n) memcpy(buffer,cfg+v->pos,n);
  v->pos+=(LONG)n;if(done)*done=n;
  reads++;if(reads<=5) report("SPIDEY-CFG: virtual ReadFile served (game requested config)");
  LeaveCriticalSection(&guard);SetLastError(ERROR_SUCCESS);return TRUE;
}
static BOOL WINAPI hooked_WriteFile(HANDLE h,LPCVOID buffer,DWORD len,LPDWORD done,LPOVERLAPPED overlapped) {
  VHandle *v;
  if(!ready)return WriteFile(h,buffer,len,done,overlapped);
  EnterCriticalSection(&guard);v=get_handle(h);
  if(!v) {LeaveCriticalSection(&guard);return WriteFile(h,buffer,len,done,overlapped);}
  if(overlapped || (!buffer && len) || v->pos<0 || (unsigned)v->pos>CFG_SIZE || len>CFG_SIZE-(DWORD)v->pos) {
    LeaveCriticalSection(&guard);SetLastError(ERROR_INVALID_PARAMETER);return FALSE;
  }
  if(len){
    DWORD old_w=get_word(0),old_h=get_word(4),old_depth=get_word(8);
    memcpy(cfg+v->pos,buffer,len);v->pos+=(LONG)len;dirty=1;writes++;
    if(old_w!=get_word(0) || old_h!=get_word(4) || old_depth!=get_word(8))
      report("SPIDEY-CFG: in-game screen/depth write intercepted; authoritative display settings restored");
    apply_locked_display();
  }
  if(done)*done=len;
  if(writes<=8)report("SPIDEY-CFG: virtual WriteFile captured (will persist to INI)");
  LeaveCriticalSection(&guard);SetLastError(ERROR_SUCCESS);return TRUE;
}
static DWORD WINAPI hooked_SetFilePointer(HANDLE h,LONG delta,PLONG high,DWORD method) {
  VHandle *v;LONGLONG offset,next;
  if(!ready)return SetFilePointer(h,delta,high,method);
  EnterCriticalSection(&guard);v=get_handle(h);
  if(!v){LeaveCriticalSection(&guard);return SetFilePointer(h,delta,high,method);}
  offset=(LONGLONG)delta;if(high) offset=(LONGLONG)(((ULONGLONG)(DWORD)*high<<32)|(DWORD)delta);
  next=offset;
  if(method==FILE_CURRENT)next+=v->pos;
  else if(method==FILE_END)next+=CFG_SIZE;
  else if(method!=FILE_BEGIN)next=-1;
  if(next<0 || next>CFG_SIZE){LeaveCriticalSection(&guard);SetLastError(ERROR_NEGATIVE_SEEK);return INVALID_SET_FILE_POINTER;}
  v->pos=(LONG)next;if(high)*high=0;
  LeaveCriticalSection(&guard);SetLastError(ERROR_SUCCESS);return (DWORD)next;
}
static BOOL WINAPI hooked_SetFilePointerEx(HANDLE h,LARGE_INTEGER dist,PLARGE_INTEGER newpos,DWORD method) {
  VHandle *v;LONGLONG next=dist.QuadPart;
  if(!ready)return SetFilePointerEx(h,dist,newpos,method);
  EnterCriticalSection(&guard);v=get_handle(h);
  if(!v){LeaveCriticalSection(&guard);return SetFilePointerEx(h,dist,newpos,method);}
  if(method==FILE_CURRENT)next+=v->pos;
  else if(method==FILE_END)next+=CFG_SIZE;
  else if(method!=FILE_BEGIN)next=-1;
  if(next<0 || next>CFG_SIZE){LeaveCriticalSection(&guard);SetLastError(ERROR_NEGATIVE_SEEK);return FALSE;}
  v->pos=(LONG)next;if(newpos)newpos->QuadPart=next;
  LeaveCriticalSection(&guard);SetLastError(ERROR_SUCCESS);return TRUE;
}
static DWORD WINAPI hooked_GetFileSize(HANDLE h,LPDWORD high) {
  VHandle *v;DWORD val;
  if(!ready)return GetFileSize(h,high);
  EnterCriticalSection(&guard);v=get_handle(h);val=v?CFG_SIZE:0;
  LeaveCriticalSection(&guard);
  if(!v)return GetFileSize(h,high);
  if(high)*high=0;SetLastError(ERROR_SUCCESS);return val;
}
static BOOL WINAPI hooked_GetFileSizeEx(HANDLE h,PLARGE_INTEGER size) {
  VHandle *v;
  if(!ready)return GetFileSizeEx(h,size);
  EnterCriticalSection(&guard);v=get_handle(h);
  if(v && size)size->QuadPart=CFG_SIZE;
  LeaveCriticalSection(&guard);
  if(!v)return GetFileSizeEx(h,size);
  SetLastError(ERROR_SUCCESS);return TRUE;
}
static DWORD WINAPI hooked_GetFileType(HANDLE h) {
  VHandle *v;
  if(!ready)return GetFileType(h);
  EnterCriticalSection(&guard);v=get_handle(h);
  LeaveCriticalSection(&guard);
  return v?FILE_TYPE_DISK:GetFileType(h);
}
static BOOL WINAPI hooked_FlushFileBuffers(HANDLE h) {
  VHandle *v;BOOL result;
  if(!ready)return FlushFileBuffers(h);
  EnterCriticalSection(&guard);v=get_handle(h);
  if(!v){LeaveCriticalSection(&guard);return FlushFileBuffers(h);}
  result=save_settings();LeaveCriticalSection(&guard);return result;
}
static BOOL WINAPI hooked_SetEndOfFile(HANDLE h) {
  VHandle *v;
  if(!ready)return SetEndOfFile(h);
  EnterCriticalSection(&guard);v=get_handle(h);
  if(!v){LeaveCriticalSection(&guard);return SetEndOfFile(h);}
  /* Do not discard unknown fields or change binary structure. */
  LeaveCriticalSection(&guard);SetLastError(ERROR_SUCCESS);return TRUE;
}
static BOOL WINAPI hooked_CloseHandle(HANDLE h) {
  VHandle *v;BOOL saved,closed;
  if(!ready)return CloseHandle(h);
  EnterCriticalSection(&guard);v=get_handle(h);
  if(!v){LeaveCriticalSection(&guard);return CloseHandle(h);}
  saved=save_settings();
  v->h=NULL;v->pos=0;
  closed=CloseHandle(h);LeaveCriticalSection(&guard);
  return closed && saved;
}
static DWORD WINAPI hooked_GetFileAttributesA(LPCSTR path) {
  if(ready && ends_in_cfg_a(path))return FILE_ATTRIBUTE_NORMAL;
  return GetFileAttributesA(path);
}
static DWORD WINAPI hooked_GetFileAttributesW(LPCWSTR path) {
  if(ready && ends_in_cfg_w(path))return FILE_ATTRIBUTE_NORMAL;
  return GetFileAttributesW(path);
}
static BOOL WINAPI hooked_GetFileAttributesExA(LPCSTR path,GET_FILEEX_INFO_LEVELS level,LPVOID info) {
  WIN32_FILE_ATTRIBUTE_DATA *d=(WIN32_FILE_ATTRIBUTE_DATA*)info;
  if(ready && ends_in_cfg_a(path) && level==GetFileExInfoStandard && d) {
    memset(d,0,sizeof(*d));d->dwFileAttributes=FILE_ATTRIBUTE_NORMAL;d->nFileSizeLow=CFG_SIZE;
    return TRUE;
  }
  return GetFileAttributesExA(path,level,info);
}
static BOOL WINAPI hooked_GetFileAttributesExW(LPCWSTR path,GET_FILEEX_INFO_LEVELS level,LPVOID info) {
  WIN32_FILE_ATTRIBUTE_DATA *d=(WIN32_FILE_ATTRIBUTE_DATA*)info;
  if(ready && ends_in_cfg_w(path) && level==GetFileExInfoStandard && d) {
    memset(d,0,sizeof(*d));d->dwFileAttributes=FILE_ATTRIBUTE_NORMAL;d->nFileSizeLow=CFG_SIZE;
    return TRUE;
  }
  return GetFileAttributesExW(path,level,info);
}
/* The game's optional C stdio route. tmpfile is a private CRT-backed stream;
 * it may use an automatically deleted temporary file on the host. It never
 * creates Spidey.cfg. All bytes are copied into/out of our virtual array.
 */
static FILE * __cdecl hooked_fopen(const char *path,const char *mode) {
  FILE *f;unsigned i;int writable=0;
  if(!ready || !ends_in_cfg_a(path))return fopen(path,mode);
  if(!mode)return NULL;
  writable=(strchr(mode,'w')!=NULL || strchr(mode,'a')!=NULL || strchr(mode,'+')!=NULL);
  EnterCriticalSection(&guard);
  for(i=0;i<CFG_STREAMS && streams[i].f;i++);
  if(i==CFG_STREAMS){LeaveCriticalSection(&guard);return NULL;}
  f=tmpfile();
  if(f) {
    fwrite(cfg,1,CFG_SIZE,f);fflush(f);fseek(f,0,SEEK_SET);
    streams[i].f=f;streams[i].writable=writable;
    stdio_opens++;
    report("SPIDEY-CFG: virtual fopen intercepted (private temporary stream)");
  }
  LeaveCriticalSection(&guard);
  return f;
}
static FILE * __cdecl hooked_wfopen(const WCHAR *path,const WCHAR *mode) {
  char a_path[MAX_PATH],a_mode[32];
  if(!path || !mode)return NULL;
  if(!WideCharToMultiByte(CP_ACP,0,path,-1,a_path,sizeof(a_path),NULL,NULL) ||
     !WideCharToMultiByte(CP_ACP,0,mode,-1,a_mode,sizeof(a_mode),NULL,NULL))return NULL;
  return hooked_fopen(a_path,a_mode);
}
static int __cdecl hooked_fclose(FILE *f) {
  VStream *v;int r;
  if(!ready)return fclose(f);
  EnterCriticalSection(&guard);v=get_stream(f);
  if(!v){LeaveCriticalSection(&guard);return fclose(f);}
  if(v->writable) {
    BYTE candidate[CFG_SIZE];size_t n;
    fflush(f);rewind(f);n=fread(candidate,1,CFG_SIZE,f);
    if(n==CFG_SIZE) {
      if(memcmp(cfg,candidate,CFG_SIZE)) {memcpy(cfg,candidate,CFG_SIZE);dirty=1;}
      apply_locked_display();
      report("SPIDEY-CFG: virtual fclose captured stdio settings");
      save_settings();
    } else report("SPIDEY-CFG: WARNING stdio wrote fewer than 252 bytes; retaining previous settings");
  }
  v->f=NULL;v->writable=0;r=fclose(f);
  LeaveCriticalSection(&guard);
  return r;
}

struct ApiHook {const char *dll; const char *name; void *hook; unsigned matches; unsigned installed;};
static struct ApiHook apis[]={
  {"kernel32.dll","CreateFileA",(void*)hooked_CreateFileA,0,0},
  {"kernel32.dll","CreateFileW",(void*)hooked_CreateFileW,0,0},
  {"kernel32.dll","ReadFile",(void*)hooked_ReadFile,0,0},
  {"kernel32.dll","WriteFile",(void*)hooked_WriteFile,0,0},
  {"kernel32.dll","SetFilePointer",(void*)hooked_SetFilePointer,0,0},
  {"kernel32.dll","SetFilePointerEx",(void*)hooked_SetFilePointerEx,0,0},
  {"kernel32.dll","GetFileSize",(void*)hooked_GetFileSize,0,0},
  {"kernel32.dll","GetFileSizeEx",(void*)hooked_GetFileSizeEx,0,0},
  {"kernel32.dll","GetFileType",(void*)hooked_GetFileType,0,0},
  {"kernel32.dll","CloseHandle",(void*)hooked_CloseHandle,0,0},
  {"kernel32.dll","FlushFileBuffers",(void*)hooked_FlushFileBuffers,0,0},
  {"kernel32.dll","SetEndOfFile",(void*)hooked_SetEndOfFile,0,0},
  {"kernel32.dll","GetFileAttributesA",(void*)hooked_GetFileAttributesA,0,0},
  {"kernel32.dll","GetFileAttributesW",(void*)hooked_GetFileAttributesW,0,0},
  {"kernel32.dll","GetFileAttributesExA",(void*)hooked_GetFileAttributesExA,0,0},
  {"kernel32.dll","GetFileAttributesExW",(void*)hooked_GetFileAttributesExW,0,0},
  {"msvcrt.dll","fopen",(void*)hooked_fopen,0,0},
  {"msvcrt.dll","_wfopen",(void*)hooked_wfopen,0,0},
  {"msvcrt.dll","fclose",(void*)hooked_fclose,0,0}
};
static int hook_allowed(unsigned i,int file_route,int stdio_route) {
  if(!lstrcmpiA(apis[i].dll,"kernel32.dll"))return file_route;
  return stdio_route;
}
static int has_import(const char *dll,const char *name) {
  unsigned i;
  for(i=0;i<sizeof(apis)/sizeof(apis[0]);i++)
    if(!lstrcmpiA(apis[i].dll,dll) && !strcmp(apis[i].name,name) && apis[i].matches)return 1;
  return 0;
}
unsigned SpiderCfg_Install(const char *path,void (*logger)(const char *)) {
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *nt;
  IMAGE_IMPORT_DESCRIPTOR *imp;
  unsigned hits=0,i,pass;
  int file_route,stdio_route;
  char summary[260];
  DWORD old,ignored;
  if(!path || strlen(path)>=sizeof(ini_file) || !base)return 0;
  note=logger;strcpy(ini_file,path);
  dos=(IMAGE_DOS_HEADER*)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>0x1000)return 0;
  nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
  if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     nt->OptionalHeader.ImageBase!=0x400000 || (uintptr_t)base!=0x400000)return 0;
  if(!nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress)return 0;
  InitializeCriticalSection(&guard);
  load_settings();
  /* Two passes: first count imports, then hook only coherent API families.
   * Never return a virtual NUL handle to a caller unless we also intercept
   * its direct ReadFile/SetFilePointer/CloseHandle path. CRT paths likewise
   * require fclose interception to preserve game writes. */
  for(pass=0;pass<2;pass++) {
    imp=(IMAGE_IMPORT_DESCRIPTOR*)(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for(;imp->Name;imp++) {
      const char *dll=(const char*)(base+imp->Name);
      IMAGE_THUNK_DATA32 *thunk,*names;
      if(!imp->OriginalFirstThunk || !imp->FirstThunk)continue;
      thunk=(IMAGE_THUNK_DATA32*)(base+imp->FirstThunk);
      names=(IMAGE_THUNK_DATA32*)(base+imp->OriginalFirstThunk);
      for(;names->u1.AddressOfData;thunk++,names++) {
        const char *name;void **slot;
        if(IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))continue;
        name=(const char*)((IMAGE_IMPORT_BY_NAME*)(base+names->u1.AddressOfData))->Name;
        for(i=0;i<sizeof(apis)/sizeof(apis[0]);i++) {
          if(lstrcmpiA(dll,apis[i].dll) || strcmp(name,apis[i].name))continue;
          if(pass==0) {apis[i].matches++;break;}
          if(!hook_allowed(i,file_route,stdio_route))break;
          slot=(void**)&thunk->u1.Function;
          if(!VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&old))break;
          *slot=apis[i].hook;
          VirtualProtect(slot,sizeof(*slot),old,&ignored);
          apis[i].installed++;hits++;break;
        }
      }
    }
    if(pass==0) {
      file_route=(has_import("kernel32.dll","CreateFileA") || has_import("kernel32.dll","CreateFileW")) &&
        has_import("kernel32.dll","ReadFile") && has_import("kernel32.dll","CloseHandle") &&
        (has_import("kernel32.dll","SetFilePointer") || has_import("kernel32.dll","SetFilePointerEx"));
      stdio_route=(has_import("msvcrt.dll","fopen") || has_import("msvcrt.dll","_wfopen")) &&
        has_import("msvcrt.dll","fclose");
      if(!file_route) report("SPIDEY-CFG: Win32 route not sufficiently imported; left untouched");
      if(!stdio_route) report("SPIDEY-CFG: C stdio route not sufficiently imported; left untouched");
    }
  }
  ready=1;
  snprintf(summary,sizeof(summary),"SPIDEY-CFG: virtual mode ENABLED; hooked EXE IAT entries=%u (game must actually call these APIs)",hits);
  report(summary);
  for(i=0;i<sizeof(apis)/sizeof(apis[0]);i++)if(apis[i].matches){
    snprintf(summary,sizeof(summary),"SPIDEY-CFG: import %s!%s present=%u hooked=%u",apis[i].dll,apis[i].name,apis[i].matches,apis[i].installed);
    report(summary);
  }
  if(!hits)report("SPIDEY-CFG: WARNING no coherent supported API route found; file virtualization NOT verified");
  return hits;
}

/* 2026-10-17 experimental bootstrap (supported exact executable only).
 * The game normally reads Spidey.cfg through its statically linked CRT at
 * VA 0x515680 before our worker has installed its EXE IAT hooks.  Once the
 * worker is ready, the first DirectDrawCreateEx is a natural MAIN-THREAD
 * synchronization point, before the graphics device and gameplay start.
 *
 * Crucially, test the actual game's CRT _fopen/_fread/_fclose first.  If
 * that code does not pass through our virtual Win32 IAT hooks, DO NOT run
 * the loader: its missing-file fallback would clobber the game's settings.
 * Only an authenticated virtual 4-byte read permits the one-time reload.
 * All pointers and instruction bytes are hard-coded for the SHA-verified
 * SpideyPC.exe. No EXE patch or undocumented cross-CRT FILE* exchange.
 */
void SpiderCfg_BootstrapGameRead(void) {
  typedef void *(__cdecl *GameFopen)(const char *,const char *);
  typedef size_t (__cdecl *GameFread)(void *,size_t,size_t,void *);
  typedef int (__cdecl *GameFclose)(void *);
  typedef void (__cdecl *GameLoadConfig)(void);
  BYTE *image=(BYTE*)GetModuleHandleA(NULL);
  GameFopen game_open=(GameFopen)(uintptr_t)0x52a9e3;
  GameFread game_read=(GameFread)(uintptr_t)0x52a9f6;
  GameFclose game_close=(GameFclose)(uintptr_t)0x52a7fc;
  GameLoadConfig game_load=(GameLoadConfig)(uintptr_t)0x515680;
  void *fp;
  DWORD expected=0,probe=0,opened,served;
  size_t nr=0;
  char msg[240];
  static const BYTE loader_code[]={0x81,0xEC,0xD8,0,0,0,0x56,0x57};
  static const BYTE fopen_code[]={0x6A,0x40,0xFF,0x74,0x24,0x0C};
  static const BYTE fread_code[]={0x55,0x8B,0xEC,0x56};
  static const BYTE fclose_code[]={0x56,0x8B,0x74,0x24,0x08};
  if(!ready || !image ||
     InterlockedCompareExchange(&bootstrap_attempted,1,0)!=0) return;
  if((uintptr_t)image!=0x400000 ||
     memcmp(image+(0x515680-0x400000),loader_code,sizeof(loader_code)) ||
     memcmp(image+(0x52a9e3-0x400000),fopen_code,sizeof(fopen_code)) ||
     memcmp(image+(0x52a9f6-0x400000),fread_code,sizeof(fread_code)) ||
     memcmp(image+(0x52a7fc-0x400000),fclose_code,sizeof(fclose_code))) {
    report("SPIDEY-CFG: BOOTSTRAP SKIPPED: supported game CRT/load opcodes not matched");
    return;
  }
  EnterCriticalSection(&guard);
  opened=opens;served=reads;expected=get_word(0);
  LeaveCriticalSection(&guard);
  /* Use precisely the game's own CRT, not our DLL's incompatible FILE type. */
  fp=game_open((const char*)(uintptr_t)0x5690b4,
               (const char*)(uintptr_t)0x567fec);
  if(fp) {
    nr=game_read(&probe,sizeof(probe),1,fp);
    game_close(fp);
  }
  EnterCriticalSection(&guard);
  if(!fp || opens<=opened || reads<=served || nr!=1 || probe!=expected) {
    snprintf(msg,sizeof(msg),
      "SPIDEY-CFG: BOOTSTRAP SKIPPED: CRT virtual read probe failed (opened=%lu read=%lu count=%lu expected=%lu got=%lu); game state unchanged",
      (unsigned long)(opens-opened),(unsigned long)(reads-served),
      (unsigned long)nr,(unsigned long)expected,(unsigned long)probe);
    report(msg);
    LeaveCriticalSection(&guard);
    return;
  }
  LeaveCriticalSection(&guard);
  report("SPIDEY-CFG: BOOTSTRAP PROBE OK: game CRT read verified virtual config bytes");
  EnterCriticalSection(&guard);
  opened=opens;served=reads;
  LeaveCriticalSection(&guard);
  /* The original loader restores all keys, gamepad and sound settings and
   * applies the locked resolution/depth without modifying game executable. */
  game_load();
  EnterCriticalSection(&guard);
  snprintf(msg,sizeof(msg),
    "SPIDEY-CFG: BOOTSTRAP game loader returned: virtual opens +%lu reads +%lu; configured=%ux%u depth=32 [%s]",
    (unsigned long)(opens-opened),(unsigned long)(reads-served),
    policy_width,policy_height,
    (opens>opened && reads>served) ? "READ CONFIRMED" : "WARNING UNCONFIRMED");
  report(msg);
  LeaveCriticalSection(&guard);
}
