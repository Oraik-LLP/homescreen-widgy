#include "render.hpp"
#include "wallpaper.hpp"
#include <commdlg.h>
#include "../third_party/json.hpp"
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <wtsapi32.h>
#include <psapi.h>
#include <fstream>
#include <functional>
#include <chrono>
#include <memory>

namespace dw {
constexpr UINT TrayMessage=WM_APP+1, RepairMessage=WM_APP+2, ActivateMessage=WM_APP+3, PaletteMessage=WM_APP+4;
constexpr UINT MediaMessage=WM_APP+6;
constexpr UINT_PTR TickTimer=1, RetryTimer=2, DiagnosticTimer=3, WallpaperTimer=4, WallpaperPoll=5;
constexpr int MenuSettings=101, MenuLock=102, MenuShow=103, MenuExit=104, MenuPalette=105, MenuFocus=106, MenuFocusReset=107, MenuFocusNext=108;
constexpr wchar_t WidgetClass[]=L"panes.Widget.v1", ControlClass[]=L"panes.Controller.v1", SettingsClass[]=L"panes.Settings.v1";
constexpr wchar_t PickerClass[]=L"panes.Picker.v1";
class App;
static App* currentApp{};
struct Widget {
    App* app{}; int index{}; HWND window{}; std::unique_ptr<Surface> surface;
    SYSTEMTIME last{}; bool dirty=true, dragging=false; POINT mouse{}, origin{};
};
enum ControlId { SelectWidget=201, Preset, Font, Scale, Opacity, Color, Accent, Visible, Hour24, Seconds, Startup, Apply, Reset, Done, Status, TabWidgets, TabAppearance, TabColors, MatchWallpaper, UseTheme, PickText, PickAccent, TextSwatch, AccentSwatch,
    FocusMinutes, BreakMinutes, FocusStart, FocusReset, FocusNext, EventTitle, EventDate, Zone, City,
    ToolHex, ToolChoose, ToolPick, CopyHex, CopyRgb, Favorite, AssignText, AssignAccent,
    ThemeFirst=400, RecentFirst=500, FavoriteFirst=520, PaletteFirst=540, AddWidget=550, BackWidgets=551, EditMode=552, WidgetFirst=600 };
class App {
public:
    HINSTANCE instance{}; HWND control{},settings{},desktopHost{}; UINT taskbarCreated{};
    HANDLE mutex{},desktopMutex{}; bool isolated=false,configWritable=true,paused=false,quitting=false,repairPending=false;
    bool diagnostics=false,previews=false,firstRun=false,prototype=false,applicationTest=false;
    int duration=0,retries=0,selected=0; ULONGLONG startMs{},lastCpu{},lastCpuMs{};
    double cpuSum=0,maxCpu=0; size_t peakPrivate=0,peakWorking=0; int samples=0;
    std::filesystem::path root; std::wstring controllerTitle=L"panes controller",warning,status=L"Desktop mode · offline";
    Config config; std::unique_ptr<Graphics> graphics;
    std::array<Widget,WidgetCount> widgets;
    Config draft; WallpaperMatcher wallpaper;MediaController media;MediaSnapshot mediaState;
    HWND panel{},picker{};int page=0,scroll=0,bodyHeight=0;
    struct UiItem {HWND window;int page,y;int kind;};std::vector<UiItem> uiItems;
    std::array<std::unique_ptr<Surface>,ThemeCount> themePreviews;
    std::array<std::unique_ptr<Surface>,WidgetCount> widgetPreviews;
    std::vector<DYNAMIC_TIME_ZONE_INFORMATION> zones;
    unsigned toolColor=0x91B89D;ULONGLONG lastPick=0;
    bool populating=false;
    std::vector<Monitor> monitors;
    std::vector<HWINEVENTHOOK> hooks;
    NOTIFYICONDATAW tray{}; HFONT uiFont{},titleFont{}; HBRUSH background{};
    explicit App(HINSTANCE h):instance(h){}
    ~App();
    void init(); void save(); void loadMonitors(); void place(Widget& w, bool defaults=false);
    void render(bool force=false); void schedule(); void repair(); void requestRepair();
    void addTray(); void trayMenu(); void command(int id); void remember(Widget& w);
    void showSettings(); void makeSettingsControls(); void populateSettings(); bool applySettings();
    bool readSettings(Config& candidate);void layoutSettings();void refreshPreviews();void refreshWidgetPreviews();void colorTool(unsigned color,bool remember=true);void chooseColor(int id);void startPicker();void endPicker(bool accept);
    void focusCommand(int action);void completeFocus();void requestWallpaper();void updateMedia();WidgetConfig effectiveWidget(const Widget& w) const;
    HWND child(int id) const {HWND w=panel?GetDlgItem(panel,id):nullptr;return w?w:GetDlgItem(settings,id);}
    void sample(); void finishDiagnostics(); bool startupEnabled() const; bool setStartup(bool on);
    void runChecks();
    static LRESULT CALLBACK ControlProc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK WidgetProc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK SettingsProc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK PickerProc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK PanelProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
};
static ULONGLONG fileValue(FILETIME f) { ULARGE_INTEGER v{}; v.LowPart=f.dwLowDateTime; v.HighPart=f.dwHighDateTime; return v.QuadPart; }
static ULONGLONG cpuTime() { FILETIME a,b,c,d; GetProcessTimes(GetCurrentProcess(),&a,&b,&c,&d); return fileValue(c)+fileValue(d); }
static HWND findDefView(HWND parent) {
    HWND found=FindWindowExW(parent,nullptr,L"SHELLDLL_DefView",nullptr);
    if(found) return found;
    for(HWND child=GetWindow(parent,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
        // Only descend Explorer's small desktop hierarchy, not arbitrary app trees.
        wchar_t name[80]{}; GetClassNameW(child,name,80);
        if(wcscmp(name,L"WorkerW")==0) if(auto def=FindWindowExW(child,nullptr,L"SHELLDLL_DefView",nullptr)) return def;
    }
    return nullptr;
}
static HWND discoverDesktop() {
    HWND result{};
    EnumWindows([](HWND w,LPARAM param)->BOOL {
        wchar_t name[80]{}; GetClassNameW(w,name,80);
        if(wcscmp(name,L"Progman") && wcscmp(name,L"WorkerW")) return TRUE;
        if(findDefView(w)) { *reinterpret_cast<HWND*>(param)=GetAncestor(w,GA_ROOT); return FALSE; }
        return TRUE;
    },reinterpret_cast<LPARAM>(&result));
    return result;
}
static void CALLBACK eventProc(HWINEVENTHOOK,DWORD event,HWND hwnd,LONG object,LONG,DWORD,DWORD) {
    if(!currentApp || !currentApp->control) return;
    if(event==EVENT_SYSTEM_FOREGROUND || event==EVENT_SYSTEM_MINIMIZESTART || event==EVENT_SYSTEM_MINIMIZEEND ||
       (event==EVENT_OBJECT_DESTROY && hwnd==currentApp->desktopHost && object==OBJID_WINDOW)) currentApp->requestRepair();
}
App::~App() {
    currentApp=nullptr;
    media.stop();
    if(picker)DestroyWindow(picker);
    for(auto hook:hooks) if(hook) UnhookWinEvent(hook);
    if(control) { WTSUnRegisterSessionNotification(control); Shell_NotifyIconW(NIM_DELETE,&tray); }
    for(auto& w:widgets) { if(w.window) DestroyWindow(w.window); w.surface.reset(); }
    if(settings) DestroyWindow(settings);
    for(auto& preview:themePreviews)preview.reset();
    for(auto& preview:widgetPreviews)preview.reset();
    if(control) DestroyWindow(control);
    if(uiFont) DeleteObject(uiFont); if(titleFont) DeleteObject(titleFont); if(background) DeleteObject(background);
    graphics.reset(); if(mutex) CloseHandle(mutex); if(desktopMutex)CloseHandle(desktopMutex);
}
void App::loadMonitors() {
    monitors.clear();
    EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR h,HDC,LPRECT,LPARAM p)->BOOL {
        auto* self=reinterpret_cast<App*>(p); MONITORINFOEXW info{}; info.cbSize=sizeof(info);
        if(GetMonitorInfoW(h,&info)) {
            UINT x=96,y=96; GetDpiForMonitor(h,MDT_EFFECTIVE_DPI,&x,&y);
            Monitor m{h,info.szDevice,info.rcWork,x};
            if(info.dwFlags&MONITORINFOF_PRIMARY) self->monitors.insert(self->monitors.begin(),m);
            else self->monitors.push_back(m);
        } return TRUE;
    },reinterpret_cast<LPARAM>(this));
    if(monitors.empty()) throw std::runtime_error("No interactive display is available");
}
void App::place(Widget& widget,bool defaults) {
    auto& cfg=config.widgets[widget.index];
    if(!cfg.visible){ShowWindow(widget.window,SW_HIDE);return;}
    auto it=std::find_if(monitors.begin(),monitors.end(),[&](const Monitor& m){return m.device==cfg.monitor;});
    const bool missing=it==monitors.end();
    const auto& monitor=missing?monitors.front():*it;
    SIZE logical=logicalSize(cfg.kind,config.preset,cfg.scale,config.seconds);
    SIZE size{MulDiv(logical.cx,monitor.dpi,96),MulDiv(logical.cy,monitor.dpi,96)};
    POINT pos{monitor.work.left+MulDiv(cfg.x,monitor.dpi,96), monitor.work.top+MulDiv(cfg.y,monitor.dpi,96)};
    if(!cfg.positioned || defaults) {
        int width=monitor.work.right-monitor.work.left,height=monitor.work.bottom-monitor.work.top;
        if(cfg.kind==Kind::Music)pos={monitor.work.left+MulDiv(30,monitor.dpi,96),monitor.work.bottom-MulDiv(700,monitor.dpi,96)};
        else if(static_cast<int>(cfg.kind)>=3){int slot=static_cast<int>(cfg.kind)-3;pos={monitor.work.left+MulDiv(30+slot%2*350,monitor.dpi,96),monitor.work.bottom-MulDiv(350-slot/2*170,monitor.dpi,96)};}
        else pos=cfg.kind==Kind::Calendar ? POINT{monitor.work.right-size.cx-40,monitor.work.top+height/3}
            : POINT{monitor.work.left+(width-size.cx)/2,monitor.work.top+height/9+(cfg.kind==Kind::Date?MulDiv(180,monitor.dpi,96):0)};
    }
    pos=clampPosition(pos,size,monitor.work);
    SetWindowPos(widget.window,nullptr,pos.x,pos.y,size.cx,size.cy,SWP_NOZORDER|SWP_NOACTIVATE);
    cfg.monitor=monitor.device; cfg.x=MulDiv(pos.x-monitor.work.left,96,monitor.dpi); cfg.y=MulDiv(pos.y-monitor.work.top,96,monitor.dpi); cfg.positioned=true;
    widget.dirty=true;
}
void App::remember(Widget& w) {
    RECT r{};GetWindowRect(w.window,&r);
    HMONITOR h=MonitorFromWindow(w.window,MONITOR_DEFAULTTONEAREST);
    auto it=std::find_if(monitors.begin(),monitors.end(),[&](const Monitor& m){return m.handle==h;});
    if(it==monitors.end()) return;
    auto& cfg=config.widgets[w.index];cfg.monitor=it->device;
    SIZE size{r.right-r.left,r.bottom-r.top};auto p=clampPosition({r.left,r.top},size,it->work);
    cfg.x=MulDiv(p.x-it->work.left,96,it->dpi);cfg.y=MulDiv(p.y-it->work.top,96,it->dpi);cfg.positioned=true;
    SetWindowPos(w.window,nullptr,p.x,p.y,0,0,SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);save();
}
void App::requestRepair() {
    if(!repairPending) {repairPending=true;PostMessageW(control,RepairMessage,0,0);}
}
void App::repair() {
    repairPending=false;
    desktopHost=discoverDesktop();
    if(!desktopHost) {
        status=L"Waiting for the Windows desktop";
        for(auto& w:widgets) ShowWindow(w.window,SW_HIDE);
        if(retries++<12) SetTimer(control,RetryTimer,std::min(2000,150*(retries+1)),nullptr);
        return;
    }
    retries=0;KillTimer(control,RetryTimer);status=L"Desktop mode · offline";
    // Place immediately above the actual icon-host root. No cross-process
    // SetParent, injected shell DLL, topmost window, or wallpaper modification.
    for(auto& w:widgets) {
        LONG_PTR ex=GetWindowLongPtrW(w.window,GWL_EXSTYLE);
        const bool passThrough=config.locked&&config.widgets[w.index].kind!=Kind::Music;
        ex=passThrough ? (ex|WS_EX_TRANSPARENT) : (ex&~WS_EX_TRANSPARENT);
        SetWindowLongPtrW(w.window,GWL_EXSTYLE,ex);
        if(!config.shown || !config.widgets[w.index].visible || paused) {ShowWindow(w.window,SW_HIDE);continue;}
        HWND before=GetWindow(desktopHost,GW_HWNDPREV);
        while(before) {
            bool ours=before==control || before==settings;
            for(const auto& other:widgets) ours=ours || before==other.window;
            if(!ours) break;
            before=GetWindow(before,GW_HWNDPREV);
        }
        if(IsIconic(w.window)) ShowWindow(w.window,SW_SHOWNOACTIVATE);
        SetWindowPos(w.window,before?before:HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    }
    if(settings) SetWindowTextW(child(Status),status.c_str());
}
void App::save() {
    if(!configWritable) return;
    try {saveConfig(root/L"settings.json",config);} catch(const std::exception& e) {
        warning=L"Could not save settings: "+wide(e.what());
        if(settings) SetWindowTextW(child(Status),warning.c_str());
    }
}
WidgetConfig App::effectiveWidget(const Widget& widget) const {
    const auto& cfg=config.widgets[widget.index];if(!config.wallpaperMatch[config.preset]||cfg.customColors)return cfg;
    auto image=std::find_if(wallpaper.result.images.begin(),wallpaper.result.images.end(),[&](const auto& i){return i.device==cfg.monitor;});
    if(image==wallpaper.result.images.end())return cfg;RECT r{};GetWindowRect(widget.window,&r);return matchWallpaper(cfg,r,*image);
}
void App::requestWallpaper(){
    KillTimer(control,WallpaperTimer);
    if(paused)return;
    if(!config.wallpaperMatch[config.preset]){wallpaper.cancel();if(wallpaper.busy())SetTimer(control,WallpaperPoll,100,nullptr);return;}
    wallpaper.request(monitors);SetTimer(control,WallpaperPoll,100,nullptr);SetTimer(control,WallpaperTimer,60000,nullptr);
}
void App::completeFocus(){
    bool changed=finishFocus(config.focus,utcNow());
    if(config.focus.pending&&!paused){
        if(!isolated){tray.uFlags=NIF_INFO;tray.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND;wcscpy_s(tray.szInfoTitle,L"panes — session complete");wcscpy_s(tray.szInfo,config.focus.isBreak?L"Your break is complete. Start the next focus when ready.":L"Focus complete. Take a quiet break when ready.");Shell_NotifyIconW(NIM_MODIFY,&tray);tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;}
        config.focus.pending=false;changed=true;
    }
    if(changed){widgets[3].dirty=true;save();if(settings)SetWindowTextW(child(FocusStart),L"Start");}
}
void App::focusCommand(int action){
    focusAction(config.focus,action,utcNow());widgets[3].dirty=true;render();schedule();save();
    if(settings)SetWindowTextW(child(FocusStart),config.focus.running?L"Pause":L"Start");
}
void App::render(bool force) {
    if(paused || !config.shown) return;
    SYSTEMTIME now{};GetLocalTime(&now);
    for(auto& w:widgets) {
        const auto& cfg=config.widgets[w.index];if(!cfg.visible) continue;
        bool dateChanged=now.wYear!=w.last.wYear || now.wMonth!=w.last.wMonth || now.wDay!=w.last.wDay;
        bool minuteChanged=dateChanged || now.wHour!=w.last.wHour || now.wMinute!=w.last.wMinute;
        bool secondChanged=minuteChanged||now.wSecond!=w.last.wSecond;
        bool changed=cfg.kind==Kind::Music?false:cfg.kind==Kind::Date||cfg.kind==Kind::Calendar?dateChanged:
            cfg.kind==Kind::Clock?(config.seconds?secondChanged:minuteChanged):
            cfg.kind==Kind::Focus?(config.focus.running&&secondChanged):minuteChanged;
        if(force||w.dirty||changed){auto effective=effectiveWidget(w);w.surface->draw(w.window,effective,config,now,!config.locked,true,0,cfg.kind==Kind::Music?&mediaState:nullptr);w.last=now;w.dirty=false;}
    }
}
void App::schedule() {
    KillTimer(control,TickTimer);if(paused)return;
    SYSTEMTIME now{};GetLocalTime(&now);UINT interval=3600000;bool needed=false;
    if(config.shown)for(const auto& w:config.widgets)if(w.visible){if(w.kind==Kind::Music)continue;needed=true;
        if(w.kind==Kind::Clock||w.kind==Kind::WorldClock||w.kind==Kind::Countdown||w.kind==Kind::Battery)interval=std::min(interval,nextTick(now,w.kind==Kind::Clock&&config.seconds));
        else if(w.kind==Kind::Focus&&config.focus.running)interval=std::min(interval,nextTick(now,true));
        else {UINT midnight=((23-now.wHour)*3600u+(59-now.wMinute)*60u+60-now.wSecond)*1000u-now.wMilliseconds;interval=std::min(interval,midnight);}
    }
    if(config.focus.running){needed=true;auto left=focusRemaining(config.focus,utcNow());interval=std::min(interval,static_cast<UINT>(std::min(3600000LL,std::max(1LL,left*1000))));}
    if(needed)SetTimer(control,TickTimer,std::max(1u,interval),nullptr);
}
void App::updateMedia(){media.setEnabled(!paused&&config.shown&&config.widgets[static_cast<int>(Kind::Music)].visible);}
void App::addTray() {
    tray={};tray.cbSize=sizeof(tray);tray.hWnd=control;tray.uID=1;
    tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;tray.uCallbackMessage=TrayMessage;
    tray.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));wcscpy_s(tray.szTip,L"panes — double-click for settings");
    Shell_NotifyIconW(NIM_ADD,&tray);tray.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&tray);
}
void App::trayMenu() {
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,MenuSettings,L"Settings…");AppendMenuW(menu,MF_STRING,MenuPalette,L"Color tools…");
    HMENU focus=CreatePopupMenu();AppendMenuW(focus,MF_STRING,MenuFocus,config.focus.running?L"Pause":L"Start");AppendMenuW(focus,MF_STRING,MenuFocusReset,L"Reset session");AppendMenuW(focus,MF_STRING,MenuFocusNext,L"Next session");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(focus),L"Focus timer");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING|(!config.locked?MF_CHECKED:0),MenuLock,L"Edit mode (move widgets)");
    AppendMenuW(menu,MF_STRING|(config.shown?MF_CHECKED:0),MenuShow,L"Show widgets");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,MenuExit,L"Exit");
    POINT point{};GetCursorPos(&point);SetForegroundWindow(control);
    int id=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,control,nullptr);
    DestroyMenu(menu);PostMessageW(control,WM_NULL,0,0);if(id)command(id);
}
void App::command(int id) {
    switch(id) {
    case MenuSettings:showSettings();break;
    case MenuPalette:showSettings();page=2;scroll=0;layoutSettings();break;
    case MenuFocus:focusCommand(0);break;
    case MenuFocusReset:focusCommand(1);break;
    case MenuFocusNext:focusCommand(2);break;
    case MenuLock:config.locked=!config.locked;draft.locked=config.locked;render(true);repair();save();if(settings){SetWindowTextW(child(EditMode),config.locked?L"Edit mode":L"Finish editing");SetWindowTextW(child(Status),config.locked?L"Layout saved. Music playback controls remain interactive.":L"Edit mode is on. Drag widgets on your desktop, then choose Finish editing.");}break;
    case MenuShow:config.shown=!config.shown;updateMedia();render(true);repair();schedule();save();break;
    case MenuExit:PostMessageW(control,WM_CLOSE,0,0);break;
    }
}
void App::init() {
    currentApp=this;
    const auto path=root/L"settings.json";firstRun=!std::filesystem::exists(path);
    auto loaded=loadConfig(path);config=loaded.value;warning=loaded.warning;configWritable=loaded.writable;
    if(prototype)for(int i=1;i<WidgetCount;++i)config.widgets[i].visible=false;
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpfnWndProc=ControlProc;wc.lpszClassName=ControlClass;
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));RegisterClassExW(&wc);
    wc.lpfnWndProc=WidgetProc;wc.lpszClassName=WidgetClass;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassExW(&wc);
    wc.lpfnWndProc=SettingsProc;wc.lpszClassName=SettingsClass;wc.hbrBackground=static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));RegisterClassExW(&wc);
    wc.lpfnWndProc=PickerProc;wc.lpszClassName=PickerClass;wc.hCursor=LoadCursorW(nullptr,IDC_CROSS);wc.hbrBackground=nullptr;RegisterClassExW(&wc);
    control=CreateWindowExW(WS_EX_TOOLWINDOW,ControlClass,controllerTitle.c_str(),WS_POPUP,0,0,0,0,nullptr,nullptr,instance,this);
    if(!control) throw std::runtime_error("Cannot create app controller");
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");WTSRegisterSessionNotification(control,NOTIFY_FOR_THIS_SESSION);
    graphics=std::make_unique<Graphics>();loadMonitors();
    for(int i=0;i<WidgetCount;++i) {
        auto& w=widgets[i];w.app=this;w.index=i;w.surface=std::make_unique<Surface>(*graphics);
        w.window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT,
            WidgetClass,kindName(static_cast<Kind>(i)),WS_POPUP,0,0,10,10,nullptr,nullptr,instance,&w);
        if(!w.window) throw std::runtime_error("Cannot create widget");place(w);
    }
    render(true);repair();addTray();completeFocus();schedule();requestWallpaper();media.start(control);updateMedia();
    hooks.push_back(SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,eventProc,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS));
    hooks.push_back(SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART,EVENT_SYSTEM_MINIMIZEEND,nullptr,eventProc,0,0,WINEVENT_OUTOFCONTEXT));
    hooks.push_back(SetWinEventHook(EVENT_OBJECT_DESTROY,EVENT_OBJECT_DESTROY,nullptr,eventProc,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS));
    save();startMs=lastCpuMs=GetTickCount64();lastCpu=cpuTime();
    if(diagnostics) SetTimer(control,DiagnosticTimer,1000,nullptr);
    if(previews) for(int i=0;i<3;++i) if(widgets[i].surface->size().cx) widgets[i].surface->saveBmp(root/(L"preview-"+std::to_wstring(i)+L".bmp"));
    if(applicationTest) runChecks();
    if(!warning.empty() && !diagnostics) MessageBoxW(nullptr,warning.c_str(),AppName,MB_OK|MB_ICONWARNING);
}
LRESULT CALLBACK App::WidgetProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* w=reinterpret_cast<Widget*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {w=static_cast<Widget*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(w));}
    if(!w) return DefWindowProcW(hwnd,msg,wp,lp);
    auto& a=*w->app;
    auto mediaHit=[&](POINT point){
        if(a.config.widgets[w->index].kind!=Kind::Music)return -1;
        const float factor=GetDpiForWindow(hwnd)/96.f*a.config.widgets[w->index].scale/100.f;
        const float x=point.x/factor,y=point.y/factor;
        return y>=115&&y<=172&&x>=135&&x<=325?(x<195?0:x<270?1:2):-1;
    };
    switch(msg) {
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT paint{};BeginPaint(hwnd,&paint);EndPaint(hwnd,&paint);return 0;}
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_SYSCOMMAND:if((wp&0xfff0)==SC_MINIMIZE)return 0;break;
    case WM_NCHITTEST:if(a.config.locked&&a.config.widgets[w->index].kind==Kind::Music){POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(hwnd,&point);return mediaHit(point)>=0?HTCLIENT:HTTRANSPARENT;}break;
    case WM_LBUTTONDOWN:{
        int action=mediaHit({GET_X_LPARAM(lp),GET_Y_LPARAM(lp)});
        if(action>=0){a.media.command(static_cast<MediaCommand>(action));return 0;}
        if(!a.config.locked){
        w->dragging=true;GetCursorPos(&w->mouse);RECT r{};GetWindowRect(hwnd,&r);w->origin={r.left,r.top};SetCapture(hwnd);
        }return 0;
    }
    case WM_MOUSEMOVE:if(w->dragging) {POINT p{};GetCursorPos(&p);SetWindowPos(hwnd,nullptr,w->origin.x+p.x-w->mouse.x,w->origin.y+p.y-w->mouse.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}return 0;
    case WM_LBUTTONUP:if(w->dragging) {w->dragging=false;ReleaseCapture();a.remember(*w);}return 0;
    case WM_CAPTURECHANGED:if(w->dragging) {w->dragging=false;a.remember(*w);}return 0;
    case WM_RBUTTONUP:a.trayMenu();return 0;
    case WM_DPICHANGED: {
        auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOACTIVATE|SWP_NOZORDER);
        w->dirty=true;PostMessageW(a.control,WM_TIMER,TickTimer,0);return 0;
    }
    case WM_CLOSE:return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
