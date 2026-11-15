// ZTX: a small, event-driven Windows desktop interface. No third-party runtime.
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include "preferences.h"
#include "../resources/resource.h"

namespace {
constexpr wchar_t MainClass[] = L"CKitastam.ZTX.Main";
constexpr wchar_t SettingsClass[] = L"CKitastam.ZTX.Settings";
constexpr int LogLimit = 32768;
enum Id { Logo = 1000, File, Settings, Help, Size, Unit, Path, Browse, Clean,
          Output, Journal, Dark, Light, French, English, Animation, Glass, Done,
          OpenFolder = 1100, Exit, About, UnitFirst = 1200 };

struct Palette {
    COLORREF background, panel, input, line, text, muted, accent, accentText, hover;
};
constexpr Palette DarkPalette{RGB(13,19,25), RGB(22,30,38), RGB(12,19,25),
    RGB(44,58,68), RGB(237,243,245), RGB(147,165,176), RGB(122,225,205),
    RGB(14,43,39), RGB(35,49,59)};
constexpr Palette LightPalette{RGB(241,245,246), RGB(255,255,255), RGB(244,248,249),
    RGB(210,222,226), RGB(22,42,51), RGB(90,112,123), RGB(20,119,105),
    RGB(255,255,255), RGB(224,237,237)};

struct GdiObjects {
    std::array<HFONT, 6> fonts{};
    HBRUSH background{}, panel{}, input{};
    void clear() noexcept {
        for (auto& font : fonts) { if (font) DeleteObject(font); font = nullptr; }
        if (background) DeleteObject(background);
        if (panel) DeleteObject(panel);
        if (input) DeleteObject(input);
        background = panel = input = nullptr;
    }
    ~GdiObjects() { clear(); }
};

// A retained paint buffer avoids allocating a full-window bitmap on every hover.
struct PaintBuffer {
    HDC dc{}; HBITMAP bitmap{}; HGDIOBJ original{}; int width{}, height{};
    ~PaintBuffer() { clear(); }
    void clear() noexcept {
        if (dc && original) SelectObject(dc, original);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        dc = nullptr; bitmap = nullptr; original = nullptr;
    }
    HDC get(HDC target, int w, int h) {
        if (dc && width == w && height == h) return dc;
        clear(); width = w; height = h;
        dc = CreateCompatibleDC(target);
        bitmap = CreateCompatibleBitmap(target, w, h);
        if (!dc || !bitmap) { clear(); return target; }
        original = SelectObject(dc, bitmap);
        return dc;
    }
};

template<class T> struct ComPtr {
    T* value{};
    ~ComPtr() { if (value) value->Release(); }
    T** out() noexcept { return &value; }
    T* operator->() const noexcept { return value; }
};

struct ButtonState {
    HWND window{};
    bool hovered{};
    float blend{};
};

RECT fitToWorkArea(RECT desired) {
    MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
    if(GetMonitorInfoW(MonitorFromRect(&desired,MONITOR_DEFAULTTONEAREST),&monitor)) {
        const RECT work=monitor.rcWork;
        const LONG width=std::min(desired.right-desired.left,work.right-work.left);
        const LONG height=std::min(desired.bottom-desired.top,work.bottom-work.top);
        desired.left=std::clamp(desired.left,work.left,work.right-width);
        desired.top=std::clamp(desired.top,work.top,work.bottom-height);
        desired.right=desired.left+width; desired.bottom=desired.top+height;
    }
    return desired;
}

struct App {
    HINSTANCE instance{};
    HWND window{}, settings{}, tooltip{};
    HWND size{}, unit{}, path{}, output{}, journal{}, slider{};
    std::array<ButtonState, 16> buttons{};
    ztx::Preferences preferences{ztx::loadPreferences()};
    Palette palette{DarkPalette};
    GdiObjects gdi, settingsGdi;
    PaintBuffer mainBuffer, settingsBuffer;
    ComPtr<IStream> logoStream;
    std::unique_ptr<Gdiplus::Image> logo;
    UINT dpi{96}, settingsDpi{96};
    int unitIndex{2};
    int scrollY{}, settingsScrollY{}, contentHeight{730}, wheelRemainder{};
    RECT config{}, files{}, logs{};
    bool ready{}, hasError{}, layingOut{}, layingOutSettings{}, updatingTerminal{};

