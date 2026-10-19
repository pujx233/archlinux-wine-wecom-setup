#include <assert.h>
#include <stdio.h>
#include <string.h>
#define __declspec(value)
#define __stdcall
#include "../scripts/titlebar-managed.c"
static HANDLE foreground,owner;
static DWORD now,pid;
static int visible,iconic,requests,open_ok,has_xid;
static RECT rect;
static const char *window_class;
int GetSystemMetrics(int index){assert(index==76||index==77);return index==76?-3456:0;}
HANDLE MonitorFromPoint(POINT p,DWORD flags){assert(0);return 0;}
BOOL GetMonitorInfoA(HANDLE m,MONITORINFO *info){assert(0);return 0;}
int MulDiv(int a,int b,int c){return a*b/c;}
BOOL SetWindowPos(HANDLE w,HANDLE z,int x,int y,int width,int height,unsigned flags){assert(0);return 0;}
DWORD GetTickCount(void){return now;}
HANDLE GetForegroundWindow(void){return foreground;}
BOOL IsWindowVisible(HANDLE w){return visible;}
BOOL IsIconic(HANDLE w){assert(w==main_window);return iconic;}
HANDLE GetParent(HANDLE w){return owner;}
int GetClassNameA(HANDLE w,char *buf,int len){strcpy(buf,window_class);return strlen(buf);}
DWORD GetWindowThreadProcessId(HANDLE w,DWORD *p){*p=pid;return 1;}
BOOL GetWindowRect(HANDLE w,RECT *r){*r=rect;return 1;}
HANDLE GetPropA(HANDLE w,const char *name){assert(!strcmp(name,"__wine_x11_whole_window"));return has_xid?(w==main_window?(HANDLE)100:(HANDLE)200):0;}
HANDLE CreateFileA(const char *path,DWORD access,DWORD sharing,void *sec,DWORD creation,DWORD attr,HANDLE template){
    assert(!strcmp(path,"C:\\windows\\temp\\wecom-tray-menu-request"));assert(access==0x40000000&&creation==2);return open_ok?(HANDLE)4:(HANDLE)-1;
}
BOOL CloseHandle(HANDLE w){assert(w==(HANDLE)4);return 1;}
HANDLE GetStdHandle(DWORD which){return (HANDLE)3;}
BOOL WriteFile(HANDLE file,const void *buf,DWORD size,DWORD *written,void *overlapped){
    if(file==(HANDLE)4){const unsigned *r=buf;assert(size==32&&r[0]==0x57434d32&&r[1]==100&&r[2]==200&&r[3]==now&&r[4]==5556&&r[5]==1900&&r[6]==116&&r[7]==97);requests++;}
    *written=size;return 1;
}
static void reset(void){
    main_window=owner=(HANDLE)1;tracked_menu=pending_menu_restore=(HANDLE)2;
    menu_tray=foreground=(HANDLE)3;main_pid=pid=10;window_class="DuiMenuWnd";
    visible=iconic=open_ok=has_xid=1;requests=0;
    rect=(RECT){2100,1900,2216,1997};now=200;menu_restore_due=100;menu_restore_deadline=1100;
}
int main(void){
    reset();restore_tray_menu();assert(requests==1&&!pending_menu_restore);restore_tray_menu();assert(requests==1);
    reset();iconic=0;restore_tray_menu();assert(!requests&&!pending_menu_restore);
    reset();visible=0;restore_tray_menu();assert(!requests&&!pending_menu_restore);
    reset();owner=(HANDLE)4;restore_tray_menu();assert(!requests&&!pending_menu_restore);
    reset();foreground=(HANDLE)4;restore_tray_menu();assert(!requests&&!pending_menu_restore);
    reset();pid=99;restore_tray_menu();assert(!requests&&!pending_menu_restore);
    reset();window_class="OtherMenu";restore_tray_menu();assert(!requests&&!pending_menu_restore);
    reset();now=50;restore_tray_menu();assert(!requests&&pending_menu_restore);
    reset();rect.right=rect.left;restore_tray_menu();assert(!requests&&menu_restore_due==250);
    now=250;rect.right+=116;restore_tray_menu();assert(requests==1&&!pending_menu_restore);
    reset();rect.bottom=rect.top;now=1100;restore_tray_menu();assert(!requests&&!pending_menu_restore);
    reset();open_ok=0;restore_tray_menu();assert(!requests&&!pending_menu_restore);
    reset();has_xid=0;restore_tray_menu();assert(!requests&&!pending_menu_restore);
    puts("tray restore: 12 scenarios passed");
}
