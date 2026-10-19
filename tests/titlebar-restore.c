/* Exercise the production restore guard without starting Wine or a desktop. */
#include <assert.h>
#include <stdio.h>
#define __declspec(value)
#define __stdcall
#include "../scripts/titlebar-managed.c"

static HANDLE foreground;
static int visible,iconic,disabled,posts;
HANDLE GetForegroundWindow(void){return foreground;}
BOOL IsWindowVisible(HANDLE w){assert(w==main_window);return visible;}
BOOL IsIconic(HANDLE w){assert(w==main_window);return iconic;}
long GetWindowLongA(HANDLE w,int index){assert(w==main_window&&index==-16);return disabled?0x08000000:0;}
BOOL PostMessageA(HANDLE w,unsigned msg,unsigned wp,long lp){
    assert(w==main_window&&msg==0x0112&&wp==0xf120&&lp==0);posts++;return 1;
}
HANDLE GetStdHandle(DWORD which){return (HANDLE)3;}
BOOL WriteFile(HANDLE file,const void *buf,DWORD size,DWORD *written,void *overlapped){
    *written=size;return 1;
}
static void reset(void){
    main_window=foreground=(HANDLE)1;visible=iconic=1;disabled=posts=0;
}
int main(void){
    reset();restore_foreground_main(main_window);assert(posts==1);
    reset();iconic=0;restore_foreground_main(main_window);assert(!posts);
    reset();visible=0;restore_foreground_main(main_window);assert(!posts);
    reset();disabled=1;restore_foreground_main(main_window);assert(!posts);
    reset();foreground=(HANDLE)2;restore_foreground_main(main_window);assert(!posts);
    reset();restore_foreground_main((HANDLE)2);assert(!posts);
    reset();main_window=foreground=0;restore_foreground_main(0);assert(!posts);
    reset();restore_foreground_main(main_window);iconic=0;
    restore_foreground_main(main_window);assert(posts==1);
    puts("titlebar restore: 8 scenarios passed");
}