    int px(int value, bool inSettings=false) const noexcept {
        return MulDiv(value, static_cast<int>(inSettings?settingsDpi:dpi), 96);
    }
    const wchar_t* tr(const wchar_t* fr, const wchar_t* en) const noexcept {
        return preferences.language == ztx::Language::French ? fr : en;
    }
    HWND control(int id, HWND parent = nullptr) const noexcept {
        return GetDlgItem(parent ? parent : window, id);
    }
    void text(HDC dc, const wchar_t* value, RECT r, int font, COLORREF color,
              UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, bool inSettings=false) {
        const auto previous = SelectObject(dc, (inSettings?settingsGdi:gdi).fonts[static_cast<size_t>(font)]);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
        DrawTextW(dc, value, -1, &r, flags | DT_NOPREFIX);
        SelectObject(dc, previous);
    }
    void text(HDC dc, const wchar_t* value, int x, int y, int w, int h,
              int font, COLORREF color, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, bool inSettings=false) {
        text(dc, value, RECT{px(x,inSettings),px(y,inSettings),px(x+w,inSettings),px(y+h,inSettings)}, font, color, flags,inSettings);
    }
    void rounded(HDC dc, RECT r, COLORREF fill, COLORREF border, int radius = 12, bool inSettings=false) {
        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        const float x=static_cast<float>(r.left)+0.5f, y=static_cast<float>(r.top)+0.5f;
        const float w=static_cast<float>(r.right-r.left)-1, h=static_cast<float>(r.bottom-r.top)-1;
        const float d=static_cast<float>(px(radius,inSettings)*2);
        Gdiplus::GraphicsPath shape;
        shape.AddArc(x,y,d,d,180,90); shape.AddArc(x+w-d,y,d,d,270,90);
        shape.AddArc(x+w-d,y+h-d,d,d,0,90); shape.AddArc(x,y+h-d,d,d,90,90);
        shape.CloseFigure();
        Gdiplus::SolidBrush brush(Gdiplus::Color(255,GetRValue(fill),GetGValue(fill),GetBValue(fill)));
        Gdiplus::Pen pen(Gdiplus::Color(255,GetRValue(border),GetGValue(border),GetBValue(border)));
        graphics.FillPath(&brush,&shape); graphics.DrawPath(&pen,&shape);
    }
    void line(HDC dc, int x, int y, int w, COLORREF color, bool inSettings=false) {
        const HPEN pen=CreatePen(PS_SOLID,1,color);
        const auto old=SelectObject(dc,pen);
        MoveToEx(dc,px(x,inSettings),px(y,inSettings),nullptr); LineTo(dc,px(x+w,inSettings),px(y,inSettings));
        SelectObject(dc,old); DeleteObject(pen);
    }
    static COLORREF mix(COLORREF a, COLORREF b, float t) noexcept {
        return RGB(static_cast<BYTE>(GetRValue(a)+(GetRValue(b)-GetRValue(a))*t),
                   static_cast<BYTE>(GetGValue(a)+(GetGValue(b)-GetGValue(a))*t),
                   static_cast<BYTE>(GetBValue(a)+(GetBValue(b)-GetBValue(a))*t));
    }
    void rebuildStyle() {
        palette = preferences.theme == ztx::Theme::Dark ? DarkPalette : LightPalette;
        constexpr std::array<int,6> heights{14,12,20,27,13,15};
        for (HWND parent : {window,settings}) if (parent) {
            const bool inSettings=parent==settings;
            GdiObjects& style=inSettings?settingsGdi:gdi;
            style.clear();
            for (size_t i=0;i<heights.size();++i) {
                style.fonts[i]=CreateFontW(-px(heights[i],inSettings),0,0,0,(i==2 || i==3 || i==5)?FW_SEMIBOLD:FW_NORMAL,
                    FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY,DEFAULT_PITCH,i==4?L"Consolas":L"Segoe UI");
            }
            style.background=CreateSolidBrush(palette.background);
            style.panel=CreateSolidBrush(palette.panel);
            style.input=CreateSolidBrush(palette.input);
            for (HWND child=GetWindow(parent,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
                SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(style.fonts[(child==output || child==journal)?4:0]),TRUE);
            const BOOL dark=preferences.theme==ztx::Theme::Dark;
            DwmSetWindowAttribute(parent,20,&dark,sizeof(dark));
            DwmSetWindowAttribute(parent,35,&palette.background,sizeof(COLORREF));
            DwmSetWindowAttribute(parent,36,&palette.text,sizeof(COLORREF));
            RedrawWindow(parent,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_FRAME);
        }
    }
    static LRESULT CALLBACK buttonProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                       UINT_PTR, DWORD_PTR data) {
        auto* app=reinterpret_cast<App*>(data);
        ButtonState* state=nullptr;
        for (auto& item:app->buttons) if (item.window==hwnd) { state=&item; break; }
        if (!state) return DefSubclassProc(hwnd,msg,wp,lp);
        const auto transition=[&] {
            if (app->preferences.animations) SetTimer(hwnd,1,15,nullptr);
            else { state->blend=state->hovered?1.f:0.f; InvalidateRect(hwnd,nullptr,FALSE); }
        };
        switch(msg) {
        case WM_SETFOCUS: app->ensureVisible(hwnd); break;
        case WM_MOUSEMOVE:
            if (!state->hovered) {
                state->hovered=true;
                TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,hwnd,0}; TrackMouseEvent(&track);
                transition();
            } break;
        case WM_MOUSELEAVE: state->hovered=false; transition(); break;
        case WM_TIMER:
            if (wp==1) {
                const float target=state->hovered?1.f:0.f;
                state->blend += (target-state->blend)*.34f;
                if (std::abs(target-state->blend)<.025f) {state->blend=target;KillTimer(hwnd,1);}
                InvalidateRect(hwnd,nullptr,FALSE); return 0;
            } break;
        case WM_NCDESTROY:
            KillTimer(hwnd,1); state->window=nullptr;
            RemoveWindowSubclass(hwnd,buttonProc,1); break;
        default: break;
        }
        return DefSubclassProc(hwnd,msg,wp,lp);
    }
    HWND button(HWND parent, int id, const wchar_t* label) {
        const HWND result=CreateWindowExW(0,L"BUTTON",label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
        // Slots are reused when the settings window closes.
        for(auto& state:buttons) if (!state.window) {
            state={result,false,0}; SetWindowSubclass(result,buttonProc,1,reinterpret_cast<DWORD_PTR>(this)); break;
        }
        SendMessageW(result,WM_SETFONT,reinterpret_cast<WPARAM>((parent==settings?settingsGdi:gdi).fonts[0]),FALSE);
        return result;
    }
    HWND edit(HWND parent, int id, DWORD style) {
        const HWND result=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,
            0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
        SendMessageW(result,WM_SETFONT,reinterpret_cast<WPARAM>(gdi.fonts[(id==Output || id==Journal)?4:0]),FALSE);
        SendMessageW(result,EM_SETLIMITTEXT,(id==Path?32767:LogLimit),0);
        SetWindowSubclass(result,editProc,2,reinterpret_cast<DWORD_PTR>(this));
        return result;
    }
    static LRESULT CALLBACK editProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR data) {
        auto* app=reinterpret_cast<App*>(data);
        if(msg==WM_SETFOCUS) app->ensureVisible(hwnd);
        if(msg==WM_PASTE && GetDlgCtrlID(hwnd)==Size) {
            // ES_NUMBER only filters typing; clipboard text needs the same rule.
            bool valid=false;
            if(OpenClipboard(hwnd)) {
                const HANDLE handle=GetClipboardData(CF_UNICODETEXT);
                const auto* value=handle?static_cast<const wchar_t*>(GlobalLock(handle)):nullptr;
                if(value) {
                    const SIZE_T capacity=GlobalSize(handle)/sizeof(wchar_t);
                    SIZE_T i=0;
                    while(i<capacity && i<12 && value[i]>=L'0' && value[i]<=L'9') ++i;
                    valid=i<capacity && value[i]==L'\0';
                    GlobalUnlock(handle);
                }
                CloseClipboard();
            }
            if(!valid) {MessageBeep(MB_OK);return 0;}
        }
        const LRESULT result=DefSubclassProc(hwnd,msg,wp,lp);
        if((hwnd==app->output || hwnd==app->journal) &&
           (msg==WM_SIZE || msg==WM_SETFONT || msg==WM_SETTEXT || msg==EM_REPLACESEL))
            app->updateTerminalScrollbar(hwnd);
        if(msg==WM_NCDESTROY) RemoveWindowSubclass(hwnd,editProc,2);
        return result;
    }
    void updateTerminalScrollbar(HWND child) {
        if(updatingTerminal || !child) return;
        updatingTerminal=true;
        RECT area{};SendMessageW(child,EM_GETRECT,0,reinterpret_cast<LPARAM>(&area));
        HDC dc=GetDC(child);
        const auto old=SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(child,WM_GETFONT,0,0)));
        TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);
        SelectObject(dc,old);ReleaseDC(child,dc);
        const int lines=static_cast<int>(SendMessageW(child,EM_GETLINECOUNT,0,0));
        const bool overflow=metrics.tmHeight>0 && lines*metrics.tmHeight>area.bottom-area.top;
        const bool visible=(GetWindowLongPtrW(child,GWL_STYLE)&WS_VSCROLL)!=0;
        if(visible!=overflow) ShowScrollBar(child,SB_VERT,overflow);
        updatingTerminal=false;
    }
    void move(HWND child,int x,int y,int w,int h) const {
        const bool inSettings=GetParent(child)==settings;
        SetWindowPos(child,nullptr,px(x,inSettings),px(y,inSettings)-(inSettings?settingsScrollY:scrollY),
            px(w,inSettings),px(h,inSettings),SWP_NOZORDER|SWP_NOACTIVATE);
    }
    void setScrollRange(HWND parent,int logicalHeight) {
        const bool inSettings=parent==settings;
        RECT client{};GetClientRect(parent,&client);
        SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
        info.nMax=px(logicalHeight,inSettings)-1;
        info.nPage=static_cast<UINT>(client.bottom);
        info.nPos=inSettings?settingsScrollY:scrollY;
        SetScrollInfo(parent,SB_VERT,&info,TRUE);
        const bool overflow=info.nMax+1>static_cast<int>(info.nPage);
        const bool visible=(GetWindowLongPtrW(parent,GWL_STYLE)&WS_VSCROLL)!=0;
        if(visible!=overflow) ShowScrollBar(parent,SB_VERT,overflow);
        info.fMask=SIF_POS;GetScrollInfo(parent,SB_VERT,&info);
        (inSettings?settingsScrollY:scrollY)=info.nPos;
    }
    void scrollTo(HWND parent,int position) {
        const bool inSettings=parent==settings;
        SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_RANGE|SIF_PAGE;
        GetScrollInfo(parent,SB_VERT,&info);
        const int next=std::clamp(position,0,std::max(0,info.nMax-static_cast<int>(info.nPage)+1));
        int& current=inSettings?settingsScrollY:scrollY;
        if(current==next) return;
        current=next;
        if(inSettings) layoutSettings(); else layout();
    }
    void scroll(HWND parent,WPARAM command) {
        SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_ALL;
        GetScrollInfo(parent,SB_VERT,&info);
        int next=info.nPos;
        switch(LOWORD(command)) {
        case SB_LINEUP:next-=px(36,parent==settings);break;
        case SB_LINEDOWN:next+=px(36,parent==settings);break;
        case SB_PAGEUP:next-=static_cast<int>(info.nPage);break;
        case SB_PAGEDOWN:next+=static_cast<int>(info.nPage);break;
        case SB_THUMBTRACK:case SB_THUMBPOSITION:next=info.nTrackPos;break;
        case SB_TOP:next=0;break;
        case SB_BOTTOM:next=info.nMax;break;
        default:return;
        }
        scrollTo(parent,next);
    }
    void mouseWheel(HWND parent,WPARAM wp) {
        wheelRemainder+=GET_WHEEL_DELTA_WPARAM(wp);
        const int steps=wheelRemainder/WHEEL_DELTA;
        wheelRemainder%=WHEEL_DELTA;
        scrollTo(parent,(parent==settings?settingsScrollY:scrollY)-steps*px(72,parent==settings));
    }
    void ensureVisible(HWND child) {
        const HWND parent=GetParent(child);
        if(parent!=window && parent!=settings) return;
        RECT r{},client{};GetWindowRect(child,&r);GetClientRect(parent,&client);
        MapWindowPoints(nullptr,parent,reinterpret_cast<POINT*>(&r),2);
        const int margin=px(12,parent==settings);
        const int current=parent==settings?settingsScrollY:scrollY;
        if(r.top<margin) scrollTo(parent,current+r.top-margin);
        else if(r.bottom>client.bottom-margin) scrollTo(parent,current+r.bottom-client.bottom+margin);
    }
    void createMain() {
        dpi=GetDpiForWindow(window); rebuildStyle();
        button(window,Logo,L"Logo ZTX"); button(window,File,L"Fichier");
        button(window,Settings,L"Paramètres"); button(window,Help,L"Aide");
        size=edit(window,Size,ES_NUMBER|ES_AUTOHSCROLL); SetWindowTextW(size,L"100");
        SendMessageW(size,EM_SETLIMITTEXT,12,0);
        unit=button(window,Unit,L"Mo"); path=edit(window,Path,ES_AUTOHSCROLL);
        button(window,Browse,L"Parcourir"); button(window,Clean,L"Nettoyer");
        output=edit(window,Output,ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL);
        journal=edit(window,Journal,ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL);
        tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
            CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,window,nullptr,instance,nullptr);
        TOOLINFOW info{};
        info.cbSize=sizeof(info);
        info.uFlags=TTF_IDISHWND|TTF_SUBCLASS; info.hwnd=window;
        info.uId=reinterpret_cast<UINT_PTR>(control(Logo)); info.lpszText=const_cast<wchar_t*>(L"Logo ZTX");
        SendMessageW(tooltip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));
        SendMessageW(tooltip,TTM_SETDELAYTIME,TTDT_INITIAL,600);
        const HRSRC resource=FindResourceW(instance,MAKEINTRESOURCEW(IDR_LOGO),RT_RCDATA);
        if(resource) {
            const HGLOBAL bytes=LoadResource(instance,resource);
            logoStream.value=SHCreateMemStream(static_cast<const BYTE*>(LockResource(bytes)),SizeofResource(instance,resource));
            if(logoStream.value) {
                logo=std::make_unique<Gdiplus::Image>(logoStream.value);
                if(logo->GetLastStatus()!=Gdiplus::Ok) logo.reset();
            }
        }
        ready=true; translate(); layout(); applyGlass();
        appendLog(tr(L"Interface prête. Moteur non connecté.",L"Interface ready. Engine not connected."));
    }
    void translate() {
        SetWindowTextW(control(File),tr(L"Fichier",L"File"));
        SetWindowTextW(control(Settings),tr(L"Paramètres",L"Settings"));
        SetWindowTextW(control(Help),tr(L"Aide",L"Help"));
        SetWindowTextW(control(Browse),tr(L"Parcourir",L"Browse"));
        SetWindowTextW(control(Clean),tr(L"Nettoyer",L"Clean"));
        static constexpr const wchar_t* frUnits[]{L"o",L"Ko",L"Mo",L"Go",L"To",L"Po"};
        static constexpr const wchar_t* enUnits[]{L"B",L"KB",L"MB",L"GB",L"TB",L"PB"};
        SetWindowTextW(unit,preferences.language==ztx::Language::French?frUnits[unitIndex]:enUnits[unitIndex]);
        SendMessageW(path,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(tr(L"Collez le chemin d’un dossier…",L"Paste a folder path…")));
        if(settings) {
            SetWindowTextW(settings,tr(L"Paramètres — ZTX",L"Settings — ZTX"));
            SetWindowTextW(control(Dark,settings),tr(L"Sombre",L"Dark"));
            SetWindowTextW(control(Light,settings),tr(L"Clair",L"Light"));
            SetWindowTextW(control(Animation,settings),preferences.animations?tr(L"Activées",L"Enabled"):tr(L"Désactivées",L"Disabled"));
            SetWindowTextW(control(Done,settings),tr(L"Terminé",L"Done"));
            InvalidateRect(settings,nullptr,FALSE);
        }
        InvalidateRect(window,nullptr,FALSE);
    }
    void applyGlass() {
        // Documented layered-window opacity: no private composition API or GPU loop.
        // 0 = 70% opacity, spacious; 100 = fully opaque, compact.
        const BYTE alpha=static_cast<BYTE>(179+(76*preferences.glass)/100);
        SetLayeredWindowAttributes(window,0,alpha,LWA_ALPHA);
        if(ready) layout();
    }
    void layout() {
        if(!ready || layingOut) return;
        layingOut=true;
        RECT r{}; GetClientRect(window,&r);
        contentHeight=std::max(730,MulDiv(r.bottom,96,static_cast<int>(dpi)));
        setScrollRange(window,contentHeight);
        GetClientRect(window,&r);
        const int w=MulDiv(r.right,96,static_cast<int>(dpi));
        const int h=contentHeight;
        const int pad=20+(100-preferences.glass)*8/100;
        const int gap=12+(100-preferences.glass)*8/100;
        const int cw=w-2*pad;
        move(control(Logo),pad,18,48,48);
        move(control(File),pad,82,64,32); move(control(Settings),pad+72,82,106,32);
        move(control(Help),pad+186,82,54,32);
        const int top=196;
        const int ch=208+(100-preferences.glass)*12/100;
        config={px(pad),px(top),px(w-pad),px(top+ch)};
        move(size,pad+30,top+77,cw-170,25);
        move(unit,w-pad-112,top+67,92,44);
        move(path,pad+30,top+155,cw-164,24);
        move(control(Browse),w-pad-122,top+145,102,44);
        const int cleanY=top+ch+gap;
        move(control(Clean),pad,cleanY,cw,48);
        const int filesY=cleanY+48+gap;
        const int remaining=h-34-filesY-gap;
        const int filesH=std::max(100,remaining*53/100);
        const int logsY=filesY+filesH+gap;
        const int logsH=std::max(82,h-34-logsY);
        files={px(pad),px(filesY),px(w-pad),px(filesY+filesH)};
        logs={px(pad),px(logsY),px(w-pad),px(logsY+logsH)};
        move(output,pad+17,filesY+49,cw-34,filesH-62);
        move(journal,pad+17,logsY+44,cw-34,logsH-55);
        updateTerminalScrollbar(output);updateTerminalScrollbar(journal);
        layingOut=false;
        InvalidateRect(window,nullptr,FALSE);
    }
    void appendLog(const wchar_t* message, bool error=false) {
        if(!journal) return;
        SYSTEMTIME now{}; GetLocalTime(&now);
        wchar_t prefix[32]{};
        swprintf(prefix,32,L"%02u:%02u:%02u  %ls  ",now.wHour,now.wMinute,now.wSecond,error?L"ERR":L"INFO");
        std::wstring entry=prefix; entry+=message; entry+=L"\r\n";
        int length=GetWindowTextLengthW(journal);
        if(length+static_cast<int>(entry.size())>LogLimit) {SetWindowTextW(journal,L"");length=0;}
        SendMessageW(journal,EM_SETSEL,length,length);
        SendMessageW(journal,EM_REPLACESEL,FALSE,reinterpret_cast<LPARAM>(entry.c_str()));
        SendMessageW(journal,EM_SCROLLCARET,0,0);
        hasError=hasError||error;
        InvalidateRect(window,nullptr,FALSE);
    }
    void folder() {
        ComPtr<IFileOpenDialog> dialog;
        HRESULT status=CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(dialog.out()));
        if(SUCCEEDED(status)) {
            DWORD options{}; status=dialog->GetOptions(&options);
            if(SUCCEEDED(status)) status=dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|FOS_NOCHANGEDIR);
            dialog->SetTitle(tr(L"Sélectionner le dossier de travail",L"Select the working folder"));
            const int length=GetWindowTextLengthW(path);
            if(length>0) {
                std::wstring current(static_cast<size_t>(length)+1,L'\0');
                GetWindowTextW(path,current.data(),length+1);
                ComPtr<IShellItem> item;
                if(SUCCEEDED(SHCreateItemFromParsingName(current.c_str(),nullptr,IID_PPV_ARGS(item.out())))) dialog->SetFolder(item.value);
            }
            if(SUCCEEDED(status)) status=dialog->Show(window);
            if(SUCCEEDED(status)) {
                ComPtr<IShellItem> item; status=dialog->GetResult(item.out());
                if(SUCCEEDED(status)) {
                    PWSTR selected=nullptr; status=item->GetDisplayName(SIGDN_FILESYSPATH,&selected);
                    if(SUCCEEDED(status)) {
                        SetWindowTextW(path,selected); CoTaskMemFree(selected);
                        appendLog(tr(L"Dossier sélectionné.",L"Folder selected."));
                    }
                }
            }
        }
        if(FAILED(status) && status!=HRESULT_FROM_WIN32(ERROR_CANCELLED))
            appendLog(tr(L"Impossible d’ouvrir le sélecteur de dossier.",L"Could not open the folder picker."),true);
    }
    void popup(int id) {
        HMENU menu=CreatePopupMenu();
        if(id==File) {
            AppendMenuW(menu,MF_STRING,OpenFolder,tr(L"Choisir un dossier…\tCtrl+O",L"Choose folder…\tCtrl+O"));
            AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
            AppendMenuW(menu,MF_STRING,Exit,tr(L"Quitter\tAlt+F4",L"Exit\tAlt+F4"));
        } else if(id==Help) {
            AppendMenuW(menu,MF_STRING,About,tr(L"À propos de ZTX",L"About ZTX"));
        } else {
            static constexpr const wchar_t* labelsFR[]{L"o — octet",L"Ko — kilooctet",L"Mo — mégaoctet",L"Go — gigaoctet",L"To — téraoctet",L"Po — pétaoctet"};
            static constexpr const wchar_t* labelsEN[]{L"B — byte",L"KB — kilobyte",L"MB — megabyte",L"GB — gigabyte",L"TB — terabyte",L"PB — petabyte"};
            for(int i=0;i<6;++i) AppendMenuW(menu,MF_STRING|(i==unitIndex?MF_CHECKED:0),UnitFirst+i,tr(labelsFR[i],labelsEN[i]));
        }
        RECT r{}; GetWindowRect(control(id),&r);
        const int selection=static_cast<int>(TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,r.left,r.bottom,window,nullptr));
        DestroyMenu(menu);
        if(selection) command(selection);
    }
    void about() {
        MessageBoxW(window,tr(
            L"ZTX\nCKitastam · Version 0.1.0\n\nInterface native C++ pour Windows.\nLe moteur de nettoyage sera connecté ultérieurement.\n\nCtrl+O : choisir un dossier\nCtrl+, : paramètres\nF1 : aide\n\nLe bouton Nettoyer est volontairement inactif.",
            L"ZTX\nCKitastam · Version 0.1.0\n\nNative C++ interface for Windows.\nThe cleaning engine will be connected later.\n\nCtrl+O: choose a folder\nCtrl+,: settings\nF1: help\n\nThe Clean button is intentionally inactive."),
            tr(L"À propos de ZTX",L"About ZTX"),MB_OK|MB_ICONINFORMATION);
    }
    void command(int id) {
        if(id>=UnitFirst && id<UnitFirst+6) {
            unitIndex=id-UnitFirst;
            wchar_t value[16]{}; GetWindowTextW(size,value,16); translate(); SetWindowTextW(size,value); return;
        }
        switch(id) {
        case File: case Help: case Unit: popup(id); break;
        case Browse: case OpenFolder: folder(); break;
        case Settings: showSettings(); break;
        case Logo: case About: about(); break;
        case Exit: SendMessageW(window,WM_CLOSE,0,0); break;
        case Clean: break; // Deliberately no filesystem operation until an engine is supplied.
        default: break;
        }
    }
    void drawButton(const DRAWITEMSTRUCT& item) {
        const int id=static_cast<int>(item.CtlID);
        const bool inSettings=GetParent(item.hwndItem)==settings;
        const auto px=[&](int value){return this->px(value,inSettings);};
        const auto text=[&](HDC dc,const wchar_t* value,RECT rect,int font,COLORREF color,UINT flags){
            this->text(dc,value,rect,font,color,flags,inSettings);
        };
        RECT r=item.rcItem;
        FillRect(item.hDC,&r,inSettings?gdi.background:(id==Unit || id==Browse)?gdi.panel:gdi.background);
        float blend=0;
        for(const auto& state:buttons) if(state.window==item.hwndItem) {blend=state.blend;break;}
        const bool selected=(id==Dark && preferences.theme==ztx::Theme::Dark)||
            (id==Light && preferences.theme==ztx::Theme::Light)||
            (id==French && preferences.language==ztx::Language::French)||
            (id==English && preferences.language==ztx::Language::English)||
            (id==Animation && preferences.animations);
        if(id==Logo) {
            const int side=px(42)+static_cast<int>(px(4)*blend);
            if(logo) {
                Gdiplus::Graphics graphics(item.hDC);
                graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                graphics.DrawImage(logo.get(),(r.right-side)/2,(r.bottom-side)/2,side,side);
            } else text(item.hDC,L"Z",r,2,palette.accent,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        } else {
            const bool accent=id==Clean||id==Done;
            const bool menu=id==File||id==Settings||id==Help;
            COLORREF fill=accent?palette.accent:selected?mix(palette.panel,palette.accent,.14f):menu?palette.background:palette.input;
            fill=mix(fill,accent?mix(palette.accent,RGB(255,255,255),.18f):palette.hover,blend);
            if(item.itemState&ODS_SELECTED) fill=mix(fill,palette.text,.13f);
            if(!menu || blend>0 || (item.itemState&ODS_SELECTED)) rounded(item.hDC,r,fill,selected?palette.accent:accent?fill:menu?fill:palette.line,8,inSettings);
            wchar_t label[100]{}; GetWindowTextW(item.hwndItem,label,100);
            RECT labelRect=r;
            if(item.itemState&ODS_SELECTED && preferences.animations) OffsetRect(&labelRect,0,px(1));
            if(id==Unit) labelRect.right-=px(18);
            text(item.hDC,label,labelRect,(accent||selected)?5:0,accent?palette.accentText:selected?palette.accent:palette.text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            if(id==Unit) {
                const HPEN pen=CreatePen(PS_SOLID,px(1),palette.muted); const auto old=SelectObject(item.hDC,pen);
                const int x=r.right-px(20), y=r.bottom/2-px(2);
                MoveToEx(item.hDC,x-px(4),y,nullptr); LineTo(item.hDC,x,y+px(4)); LineTo(item.hDC,x+px(4),y);
                SelectObject(item.hDC,old); DeleteObject(pen);
            }
            if(id==Clean) text(item.hDC,L"→",RECT{r.right-px(48),0,r.right-px(20),r.bottom},2,palette.accentText,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
        }
        if((item.itemState&ODS_FOCUS) && !(item.itemState&ODS_NOFOCUSRECT)) {
            InflateRect(&r,-px(4),-px(4)); DrawFocusRect(item.hDC,&r);
        }
    }
    void paintMain(HDC target) {
        RECT bounds{}; GetClientRect(window,&bounds);
        HDC dc=mainBuffer.get(target,bounds.right,px(contentHeight));
        RECT canvas{0,0,bounds.right,px(contentHeight)};
        FillRect(dc,&canvas,gdi.background);
        const int w=MulDiv(bounds.right,96,static_cast<int>(dpi));
        const int h=contentHeight;
        const int pad=MulDiv(config.left,96,static_cast<int>(dpi));
        text(dc,L"ZTX",pad+60,20,100,28,2,palette.text);
        text(dc,L"C K i t a s t a m",pad+60,47,160,17,1,palette.muted);
        rounded(dc,RECT{px(w-pad-64),px(29),px(w-pad),px(53)},palette.panel,palette.line,12);
        text(dc,tr(L"PRÊT",L"READY"),w-pad-64,29,64,24,1,palette.accent,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        line(dc,pad,125,w-2*pad,palette.line);
        text(dc,tr(L"Votre espace, au propre.",L"A cleaner workspace."),pad,139,w-2*pad,35,3,palette.text);
        text(dc,tr(L"Chaque opération commence ici.",L"Every operation starts here."),pad,175,w-2*pad,20,0,palette.muted);
        rounded(dc,config,palette.panel,palette.line);
        text(dc,L"01",pad+20,210,27,24,1,palette.accent);
        text(dc,tr(L"Configuration",L"Configuration"),pad+53,208,w-2*pad-74,28,5,palette.text);
        text(dc,tr(L"Taille cible",L"Target size"),pad+20,239,200,22,1,palette.muted);
        rounded(dc,RECT{px(pad+20),px(263),px(w-pad-122),px(307)},palette.input,palette.line,8);
        text(dc,tr(L"Répertoire de travail",L"Working directory"),pad+20,317,260,22,1,palette.muted);
        rounded(dc,RECT{px(pad+20),px(341),px(w-pad-132),px(385)},palette.input,palette.line,8);
        rounded(dc,files,palette.panel,palette.line);
        const int fy=MulDiv(files.top,96,static_cast<int>(dpi));
        text(dc,L"02",pad+17,fy+10,26,26,1,palette.accent);
        text(dc,tr(L"Sortie des fichiers",L"File output"),pad+48,fy+10,220,26,5,palette.text);
        text(dc,tr(L"EN ATTENTE",L"IDLE"),w-pad-110,fy+13,92,20,1,palette.muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        line(dc,pad+17,fy+40,w-2*pad-34,palette.line);
        rounded(dc,logs,palette.panel,palette.line);
        const int ly=MulDiv(logs.top,96,static_cast<int>(dpi));
        text(dc,L"03",pad+17,ly+9,26,25,1,palette.accent);
        text(dc,tr(L"Journal d’erreurs",L"Error log"),pad+48,ly+9,220,25,5,palette.text);
        text(dc,hasError?tr(L"À VÉRIFIER",L"CHECK LOG"):tr(L"AUCUNE ERREUR",L"NO ERRORS"),w-pad-128,ly+12,110,20,1,hasError?RGB(232,150,110):palette.muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        text(dc,tr(L"Interface prête · Moteur à connecter",L"Interface ready · Engine pending"),pad,h-28,w-2*pad-65,22,1,palette.muted);
        text(dc,L"v0.1.0",w-pad-60,h-28,60,22,1,palette.muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        if(dc!=target) BitBlt(target,0,0,bounds.right,bounds.bottom,dc,0,scrollY,SRCCOPY);
    }
    void showSettings();
    void createSettings();
    void layoutSettings();
    void settingsCommand(int id);
    void paintSettings(HDC dc);
    void closeSettings();
};

LRESULT CALLBACK mainProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp);
LRESULT CALLBACK settingsProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp);

void App::showSettings() {
    if(settings) {SetForegroundWindow(settings);return;}
    RECT owner{}; GetWindowRect(window,&owner);
    RECT r{0,0,px(420),px(522)};
    const DWORD style=WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN;
    AdjustWindowRectExForDpi(&r,style,FALSE,WS_EX_DLGMODALFRAME,dpi);
    const int width=r.right-r.left,height=r.bottom-r.top;
    r.left=owner.left+(owner.right-owner.left-width)/2;
    r.top=owner.top+(owner.bottom-owner.top-height)/2;
    r.right=r.left+width;r.bottom=r.top+height;
    r=fitToWorkArea(r);
    settingsScrollY=0;
    settings=CreateWindowExW(WS_EX_DLGMODALFRAME,SettingsClass,tr(L"Paramètres — ZTX",L"Settings — ZTX"),style,
        r.left,r.top,r.right-r.left,r.bottom-r.top,
        window,nullptr,instance,this);
    if(settings) {EnableWindow(window,FALSE);ShowWindow(settings,SW_SHOW);SetFocus(control(Dark,settings));}
}

void App::createSettings() {
    settingsDpi=GetDpiForWindow(settings);
    button(settings,Dark,tr(L"Sombre",L"Dark"));button(settings,Light,tr(L"Clair",L"Light"));
    button(settings,French,L"Français");button(settings,English,L"English");
    button(settings,Animation,preferences.animations?tr(L"Activées",L"Enabled"):tr(L"Désactivées",L"Disabled"));
    button(settings,Done,tr(L"Terminé",L"Done"));
    slider=CreateWindowExW(0,TRACKBAR_CLASSW,tr(L"Liquid Glass : transparence et compacité",L"Liquid Glass: transparency and density"),
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,0,0,0,0,settings,reinterpret_cast<HMENU>(Glass),instance,nullptr);
    SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELONG(0,100));
    SendMessageW(slider,TBM_SETPOS,TRUE,preferences.glass);
    SendMessageW(slider,TBM_SETPAGESIZE,0,10);
    SetWindowSubclass(slider,editProc,2,reinterpret_cast<DWORD_PTR>(this));
    rebuildStyle();
    layoutSettings();
}

void App::layoutSettings() {
    if(!slider || layingOutSettings) return;
    layingOutSettings=true;
    setScrollRange(settings,522);
    move(control(Dark,settings),24,112,180,40); move(control(Light,settings),216,112,180,40);
    move(control(French,settings),24,204,180,40);move(control(English,settings),216,204,180,40);
    move(control(Animation,settings),278,274,118,38);
    move(slider,22,380,376,34); move(control(Done,settings),24,456,372,42);
    layingOutSettings=false;
    InvalidateRect(settings,nullptr,FALSE);
}

void App::settingsCommand(int id) {
    switch(id) {
    case Dark:preferences.theme=ztx::Theme::Dark;rebuildStyle();break;
    case Light:preferences.theme=ztx::Theme::Light;rebuildStyle();break;
    case French:case English:{
        wchar_t current[16]{};GetWindowTextW(size,current,16);
        preferences.language=id==French?ztx::Language::French:ztx::Language::English;
        translate();SetWindowTextW(size,current);break;
    }
    case Animation:
        preferences.animations=!preferences.animations;
        for(auto& state:buttons) if(state.window) {
            KillTimer(state.window,1);state.blend=state.hovered?1.f:0.f;
            InvalidateRect(state.window,nullptr,FALSE);
        }
        SetWindowTextW(control(Animation,settings),preferences.animations?tr(L"Activées",L"Enabled"):tr(L"Désactivées",L"Disabled"));
        break;
    case Done:closeSettings();return;
    default:return;
    }
    RedrawWindow(settings,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}

void App::paintSettings(HDC target) {
    RECT bounds{};GetClientRect(settings,&bounds);
    const int height=std::max(static_cast<int>(bounds.bottom),px(522,true));
    HDC dc=settingsBuffer.get(target,bounds.right,height);
    RECT canvas{0,0,bounds.right,height};
    FillRect(dc,&canvas,settingsGdi.background);
    const auto text=[&](HDC hdc,const wchar_t* value,int x,int y,int w,int h,int font,COLORREF color,
                       UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS){
        this->text(hdc,value,x,y,w,h,font,color,flags,true);
    };
    text(dc,tr(L"À votre image.",L"Make it yours."),24,20,372,38,3,palette.text);
    text(dc,tr(L"Vos préférences s’appliquent en direct.",L"Your preferences update instantly."),24,60,372,22,0,palette.muted);
    text(dc,tr(L"Apparence",L"Appearance"),24,87,372,22,5,palette.text);
    text(dc,tr(L"Langue",L"Language"),24,177,372,22,5,palette.text);
    text(dc,tr(L"Animations",L"Animations"),24,270,230,24,5,palette.text);
    text(dc,tr(L"Des interactions tout en douceur.",L"Subtle motion, on demand."),24,297,250,22,1,palette.muted);
    line(dc,24,335,372,palette.line,true);
    text(dc,L"Liquid Glass",24,346,280,28,5,palette.text);
    wchar_t amount[16]{};swprintf(amount,16,L"%d %%",preferences.glass);
    text(dc,amount,326,346,70,28,0,palette.accent,DT_RIGHT|DT_SINGLELINE|DT_VCENTER);
    text(dc,tr(L"Transparent",L"Transparent"),24,414,180,22,1,palette.muted);
    text(dc,tr(L"Compact",L"Compact"),216,414,180,22,1,palette.muted,DT_RIGHT|DT_SINGLELINE|DT_VCENTER);
    if(dc!=target) BitBlt(target,0,0,bounds.right,bounds.bottom,dc,0,settingsScrollY,SRCCOPY);
}

void App::closeSettings() {
    if(!settings) return;
    const bool saved=ztx::savePreferences(preferences);
    EnableWindow(window,TRUE);
    DestroyWindow(settings);settings=nullptr;slider=nullptr;
    settingsBuffer.clear();settingsGdi.clear();
    SetForegroundWindow(window);SetFocus(control(Settings));
    appendLog(saved?tr(L"Préférences enregistrées.",L"Preferences saved."):
        tr(L"Échec de l’enregistrement des préférences.",L"Could not save preferences."),!saved);
}

LRESULT controlColor(App& app,HDC dc,HWND child) {
    const bool terminal=child==app.output||child==app.journal;
    SetTextColor(dc,terminal?app.palette.muted:app.palette.text);
    SetBkColor(dc,terminal?app.palette.panel:app.palette.input);
    return reinterpret_cast<LRESULT>(terminal?app.gdi.panel:app.gdi.input);
}

LRESULT CALLBACK settingsProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
        app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));app->settings=hwnd;
    }
    if(!app) return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg) {
    case WM_CREATE:app->createSettings();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);app->paintSettings(dc);EndPaint(hwnd,&ps);return 0;}
    case WM_DRAWITEM:app->drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED)app->settingsCommand(LOWORD(wp));return 0;
    case WM_SIZE:app->layoutSettings();return 0;
    case WM_VSCROLL:if(lp==0)app->scroll(hwnd,wp);return 0;
    case WM_MOUSEWHEEL:app->mouseWheel(hwnd,wp);return 0;
    case WM_DPICHANGED:{
        app->settingsDpi=HIWORD(wp);app->rebuildStyle();
        const RECT r=fitToWorkArea(*reinterpret_cast<RECT*>(lp));
        SetWindowPos(hwnd,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);
        app->layoutSettings();return 0;
    }
    case WM_HSCROLL:
        if(reinterpret_cast<HWND>(lp)==app->slider) {
            app->preferences.glass=static_cast<int>(SendMessageW(app->slider,TBM_GETPOS,0,0));
            app->applyGlass();InvalidateRect(hwnd,nullptr,FALSE);
            InvalidateRect(app->slider,nullptr,FALSE);
        }return 0;
    case WM_NOTIFY:{
        auto* draw=reinterpret_cast<NMCUSTOMDRAW*>(lp);
        if(draw->hdr.hwndFrom==app->slider && draw->hdr.code==NM_CUSTOMDRAW) {
            if(draw->dwDrawStage==CDDS_PREPAINT) {
                RECT r{};GetClientRect(app->slider,&r);
                FillRect(draw->hdc,&r,app->settingsGdi.background);
                return CDRF_NOTIFYITEMDRAW;
            }
            if(draw->dwDrawStage==CDDS_ITEMPREPAINT) {
                RECT r=draw->rc;
                if(draw->dwItemSpec==TBCD_CHANNEL) {
                    r.top=(r.top+r.bottom)/2-app->px(2,true);r.bottom=r.top+app->px(4,true);
                    app->rounded(draw->hdc,r,app->palette.line,app->palette.line,2,true);
                    r.right=r.left+(r.right-r.left)*app->preferences.glass/100;
                    if(r.right-r.left>app->px(4,true))app->rounded(draw->hdc,r,app->palette.accent,app->palette.accent,2,true);
                    return CDRF_SKIPDEFAULT;
                }
                if(draw->dwItemSpec==TBCD_THUMB) {
                    app->rounded(draw->hdc,r,app->palette.accent,app->palette.accent,5,true);return CDRF_SKIPDEFAULT;
                }
            }
        }break;
    }
    case WM_CTLCOLORSTATIC:SetBkColor(reinterpret_cast<HDC>(wp),app->palette.background);return reinterpret_cast<LRESULT>(app->settingsGdi.background);
    case WM_CLOSE:app->closeSettings();return 0;
    default:break;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

