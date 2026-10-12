/* Maintain only the current prefix's WeCom windows and unique Wine tray. */
typedef unsigned long DWORD;
typedef int BOOL;
typedef void *HANDLE;
typedef struct {long left,top,right,bottom;} RECT;
typedef struct {long x,y;} POINT;
typedef struct {HANDLE hwnd;unsigned message,wparam;long lparam;DWORD time;POINT pt;DWORD private_data;} MSG;
typedef struct {DWORD size;RECT monitor,work;DWORD flags;} MONITORINFO;
#define API __declspec(dllimport) __stdcall
API HANDLE FindWindowA(const char *, const char *);
API HANDLE FindWindowExA(HANDLE,HANDLE,const char *,const char *);
API BOOL EnumWindows(BOOL (__stdcall *)(HANDLE,long),long);
API HANDLE GetParent(HANDLE);
API int GetClassNameA(HANDLE,char *,int);
API DWORD GetWindowThreadProcessId(HANDLE,DWORD *);
API long GetWindowLongA(HANDLE,int);
API long SetWindowLongA(HANDLE,int,long);
API BOOL SetWindowPos(HANDLE,HANDLE,int,int,int,int,unsigned);
API BOOL PostMessageA(HANDLE,unsigned,unsigned,long);
API BOOL GetWindowRect(HANDLE,RECT *);
API HANDLE GetStdHandle(DWORD);
API BOOL WriteFile(HANDLE,const void *,DWORD,DWORD *,void *);
API char *GetCommandLineA(void);
API void ExitProcess(DWORD);
API HANDLE CreateMutexA(void *,BOOL,const char *);
API DWORD GetLastError(void);
API HANDLE OpenProcess(DWORD,BOOL,DWORD);
API DWORD WaitForSingleObject(HANDLE,DWORD);
API BOOL CloseHandle(HANDLE);
API void Sleep(DWORD);
API DWORD GetEnvironmentVariableA(const char *,char *,DWORD);
API int GetSystemMetrics(int);
API HANDLE GetForegroundWindow(void);
API BOOL SetForegroundWindow(HANDLE);
API DWORD GetCurrentThreadId(void);
API HANDLE GetModuleHandleA(const char *);
API BOOL AttachThreadInput(DWORD,DWORD,BOOL);
API BOOL IsIconic(HANDLE);
API HANDLE WindowFromPoint(POINT);
API HANDLE SetWindowsHookExA(int,long (__stdcall *)(int,unsigned,long),HANDLE,DWORD);
API long CallNextHookEx(HANDLE,int,unsigned,long);
API BOOL UnhookWindowsHookEx(HANDLE);
API BOOL IsWindowVisible(HANDLE);
API HANDLE MonitorFromRect(const RECT *,DWORD);
API BOOL GetMonitorInfoA(HANDLE,MONITORINFO *);
API BOOL GetCursorPos(POINT *);
API HANDLE MonitorFromPoint(POINT,DWORD);
API int MulDiv(int,int,int);
API DWORD GetTickCount(void);
API HANDLE SetWinEventHook(DWORD,DWORD,HANDLE,void (__stdcall *)(HANDLE,DWORD,HANDLE,long,long,DWORD,DWORD),DWORD,DWORD,DWORD);
API BOOL UnhookWinEvent(HANDLE);
API DWORD MsgWaitForMultipleObjects(DWORD,const HANDLE *,BOOL,DWORD,DWORD);
API BOOL PeekMessageA(MSG *,HANDLE,unsigned,unsigned,unsigned);
API BOOL TranslateMessage(const MSG *);
API long DispatchMessageA(const MSG *);
static HANDLE main_window;static DWORD main_pid;static int apply,found,changed,watch;
static long tray_geometry[7],tray_maps[16][8];static int tray_enabled;static HANDLE pending_menu;
static DWORD menu_due;static RECT tray_rect;static POINT tray_point;
static HANDLE pending_tooltip,tooltip_owner;static DWORD tooltip_due;
static RECT tooltip_rect;static POINT tooltip_point;
static HANDLE pending_activation;static DWORD activation_due;
static HANDLE tracked_menu,menu_tray;static DWORD menu_opened_at,menu_external_since;
static POINT menu_click;static int click_in_menu;
static char output[2048];static unsigned pos;
static void text(const char *s){while(*s&&pos<sizeof(output)-64)output[pos++]=*s++;}
static void hex(DWORD v){text("0x");for(int i=7;i>=0;i--)output[pos++]="0123456789abcdef"[(v>>(4*i))&15];}
static int same(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static BOOL __stdcall inspect(HANDLE w,long unused){
    char cls[128];DWORD pid=0;GetClassNameA(w,cls,128);
    int is_titlebar=same(cls,"TitleBarWindow"),is_shadow=same(cls,"PerryShadowWnd");
    if((!is_titlebar&&!is_shadow)||GetParent(w)!=main_window)return 1;
    GetWindowThreadProcessId(w,&pid);if(pid!=main_pid)return 1;
    DWORD style=GetWindowLongA(w,-16),exstyle=GetWindowLongA(w,-20);RECT before,after;
    if(is_shadow&&!(exstyle&0x00080000))return 1; /* Only layered shadows. */
    GetWindowRect(w,&before);found++;
    int needs_change=is_titlebar?(!(style&0x00080000)||(exstyle&0x08000000)):!(exstyle&0x00000020);
    if(!watch || needs_change) {text(cls);text(" hwnd=");hex((DWORD)w);text(" owner=");hex((DWORD)main_window);text(" style=");hex(style);text(" exstyle=");hex(exstyle);text("\n");}
    if(apply&&needs_change){
        /* WS_SYSMENU makes an owned popup managed in winex11.drv;
           clearing WS_EX_NOACTIVATE lets a title-bar click raise the group;
           WS_EX_TRANSPARENT lets border input pass through the layered shadow
           to WeWorkWindow, which implements the actual resizing. */
        if(is_titlebar){
            if(!(style&0x00080000))SetWindowLongA(w,-16,style|0x00080000);
            if(exstyle&0x08000000)SetWindowLongA(w,-20,exstyle&~0x08000000);
        }
        else SetWindowLongA(w,-20,exstyle|0x00000020);
        BOOL ok=SetWindowPos(w,0,0,0,0,0,0x37); /* FRAMECHANGED,NOMOVE,NOSIZE,NOZORDER,NOACTIVATE */
        GetWindowRect(w,&after);
        text("new_style=");hex(GetWindowLongA(w,-16));text(" new_exstyle=");hex(GetWindowLongA(w,-20));text(" position_refresh=");hex(ok);
        text(" geometry_unchanged=");hex(before.left==after.left&&before.top==after.top&&before.right==after.right&&before.bottom==after.bottom);text("\n");
        if(ok&&(is_titlebar?((GetWindowLongA(w,-16)&0x00080000)&&!(GetWindowLongA(w,-20)&0x08000000)):(GetWindowLongA(w,-20)&0x00000020)))changed++;
    }
    return 1;
}
static void flush(void){DWORD written;if(pos)WriteFile(GetStdHandle((DWORD)-11),output,pos,&written,0);pos=0;}
static int read_values(char **cursor,long *values,unsigned count){
    char *s=*cursor;
    for(unsigned i=0;i<count;i++){
        int sign=1;long n=0;if(*s=='-'){sign=-1;s++;}
        if(*s<'0'||*s>'9')return 0;
        while(*s>='0'&&*s<='9'){n=n*10+(*s++-'0');if(n>1000000)return 0;}
        values[i]=n*sign;
        if(i+1<count&&*s++!=',')return 0;
    }
    *cursor=s;return 1;
}
static void read_tray_geometry(void){
    char value[2048];DWORD length=GetEnvironmentVariableA("WECOM_TRAY_GEOMETRY",value,sizeof(value));
    if(!length||length>=sizeof(value))return;
    char *s=value;long version;
    if(!read_values(&s,&version,1)||version!=2||*s++!=',')return;
    if(!read_values(&s,tray_geometry,7)||tray_geometry[6]<1||tray_geometry[6]>16)return;
    for(long i=0;i<tray_geometry[6];i++){
        if(*s++!=';'||!read_values(&s,tray_maps[i],8))return;
        if(tray_maps[i][2]<=0||tray_maps[i][3]<=0||tray_maps[i][6]<=0||tray_maps[i][7]<=0)return;
    }
    if(!*s)tray_enabled=1;
}
static int tray_layout_matches(void){
    static const int metrics[7]={76,77,78,79,0,1,80};
    for(unsigned i=0;i<7;i++)if(GetSystemMetrics(metrics[i])!=tray_geometry[i])return 0;
    return 1;
}
static int is_tray_tooltip(HANDLE w,HANDLE owner){
    char cls[128];DWORD pid=0,owner_pid=0;
    if(!owner||GetParent(w)!=owner||FindWindowA("__wine_tray_icon",0)!=owner||
       FindWindowExA(0,owner,"__wine_tray_icon",0))return 0;
    if(!GetClassNameA(w,cls,sizeof(cls))||!same(cls,"tooltips_class32"))return 0;
    if(GetWindowLongA(w,-16)&0x40)return 0; /* TTS_BALLOON: not a hover tooltip. */
    GetWindowThreadProcessId(w,&pid);GetWindowThreadProcessId(owner,&owner_pid);
    return pid&&pid==owner_pid;
}
static int is_wecom_menu(HANDLE w){
    char cls[128];DWORD pid=0;
    if(!w||!IsWindowVisible(w)||!GetClassNameA(w,cls,sizeof(cls))||!same(cls,"DuiMenuWnd"))return 0;
    GetWindowThreadProcessId(w,&pid);return pid==main_pid;
}
static void dismiss_tray_menu(const char *reason){
    HANDLE w=tracked_menu;tracked_menu=0;menu_external_since=0;
    if(pending_menu==w)pending_menu=0;
    if(!is_wecom_menu(w)||GetParent(w)!=main_window)return;
    /* Ask the application to close normally, including its shadow/submenus. */
    BOOL posted=PostMessageA(w,0x0010,0,0); /* WM_CLOSE */
    text("tray menu dismiss reason=");text(reason);text(" posted=");hex(posted);text("\n");flush();
}
static BOOL __stdcall menu_contains_click(HANDLE w,long unused){
    RECT r;
    if(is_wecom_menu(w)&&GetWindowRect(w,&r)&&menu_click.x>=r.left&&menu_click.x<r.right&&
       menu_click.y>=r.top&&menu_click.y<r.bottom){click_in_menu=1;return 0;}
    return 1;
}
static void __stdcall foreground_changed(HANDLE hook,DWORD event,HANDLE w,long object,long child,DWORD thread,DWORD time){
    if(!tracked_menu||object||child||(long)(time-menu_opened_at)<0)return;
    if(!is_wecom_menu(tracked_menu)){tracked_menu=0;return;}
    /* Ignore delayed events and transitions within the tray/menu group. */
    if(w!=GetForegroundWindow()||w==menu_tray||is_wecom_menu(w))return;
    dismiss_tray_menu("foreground change");
}
static void check_tray_menu_focus(void){
    if(!tracked_menu)return;
    if(!is_wecom_menu(tracked_menu)||GetParent(tracked_menu)!=main_window){
        tracked_menu=0;menu_external_since=0;return;
    }
    HANDLE foreground=GetForegroundWindow();
    if(foreground==menu_tray||is_wecom_menu(foreground)){menu_external_since=0;return;}
    /* Wine can clear foreground on desktop/native-app activation without a
       WinEvent. Check only while this popup lives; allow a brief handoff. */
    DWORD now=GetTickCount();
    if(!menu_external_since)menu_external_since=now;
    else if((long)(now-menu_external_since)>=100)dismiss_tray_menu("foreground left");
}
static void __stdcall popup_shown(HANDLE hook,DWORD event,HANDLE w,long object,long child,DWORD thread,DWORD time){
    char cls[128];DWORD pid=0;
    if(object||child)return;
    GetWindowThreadProcessId(w,&pid);GetClassNameA(w,cls,sizeof(cls));
    if(same(cls,"tooltips_class32")){
        HANDLE owner=GetParent(w);RECT r;POINT p;
        if(!tray_enabled||!is_tray_tooltip(w,owner)||!GetWindowRect(owner,&r)||!GetCursorPos(&p))return;
        if(p.x<r.left||p.x>=r.right||p.y<r.top||p.y>=r.bottom)return;
        tooltip_owner=owner;tooltip_rect=r;tooltip_point=p;
        pending_tooltip=w;tooltip_due=GetTickCount()+50;
        return;
    }
    if(pid!=main_pid||GetParent(w)!=main_window||!same(cls,"DuiMenuWnd"))return;
    HANDLE foreground=GetForegroundWindow();GetClassNameA(foreground,cls,sizeof(cls));
    if(!same(cls,"__wine_tray_icon")||!GetWindowRect(foreground,&tray_rect))return;
    if(!GetCursorPos(&tray_point)||tray_point.x<tray_rect.left||tray_point.x>=tray_rect.right||
       tray_point.y<tray_rect.top||tray_point.y>=tray_rect.bottom)return;
    tracked_menu=w;menu_tray=foreground;menu_opened_at=time;menu_external_since=0;
    pending_menu=w;menu_due=GetTickCount()+50;
}
static void correct_tray_popup(HANDLE w,const RECT *anchor,POINT point,int tooltip){
    if(!tray_enabled||!IsWindowVisible(w)||!tray_layout_matches())return;
    RECT r;MONITORINFO m;m.size=sizeof(m);
    if(!GetWindowRect(w,&r))return;
    long width=r.right-r.left,height=r.bottom-r.top;
    if(width<=0||height<=0||width>600||height>600)return;
    /* Only a tray popup close to its icon. Normal context
       menus and submenus do not satisfy this anchor. A tooltip can extend
       to the left of its icon when Wine clamps it to a screen edge. */
    if(r.left<anchor->left-(tooltip?width+32:32)||r.left>anchor->right+32||
       r.top<anchor->top-600||r.top>anchor->bottom+32)return;
    long *mapping=0;
    for(long i=0;i<tray_geometry[6];i++){
        long *v=tray_maps[i];
        if(point.x<v[0]||point.x>=v[0]+v[2]||point.y<v[1]||point.y>=v[1]+v[3])continue;
        if(mapping)return; /* Ambiguous source rectangles: do not guess a screen. */
        mapping=v;
    }
    if(!mapping)return;
    POINT target={mapping[4]+MulDiv(point.x-mapping[0],mapping[6],mapping[2]),
                  mapping[5]+MulDiv(point.y-mapping[1],mapping[7],mapping[3])};
    if(!GetMonitorInfoA(MonitorFromPoint(target,0),&m))return;
    long dx=target.x-point.x,dy=target.y-point.y;
    if(!dx&&!dy)return;
    long x=r.left+dx,y=r.top+dy;
    if(width>m.work.right-m.work.left||height>m.work.bottom-m.work.top)return;
    if(x<m.work.left)x=m.work.left;if(y<m.work.top)y=m.work.top;
    if(x+width>m.work.right)x=m.work.right-width;if(y+height>m.work.bottom)y=m.work.bottom-height;
    if(SetWindowPos(w,0,x,y,0,0,0x15)){ /* NOSIZE,NOZORDER,NOACTIVATE */
        text(tooltip?"tray tooltip coordinate correction dx=":"tray menu coordinate correction dx=");hex(x-r.left);
        text(" dy=");hex(y-r.top);text("\n");flush();
    }
}
static void correct_pending_popups(void){
    if(pending_menu&&(long)(GetTickCount()-menu_due)>=0){
        HANDLE w=pending_menu;pending_menu=0;
        if(GetParent(w)==main_window)correct_tray_popup(w,&tray_rect,tray_point,0);
    }
    if(pending_tooltip&&(long)(GetTickCount()-tooltip_due)>=0){
        HANDLE w=pending_tooltip;pending_tooltip=0;RECT current;
        /* A click may relocate the tray between SHOW delivery and this pass. */
        if(!is_tray_tooltip(w,tooltip_owner)||!GetWindowRect(tooltip_owner,&current))return;
        if(current.left!=tooltip_rect.left||current.top!=tooltip_rect.top||
           current.right!=tooltip_rect.right||current.bottom!=tooltip_rect.bottom)return;
        correct_tray_popup(w,&tooltip_rect,tooltip_point,1);
    }
}
static long __stdcall tray_mouse(int code,unsigned message,long data){
    if(code>=0&&data&&tracked_menu&&
       (message==0x0201||message==0x0204||message==0x0207||message==0x020b)){
        /* Wine clicks outside the popup need not change the foreground.
           Native Wayland applications are covered by the foreground hook. */
        menu_click=*(POINT *)data;click_in_menu=0;
        EnumWindows(menu_contains_click,0);
        if(!click_in_menu)dismiss_tray_menu("outside click");
    }
    /* WH_MOUSE_LL runs in this watcher. Only an actual left-button release
       over this prefix's unique Wine tray icon may request activation. */
    if(code>=0&&message==0x0202&&data){
        POINT point=*(POINT *)data;HANDLE w=WindowFromPoint(point);char cls[128];
        if(w&&GetClassNameA(w,cls,sizeof(cls))&&same(cls,"__wine_tray_icon")&&
           FindWindowA("__wine_tray_icon",0)==w&&!FindWindowExA(0,w,"__wine_tray_icon",0)){
            pending_activation=w;activation_due=GetTickCount()+100;
        }
    }
    return CallNextHookEx(0,code,message,data);
}
static void activate_after_tray_click(void){
    if(!pending_activation||(long)(GetTickCount()-activation_due)<0)return;
    HANDLE tray=pending_activation;pending_activation=0;
    /* Let WeCom perform its own restore/hide operation first. Do not undo a
       minimize, activate after a right click, or steal focus after it moved. */
    if(GetForegroundWindow()!=tray||!IsWindowVisible(main_window)||IsIconic(main_window))return;
    char cls[128];
    if(!GetClassNameA(tray,cls,sizeof(cls))||!same(cls,"__wine_tray_icon"))return;
    DWORD own=GetCurrentThreadId(),other=GetWindowThreadProcessId(tray,0);
    if(!other||own==other)return;
    /* Explorer owns the foreground input queue during a tray click. WeCom's
       asynchronous callback alone does not grant foreground permission. */
    BOOL attached=AttachThreadInput(own,other,1);
    BOOL activated=attached&&SetForegroundWindow(main_window);
    if(attached)AttachThreadInput(own,other,0);
    text("tray left-click activation attached=");hex(attached);
    text(" activated=");hex(activated);text("\n");flush();
}
void mainCRTStartup(void){
    char *s=GetCommandLineA();
    while(*s){
        if(s[0]=='-'&&s[1]=='a'&&s[2]=='p'&&s[3]=='p'&&s[4]=='l'&&s[5]=='y')apply=1;
        if(s[0]=='-'&&s[1]=='w'&&s[2]=='a'&&s[3]=='t'&&s[4]=='c'&&s[5]=='h'){apply=1;watch=1;}
        s++;
    }
    HANDLE mutex=0,process=0,event_hook=0,foreground_hook=0,mouse_hook=0;
    if(watch){
        mutex=CreateMutexA(0,0,"Local\\WeComTitleBarManaged");
        if(!mutex||GetLastError()==183)ExitProcess(0);
        for(unsigned i=0;i<120;i++){
            main_window=FindWindowA("WeWorkWindow",0);
            if(main_window)break;
            Sleep(1000);
        }
    }else main_window=FindWindowA("WeWorkWindow",0);
    if(!main_window){text("WeCom main window not found\n");flush();ExitProcess(1);}
    GetWindowThreadProcessId(main_window,&main_pid);
    if(watch){
        process=OpenProcess(0x00100000,0,main_pid);
        if(!process){text("Cannot watch WeCom process\n");flush();ExitProcess(1);}
        read_tray_geometry();
        mouse_hook=SetWindowsHookExA(14,tray_mouse,GetModuleHandleA(0),0);
        text("tray activation hook installed=");hex(mouse_hook!=0);text("\n");flush();
        /* Menus belong to WeCom; hover tooltips belong to Wine Explorer.
           Filter both by class and owner in the current Wine desktop. */
        event_hook=SetWinEventHook(0x8002,0x8002,0,popup_shown,0,0,0);
        foreground_hook=SetWinEventHook(3,3,0,foreground_changed,0,0,0);
        text("tray popup hook installed=");hex(event_hook!=0);
        text(" dismiss hook installed=");hex(foreground_hook!=0);text("\n");flush();
    }
    do{
        if(watch){
            HANDLE candidate=FindWindowA("WeWorkWindow",0);DWORD candidate_pid=0;
            if(candidate)GetWindowThreadProcessId(candidate,&candidate_pid);
            if(candidate_pid==main_pid)main_window=candidate;
        }
        found=0;changed=0;EnumWindows(inspect,0);
        if(!watch){text("matched=");hex(found);text(" changed=");hex(changed);text("\n");}
        flush();
        if(!watch)break;
        DWORD next_scan=GetTickCount()+1000;
        do{
            DWORD result=MsgWaitForMultipleObjects(1,&process,0,(pending_menu||pending_tooltip||pending_activation||tracked_menu)?50:1000,0x04ff);
            if(result==0||result==0xffffffff)goto done;
            MSG message;
            while(PeekMessageA(&message,0,0,0,1)){TranslateMessage(&message);DispatchMessageA(&message);}
            correct_pending_popups();
            activate_after_tray_click();
            check_tray_menu_focus();
        }while((long)(GetTickCount()-next_scan)<0);
    }while(WaitForSingleObject(process,0)==258);
done:
    if(mouse_hook)UnhookWindowsHookEx(mouse_hook);
    if(foreground_hook)UnhookWinEvent(foreground_hook);
    if(event_hook)UnhookWinEvent(event_hook);
    if(process)CloseHandle(process);
    if(mutex)CloseHandle(mutex);
    ExitProcess(watch||found?0:1);
}
