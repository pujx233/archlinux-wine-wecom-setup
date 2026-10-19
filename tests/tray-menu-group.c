#include <assert.h>
#include <stdio.h>
#include <string.h>
#define __declspec(value)
#define __stdcall
#include "../scripts/titlebar-managed.c"
struct window {const char *cls;unsigned owner,pid;int visible;};
static struct window windows[10];static HANDLE foreground;static DWORD now;static int closed;
BOOL IsWindowVisible(HANDLE w){return windows[(unsigned long)w].visible;}
HANDLE GetParent(HANDLE w){return (HANDLE)(unsigned long)windows[(unsigned long)w].owner;}
int GetClassNameA(HANDLE w,char *buf,int len){strcpy(buf,windows[(unsigned long)w].cls);return strlen(buf);}
DWORD GetWindowThreadProcessId(HANDLE w,DWORD *pid){*pid=windows[(unsigned long)w].pid;return 1;}
HANDLE GetForegroundWindow(void){return foreground;}
DWORD GetTickCount(void){return now;}
BOOL IsIconic(HANDLE w){return 0;}
long GetWindowLongA(HANDLE w,int i){return 0;}
BOOL PostMessageA(HANDLE w,unsigned msg,unsigned wp,long lp){assert(w==(HANDLE)2&&msg==0x10);closed++;return 1;}
HANDLE GetStdHandle(DWORD which){return (HANDLE)9;}
BOOL WriteFile(HANDLE f,const void *buf,DWORD size,DWORD *written,void *overlapped){*written=size;return 1;}
static void reset(void){
 windows[0]=(struct window){"",0,0,0};
 windows[1]=(struct window){"WeWorkWindow",0,10,1};
 windows[2]=(struct window){"DuiMenuWnd",1,10,1};
 windows[3]=(struct window){"DuiMenuWnd",2,10,1};
 windows[4]=(struct window){"PerryShadowWnd",3,10,1};
 windows[5]=(struct window){"PerryShadowWnd",2,10,1};
 windows[6]=(struct window){"PerryShadowWnd",1,10,1};
 windows[7]=(struct window){"DuiMenuWnd",1,10,1};
 windows[8]=(struct window){"OtherApp",0,99,1};
 main_window=(HANDLE)1;tracked_menu=(HANDLE)2;menu_tray=(HANDLE)9;main_pid=10;
 menu_external_since=0;menu_opened_at=10;now=100;closed=0;
}
int main(void){
 reset();for(unsigned long i=2;i<=5;i++)assert(is_tray_menu_group((HANDLE)i));
 assert(!is_tray_menu_group((HANDLE)1)&&!is_tray_menu_group((HANDLE)6)&&!is_tray_menu_group((HANDLE)7)&&!is_tray_menu_group((HANDLE)8));
 windows[4].pid=99;assert(!is_tray_menu_group((HANDLE)4));
 reset();windows[3].visible=0;assert(!is_tray_menu_group((HANDLE)4));
 reset();windows[3].owner=4;assert(!is_tray_menu_group((HANDLE)4)); /* cycle bounded */
 reset();foreground=(HANDLE)4;foreground_changed(0,3,foreground,0,0,0,now);check_tray_menu_focus();now+=500;check_tray_menu_focus();assert(!closed&&tracked_menu);
 foreground=(HANDLE)8;foreground_changed(0,3,foreground,0,0,0,now);assert(closed==1&&!tracked_menu);
 reset();foreground=(HANDLE)8;check_tray_menu_focus();now+=100;check_tray_menu_focus();assert(closed==1&&!tracked_menu);
 puts("tray menu group: owner-chain, focus handoff and external dismissal passed");
}
