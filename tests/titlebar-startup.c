/* Exercise the real startup loop with mocked Wine APIs and a virtual clock. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define __declspec(value)
#define __stdcall
#include "../scripts/titlebar-managed.c"

static DWORD now, error;
static int scenario, entries, closes;
HANDLE CreateToolhelp32Snapshot(DWORD flags,DWORD pid){
    assert(flags==2&&pid==0);entries=0;
    if(scenario==5&&now>=10000&&now<20000)return (HANDLE)-1;
    return (HANDLE)1;
}
static int alive(void){
    if(scenario==1)return 0;
    if(scenario==2)return now<10000;
    if(scenario==3)return now<10000||now>=14000;
    if(scenario==5)return now<10000;
    return 1;
}
BOOL Process32First(HANDLE snapshot,PROCESSENTRY32 *entry){
    assert(snapshot==(HANDLE)1&&entry->size==sizeof(*entry));
    /* Ignore an exact image with invalid PID and an unrelated process whose
       name merely contains WXWork.exe before reaching the real candidate. */
    entry->pid=0;strcpy(entry->exe,"WXWork.exe");return 1;
}
BOOL Process32Next(HANDLE snapshot,PROCESSENTRY32 *entry){
    assert(snapshot==(HANDLE)1);
    entries++;
    if(entries==1){entry->pid=9;strcpy(entry->exe,"WXWork.exe.bak");return 1;}
    if(entries==2&&alive()){entry->pid=10;strcpy(entry->exe,"C:\\WXWork\\WXWork.EXE");return 1;}
    error=18;return 0;
}
DWORD GetLastError(void){return error;}
BOOL CloseHandle(HANDLE handle){assert(handle==(HANDLE)1);closes++;return 1;}
DWORD GetTickCount(void){return now;}
void Sleep(DWORD delay){assert(delay==1000);now+=delay;assert(now<=300000);}
HANDLE FindWindowA(const char *cls,const char *title){
    assert(!strcmp(cls,"WeWorkWindow")&&!title);
    DWORD ready=scenario==0?250000:scenario==3?180000:scenario==4?0:scenario==6?121000:999999;
    return now>=ready?(HANDLE)2:0;
}
static void check(int selected,DWORD expected,int window){
    scenario=selected;now=error=0;closes=0;main_window=0;
    wait_for_main_window();
    assert(now==expected);assert(!!main_window==window);
    if(selected!=4)assert(closes>0);
}
int main(void){
    assert(is_wecom_exe("WXWork.exe"));assert(is_wecom_exe("C:\\WXWork\\wxwork.EXE"));
    assert(!is_wecom_exe("OtherWXWork.exe"));assert(!is_wecom_exe("WXWork.exe.bak"));
    check(0,250000,1); /* Live process without a window beyond 120 seconds. */
    check(1,120000,0); /* No process: bounded failed-launch grace. */
    check(2,16000,0);  /* Observed process exits before any main window. */
    check(3,180000,1); /* Four-second startup handoff. */
    check(4,0,1);      /* Existing main window retains the immediate bind. */
    check(5,26000,0);  /* Snapshot failure cannot count as confirmed absence. */
    check(6,121000,1); /* The original 120-second boundary no longer ends a live app. */
    puts("titlebar startup: 7 scenarios passed");
}