LRESULT CALLBACK App::ControlProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* a=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}
    if(!a)return DefWindowProcW(hwnd,msg,wp,lp);
    try {
        if(a->taskbarCreated && msg==a->taskbarCreated) {a->addTray();a->retries=0;a->repair();return 0;}
        switch(msg) {
        case TrayMessage:
            if(LOWORD(lp)==WM_CONTEXTMENU || LOWORD(lp)==WM_RBUTTONUP)a->trayMenu();
            else if(LOWORD(lp)==WM_LBUTTONDBLCLK || LOWORD(lp)==NIN_KEYSELECT)a->showSettings();return 0;
        case ActivateMessage:a->showSettings();return 0;
        case PaletteMessage:a->showSettings();a->page=2;a->scroll=0;a->layoutSettings();return 0;
        case RepairMessage:a->repair();return 0;
        case MediaMessage:a->mediaState=a->media.snapshot();a->widgets[static_cast<int>(Kind::Music)].dirty=true;a->render();return 0;
        case WM_TIMER:
            if(wp==TickTimer){KillTimer(hwnd,TickTimer);a->completeFocus();a->render();a->schedule();}
            else if(wp==RetryTimer){KillTimer(hwnd,RetryTimer);a->repair();}
            else if(wp==DiagnosticTimer)a->sample();
            else if(wp==WallpaperTimer)a->requestWallpaper();
            else if(wp==WallpaperPoll){if(a->wallpaper.poll()){a->render(true);if(a->settings&&!a->wallpaper.result.warning.empty())SetWindowTextW(a->child(Status),a->wallpaper.result.warning.c_str());}if(!a->wallpaper.busy())KillTimer(hwnd,WallpaperPoll);}
            return 0;
        case WM_DISPLAYCHANGE:case WM_SETTINGCHANGE:
            a->loadMonitors();for(auto& w:a->widgets)if(!w.dragging)a->place(w);a->render(true);a->repair();a->requestWallpaper();a->save();return 0;
        case WM_TIMECHANGE:a->completeFocus();a->render(true);a->schedule();return 0;
        case WM_WTSSESSION_CHANGE:
            if(wp==WTS_SESSION_LOCK){a->paused=true;a->updateMedia();KillTimer(a->control,WallpaperTimer);a->schedule();a->repair();}
            else if(wp==WTS_SESSION_UNLOCK){a->paused=false;a->updateMedia();a->completeFocus();a->requestWallpaper();a->render(true);a->repair();a->schedule();}return 0;
        case WM_POWERBROADCAST:
            if(wp==PBT_APMPOWERSTATUSCHANGE){a->widgets[5].dirty=true;a->render();}
            if(wp==PBT_APMSUSPEND){a->paused=true;a->updateMedia();a->schedule();}
            else if(wp==PBT_APMRESUMEAUTOMATIC || wp==PBT_APMRESUMESUSPEND){a->paused=false;a->updateMedia();a->completeFocus();a->requestWallpaper();a->render(true);a->repair();a->schedule();}return TRUE;
        case WM_QUERYENDSESSION:a->save();return TRUE;
        case WM_CLOSE:a->quitting=true;a->save();if(a->diagnostics)a->finishDiagnostics();PostQuitMessage(0);return 0;
        }
    } catch(const std::exception& e) {
        a->warning=wide(e.what());
        if(a->settings)SetWindowTextW(a->child(Status),a->warning.c_str());
        if(a->diagnostics){std::ofstream(a->root/L"error.txt")<<e.what();PostQuitMessage(1);}
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
void App::sample() {
    const auto now=GetTickCount64(),cpu=cpuTime();
    SYSTEM_INFO info{};GetSystemInfo(&info);
    double percent=(cpu-lastCpu)/10000.0/std::max<ULONGLONG>(1,now-lastCpuMs)/info.dwNumberOfProcessors*100.0;
    lastCpu=cpu;lastCpuMs=now;
    PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
    GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));
    if(now-startMs>=5000) {
        ++samples;cpuSum+=percent;maxCpu=std::max(maxCpu,percent);
        peakPrivate=std::max(peakPrivate,static_cast<size_t>(memory.PrivateUsage));peakWorking=std::max(peakWorking,static_cast<size_t>(memory.WorkingSetSize));
    }
    if(duration>0 && now-startMs>=static_cast<ULONGLONG>(duration)*1000) PostMessageW(control,WM_CLOSE,0,0);
}
void App::finishDiagnostics() {
    nlohmann::json j{{"elapsedSeconds",(GetTickCount64()-startMs)/1000.0},{"samplesAfterWarmup",samples},
        {"averageTotalCpuPercent",samples?cpuSum/samples:0},{"peakTotalCpuPercent",maxCpu},
        {"peakPrivateMiB",peakPrivate/1048576.0},{"peakWorkingSetMiB",peakWorking/1048576.0},
        {"desktopHostFound",desktopHost!=nullptr},{"settingsWritable",configWritable},
        {"wallpaperDecodeCount",wallpaper.totalDecoded},{"wallpaperExtractionPeakPrivateMiB",wallpaper.peakPrivate/1048576.0},{"secondsEnabled",config.seconds},{"warning",utf8(warning)}};
    j["widgets"]=nlohmann::json::array();
    for(auto& w:widgets) {
        RECT r{};GetWindowRect(w.window,&r);
        j["widgets"].push_back({{"name",utf8(kindName(config.widgets[w.index].kind))},{"visible",IsWindowVisible(w.window)!=FALSE},
            {"clickThrough",(GetWindowLongPtrW(w.window,GWL_EXSTYLE)&WS_EX_TRANSPARENT)!=0},
            {"topmost",(GetWindowLongPtrW(w.window,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0},
            {"dpi",GetDpiForWindow(w.window)},{"x",r.left},{"y",r.top},{"width",r.right-r.left},{"height",r.bottom-r.top}});
    }
    std::ofstream(root/L"diagnostics.json")<<j.dump(2)<<'\n';
}
void App::runChecks() {
    nlohmann::json report;report["checks"]=nlohmann::json::array();
    auto expect=[&](bool pass,const char* name){report["checks"].push_back({{"name",name},{"passed",pass}});if(!pass)throw std::runtime_error(name);};
    const auto original=config;
    try {
        expect(desktopHost && IsWindow(desktopHost),"Discovered Explorer desktop host");
        for(auto& w:widgets) {
            expect((IsWindowVisible(w.window)!=FALSE)==config.widgets[w.index].visible,"Widget visibility follows configuration");
            auto ex=GetWindowLongPtrW(w.window,GWL_EXSTYLE);
            expect((ex&WS_EX_TOPMOST)==0 && (ex&WS_EX_NOACTIVATE)!=0 && (ex&WS_EX_TOOLWINDOW)!=0,"Nonactivating non-topmost tool window");
            expect(config.widgets[w.index].kind==Kind::Music?(ex&WS_EX_TRANSPARENT)==0:(ex&WS_EX_TRANSPARENT)!=0,"Fixed layouts keep Music interactive and other widgets click-through");
        }
        auto foreground=GetForegroundWindow();repair();expect(GetForegroundWindow()==foreground,"Repair does not steal focus");
        command(MenuLock);expect(!config.locked && !(GetWindowLongPtrW(widgets[0].window,GWL_EXSTYLE)&WS_EX_TRANSPARENT),"Unlock permits dragging and interaction");
        command(MenuLock);command(MenuShow);expect(!IsWindowVisible(widgets[0].window),"Hide command hides widgets");
        command(MenuShow);expect(IsWindowVisible(widgets[0].window)!=FALSE,"Show command restores widgets");
        SendMessageW(control,WM_WTSSESSION_CHANGE,WTS_SESSION_LOCK,0);
        expect(paused&&!IsWindowVisible(widgets[0].window),"Session lock pauses and hides widgets");
        SendMessageW(control,WM_WTSSESSION_CHANGE,WTS_SESSION_UNLOCK,0);
        expect(!paused&&IsWindowVisible(widgets[0].window),"Session unlock refreshes widgets");
        SendMessageW(control,WM_POWERBROADCAST,PBT_APMSUSPEND,0);expect(paused,"Suspend notification pauses widget timers");
        SendMessageW(control,WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC,0);expect(!paused,"Resume notification restarts widget timers");
        auto& wc=config.widgets[0];wc.monitor=L"NONEXISTENT-DISPLAY";wc.x=50000;wc.y=50000;place(widgets[0]);
        RECT rect{};GetWindowRect(widgets[0].window,&rect);auto work=monitors.front().work;
        expect(wc.monitor==monitors.front().device && rect.left>=work.left && rect.top>=work.top && rect.left<work.right,"Disconnected monitor restores to primary display");
        showSettings();
        command(MenuLock);expect(!config.locked&&applySettings()&&!config.locked,"Applying Settings retains Edit mode");command(MenuLock);
        auto& musicWidget=widgets[static_cast<int>(Kind::Music)];RECT musicRect{};GetWindowRect(musicWidget.window,&musicRect);
        const int musicDpi=GetDpiForWindow(musicWidget.window);
        auto musicPoint=MAKELPARAM(musicRect.left+MulDiv(231,musicDpi,96),musicRect.top+MulDiv(137,musicDpi,96));
        expect(SendMessageW(musicWidget.window,WM_NCHITTEST,0,musicPoint)==HTCLIENT,"Music playback controls receive clicks in a fixed layout");
        SetWindowTextW(child(Scale),L"0");expect(!applySettings(),"Settings reject out-of-range scale");
        SetWindowTextW(child(Scale),L"100");SetWindowTextW(child(Color),L"#ZZZZZZ");expect(!applySettings(),"Settings reject invalid color");
        SetWindowTextW(child(Color),L"#F2F4F1");SetWindowTextW(child(Font),L"panes missing font");
        expect(applySettings() && config.widgets[0].font==L"Segoe UI","Missing font falls back to Segoe UI");
        const DWORD handles=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        for(int i=0;i<80;++i) {
            SetWindowTextW(child(Scale),i%2?L"100":L"115");
            SendMessageW(child(Preset),CB_SETCURSEL,i%2,0);
            SendMessageW(child(Seconds),BM_SETCHECK,i%2?BST_CHECKED:BST_UNCHECKED,0);
            if(!applySettings())throw std::runtime_error("Repeated settings application failed");
        }
        expect(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=handles+4,"Repeated editing does not leak GDI handles");
        expect(config.seconds && config.preset==1,"Compact preset and seconds are configurable");
        for(int i=0;i<3;++i)widgets[i].surface->saveBmp(root/(L"compact-"+std::to_wstring(i)+L".bmp"));
        SendMessageW(control,WM_TIMECHANGE,0,0);expect(widgets[0].last.wYear>2020,"System time change refreshes display");
        report["passed"]=true;
    }catch(const std::exception& e){report["passed"]=false;report["error"]=e.what();}
    config=original;paused=false;
    for(auto& w:widgets){w.surface->clearFormats();place(w);}
    if(settings)DestroyWindow(settings);render(true);repair();schedule();save();
    std::ofstream(root/L"application-tests.json")<<report.dump(2)<<'\n';
}
static std::wstring controlText(HWND w) {
    int length=GetWindowTextLengthW(w);std::wstring result(length+1,L'\0');
    GetWindowTextW(w,result.data(),length+1);result.resize(length);return result;
}
bool App::startupEnabled() const {
    HKEY key{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return false;
    DWORD type{},bytes{};bool exists=RegQueryValueExW(key,L"panes",nullptr,&type,nullptr,&bytes)==ERROR_SUCCESS && type==REG_SZ;
    RegCloseKey(key);return exists;
}
bool App::setStartup(bool on) {
    if(isolated)return true;
    HKEY key{};if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    LSTATUS result{};
    if(on) {
        wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);std::wstring command=L"\""+std::wstring(exe)+L"\"";
        result=RegSetValueExW(key,L"panes",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),static_cast<DWORD>((command.size()+1)*sizeof(wchar_t)));
    }else {result=RegDeleteValueW(key,L"panes");if(result==ERROR_FILE_NOT_FOUND)result=ERROR_SUCCESS;}
    RegCloseKey(key);return result==ERROR_SUCCESS;
}
#include "settings.inc"

}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int) {
    using namespace dw;
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int result=0;
    try {
        INITCOMMONCONTROLSEX init{sizeof(init),ICC_STANDARD_CLASSES};InitCommonControlsEx(&init);
        App app(instance);
        int count{};LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);
        bool settings=false,palette=false,quit=false;
        for(int i=1;i<count;++i) {
            std::wstring arg=args[i];
            if(arg==L"--test-root" && i+1<count){app.root=std::filesystem::absolute(args[++i]);app.isolated=true;}
            else if(arg==L"--diagnostic")app.diagnostics=true;
            else if(arg==L"--export-previews")app.previews=true;
            else if(arg==L"--prototype")app.prototype=true;
            else if(arg==L"--application-test"){app.applicationTest=true;app.diagnostics=true;app.duration=12;}
            else if(arg==L"--settings")settings=true;
            else if(arg==L"--palette")palette=true;
            else if(arg==L"--quit")quit=true;
            else if(arg.rfind(L"--duration=",0)==0)app.duration=std::clamp(std::stoi(arg.substr(11)),1,86400);
            else throw std::runtime_error("Unknown argument");
        }
        LocalFree(args);
        if((app.diagnostics||app.previews||app.prototype||app.applicationTest) && !app.isolated) throw std::runtime_error("Diagnostics require --test-root <separate folder>");
        if(app.root.empty())app.root=defaultRoot();
        app.root=std::filesystem::weakly_canonical(app.root);
        if(app.isolated && _wcsicmp(std::filesystem::weakly_canonical(app.root).c_str(),std::filesystem::weakly_canonical(defaultRoot()).c_str())==0)
            throw std::runtime_error("Test root must differ from the normal settings folder");
        std::filesystem::create_directories(app.root);
        // Tests never activate or stop the user's normal instance.
        std::wstring mutexName=L"Local\\panes.v1";
        if(app.isolated) {
            auto suffix=L"."+std::to_wstring(std::hash<std::wstring>{}(app.root.wstring()));
            mutexName+=suffix;app.controllerTitle+=suffix;
        }
        app.mutex=CreateMutexW(nullptr,FALSE,mutexName.c_str());
        if(!app.mutex)throw std::runtime_error("Cannot create application mutex");
        if(GetLastError()==ERROR_ALREADY_EXISTS) {
            HWND running=FindWindowW(ControlClass,app.controllerTitle.c_str());
            if(running)PostMessageW(running,quit?WM_CLOSE:palette?PaletteMessage:ActivateMessage,0,0);
        } else if(!quit) {
            // Isolated settings must never mean several overlapping desktop sets.
            // Across all normal/test roots, only one process may present widgets.
            app.desktopMutex=CreateMutexW(nullptr,FALSE,L"Local\\panes.DesktopPresentation.v1");
            if(!app.desktopMutex)throw std::runtime_error("Cannot create desktop presentation mutex");
            if(GetLastError()==ERROR_ALREADY_EXISTS) {
                std::ofstream(app.root/L"instance-conflict.txt")<<"Another panes instance is displaying widgets. Close it before starting this test or app.\n";
                if(!app.diagnostics)MessageBoxW(nullptr,L"panes is already running in another configuration. Close that copy from its tray menu first.",AppName,MB_OK|MB_ICONINFORMATION);
                if(SUCCEEDED(com))CoUninitialize();
                return 2;
            }
            app.init();if(palette){app.showSettings();app.page=2;app.layoutSettings();}
            if(settings || (app.firstRun && !app.diagnostics && !app.prototype))app.showSettings();
            MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0) {
                if(!app.picker && app.settings && IsDialogMessageW(app.settings,&msg))continue;
                TranslateMessage(&msg);DispatchMessageW(&msg);
            }
            result=static_cast<int>(msg.wParam);
        }
    } catch(const std::exception& e) {MessageBoxW(nullptr,wide(e.what()).c_str(),L"panes — startup error",MB_OK|MB_ICONERROR);result=1;}
    if(SUCCEEDED(com))CoUninitialize();
    return result;
}