LRESULT CALLBACK mainProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
        app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));app->window=hwnd;
    }
    if(!app) return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg) {
    case WM_CREATE:app->createMain();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);app->paintMain(dc);EndPaint(hwnd,&ps);return 0;}
    case WM_DRAWITEM:app->drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:return controlColor(*app,reinterpret_cast<HDC>(wp),reinterpret_cast<HWND>(lp));
    case WM_SIZE:if(wp!=SIZE_MINIMIZED)app->layout();return 0;
    case WM_VSCROLL:if(lp==0)app->scroll(hwnd,wp);return 0;
    case WM_MOUSEWHEEL:app->mouseWheel(hwnd,wp);return 0;
    case WM_GETMINMAXINFO:{
        auto* limits=reinterpret_cast<MINMAXINFO*>(lp);
        RECT r{0,0,app->px(460),app->px(420)};
        AdjustWindowRectExForDpi(&r,static_cast<DWORD>(GetWindowLongPtrW(hwnd,GWL_STYLE)),FALSE,WS_EX_LAYERED,app->dpi);
        limits->ptMinTrackSize={r.right-r.left,r.bottom-r.top};
        limits->ptMaxTrackSize.x=app->px(620);
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
        if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor)) {
            limits->ptMinTrackSize.x=std::min(limits->ptMinTrackSize.x,monitor.rcWork.right-monitor.rcWork.left);
            limits->ptMinTrackSize.y=std::min(limits->ptMinTrackSize.y,monitor.rcWork.bottom-monitor.rcWork.top);
        }
        return 0;
    }
    case WM_DPICHANGED:{
        app->dpi=HIWORD(wp);app->rebuildStyle();
        const RECT r=fitToWorkArea(*reinterpret_cast<RECT*>(lp));
        SetWindowPos(hwnd,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);
        app->layout();return 0;
    }
    case WM_COMMAND:
        if(HIWORD(wp)==BN_CLICKED || HIWORD(wp)==1) app->command(LOWORD(wp));
        return 0;
    case WM_CLOSE:
        if(app->settings)app->closeSettings();
        DestroyWindow(hwnd);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    default:break;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

