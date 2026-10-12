typedef unsigned long DWORD;
typedef unsigned short WCHAR;
typedef int BOOL;
typedef void *HANDLE;
#define API __declspec(dllimport) __stdcall
typedef struct {long height,width,escapement,orientation,weight;unsigned char italic,underline,strikeout,charset,outprecision,clipprecision,quality,pitch;WCHAR face[32];} LOGFONTW;
typedef struct {unsigned size;int border,scrollw,scrollh,captionw,captionh;LOGFONTW caption;int smcaptionw,smcaptionh;LOGFONTW smcaption;int menuw,menuh;LOGFONTW menu,status,message;int padded;} METRICS;
API BOOL SystemParametersInfoW(unsigned,unsigned,void *,unsigned);
API HANDLE CreateFontIndirectW(const LOGFONTW *);
API HANDLE CreateCompatibleDC(HANDLE);
API HANDLE SelectObject(HANDLE,HANDLE);
API BOOL DeleteObject(HANDLE);
API BOOL DeleteDC(HANDLE);
API int GetTextFaceW(HANDLE,int,WCHAR *);
API DWORD GetGlyphIndicesW(HANDLE,const WCHAR *,int,unsigned short *,DWORD);
API int WideCharToMultiByte(unsigned,DWORD,const WCHAR *,int,char *,int,const char *,BOOL *);
API char *GetCommandLineA(void);
API unsigned GetACP(void);
API HANDLE GetStdHandle(DWORD);
API BOOL WriteFile(HANDLE,const void *,DWORD,DWORD *,void *);
API void ExitProcess(DWORD);
static char output[4096];static unsigned pos;
static void text(const char*s){while(*s)output[pos++]=*s++;}
static void wide(const WCHAR*s){char b[160];WideCharToMultiByte(65001,0,s,-1,b,sizeof(b),0,0);text(b);}
static void number(int n){char b[16];int k=0;if(n<0){text("-");n=-n;}do{b[k++]='0'+n%10;n/=10;}while(n);while(k)output[pos++]=b[--k];}
void mainCRTStartup(void){
    static METRICS m;m.size=sizeof(m);DWORD written;int ok=SystemParametersInfoW(0x29,sizeof(m),&m,0);
    text("SPI_GETNONCLIENTMETRICS=");number(ok);text(" ACP=");number(GetACP());text(" status=");wide(m.status.face);text(" height=");number(m.status.height);text(" charset=");number(m.status.charset);text("\n");
    char *s=GetCommandLineA();int change=0;while(*s){if(s[0]=='-'&&s[1]=='s'&&s[2]=='e'&&s[3]=='t'){change=1;break;}s++;}
    if(ok&&change){static WCHAR face[]=L"Source Han Sans CN";unsigned i=0;while(face[i]){m.status.face[i]=face[i];i++;}while(i<32)m.status.face[i++]=0;m.status.charset=1;
        ok=SystemParametersInfoW(0x2a,sizeof(m),&m,3);text("SPI_SETNONCLIENTMETRICS status-only=");number(ok);text("\n");
        m.size=sizeof(m);SystemParametersInfoW(0x29,sizeof(m),&m,0);text("readback status=");wide(m.status.face);text("\n");
    }
    HANDLE dc=CreateCompatibleDC(0),font=CreateFontIndirectW(&m.status),old=SelectObject(dc,font);WCHAR actual[64];unsigned short glyphs[6];static WCHAR sample[]=L"截图表情图片";
    GetTextFaceW(dc,64,actual);text("selected=");wide(actual);text(" missing samples:");DWORD count=GetGlyphIndicesW(dc,sample,6,glyphs,1);for(unsigned i=0;i<6;i++)if(count==(DWORD)-1||glyphs[i]==0xffff){WCHAR c[2]={sample[i],0};wide(c);}text("\n");
    SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);WriteFile(GetStdHandle((DWORD)-11),output,pos,&written,0);ExitProcess(ok?0:1);
}
