// TabIt MIDI Import helper - self-contained x64 Windows GUI utility.
// No CRT and no third-party runtime. Converts a single pitched MIDI part to
// a one-track TabIt 2.x .tbt file (4-string bass or 6-string guitar auto-detected).

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
typedef void* HANDLE;
typedef void* HWND;
typedef void* HINSTANCE;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef unsigned long long ULONG_PTR;
typedef long long LPARAM;
typedef unsigned int UINT;
typedef int BOOL;
typedef unsigned short WORD;
typedef unsigned long DWORD;

#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define MAX_PATHX 1024
#define INVALID_HANDLE_VALUE ((HANDLE)(i64)-1)
#define GENERIC_READ  0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define FILE_SHARE_READ 0x00000001UL
#define OPEN_EXISTING 3UL
#define CREATE_ALWAYS 2UL
#define FILE_ATTRIBUTE_NORMAL 0x00000080UL
#define MEM_COMMIT 0x1000UL
#define MEM_RESERVE 0x2000UL
#define MEM_RELEASE 0x8000UL
#define PAGE_READWRITE 0x04UL
#define MB_OK 0x00000000U
#define MB_ICONERROR 0x00000010U
#define MB_ICONINFORMATION 0x00000040U
#define OFN_FILEMUSTEXIST 0x00001000UL
#define OFN_PATHMUSTEXIST 0x00000800UL
#define OFN_HIDEREADONLY  0x00000004UL
#define OFN_OVERWRITEPROMPT 0x00000002UL
#define SW_SHOWNORMAL 1

#pragma pack(push, 8)
typedef struct tagOFNA {
    DWORD lStructSize;
    HWND hwndOwner;
    HINSTANCE hInstance;
    LPCSTR lpstrFilter;
    LPSTR lpstrCustomFilter;
    DWORD nMaxCustFilter;
    DWORD nFilterIndex;
    LPSTR lpstrFile;
    DWORD nMaxFile;
    LPSTR lpstrFileTitle;
    DWORD nMaxFileTitle;
    LPCSTR lpstrInitialDir;
    LPCSTR lpstrTitle;
    DWORD Flags;
    WORD nFileOffset;
    WORD nFileExtension;
    LPCSTR lpstrDefExt;
    LPARAM lCustData;
    void* lpfnHook;
    LPCSTR lpTemplateName;
    void* pvReserved;
    DWORD dwReserved;
    DWORD FlagsEx;
} OPENFILENAMEA;
#pragma pack(pop)

