#include "render.hpp"
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
constexpr UINT TrayMessage=WM_APP+1, RepairMessage=WM_APP+2, ActivateMessage=WM_APP+3;
constexpr UINT_PTR TickTimer=1, RetryTimer=2, DiagnosticTimer=3;
constexpr int MenuSettings=101, MenuLock=102, MenuShow=103, MenuExit=104;
constexpr wchar_t WidgetClass[]=L"panes.Widget.v1", ControlClass[]=L"panes.Controller.v1", SettingsClass[]=L"panes.Settings.v1";
class App;
static App* currentApp{};
struct Widget {
    App* app{}; int index{}; HWND window{}; std::unique_ptr<Surface> surface;
    SYSTEMTIME last{}; bool dirty=true, dragging=false; POINT mouse{}, origin{};
};
enum ControlId { SelectWidget=201, Preset, Font, Scale, Opacity, Color, Accent, Visible, Hour24, Seconds, Startup, Apply, Reset, Done, Status };
class App {
public:
    HINSTANCE instance{}; HWND control{},settings{},desktopHost{}; UINT taskbarCreated{};
    HANDLE mutex{},desktopMutex{}; bool isolated=false,configWritable=true,paused=false,quitting=false,repairPending=false;
    bool diagnostics=false,previews=false,firstRun=false,prototype=false,applicationTest=false;
    int duration=0,retries=0,selected=0; ULONGLONG startMs{},lastCpu{},lastCpuMs{};
    double cpuSum=0,maxCpu=0; size_t peakPrivate=0,peakWorking=0; int samples=0;
    std::filesystem::path root; std::wstring controllerTitle=L"panes controller",warning,status=L"Desktop mode · offline";
    Config config; std::unique_ptr<Graphics> graphics;
    std::array<Widget,3> widgets;
    std::vector<Monitor> monitors;
    std::vector<HWINEVENTHOOK> hooks;
    NOTIFYICONDATAW tray{}; HFONT uiFont{},titleFont{}; HBRUSH background{};
    explicit App(HINSTANCE h):instance(h){}
    ~App();
    void init(); void save(); void loadMonitors(); void place(Widget& w, bool defaults=false);
    void render(bool force=false); void schedule(); void repair(); void requestRepair();
    void addTray(); void trayMenu(); void command(int id); void remember(Widget& w);
    void showSettings(); void makeSettingsControls(); void populateSettings(); bool applySettings();
    HWND child(int id) const {return GetDlgItem(settings,id);}
    void sample(); void finishDiagnostics(); bool startupEnabled() const; bool setStartup(bool on);
    void runChecks();
    static LRESULT CALLBACK ControlProc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK WidgetProc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK SettingsProc(HWND,UINT,WPARAM,LPARAM);
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
    for(auto hook:hooks) if(hook) UnhookWinEvent(hook);
    if(control) { WTSUnRegisterSessionNotification(control); Shell_NotifyIconW(NIM_DELETE,&tray); }
    for(auto& w:widgets) { if(w.window) DestroyWindow(w.window); w.surface.reset(); }
    if(settings) DestroyWindow(settings);
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
    auto it=std::find_if(monitors.begin(),monitors.end(),[&](const Monitor& m){return m.device==cfg.monitor;});
    const bool missing=it==monitors.end();
    const auto& monitor=missing?monitors.front():*it;
    SIZE logical=logicalSize(cfg.kind,config.preset,cfg.scale,config.seconds);
    SIZE size{MulDiv(logical.cx,monitor.dpi,96),MulDiv(logical.cy,monitor.dpi,96)};
    POINT pos{monitor.work.left+MulDiv(cfg.x,monitor.dpi,96), monitor.work.top+MulDiv(cfg.y,monitor.dpi,96)};
    if(!cfg.positioned || defaults) {
        int width=monitor.work.right-monitor.work.left,height=monitor.work.bottom-monitor.work.top;
        pos=cfg.kind==Kind::Calendar ? POINT{monitor.work.right-size.cx-40,monitor.work.top+height/3}
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
        ex=config.locked ? (ex|WS_EX_TRANSPARENT) : (ex&~WS_EX_TRANSPARENT);
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
void App::render(bool force) {
    if(paused || !config.shown) return;
    SYSTEMTIME now{};GetLocalTime(&now);
    for(auto& w:widgets) {
        const auto& cfg=config.widgets[w.index];if(!cfg.visible) continue;
        bool dateChanged=now.wYear!=w.last.wYear || now.wMonth!=w.last.wMonth || now.wDay!=w.last.wDay;
        bool timeChanged=dateChanged || now.wHour!=w.last.wHour || now.wMinute!=w.last.wMinute || (config.seconds && now.wSecond!=w.last.wSecond);
        if(force || w.dirty || (cfg.kind==Kind::Clock?timeChanged:dateChanged)) {
            w.surface->draw(w.window,cfg,config,now,!config.locked);w.last=now;w.dirty=false;
        }
    }
}
void App::schedule() {
    KillTimer(control,TickTimer);
    if(paused || !config.shown) return;
    bool any=false;for(const auto& w:config.widgets) any=any || w.visible;if(!any)return;
    SYSTEMTIME now{};GetLocalTime(&now);
    UINT interval=nextTick(now,config.seconds);
    if(!config.widgets[0].visible) {
        unsigned midnight=((23-now.wHour)*3600u+(59-now.wMinute)*60u+60-now.wSecond)*1000u-now.wMilliseconds;
        interval=std::min(midnight,3600000u); // Recheck timezone/DST without drawing unchanged dates.
    }
    SetTimer(control,TickTimer,std::max(1u,interval),nullptr);
}
void App::addTray() {
    tray={};tray.cbSize=sizeof(tray);tray.hWnd=control;tray.uID=1;
    tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;tray.uCallbackMessage=TrayMessage;
    tray.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));wcscpy_s(tray.szTip,L"panes — double-click for settings");
    Shell_NotifyIconW(NIM_ADD,&tray);tray.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&tray);
}
void App::trayMenu() {
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,MenuSettings,L"Settings…");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING|(config.locked?MF_CHECKED:0),MenuLock,L"Lock widgets (click-through)");
    AppendMenuW(menu,MF_STRING|(config.shown?MF_CHECKED:0),MenuShow,L"Show widgets");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,MenuExit,L"Exit");
    POINT point{};GetCursorPos(&point);SetForegroundWindow(control);
    int id=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,control,nullptr);
    DestroyMenu(menu);PostMessageW(control,WM_NULL,0,0);if(id)command(id);
}
void App::command(int id) {
    switch(id) {
    case MenuSettings:showSettings();break;
    case MenuLock:config.locked=!config.locked;render(true);repair();save();break;
    case MenuShow:config.shown=!config.shown;render(true);repair();schedule();save();break;
    case MenuExit:PostMessageW(control,WM_CLOSE,0,0);break;
    }
}
void App::init() {
    currentApp=this;
    const auto path=root/L"settings.json";firstRun=!std::filesystem::exists(path);
    auto loaded=loadConfig(path);config=loaded.value;warning=loaded.warning;configWritable=loaded.writable;
    if(prototype) {config.widgets[1].visible=false;config.widgets[2].visible=false;}
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpfnWndProc=ControlProc;wc.lpszClassName=ControlClass;
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));RegisterClassExW(&wc);
    wc.lpfnWndProc=WidgetProc;wc.lpszClassName=WidgetClass;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassExW(&wc);
    wc.lpfnWndProc=SettingsProc;wc.lpszClassName=SettingsClass;wc.hbrBackground=static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));RegisterClassExW(&wc);
    control=CreateWindowExW(WS_EX_TOOLWINDOW,ControlClass,controllerTitle.c_str(),WS_POPUP,0,0,0,0,nullptr,nullptr,instance,this);
    if(!control) throw std::runtime_error("Cannot create app controller");
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");WTSRegisterSessionNotification(control,NOTIFY_FOR_THIS_SESSION);
    graphics=std::make_unique<Graphics>();loadMonitors();
    for(int i=0;i<3;++i) {
        auto& w=widgets[i];w.app=this;w.index=i;w.surface=std::make_unique<Surface>(*graphics);
        w.window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT,
            WidgetClass,kindName(static_cast<Kind>(i)),WS_POPUP,0,0,10,10,nullptr,nullptr,instance,&w);
        if(!w.window) throw std::runtime_error("Cannot create widget");place(w);
    }
    render(true);repair();schedule();addTray();
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
    switch(msg) {
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT paint{};BeginPaint(hwnd,&paint);EndPaint(hwnd,&paint);return 0;}
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_SYSCOMMAND:if((wp&0xfff0)==SC_MINIMIZE)return 0;break;
    case WM_LBUTTONDOWN:if(!a.config.locked) {w->dragging=true;GetCursorPos(&w->mouse);RECT r{};GetWindowRect(hwnd,&r);w->origin={r.left,r.top};SetCapture(hwnd);}return 0;
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
        case RepairMessage:a->repair();return 0;
        case WM_TIMER:
            if(wp==TickTimer){KillTimer(hwnd,TickTimer);a->render();a->schedule();}
            else if(wp==RetryTimer){KillTimer(hwnd,RetryTimer);a->repair();}
            else if(wp==DiagnosticTimer)a->sample();return 0;
        case WM_DISPLAYCHANGE:case WM_SETTINGCHANGE:
            a->loadMonitors();for(auto& w:a->widgets)if(!w.dragging)a->place(w);a->render(true);a->repair();a->save();return 0;
        case WM_TIMECHANGE:a->render(true);a->schedule();return 0;
        case WM_WTSSESSION_CHANGE:
            if(wp==WTS_SESSION_LOCK){a->paused=true;a->schedule();a->repair();}
            else if(wp==WTS_SESSION_UNLOCK){a->paused=false;a->render(true);a->repair();a->schedule();}return 0;
        case WM_POWERBROADCAST:
            if(wp==PBT_APMSUSPEND){a->paused=true;a->schedule();}
            else if(wp==PBT_APMRESUMEAUTOMATIC || wp==PBT_APMRESUMESUSPEND){a->paused=false;a->render(true);a->repair();a->schedule();}return TRUE;
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
        {"secondsEnabled",config.seconds},{"warning",utf8(warning)}};
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
            expect(IsWindowVisible(w.window)!=FALSE,"Widget visible on desktop");
            auto ex=GetWindowLongPtrW(w.window,GWL_EXSTYLE);
            expect((ex&WS_EX_TOPMOST)==0 && (ex&WS_EX_NOACTIVATE)!=0 && (ex&WS_EX_TOOLWINDOW)!=0,"Nonactivating non-topmost tool window");
            expect((ex&WS_EX_TRANSPARENT)!=0,"Locked widgets pass pointer input through");
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
void App::showSettings() {
    if(settings) {ShowWindow(settings,SW_RESTORE);SetForegroundWindow(settings);return;}
    const UINT dpi=monitors.empty()?96:monitors.front().dpi;
    auto rect=RECT{0,0,MulDiv(660,dpi,96),MulDiv(650,dpi,96)};
    AdjustWindowRectExForDpi(&rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0,dpi);
    settings=CreateWindowExW(0,SettingsClass,L"panes — Settings",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,this);
    if(!settings)throw std::runtime_error("Cannot open settings");
    makeSettingsControls();populateSettings();ShowWindow(settings,SW_SHOW);SetForegroundWindow(settings);
}
void App::makeSettingsControls() {
    const UINT dpi=GetDpiForWindow(settings);auto px=[&](int v){return MulDiv(v,dpi,96);};
    if(uiFont)DeleteObject(uiFont);if(titleFont)DeleteObject(titleFont);
    uiFont=CreateFontW(-px(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    titleFont=CreateFontW(-px(30),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    auto add=[&](const wchar_t* cls,const wchar_t* text,int id,int x,int y,int w,int h,DWORD style=0)->HWND {
        DWORD ex=(wcscmp(cls,L"EDIT")==0)?WS_EX_CLIENTEDGE:0;
        HWND child=CreateWindowExW(ex,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),settings,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
        SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(uiFont),TRUE);return child;
    };
    HWND heading=add(L"STATIC",L"Your desktop, quietly.",0,30,23,600,45);
    SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
    add(L"STATIC",L"Clock, date and calendar. Native, offline, and made to stay out of the way.",0,32,77,590,25);
    add(L"STATIC",L"EDIT WIDGET",0,32,120,200,22);
    HWND select=add(L"COMBOBOX",L"",SelectWidget,32,147,280,160,CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP);
    for(const auto& w:config.widgets)SendMessageW(select,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(kindName(w.kind)));
    add(L"BUTTON",L"Visible",Visible,342,147,180,27,BS_AUTOCHECKBOX|WS_TABSTOP);
    add(L"STATIC",L"Layout preset (all widgets)",0,32,192,280,22);
    HWND preset=add(L"COMBOBOX",L"",Preset,32,218,280,140,CBS_DROPDOWNLIST|WS_TABSTOP);
    SendMessageW(preset,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Thin · white & sage"));
    SendMessageW(preset,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Compact · digital"));
    add(L"STATIC",L"Font family",0,342,192,280,22);
    HWND font=add(L"COMBOBOX",L"",Font,342,218,280,200,CBS_DROPDOWN|CBS_AUTOHSCROLL|WS_VSCROLL|WS_TABSTOP);
    for(auto name:{L"Segoe UI Light",L"Segoe UI",L"Segoe UI Semilight",L"Bahnschrift",L"Consolas",L"Arial",L"Georgia"})
        SendMessageW(font,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
    SendMessageW(font,CB_LIMITTEXT,100,0);
    add(L"STATIC",L"Size % (50–200)",0,32,263,280,22);
    add(L"EDIT",L"",Scale,32,289,280,30,ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP);
    add(L"STATIC",L"Opacity % (10–100)",0,342,263,280,22);
    add(L"EDIT",L"",Opacity,342,289,280,30,ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP);
    add(L"STATIC",L"Text color",0,32,337,280,22);
    add(L"EDIT",L"",Color,32,363,280,30,ES_AUTOHSCROLL|WS_TABSTOP);
    add(L"STATIC",L"Accent color",0,342,337,280,22);
    add(L"EDIT",L"",Accent,342,363,280,30,ES_AUTOHSCROLL|WS_TABSTOP);
    SendMessageW(child(Color),EM_SETLIMITTEXT,7,0);SendMessageW(child(Accent),EM_SETLIMITTEXT,7,0);
    add(L"BUTTON",L"24-hour clock",Hour24,32,416,260,26,BS_AUTOCHECKBOX|WS_TABSTOP);
    add(L"BUTTON",L"Show seconds",Seconds,342,416,260,26,BS_AUTOCHECKBOX|WS_TABSTOP);
    add(L"BUTTON",L"Launch at sign-in",Startup,32,454,280,26,BS_AUTOCHECKBOX|WS_TABSTOP);
    add(L"STATIC",L"To move widgets: right-click the tray icon and turn off Lock widgets.\nLock again to let desktop clicks pass through.",0,32,500,590,46);
    add(L"STATIC",L"",Status,32,553,590,38);
    add(L"BUTTON",L"Reset layout",Reset,32,600,132,32,WS_TABSTOP);
    add(L"BUTTON",L"Apply",Apply,374,600,116,32,BS_DEFPUSHBUTTON|WS_TABSTOP);
    add(L"BUTTON",L"Done",Done,506,600,116,32,WS_TABSTOP);
    if(isolated){EnableWindow(child(Startup),FALSE);SetWindowTextW(child(Startup),L"Sign-in disabled in test mode");}
    BOOL dark=FALSE;DwmSetWindowAttribute(settings,20,&dark,sizeof(dark));
}
void App::populateSettings() {
    const auto& w=config.widgets[selected];
    SendMessageW(child(SelectWidget),CB_SETCURSEL,selected,0);SendMessageW(child(Preset),CB_SETCURSEL,config.preset,0);
    SetWindowTextW(child(Font),w.font.c_str());SetWindowTextW(child(Scale),std::to_wstring(w.scale).c_str());
    SetWindowTextW(child(Opacity),std::to_wstring(w.opacity).c_str());
    SetWindowTextW(child(Color),colorText(w.color).c_str());SetWindowTextW(child(Accent),colorText(w.accent).c_str());
    SendMessageW(child(Visible),BM_SETCHECK,w.visible?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(child(Hour24),BM_SETCHECK,config.hour24?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(child(Seconds),BM_SETCHECK,config.seconds?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(child(Startup),BM_SETCHECK,!isolated && startupEnabled()?BST_CHECKED:BST_UNCHECKED,0);
    SetWindowTextW(child(Status),warning.empty()?status.c_str():warning.c_str());
}
bool App::applySettings() {
    try {
        auto number=[&](int id,int min,int max) {
            auto s=controlText(child(id));
            if(s.empty() || s.size()>3 || s.find_first_not_of(L"0123456789")!=std::wstring::npos)throw std::runtime_error("Enter a valid size and opacity percentage.");
            int value=std::stoi(s);if(value<min||value>max)throw std::runtime_error("Size must be 50–200%; opacity must be 10–100%.");return value;
        };
        auto candidate=config;auto& w=candidate.widgets[selected];
        w.scale=number(Scale,50,200);w.opacity=number(Opacity,10,100);
        if(!parseColor(controlText(child(Color)),w.color)||!parseColor(controlText(child(Accent)),w.accent))
            throw std::runtime_error("Use a six-digit color such as #91B89D.");
        w.font=controlText(child(Font));if(w.font.empty()||w.font.size()>100)throw std::runtime_error("Choose a font family.");
        const auto requested=w.font;w.font=graphics->resolveFont(w.font);
        w.visible=SendMessageW(child(Visible),BM_GETCHECK,0,0)==BST_CHECKED;
        candidate.hour24=SendMessageW(child(Hour24),BM_GETCHECK,0,0)==BST_CHECKED;
        candidate.seconds=SendMessageW(child(Seconds),BM_GETCHECK,0,0)==BST_CHECKED;
        candidate.preset=static_cast<int>(SendMessageW(child(Preset),CB_GETCURSEL,0,0));
        bool startup=SendMessageW(child(Startup),BM_GETCHECK,0,0)==BST_CHECKED;
        if(!isolated && startup!=startupEnabled() && !setStartup(startup))throw std::runtime_error("Windows could not update launch at sign-in.");
        config=candidate;
        for(auto& widget:widgets){widget.surface->clearFormats();place(widget);}
        render(true);repair();schedule();save();
        std::wstring message=configWritable?L"Applied. Widgets stay behind your apps.":L"Applied for this session. Reset layout to recover the saved settings.";
        if(requested!=w.font)message=L"Font not installed. Using Segoe UI.";
        SetWindowTextW(child(Font),w.font.c_str());SetWindowTextW(child(Status),message.c_str());return true;
    } catch(const std::exception& e) {SetWindowTextW(child(Status),wide(e.what()).c_str());return false;}
}
LRESULT CALLBACK App::SettingsProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* a=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}
    if(!a)return DefWindowProcW(hwnd,msg,wp,lp);
    try {
        switch(msg) {
        case WM_COMMAND:
            if(LOWORD(wp)==SelectWidget && HIWORD(wp)==CBN_SELCHANGE) {
                const int next=static_cast<int>(SendMessageW(a->child(SelectWidget),CB_GETCURSEL,0,0));
                if(a->applySettings()){a->selected=next;a->populateSettings();}
                else SendMessageW(a->child(SelectWidget),CB_SETCURSEL,a->selected,0);
            } else if(LOWORD(wp)==Apply || LOWORD(wp)==IDOK) a->applySettings();
            else if(LOWORD(wp)==Done){if(a->applySettings())DestroyWindow(hwnd);}
            else if(LOWORD(wp)==IDCANCEL)DestroyWindow(hwnd);
            else if(LOWORD(wp)==Reset) {
                if(!a->configWritable) {
                    auto backup=a->root/(L"settings.recovered-"+std::to_wstring(GetTickCount64())+L".json");
                    std::filesystem::copy_file(a->root/L"settings.json",backup,std::filesystem::copy_options::none);
                    a->configWritable=true;
                }
                a->warning.clear();a->config=Config{};
                for(auto& w:a->widgets){w.surface->clearFormats();a->place(w,true);}
                a->render(true);a->repair();a->schedule();a->save();a->populateSettings();
            }
            return 0;
        case WM_CTLCOLORSTATIC:SetTextColor(reinterpret_cast<HDC>(wp),RGB(42,56,48));SetBkColor(reinterpret_cast<HDC>(wp),RGB(255,255,255));return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
        case WM_DPICHANGED: {
            auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
            for(HWND c=GetWindow(hwnd,GW_CHILD);c;) {HWND next=GetWindow(c,GW_HWNDNEXT);DestroyWindow(c);c=next;}
            a->makeSettingsControls();a->populateSettings();return 0;
        }
        case WM_CLOSE:DestroyWindow(hwnd);return 0;
        case WM_NCDESTROY:a->settings=nullptr;return DefWindowProcW(hwnd,msg,wp,lp);
        }
    }catch(const std::exception& e){if(a->settings)SetWindowTextW(a->child(Status),wide(e.what()).c_str());}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int) {
    using namespace dw;
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int result=0;
    try {
        INITCOMMONCONTROLSEX init{sizeof(init),ICC_STANDARD_CLASSES};InitCommonControlsEx(&init);
        App app(instance);
        int count{};LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);
        bool settings=false,quit=false;
        for(int i=1;i<count;++i) {
            std::wstring arg=args[i];
            if(arg==L"--test-root" && i+1<count){app.root=std::filesystem::absolute(args[++i]);app.isolated=true;}
            else if(arg==L"--diagnostic")app.diagnostics=true;
            else if(arg==L"--export-previews")app.previews=true;
            else if(arg==L"--prototype")app.prototype=true;
            else if(arg==L"--application-test"){app.applicationTest=true;app.diagnostics=true;app.duration=12;}
            else if(arg==L"--settings")settings=true;
            else if(arg==L"--quit")quit=true;
            else if(arg.rfind(L"--duration=",0)==0)app.duration=std::clamp(std::stoi(arg.substr(11)),1,86400);
            else throw std::runtime_error("Unknown argument");
        }
        LocalFree(args);
        if((app.diagnostics||app.previews||app.prototype||app.applicationTest) && !app.isolated) throw std::runtime_error("Diagnostics require --test-root <separate folder>");
        if(app.root.empty())app.root=defaultRoot();
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
            if(running)PostMessageW(running,quit?WM_CLOSE:ActivateMessage,0,0);
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
            app.init();if(settings || (app.firstRun && !app.diagnostics && !app.prototype))app.showSettings();
            MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0) {
                if(app.settings && IsDialogMessageW(app.settings,&msg))continue;
                TranslateMessage(&msg);DispatchMessageW(&msg);
            }
            result=static_cast<int>(msg.wParam);
        }
    } catch(const std::exception& e) {MessageBoxW(nullptr,wide(e.what()).c_str(),L"panes — startup error",MB_OK|MB_ICONERROR);result=1;}
    if(SUCCEEDED(com))CoUninitialize();
    return result;
}