struct Runtime {
    ULONG_PTR graphics{};HRESULT com{E_FAIL};
    ~Runtime(){if(graphics)Gdiplus::GdiplusShutdown(graphics);if(SUCCEEDED(com))CoUninitialize();}
};
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Runtime runtime;
    runtime.com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    Gdiplus::GdiplusStartupInput startup;
    if(FAILED(runtime.com) || Gdiplus::GdiplusStartup(&runtime.graphics,&startup,nullptr)!=Gdiplus::Ok) return 1;
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};InitCommonControlsEx(&controls);
    App app;app.instance=instance;
    WNDCLASSEXW wc{};
    wc.cbSize=sizeof(wc);
    wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_APP));wc.hIconSm=wc.hIcon;
    wc.lpszClassName=MainClass;wc.lpfnWndProc=mainProc;
    if(!RegisterClassExW(&wc))return 1;
    wc.lpszClassName=SettingsClass;wc.lpfnWndProc=settingsProc;
    if(!RegisterClassExW(&wc))return 1;
    app.dpi=GetDpiForSystem();
    const DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_THICKFRAME|WS_CLIPCHILDREN;
    RECT r{0,0,app.px(500),app.px(790)};
    AdjustWindowRectExForDpi(&r,style,FALSE,WS_EX_LAYERED,app.dpi);
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    const int width=r.right-r.left,height=std::min(r.bottom-r.top,work.bottom-work.top);
    if(!CreateWindowExW(WS_EX_LAYERED,MainClass,L"ZTX — CKitastam",style,
        work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,
        width,height,nullptr,nullptr,instance,&app))return 1;
    ShowWindow(app.window,show);UpdateWindow(app.window);
    const ACCEL shortcuts[]{{FVIRTKEY|FCONTROL,'O',OpenFolder},{FVIRTKEY|FCONTROL,VK_OEM_COMMA,Settings},{FVIRTKEY,VK_F1,About}};
    HACCEL accelerators=CreateAcceleratorTableW(const_cast<ACCEL*>(shortcuts),3);
    MSG msg{};int status{};
    while((status=GetMessageW(&msg,nullptr,0,0))>0) {
        if(app.settings) {
            if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE){app.closeSettings();continue;}
            if(IsDialogMessageW(app.settings,&msg))continue;
        } else {
            if(TranslateAcceleratorW(app.window,accelerators,&msg))continue;
            if(IsDialogMessageW(app.window,&msg))continue;
        }
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    if(accelerators)DestroyAcceleratorTable(accelerators);
    return status<0?1:static_cast<int>(msg.wParam);
}