__declspec(dllimport) BOOL GetOpenFileNameA(OPENFILENAMEA*);
__declspec(dllimport) BOOL GetSaveFileNameA(OPENFILENAMEA*);
__declspec(dllimport) int MessageBoxA(HWND,LPCSTR,LPCSTR,UINT);
__declspec(dllimport) HANDLE CreateFileA(LPCSTR,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL ReadFile(HANDLE,void*,DWORD,DWORD*,void*);
__declspec(dllimport) BOOL WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
__declspec(dllimport) DWORD GetFileSize(HANDLE,DWORD*);
__declspec(dllimport) BOOL CloseHandle(HANDLE);
__declspec(dllimport) void* VirtualAlloc(void*,u64,DWORD,DWORD);
__declspec(dllimport) BOOL VirtualFree(void*,u64,DWORD);
__declspec(dllimport) DWORD GetModuleFileNameA(HINSTANCE,LPSTR,DWORD);
__declspec(dllimport) void ExitProcess(UINT);
__declspec(dllimport) HINSTANCE ShellExecuteA(HWND,LPCSTR,LPCSTR,LPCSTR,LPCSTR,int);

void* memset(void* d, int c, u64 n) { u8* p=(u8*)d; while(n--) *p++=(u8)c; return d; }
void* memcpy(void* d, const void* s, u64 n) { u8* a=(u8*)d; const u8* b=(const u8*)s; while(n--) *a++=*b++; return d; }
static u64 slen(const char* s){u64 n=0; if(!s)return 0; while(s[n])n++; return n;}
static void scopy(char* d,const char* s,u64 cap){u64 i=0;if(!cap)return;while(s&&s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static int seq(const char* a,const char* b){u64 i=0;while(a[i]&&b[i]){if(a[i]!=b[i])return 0;i++;}return a[i]==b[i];}
static void put_le16(u8* p,u16 v){p[0]=(u8)v;p[1]=(u8)(v>>8);}
static void put_le32(u8* p,u32 v){p[0]=(u8)v;p[1]=(u8)(v>>8);p[2]=(u8)(v>>16);p[3]=(u8)(v>>24);}
static u16 be16(const u8* p){return (u16)(((u16)p[0]<<8)|p[1]);}
static u32 be32(const u8* p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}

static char gMidi[MAX_PATHX];
static char gOut[MAX_PATHX];

static void err(const char* m){MessageBoxA(0,m,"TabIt MIDI Import",MB_OK|MB_ICONERROR);}
static void info(const char* m){MessageBoxA(0,m,"TabIt MIDI Import",MB_OK|MB_ICONINFORMATION);}

static int read_vlq(const u8** pp,const u8* end,u32* out){
    u32 v=0; int n=0; const u8* p=*pp;
    while(p<end && n<4){u8 b=*p++;v=(v<<7)|(b&0x7f);n++;if(!(b&0x80)){*pp=p;*out=v;return 1;}}
    return 0;
}

typedef struct NoteEvent { u64 tick; u32 space; u8 pitch; } NoteEvent;

typedef struct MidiData {
    u16 ppq;
    u32 tempoUs;
    u8 tsNum;
    u8 tsDenPow;
    int haveTempo;
    int haveTS;
    int changedTS;
    u64 endTick;
    u32 program;
    int haveProgram;
    u16 channelMask;
    NoteEvent* ev;
    u32 count;
    u32 cap;
} MidiData;

static int add_event(MidiData* m,u64 tick,u8 pitch,u8 channel){
    if(m->count>=m->cap)return 0;
    m->ev[m->count].tick=tick;m->ev[m->count].pitch=pitch;m->ev[m->count].space=0;m->count++;
    if(channel!=9)m->channelMask|=(u16)(1u<<channel);
    return 1;
}

static int parse_midi(const u8* b,u32 sz,MidiData* m){
    if(sz<14 || b[0]!='M'||b[1]!='T'||b[2]!='h'||b[3]!='d')return 0;
    u32 hlen=be32(b+4); if(hlen<6 || 8+hlen>sz)return 0;
    u16 fmt=be16(b+8); u16 ntr=be16(b+10); u16 div=be16(b+12);
    (void)fmt;
    if(div&0x8000)return 0;
    m->ppq=div; if(!m->ppq)return 0;
    m->tempoUs=500000; m->tsNum=4; m->tsDenPow=2;
    const u8* p=b+8+hlen; const u8* fileEnd=b+sz;
    u16 ti;
    for(ti=0;ti<ntr;ti++){
        if(p+8>fileEnd || p[0]!='M'||p[1]!='T'||p[2]!='r'||p[3]!='k')return 0;
        u32 len=be32(p+4);p+=8;if(p+len>fileEnd)return 0;
        const u8* e=p+len;u64 tick=0;u8 running=0;
        while(p<e){
            u32 delta;if(!read_vlq(&p,e,&delta))return 0;tick+=(u64)delta;if(tick>m->endTick)m->endTick=tick;
            if(p>=e)return 0;
            u8 first=*p++;u8 status;u8 d1=0;int haveD1=0;
            if(first<0x80){if(!running)return 0;status=running;d1=first;haveD1=1;}else status=first;
            if(status==0xff){
                running=0;if(p>=e)return 0;u8 type=*p++;u32 ml;if(!read_vlq(&p,e,&ml)||p+ml>e)return 0;
                if(type==0x51 && ml==3){u32 us=((u32)p[0]<<16)|((u32)p[1]<<8)|p[2];if(!m->haveTempo || tick==0){m->tempoUs=us;m->haveTempo=1;}}
                if(type==0x58 && ml>=2){if(!m->haveTS || tick==0){m->tsNum=p[0];m->tsDenPow=p[1];m->haveTS=1;}else if(p[0]!=m->tsNum||p[1]!=m->tsDenPow)m->changedTS=1;}
                p+=ml;if(type==0x2f)break;continue;
            }
            if(status==0xf0||status==0xf7){running=0;u32 sl;if(!read_vlq(&p,e,&sl)||p+sl>e)return 0;p+=sl;continue;}
            if(status>=0xf0){
                running=0;int need=0;if(status==0xf1||status==0xf3)need=1;else if(status==0xf2)need=2;else need=0;
                if(p+need>e)return 0;p+=need;continue;
            }
            running=status;u8 hi=(u8)(status&0xf0);u8 ch=(u8)(status&0x0f);int need=(hi==0xc0||hi==0xd0)?1:2;u8 a,b2=0;
            if(haveD1)a=d1;else{if(p>=e)return 0;a=*p++;}
            if(need==2){if(p>=e)return 0;b2=*p++;}
            if(hi==0x90 && b2!=0 && ch!=9){if(!add_event(m,tick,a,ch))return 0;}
            if(hi==0xc0 && ch!=9 && !m->haveProgram){m->program=a;m->haveProgram=1;}
        }
        p=e;
    }
    return m->count>0;
}

static int cmp_ev(const NoteEvent* a,const NoteEvent* b){if(a->space<b->space)return -1;if(a->space>b->space)return 1;if(a->pitch<b->pitch)return -1;if(a->pitch>b->pitch)return 1;return 0;}
static void swap_ev(NoteEvent* a,NoteEvent* b){NoteEvent t=*a;*a=*b;*b=t;}
static void qsort_ev(NoteEvent* a,int lo,int hi){
    while(lo<hi){int i=lo,j=hi;NoteEvent piv=a[(lo+hi)>>1];while(i<=j){while(cmp_ev(&a[i],&piv)<0)i++;while(cmp_ev(&a[j],&piv)>0)j--;if(i<=j){swap_ev(&a[i],&a[j]);i++;j--;}}
        if(j-lo<hi-i){if(lo<j)qsort_ev(a,lo,j);lo=i;}else{if(i<hi)qsort_ev(a,i,hi);hi=j;}}
}

static u32 crc32_calc(const u8* data,u32 len){u32 c=0xffffffffu;u32 i,j;for(i=0;i<len;i++){c^=data[i];for(j=0;j<8;j++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return c^0xffffffffu;}
static u32 adler32_calc(const u8* data,u32 len){u32 a=1,b=0,i;for(i=0;i<len;i++){a+=data[i];if(a>=65521)a-=65521;b+=a;if(b>=65521)b%=65521;}return (b<<16)|a;}

// zlib wrapper using RFC1951 stored (uncompressed) blocks. Universally accepted by zlib inflate.
static u8* zlib_store(const u8* src,u32 len,u32* outLen){
    u32 blocks=(len+65534u)/65535u;if(blocks==0)blocks=1;
    u64 cap=2ull+(u64)len+(u64)blocks*5ull+4ull;if(cap>0xffffffffu)return NULL;
    u8* out=(u8*)VirtualAlloc(NULL,cap,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!out)return NULL;
    u32 o=0;out[o++]=0x78;out[o++]=0x01;u32 pos=0;u32 bi;
    for(bi=0;bi<blocks;bi++){u32 n=len-pos;if(n>65535)n=65535;int final=(bi==blocks-1);out[o++]=(u8)(final?1:0);put_le16(out+o,(u16)n);o+=2;put_le16(out+o,(u16)(~n));o+=2;if(n){memcpy(out+o,src+pos,n);o+=n;pos+=n;}}
    u32 ad=adler32_calc(src,len);out[o++]=(u8)(ad>>24);out[o++]=(u8)(ad>>16);out[o++]=(u8)(ad>>8);out[o++]=(u8)ad;*outLen=o;return out;
}

static u32 p2_write(u8* d,u32 o,const char* s){u32 n=(u32)slen(s);if(n>65535)n=65535;put_le16(d+o,(u16)n);o+=2;memcpy(d+o,s,n);return o+n;}

static int candidates(u8 pitch,const int* open,int sc,int cand[8][2]){int n=0,s;for(s=0;s<sc;s++){int f=(int)pitch-open[s];if(f>=0&&f<=24){cand[n][0]=s;cand[n][1]=f;n++;}}return n;}

typedef struct AssignCtx {int n,sc;u8 pitch[8];int open[8];int bestCost;int bestS[8],bestF[8],curS[8],curF[8];} AssignCtx;
static void assign_rec(AssignCtx* c,int i,u32 used,int cost){
    if(i==c->n){if(cost<c->bestCost){int k;c->bestCost=cost;for(k=0;k<c->n;k++){c->bestS[k]=c->curS[k];c->bestF[k]=c->curF[k];}}return;}
    int cand[8][2];int cn=candidates(c->pitch[i],c->open,c->sc,cand),k;
    for(k=0;k<cn;k++){int s=cand[k][0],f=cand[k][1];if(used&(1u<<s))continue;int add=f*10+(c->sc-1-s);if(cost+add>=c->bestCost)continue;c->curS[i]=s;c->curF[i]=f;assign_rec(c,i+1,used|(1u<<s),cost+add);}
}

static int make_tbt(const char* outPath,MidiData* m){
    u32 i;u8 minp=127,maxp=0;u64 sum=0;for(i=0;i<m->count;i++){if(m->ev[i].pitch<minp)minp=m->ev[i].pitch;if(m->ev[i].pitch>maxp)maxp=m->ev[i].pitch;sum+=m->ev[i].pitch;}
    int bass=0;if(m->haveProgram&&m->program>=32&&m->program<=39)bass=1;else if(minp<40)bass=1;else if(maxp<=67 && sum/m->count<52)bass=1;
    int sc=bass?4:6;int open[8]={40,45,50,55,59,64,0,0};s8 tuning[8]={0,0,0,0,0,0,0,0};u8 program=(u8)(m->haveProgram?m->program:(bass?33:27));
    if(bass){open[0]=28;open[1]=33;open[2]=38;open[3]=43;tuning[0]=tuning[1]=tuning[2]=tuning[3]=-12;program=33;}
    for(i=0;i<m->count;i++){int can=0,s;for(s=0;s<sc;s++){int f=(int)m->ev[i].pitch-open[s];if(f>=0&&f<=24){can=1;break;}}if(!can){err(bass?"A MIDI note falls outside standard 4-string bass range (E1-G4 at 24 frets).":"A MIDI note falls outside standard 6-string guitar range at 24 frets.");return 0;}}
    if(m->changedTS){err("This beta importer does not support time-signature changes inside a MIDI file yet.");return 0;}
    u32 den=1u<<m->tsDenPow;if(den==0||den>64)return 0;u32 numeratorSpaces=(u32)m->tsNum*16u;if(numeratorSpaces%den){err("Unsupported MIDI time signature for TabIt's 1/16-note grid.");return 0;}u32 barSpaces=numeratorSpaces/den;if(!barSpaces){return 0;}
    for(i=0;i<m->count;i++)m->ev[i].space=(u32)((m->ev[i].tick*4ull+(u64)m->ppq/2ull)/(u64)m->ppq);
    if(m->count>1)qsort_ev(m->ev,0,(int)m->count-1);
    u64 endSpaces=(m->endTick*4ull+(u64)m->ppq-1ull)/(u64)m->ppq;u64 minSpaces=(u64)m->ev[m->count-1].space+1ull;if(endSpaces<minSpaces)endSpaces=minSpaces;
    u64 bars64=(endSpaces+barSpaces-1u)/barSpaces;if(!bars64)bars64=1;if(bars64>65535){err("MIDI is too long for this TabIt file version.");return 0;}u32 barCount=(u32)bars64;u32 spaceCount=barCount*barSpaces;
    if((u64)spaceCount*20ull>128ull*1024ull*1024ull){err("MIDI is too large for the importer safety limit.");return 0;}
    u32 expandedLen=spaceCount*20u;u8* expanded=(u8*)VirtualAlloc(NULL,expandedLen,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!expanded){err("Out of memory building tablature.");return 0;}memset(expanded,0,expandedLen);
    i=0;while(i<m->count){u32 sp=m->ev[i].space;u8 pitches[8];int n=0;u32 j=i;u8 last=255;while(j<m->count&&m->ev[j].space==sp){if(m->ev[j].pitch!=last){if(n>=sc){VirtualFree(expanded,0,MEM_RELEASE);err("More simultaneous MIDI notes occur than the selected bass/guitar has strings.");return 0;}pitches[n++]=m->ev[j].pitch;last=m->ev[j].pitch;}j++;}
        AssignCtx c;memset(&c,0,sizeof(c));c.n=n;c.sc=sc;c.bestCost=0x7fffffff;int k;for(k=0;k<n;k++)c.pitch[k]=pitches[k];for(k=0;k<sc;k++)c.open[k]=open[k];assign_rec(&c,0,0,0);if(c.bestCost==0x7fffffff){VirtualFree(expanded,0,MEM_RELEASE);err("A chord cannot be fingered on distinct strings within 24 frets.");return 0;}
        for(k=0;k<n;k++){u32 idx=sp*20u+(u32)c.bestS[k];expanded[idx]=(u8)(0x80+c.bestF[k]);}i=j;}
    u64 encCap=(u64)expandedLen*2ull+2;if(encCap>0xffffffffu){VirtualFree(expanded,0,MEM_RELEASE);return 0;}u8* enc=(u8*)VirtualAlloc(NULL,encCap,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!enc){VirtualFree(expanded,0,MEM_RELEASE);return 0;}u32 eo=2,records=0,pos=0;
    while(pos<expandedLen){u8 v=expanded[pos];u32 q=pos+1;while(q<expandedLen&&expanded[q]==v)q++;u32 run=q-pos;while(run){u32 part=run>255?255:run;if(records>=65535){VirtualFree(expanded,0,MEM_RELEASE);VirtualFree(enc,0,MEM_RELEASE);err("TabIt delta list exceeded 65,535 records.");return 0;}enc[eo++]=(u8)part;enc[eo++]=v;records++;run-=part;}pos=q;}put_le16(enc,(u16)records);VirtualFree(expanded,0,MEM_RELEASE);
    u64 bodyLen64=(u64)barCount*6ull+(u64)eo+4ull;if(bodyLen64>0xffffffffu){VirtualFree(enc,0,MEM_RELEASE);return 0;}u32 bodyLen=(u32)bodyLen64;u8* body=(u8*)VirtualAlloc(NULL,bodyLen,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!body){VirtualFree(enc,0,MEM_RELEASE);return 0;}u32 bo=0;for(i=0;i<barCount;i++){put_le32(body+bo,barSpaces);bo+=4;body[bo++]=0;body[bo++]=0;}memcpy(body+bo,enc,eo);bo+=eo;put_le32(body+bo,0);bo+=4;VirtualFree(enc,0,MEM_RELEASE);
    u8 md[1024];memset(md,0,sizeof(md));u32 mo=0;put_le32(md+mo,spaceCount);mo+=4;md[mo++]=(u8)sc;md[mo++]=(u8)(0x80|(program&0x7f));md[mo++]=28;md[mo++]=96;md[mo++]=0;put_le16(md+mo,0);mo+=2;md[mo++]=0;md[mo++]=0;md[mo++]=0;md[mo++]=0;md[mo++]=64;md[mo++]=24;md[mo++]=0;md[mo++]=0xff;md[mo++]=0;md[mo++]=0;for(i=0;i<8;i++)md[mo++]=(u8)tuning[i];md[mo++]=0;
    mo=p2_write(md,mo,bass?"Imported MIDI - bass":"Imported MIDI - guitar");mo=p2_write(md,mo,"");mo=p2_write(md,mo,"");mo=p2_write(md,mo,"");mo=p2_write(md,mo,"Imported by the TabIt MIDI hot-patch helper; notes quantized to TabIt's 1/16-note grid.");
    u32 mdZLen=0,bodyZLen=0;u8* mdZ=zlib_store(md,mo,&mdZLen);u8* bodyZ=zlib_store(body,bodyLen,&bodyZLen);VirtualFree(body,0,MEM_RELEASE);if(!mdZ||!bodyZ){if(mdZ)VirtualFree(mdZ,0,MEM_RELEASE);if(bodyZ)VirtualFree(bodyZ,0,MEM_RELEASE);return 0;}
    u64 total64=64ull+mdZLen+bodyZLen;if(total64>0xffffffffu){VirtualFree(mdZ,0,MEM_RELEASE);VirtualFree(bodyZ,0,MEM_RELEASE);return 0;}u32 total=(u32)total64;u8* out=(u8*)VirtualAlloc(NULL,total,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!out){VirtualFree(mdZ,0,MEM_RELEASE);VirtualFree(bodyZ,0,MEM_RELEASE);return 0;}memset(out,0,64);out[0]='T';out[1]='B';out[2]='T';out[3]=0x72;u32 bpm=m->tempoUs?60000000u/m->tempoUs:120u;if(bpm<1)bpm=1;if(bpm>999)bpm=999;out[4]=(u8)(bpm<250?bpm:250);out[5]=1;out[6]=3;out[7]='2';out[8]='.';out[9]='0';out[10]=0;out[11]=0x0b;put_le16(out+0x28,(u16)barCount);put_le16(out+0x2a,0);put_le16(out+0x2c,0);put_le16(out+0x2e,(u16)bpm);put_le32(out+0x30,mdZLen);memcpy(out+64,mdZ,mdZLen);memcpy(out+64+mdZLen,bodyZ,bodyZLen);put_le32(out+0x34,crc32_calc(out+64,mdZLen+bodyZLen));put_le32(out+0x38,total);put_le32(out+0x3c,crc32_calc(out,60));VirtualFree(mdZ,0,MEM_RELEASE);VirtualFree(bodyZ,0,MEM_RELEASE);
    HANDLE h=CreateFileA(outPath,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(h==INVALID_HANDLE_VALUE){VirtualFree(out,0,MEM_RELEASE);err("Could not create the destination .tbt file.");return 0;}DWORD wrote=0;BOOL ok=WriteFile(h,out,total,&wrote,NULL);CloseHandle(h);VirtualFree(out,0,MEM_RELEASE);if(!ok||wrote!=total){err("Failed while writing the destination .tbt file.");return 0;}return 1;
}

static int load_file(const char* path,u8** out,u32* sz){HANDLE h=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(h==INVALID_HANDLE_VALUE)return 0;DWORD s=GetFileSize(h,NULL);if(s==0xffffffffu||s<14){CloseHandle(h);return 0;}u8* b=(u8*)VirtualAlloc(NULL,s,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!b){CloseHandle(h);return 0;}DWORD got=0;BOOL ok=ReadFile(h,b,s,&got,NULL);CloseHandle(h);if(!ok||got!=s){VirtualFree(b,0,MEM_RELEASE);return 0;}*out=b;*sz=s;return 1;}

static void default_out_path(const char* in,char* out,u32 cap){scopy(out,in,cap);u32 n=(u32)slen(out);u32 slash=0,dot=n,i;for(i=0;i<n;i++){if(out[i]=='\\'||out[i]=='/')slash=i+1;if(out[i]=='.'&&i>=slash)dot=i;}if(dot==n && n+4<cap){out[n++]='.';out[n++]='t';out[n++]='b';out[n++]='t';out[n]=0;}else if(dot+4<cap){out[dot]='.';out[dot+1]='t';out[dot+2]='b';out[dot+3]='t';out[dot+4]=0;}}

static int popcount16(u16 x){int n=0;while(x){n+=(x&1);x>>=1;}return n;}

void mainCRTStartup(void){
    memset(gMidi,0,sizeof(gMidi));OPENFILENAMEA o;memset(&o,0,sizeof(o));o.lStructSize=sizeof(o);o.lpstrFilter="MIDI files (*.mid;*.midi)\0*.mid;*.midi\0All files (*.*)\0*.*\0\0";o.lpstrFile=gMidi;o.nMaxFile=MAX_PATHX;o.lpstrTitle="Import MIDI into TabIt";o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY;
    if(!GetOpenFileNameA(&o))ExitProcess(0);
    u8* data=NULL;u32 size=0;if(!load_file(gMidi,&data,&size)){err("Could not read the selected MIDI file.");ExitProcess(2);}MidiData m;memset(&m,0,sizeof(m));m.cap=size/3u+64u;m.ev=(NoteEvent*)VirtualAlloc(NULL,(u64)m.cap*sizeof(NoteEvent),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!m.ev){VirtualFree(data,0,MEM_RELEASE);err("Out of memory parsing MIDI.");ExitProcess(3);}if(!parse_midi(data,size,&m)){VirtualFree(data,0,MEM_RELEASE);VirtualFree(m.ev,0,MEM_RELEASE);err("Unsupported or malformed MIDI. This beta supports PPQ Standard MIDI Files with at least one pitched note part.");ExitProcess(4);}VirtualFree(data,0,MEM_RELEASE);
    if(popcount16(m.channelMask)>1){VirtualFree(m.ev,0,MEM_RELEASE);err("This beta importer currently supports one pitched MIDI channel at a time. Export the bass/guitar part as its own MIDI and import that file.");ExitProcess(5);}
    default_out_path(gMidi,gOut,MAX_PATHX);OPENFILENAMEA s;memset(&s,0,sizeof(s));s.lStructSize=sizeof(s);s.lpstrFilter="TabIt tablature (*.tbt)\0*.tbt\0All files (*.*)\0*.*\0\0";s.lpstrFile=gOut;s.nMaxFile=MAX_PATHX;s.lpstrTitle="Save imported TabIt file";s.lpstrDefExt="tbt";s.Flags=OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;if(!GetSaveFileNameA(&s)){VirtualFree(m.ev,0,MEM_RELEASE);ExitProcess(0);}if(!make_tbt(gOut,&m)){VirtualFree(m.ev,0,MEM_RELEASE);ExitProcess(6);}VirtualFree(m.ev,0,MEM_RELEASE);
    HINSTANCE r=ShellExecuteA(0,"open",gOut,NULL,NULL,SW_SHOWNORMAL);if((ULONG_PTR)r<=32ull)info("MIDI import succeeded and the .tbt file was saved. TabIt did not auto-open it, so use File > Open to load the saved file.");
    ExitProcess(0);
}
