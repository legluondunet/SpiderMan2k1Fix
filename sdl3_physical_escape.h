/* Raw hardware keyboard Escape detector for SDL3 modal diagnostics.
 * Wine/Proton may translate physical controller Circle/B to WM_KEYDOWN
 * VK_ESCAPE. Do not trust that message during a controller capture.
 *
 * Foreground WM_INPUT (RIM_TYPEKEYBOARD) can distinguish a physical
 * keyboard's Escape key from the gamepad's synthesized Win32 event.
 * Preserve any pre-existing app keyboard raw-input registration: if
 * someone else already registered it, fail closed and keep the existing
 * Ctrl+Shift+Q/T escape chord rather than hijack their recipient.
 *
 * Register only while a modal is open and explicitly unregister when
 * leaving. No native game DirectInput state is ever touched.
 */
#ifndef SPIDERMAN2K1FIX_SDL3_PHYSICAL_ESCAPE_H
#define SPIDERMAN2K1FIX_SDL3_PHYSICAL_ESCAPE_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdlib.h>

static int spider_sdl3_escape_begin(HWND hwnd) {
  RAWINPUTDEVICE devices[64],keyboard;
  UINT count=0,i;
  if(!hwnd || !IsWindow(hwnd))return 0;
  if(GetRegisteredRawInputDevices(NULL,&count,sizeof(RAWINPUTDEVICE))!=(UINT)0)
    return 0;
  if(count>64u)return 0;
  if(count && GetRegisteredRawInputDevices(devices,&count,
       sizeof(RAWINPUTDEVICE))==(UINT)-1)return 0;
  for(i=0;i<count;i++) {
    /* Do not steal the existing game's or another mod's keyboard raw
     * input registration. 1/6 is the HID generic desktop keyboard. */
    if(devices[i].usUsagePage==0x01u && devices[i].usUsage==0x06u)
      return 0;
  }
  keyboard.usUsagePage=0x01u;
  keyboard.usUsage=0x06u;
  keyboard.dwFlags=0u; /* foreground only, no NOLEGACY */
  keyboard.hwndTarget=hwnd;
  return RegisterRawInputDevices(&keyboard,1u,sizeof(keyboard))!=FALSE;
}
static int spider_sdl3_escape_key(HRAWINPUT input) {
  RAWINPUT packet;
  UINT size=(UINT)sizeof(packet);
  UINT got;
  if(!input)return 0;
  got=GetRawInputData(input,RID_INPUT,&packet,&size,
                      sizeof(RAWINPUTHEADER));
  if(got==(UINT)-1 || got!=(UINT)sizeof(packet) &&
     got<(UINT)(sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD)))return 0;
  if(packet.header.dwType!=RIM_TYPEKEYBOARD)return 0;
  return packet.data.keyboard.VKey==VK_ESCAPE &&
         !(packet.data.keyboard.Flags&RI_KEY_BREAK) &&
         (packet.data.keyboard.Message==WM_KEYDOWN ||
          packet.data.keyboard.Message==WM_SYSKEYDOWN);
}
static void spider_sdl3_escape_end(int registered) {
  RAWINPUTDEVICE keyboard;
  if(!registered)return;
  keyboard.usUsagePage=0x01u;
  keyboard.usUsage=0x06u;
  keyboard.dwFlags=RIDEV_REMOVE;
  keyboard.hwndTarget=NULL;
  RegisterRawInputDevices(&keyboard,1u,sizeof(keyboard));
}
#endif
