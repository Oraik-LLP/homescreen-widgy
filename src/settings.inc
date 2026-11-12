// Included in app.cpp after App and the shared native-control helpers.
void App::showSettings() {
    if(settings){ShowWindow(settings,SW_RESTORE);SetForegroundWindow(settings);return;}
    draft=config;scroll=0;page=0;zones=timeZones();
    const UINT dpi=monitors.empty()?96:monitors.front().dpi;
    int height=std::min(820,MulDiv(monitors.front().work.bottom-monitors.front().work.top-70,96,dpi));height=std::max(400,height);
    RECT rect{0,0,MulDiv(740,dpi,96),MulDiv(height,dpi,96)};
    AdjustWindowRectExForDpi(&rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0,dpi);
    settings=CreateWindowExW(WS_EX_CONTROLPARENT,SettingsClass,L"panes — Your desktop, quietly.",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,this);
    if(!settings)throw std::runtime_error("Cannot open settings");
    makeSettingsControls();populateSettings();ShowWindow(settings,SW_SHOW);SetForegroundWindow(settings);
}
LRESULT CALLBACK App::PanelProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR data){
    auto* a=reinterpret_cast<App*>(data);
    if(msg==WM_COMMAND||msg==WM_DRAWITEM||msg==WM_VSCROLL||msg==WM_MOUSEWHEEL||msg==WM_CTLCOLORSTATIC)return SettingsProc(a->settings,msg,wp,lp);
    return DefSubclassProc(hwnd,msg,wp,lp);
}
void App::makeSettingsControls(){
    const UINT dpi=GetDpiForWindow(settings);auto px=[&](int v){return MulDiv(v,dpi,96);};
    if(uiFont)DeleteObject(uiFont);if(titleFont)DeleteObject(titleFont);
    uiFont=CreateFontW(-px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    titleFont=CreateFontW(-px(27),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    uiItems.clear();
    auto control=[&](HWND parent,const wchar_t* cls,const wchar_t* text,int id,int x,int y,int width,int height,DWORD style=0)->HWND{
        if(wcscmp(cls,L"BUTTON")==0)style|=BS_NOTIFY;
        auto w=CreateWindowExW(wcscmp(cls,L"EDIT")==0?WS_EX_CLIENTEDGE:0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(width),px(height),parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
        if(!w)throw std::runtime_error("Cannot create a settings control");SendMessageW(w,WM_SETFONT,reinterpret_cast<WPARAM>(uiFont),TRUE);return w;
    };
    auto heading=control(settings,L"STATIC",L"Your desktop, quietly.",0,24,15,670,38);SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
    control(settings,L"BUTTON",L"Widgets",TabWidgets,24,62,168,32,WS_TABSTOP);
    control(settings,L"BUTTON",L"Appearance",TabAppearance,202,62,168,32,WS_TABSTOP);
    control(settings,L"BUTTON",L"Color tools",TabColors,380,62,168,32,WS_TABSTOP);
    control(settings,L"BUTTON",config.locked?L"Edit mode":L"Finish editing",EditMode,558,62,154,32,WS_TABSTOP);
    control(settings,L"BUTTON",L"Add Widget…",AddWidget,482,108,230,30,WS_TABSTOP);
    control(settings,L"STATIC",L"Selected widget",0,24,112,130,25);
    auto select=control(settings,L"COMBOBOX",L"",SelectWidget,166,108,300,240,CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP);
    for(const auto& w:config.widgets)SendMessageW(select,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(kindName(w.kind)));
    panel=CreateWindowExW(WS_EX_CONTROLPARENT,L"STATIC",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_CLIPCHILDREN,px(16),px(150),px(708),px(570),settings,nullptr,instance,nullptr);
    SetWindowSubclass(panel,PanelProc,1,reinterpret_cast<DWORD_PTR>(this));
    auto add=[&](int page,const wchar_t* cls,const wchar_t* text,int id,int x,int y,int w,int h,DWORD style=0,int kind=-1){auto child=control(panel,cls,text,id,x,y,w,h,style);uiItems.push_back({child,page,y,kind});return child;};
    auto label=[&](int page,const wchar_t* text,int x,int y,int kind=-1){return add(page,L"STATIC",text,0,x,y,(x>=350||y==88||y==562||(page==0&&kind==3&&y==322))?290:650,24,0,kind);};
    label(0,L"WIDGET SETTINGS",12,8);
    add(0,L"BUTTON",L"Show this widget",Visible,12,40,300,28,BS_AUTOCHECKBOX|WS_TABSTOP);
    label(0,L"Size % (50–200)",12,88);label(0,L"Opacity % (10–100)",350,88);
    add(0,L"EDIT",L"",Scale,12,116,280,30,ES_NUMBER|WS_TABSTOP);
    add(0,L"EDIT",L"",Opacity,350,116,280,30,ES_NUMBER|WS_TABSTOP);
    add(0,L"BUTTON",L"24-hour clock",Hour24,12,174,280,28,BS_AUTOCHECKBOX|WS_TABSTOP);
    add(0,L"BUTTON",L"Show clock seconds",Seconds,350,174,280,28,BS_AUTOCHECKBOX|WS_TABSTOP);
    add(0,L"BUTTON",L"Launch at sign-in",Startup,12,222,280,28,BS_AUTOCHECKBOX|WS_TABSTOP);
    label(0,L"FOCUS TIMER",12,286,3);
    label(0,L"Focus minutes (1–240)",12,322,3);label(0,L"Break minutes (1–120)",350,322,3);
    add(0,L"EDIT",L"",FocusMinutes,12,350,280,30,ES_NUMBER|WS_TABSTOP,3);
    add(0,L"EDIT",L"",BreakMinutes,350,350,280,30,ES_NUMBER|WS_TABSTOP,3);
    add(0,L"BUTTON",L"Start",FocusStart,12,402,190,32,WS_TABSTOP,3);
    add(0,L"BUTTON",L"Reset",FocusReset,226,402,190,32,WS_TABSTOP,3);
    add(0,L"BUTTON",L"Next session",FocusNext,440,402,190,32,WS_TABSTOP,3);
    label(0,L"Sessions continue while hidden or closed. Each next session starts manually.",12,458,3);
    label(0,L"EVENT COUNTDOWN",12,286,4);label(0,L"Event title",12,322,4);
    add(0,L"EDIT",L"",EventTitle,12,350,618,30,ES_AUTOHSCROLL|WS_TABSTOP,4);
    label(0,L"Local target — YYYY-MM-DD HH:MM",12,402,4);
    add(0,L"EDIT",L"",EventDate,12,434,618,30,ES_AUTOHSCROLL|WS_TABSTOP,4);
    label(0,L"WORLD CLOCK",12,286,6);label(0,L"City label",12,322,6);
    add(0,L"EDIT",L"",City,12,350,618,30,ES_AUTOHSCROLL|WS_TABSTOP,6);
    label(0,L"Windows time zone (daylight saving is automatic)",12,402,6);
    auto zone=add(0,L"COMBOBOX",L"",Zone,12,434,618,260,CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,6);
    SendMessageW(zone,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"UTC"));for(const auto& z:zones)SendMessageW(zone,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(z.TimeZoneKeyName));
    label(0,L"Edit mode: drag on the 24 px grid. Hold Shift to move freely.",12,524);
    label(0,L"New widgets start hidden. Notes live in Tagme.",12,558);

    label(1,L"THEMES — original static styles inspired by expressive lock screens",12,8);
    for(int i=0;i<ThemeCount;++i){int x=12+i%3*218,y=44+i/3*145;
        add(1,L"BUTTON",Themes[i].name,ThemeFirst+i,x,y,204,132,BS_OWNERDRAW|WS_TABSTOP);
    }
    auto preset=add(1,L"COMBOBOX",L"",Preset,12,350,630,180,CBS_DROPDOWNLIST|WS_TABSTOP);
    for(auto& t:Themes)SendMessageW(preset,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(t.name));
    add(1,L"BUTTON",L"Match wallpaper for this theme",MatchWallpaper,12,397,630,28,BS_AUTOCHECKBOX|WS_TABSTOP);
    label(1,L"Colors adapt locally. Manual widget colors take precedence.",12,430);
    label(1,L"Font family — selected widget",12,480);
    auto font=add(1,L"COMBOBOX",L"",Font,12,510,630,260,CBS_DROPDOWN|CBS_AUTOHSCROLL|WS_VSCROLL|WS_TABSTOP);
    for(auto name:{L"Segoe UI Light",L"Segoe UI",L"Segoe UI Semilight"})SendMessageW(font,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
    for(auto family:BundledFontFamilies)for(auto suffix:{L" Light",L"",L" Semibold"}){auto name=std::wstring(family)+suffix;SendMessageW(font,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}
    for(auto name:{L"Bahnschrift",L"Consolas",L"Arial",L"Georgia"})SendMessageW(font,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
    label(1,L"Text color",12,562);label(1,L"Accent color",350,562);
    add(1,L"EDIT",L"",Color,12,590,176,30,ES_AUTOHSCROLL|WS_TABSTOP);
    add(1,L"BUTTON",L"Text color",TextSwatch,200,590,40,30,BS_OWNERDRAW|WS_TABSTOP);
    add(1,L"BUTTON",L"Pick…",PickText,250,590,64,30,WS_TABSTOP);
    add(1,L"EDIT",L"",Accent,350,590,176,30,ES_AUTOHSCROLL|WS_TABSTOP);
    add(1,L"BUTTON",L"Accent color",AccentSwatch,538,590,40,30,BS_OWNERDRAW|WS_TABSTOP);
    add(1,L"BUTTON",L"Pick…",PickAccent,588,590,64,30,WS_TABSTOP);
    label(1,L"Theme palette — select a swatch to open Color tools",12,650);
    for(int i=0;i<ThemeCount*2;++i)add(1,L"BUTTON",L"Theme color",PaletteFirst+i,12+i*54,680,44,32,BS_OWNERDRAW|WS_TABSTOP);
    add(1,L"BUTTON",L"Use theme colors",UseTheme,12,740,630,32,WS_TABSTOP);

    label(2,L"COLOR TOOLS",12,8);label(2,L"Pick, save, and copy colors. Assign them to the selected widget when ready.",12,40);
    add(2,L"EDIT",L"",ToolHex,12,90,300,36,ES_AUTOHSCROLL|WS_TABSTOP);
    add(2,L"BUTTON",L"Choose color…",ToolChoose,330,90,155,36,WS_TABSTOP);
    add(2,L"BUTTON",L"Screen eyedropper",ToolPick,500,90,155,36,WS_TABSTOP);
    add(2,L"BUTTON",L"Copy HEX",CopyHex,12,150,190,32,WS_TABSTOP);
    add(2,L"BUTTON",L"Copy RGB",CopyRgb,226,150,190,32,WS_TABSTOP);
    add(2,L"BUTTON",L"Save favorite",Favorite,440,150,215,32,WS_TABSTOP);
    label(2,L"RECENT COLORS",12,222);
    for(int i=0;i<12;++i)add(2,L"BUTTON",L"Recent color",RecentFirst+i,12+i%6*108,256+i/6*48,94,36,BS_OWNERDRAW|WS_TABSTOP);
    label(2,L"FAVORITES — eight saved swatches",12,382);
    for(int i=0;i<8;++i)add(2,L"BUTTON",L"Favorite color",FavoriteFirst+i,12+i%4*162,416+i/4*48,148,36,BS_OWNERDRAW|WS_TABSTOP);
    add(2,L"BUTTON",L"Assign to text",AssignText,12,550,300,36,WS_TABSTOP);
    add(2,L"BUTTON",L"Assign to accent",AssignAccent,350,550,300,36,WS_TABSTOP);
    label(2,L"Assignments use Apply / Done. Escape cancels screen sampling.",12,612);

    label(3,L"ADD A WIDGET",12,8);
    label(3,L"Choose a preview to add it to your desktop. New widgets start at default positions.",12,32);
    add(3,L"BUTTON",L"Back to widget settings",BackWidgets,488,5,162,30,WS_TABSTOP);
    for(int i=0;i<WidgetCount;++i){int x=12+i%3*218,y=68+i/3*156;
        add(3,L"BUTTON",kindName(static_cast<Kind>(i)),WidgetFirst+i,x,y,204,146,BS_OWNERDRAW|WS_TABSTOP);
    }

    control(settings,L"STATIC",L"",Status,24,730,430,46);
    control(settings,L"BUTTON",L"Reset to defaults",Reset,24,780,154,32,WS_TABSTOP);
    control(settings,L"BUTTON",L"Apply",Apply,476,780,110,32,BS_DEFPUSHBUTTON|WS_TABSTOP);
    control(settings,L"BUTTON",L"Done",Done,602,780,110,32,WS_TABSTOP);
    for(auto id:{Color,Accent,ToolHex})SendMessageW(child(id),EM_SETLIMITTEXT,7,0);
    SendMessageW(child(EventTitle),EM_SETLIMITTEXT,80,0);SendMessageW(child(City),EM_SETLIMITTEXT,60,0);SendMessageW(child(EventDate),EM_SETLIMITTEXT,16,0);SendMessageW(child(Font),CB_LIMITTEXT,100,0);
    if(isolated){EnableWindow(child(Startup),FALSE);SetWindowTextW(child(Startup),L"Sign-in disabled in test mode");}
    refreshPreviews();layoutSettings();
}
void App::layoutSettings(){
    if(!settings||!panel)return;
    UINT dpi=GetDpiForWindow(settings);auto px=[&](int v){return MulDiv(v,dpi,96);};RECT r{};GetClientRect(settings,&r);
    int height=MulDiv(r.bottom,96,dpi),width=MulDiv(r.right,96,dpi),viewport=std::max(60,height-252);
    bodyHeight=page==1?800:page==2?660:page==3?548:610;scroll=std::clamp(scroll,0,std::max(0,bodyHeight-viewport));
    SetWindowPos(panel,nullptr,px(16),px(150),px(width-32),px(viewport),SWP_NOZORDER);
    SCROLLINFO info{sizeof(info),SIF_RANGE|SIF_PAGE|SIF_POS,0,bodyHeight-1,static_cast<UINT>(viewport),scroll,0};SetScrollInfo(panel,SB_VERT,&info,TRUE);
    for(auto& item:uiItems){RECT cr{};GetWindowRect(item.window,&cr);POINT pt{cr.left,cr.top};ScreenToClient(panel,&pt);SetWindowPos(item.window,nullptr,pt.x,px(item.y-scroll),0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);ShowWindow(item.window,item.page==page&&(item.kind<0||item.kind==selected)?SW_SHOW:SW_HIDE);}
    SetWindowPos(child(Status),nullptr,px(24),px(height-94),px(width-48),px(38),SWP_NOZORDER);
    SetWindowPos(child(Reset),nullptr,px(24),px(height-44),0,0,SWP_NOSIZE|SWP_NOZORDER);
    SetWindowPos(child(Apply),nullptr,px(width-264),px(height-44),0,0,SWP_NOSIZE|SWP_NOZORDER);
    SetWindowPos(child(Done),nullptr,px(width-138),px(height-44),0,0,SWP_NOSIZE|SWP_NOZORDER);
    for(int i=0;i<3;++i)SetWindowTextW(child(TabWidgets+i),i==page?(i==0?L"• Widgets":i==1?L"• Appearance":L"• Color tools"):(i==0?L"Widgets":i==1?L"Appearance":L"Color tools"));
    InvalidateRect(panel,nullptr,TRUE);
}
void App::refreshPreviews(){
    SYSTEMTIME t{};t.wYear=2026;t.wMonth=11;t.wDay=12;t.wHour=10;t.wMinute=8;
    for(int i=0;i<ThemeCount;++i){if(!themePreviews[i])themePreviews[i]=std::make_unique<Surface>(*graphics);Config c;applyTheme(c,i);themePreviews[i]->draw(settings,c.widgets[0],c,t,false,false,96);}
}
void App::refreshWidgetPreviews(){
    SYSTEMTIME t{};t.wYear=2026;t.wMonth=11;t.wDay=12;t.wHour=10;t.wMinute=8;
    MediaSnapshot demo;demo.apiAvailable=demo.hasSession=demo.playing=demo.canPrevious=demo.canToggle=demo.canNext=true;demo.source=L"Now playing";demo.title=L"Midnight City";demo.artist=L"M83";
    for(int i=0;i<WidgetCount;++i){
        if(!widgetPreviews[i])widgetPreviews[i]=std::make_unique<Surface>(*graphics);
        Config c=draft;auto& w=c.widgets[i];w.visible=true;w.opacity=100;w.scale=100;w.backing=true;
        widgetPreviews[i]->draw(settings,w,c,t,false,false,96,w.kind==Kind::Music?&demo:nullptr);
    }
}
void App::populateSettings(){
    populating=true;const auto& w=draft.widgets[selected];
    SendMessageW(child(SelectWidget),CB_SETCURSEL,selected,0);SendMessageW(child(Preset),CB_SETCURSEL,draft.preset,0);
    SetWindowTextW(child(Font),w.font.c_str());SetWindowTextW(child(Scale),std::to_wstring(w.scale).c_str());SetWindowTextW(child(Opacity),std::to_wstring(w.opacity).c_str());
    SetWindowTextW(child(Color),colorText(w.color).c_str());SetWindowTextW(child(Accent),colorText(w.accent).c_str());
    SendMessageW(child(Visible),BM_SETCHECK,w.visible?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(child(Hour24),BM_SETCHECK,draft.hour24?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(child(Seconds),BM_SETCHECK,draft.seconds?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(child(MatchWallpaper),BM_SETCHECK,draft.wallpaperMatch[draft.preset]?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(child(Startup),BM_SETCHECK,!isolated&&startupEnabled()?BST_CHECKED:BST_UNCHECKED,0);
    SetWindowTextW(child(FocusMinutes),std::to_wstring(draft.focus.focusMinutes).c_str());SetWindowTextW(child(BreakMinutes),std::to_wstring(draft.focus.breakMinutes).c_str());SetWindowTextW(child(FocusStart),config.focus.running?L"Pause":L"Start");
    SetWindowTextW(child(EventTitle),draft.eventTitle.c_str());SetWindowTextW(child(EventDate),targetText(draft.eventUtc).c_str());SetWindowTextW(child(City),draft.city.c_str());
    int zoneIndex=0;for(size_t i=0;i<zones.size();++i)if(draft.zone==zones[i].TimeZoneKeyName)zoneIndex=static_cast<int>(i+1);SendMessageW(child(Zone),CB_SETCURSEL,zoneIndex,0);
    SetWindowTextW(child(ToolHex),colorText(toolColor).c_str());
    SetWindowTextW(child(Status),warning.empty()?L"Native · offline · static themes":warning.c_str());
    SetWindowTextW(child(EditMode),config.locked?L"Edit mode":L"Finish editing");
    if(w.kind==Kind::Music&&warning.empty())SetWindowTextW(child(Status),L"Playback controls work anytime. Use Edit mode to move Music by dragging outside its controls.");
    for(size_t i=0;i<12;++i)SetWindowTextW(child(RecentFirst+static_cast<int>(i)),i<config.recent.size()?colorText(config.recent[i]).c_str():L"Empty recent");
    for(size_t i=0;i<8;++i)SetWindowTextW(child(FavoriteFirst+static_cast<int>(i)),i<config.favorites.size()?colorText(config.favorites[i]).c_str():L"Empty favorite");
    populating=false;layoutSettings();
}
bool App::readSettings(Config& candidate){
    try {
        auto number=[&](int id,int min,int max){auto s=controlText(child(id));if(s.empty()||s.size()>3||s.find_first_not_of(L"0123456789")!=std::wstring::npos)throw std::runtime_error("Enter a valid percentage or duration.");int n=std::stoi(s);if(n<min||n>max)throw std::runtime_error("A size, opacity, or duration is outside the displayed range.");return n;};
        candidate=draft;candidate.locked=config.locked;candidate.shown=config.shown;
        for(int i=0;i<WidgetCount;++i){auto& target=candidate.widgets[i];const auto& live=config.widgets[i];target.x=live.x;target.y=live.y;target.monitor=live.monitor;target.positioned=live.positioned;}
        auto& w=candidate.widgets[selected];w.scale=number(Scale,50,200);w.opacity=number(Opacity,10,100);
        unsigned color{},accent{};if(!parseColor(controlText(child(Color)),color)||!parseColor(controlText(child(Accent)),accent))throw std::runtime_error("Use six-digit HEX colors, such as #91B89D.");
        if(color!=w.color||accent!=w.accent)w.customColors=true;w.color=color;w.accent=accent;
        w.font=controlText(child(Font));if(w.font.empty()||w.font.size()>100)throw std::runtime_error("Choose a font family.");w.font=graphics->resolveFont(w.font);
        w.visible=SendMessageW(child(Visible),BM_GETCHECK,0,0)==BST_CHECKED;candidate.hour24=SendMessageW(child(Hour24),BM_GETCHECK,0,0)==BST_CHECKED;candidate.seconds=SendMessageW(child(Seconds),BM_GETCHECK,0,0)==BST_CHECKED;
        candidate.wallpaperMatch[draft.preset]=SendMessageW(child(MatchWallpaper),BM_GETCHECK,0,0)==BST_CHECKED;
        int preset=static_cast<int>(SendMessageW(child(Preset),CB_GETCURSEL,0,0));if(preset!=draft.preset)applyTheme(candidate,preset);
        candidate.focus=config.focus;candidate.focus.focusMinutes=number(FocusMinutes,1,240);candidate.focus.breakMinutes=number(BreakMinutes,1,120);
        if(!candidate.focus.running&&((!candidate.focus.isBreak&&candidate.focus.focusMinutes!=config.focus.focusMinutes)||(candidate.focus.isBreak&&candidate.focus.breakMinutes!=config.focus.breakMinutes)))focusAction(candidate.focus,1,utcNow());
        candidate.eventTitle=controlText(child(EventTitle));if(candidate.eventTitle.empty())candidate.eventTitle=L"Your next moment";
        auto target=controlText(child(EventDate));if(target.empty())candidate.eventUtc=0;else if(target!=targetText(draft.eventUtc)&&!localTarget(target,candidate.eventUtc))throw std::runtime_error("Enter a real local date as YYYY-MM-DD HH:MM (daylight-saving gaps are invalid).");
        candidate.city=controlText(child(City));if(candidate.city.empty())candidate.city=L"UTC";
        auto zi=SendMessageW(child(Zone),CB_GETCURSEL,0,0);candidate.zone=zi>0&&static_cast<size_t>(zi)<=zones.size()?zones[zi-1].TimeZoneKeyName:L"UTC";
        candidate.recent=config.recent;candidate.favorites=config.favorites;return true;
    }catch(const std::exception& e){SetWindowTextW(child(Status),wide(e.what()).c_str());return false;}
}
bool App::applySettings(){
    Config candidate;if(!readSettings(candidate))return false;
    bool startup=SendMessageW(child(Startup),BM_GETCHECK,0,0)==BST_CHECKED;
    if(!isolated&&startup!=startupEnabled()&&!setStartup(startup)){SetWindowTextW(child(Status),L"Windows could not update launch at sign-in.");return false;}
    bool matchChanged=candidate.preset!=config.preset||candidate.wallpaperMatch!=config.wallpaperMatch;
    config=candidate;draft=config;updateMedia();
    for(auto& w:widgets){w.surface->clearFormats();place(w);}if(matchChanged)requestWallpaper();render(true);repair();schedule();save();populateSettings();
    SetWindowTextW(child(Status),configWritable?L"Applied. Widgets stay behind your apps.":L"Applied for this session. Reset layout to recover saved settings.");return true;
}
void App::colorTool(unsigned color,bool remember){
    toolColor=color&0xFFFFFF;if(remember){rememberColor(config.recent,toolColor,12);save();}
    SetWindowTextW(child(ToolHex),colorText(toolColor).c_str());
    for(size_t i=0;i<config.recent.size();++i)SetWindowTextW(child(RecentFirst+static_cast<int>(i)),colorText(config.recent[i]).c_str());
    InvalidateRect(panel,nullptr,TRUE);
}
void App::chooseColor(int id){
    unsigned value=toolColor;parseColor(controlText(child(id)),value);static COLORREF custom[16]{};
    CHOOSECOLORW cc{};cc.lStructSize=sizeof(cc);cc.hwndOwner=settings;cc.rgbResult=RGB(value>>16&255,value>>8&255,value&255);cc.lpCustColors=custom;cc.Flags=CC_FULLOPEN|CC_RGBINIT;
    if(ChooseColorW(&cc)){unsigned color=GetRValue(cc.rgbResult)<<16|GetGValue(cc.rgbResult)<<8|GetBValue(cc.rgbResult);colorTool(color);if(id!=ToolHex)SetWindowTextW(child(id),colorText(color).c_str());InvalidateRect(panel,nullptr,TRUE);}
}
static bool copyColor(HWND owner,const std::wstring& value){
    if(!OpenClipboard(owner))return false;
    HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,(value.size()+1)*sizeof(wchar_t));if(!data){CloseClipboard();return false;}void* p=GlobalLock(data);if(!p){GlobalFree(data);CloseClipboard();return false;}
    memcpy(p,value.c_str(),(value.size()+1)*sizeof(wchar_t));GlobalUnlock(data);bool ok=EmptyClipboard()&&SetClipboardData(CF_UNICODETEXT,data)!=nullptr;if(!ok)GlobalFree(data);CloseClipboard();return ok;
}
void App::startPicker(){
    if(picker)return;lastPick=0;
    picker=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED,PickerClass,L"panes — press Escape to cancel",WS_POPUP,GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),GetSystemMetrics(SM_CXVIRTUALSCREEN),GetSystemMetrics(SM_CYVIRTUALSCREEN),nullptr,nullptr,instance,this);
    if(!picker)throw std::runtime_error("Cannot start screen eyedropper");
    SetLayeredWindowAttributes(picker,RGB(1,2,3),255,LWA_COLORKEY);ShowWindow(picker,SW_SHOW);SetForegroundWindow(picker);SetFocus(picker);SetCapture(picker);SetCursor(LoadCursorW(nullptr,IDC_CROSS));SetWindowTextW(child(Status),L"Click a screen color. Escape cancels.");
}
void App::endPicker(bool accept){
    if(!picker)return;
    if(accept){POINT point{};GetCursorPos(&point);HDC screen=GetDC(nullptr);auto color=GetPixel(screen,point.x,point.y);ReleaseDC(nullptr,screen);if(color!=CLR_INVALID)colorTool(GetRValue(color)<<16|GetGValue(color)<<8|GetBValue(color));}
    HWND old=picker;picker=nullptr;ReleaseCapture();DestroyWindow(old);SetForegroundWindow(settings);SetWindowTextW(child(Status),accept?L"Color captured. Copy, save, or assign it.":L"Screen sampling canceled.");
}
LRESULT CALLBACK App::PickerProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    auto* a=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}if(!a)return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps{};auto dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);auto brush=CreateSolidBrush(RGB(1,2,3));FillRect(dc,&r,brush);DeleteObject(brush);EndPaint(hwnd,&ps);return 0;}
    case WM_SETCURSOR:SetCursor(LoadCursorW(nullptr,IDC_CROSS));return TRUE;
    case WM_MOUSEMOVE:if(GetTickCount64()-a->lastPick>=33){a->lastPick=GetTickCount64();POINT point{};GetCursorPos(&point);auto dc=GetDC(nullptr);auto color=GetPixel(dc,point.x,point.y);ReleaseDC(nullptr,dc);if(color!=CLR_INVALID){auto value=GetRValue(color)<<16|GetGValue(color)<<8|GetBValue(color);auto text=L"Sampling "+colorText(value)+L" — click to capture; Escape to cancel";SetWindowTextW(a->child(Status),text.c_str());}}return 0;
    case WM_LBUTTONUP:a->endPicker(true);return 0;
    case WM_RBUTTONUP:case WM_CLOSE:a->endPicker(false);return 0;
    case WM_KEYDOWN:if(wp==VK_ESCAPE){a->endPicker(false);return 0;}break;
    case WM_CAPTURECHANGED:if(a->picker==hwnd)a->endPicker(false);return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
LRESULT CALLBACK App::SettingsProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    auto* a=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}if(!a)return DefWindowProcW(hwnd,msg,wp,lp);
    try{switch(msg){
    case WM_COMMAND:{
        int id=LOWORD(wp),notice=HIWORD(wp);
        if(a->populating)return 0;
        if(notice==EN_SETFOCUS||notice==CBN_SETFOCUS||notice==BN_SETFOCUS){
            auto item=std::find_if(a->uiItems.begin(),a->uiItems.end(),[&](const auto& i){return i.window==reinterpret_cast<HWND>(lp);});
            if(item!=a->uiItems.end()&&item->page==a->page){RECT rect{};GetClientRect(a->panel,&rect);int view=MulDiv(rect.bottom,96,GetDpiForWindow(hwnd));if(item->y<a->scroll)a->scroll=item->y;else if(item->y+40>a->scroll+view)a->scroll=item->y+40-view;a->layoutSettings();}return 0;
        }
        if(id==SelectWidget&&notice==CBN_SELCHANGE){int next=static_cast<int>(SendMessageW(a->child(SelectWidget),CB_GETCURSEL,0,0));if(a->applySettings()){a->selected=std::clamp(next,0,WidgetCount-1);a->scroll=0;a->populateSettings();}else SendMessageW(a->child(SelectWidget),CB_SETCURSEL,a->selected,0);}
        else if(id==EditMode&&notice==BN_CLICKED)a->command(MenuLock);
        else if(id==AddWidget&&notice==BN_CLICKED){if(a->applySettings()){a->page=3;a->scroll=0;a->refreshWidgetPreviews();a->layoutSettings();SetWindowTextW(a->child(Status),L"Choose a widget preview to add it to your desktop.");}}
        else if(id==BackWidgets){a->page=0;a->scroll=0;a->layoutSettings();}
        else if(id>=WidgetFirst&&id<WidgetFirst+WidgetCount){
            int index=id-WidgetFirst;
            if(a->applySettings()){
                a->selected=index;auto& cfg=a->config.widgets[index];bool added=!cfg.visible;cfg.visible=true;a->config.shown=true;a->draft=a->config;a->updateMedia();
                a->place(a->widgets[index]);a->widgets[index].dirty=true;a->page=0;a->scroll=0;a->render(true);a->repair();a->schedule();a->save();a->populateSettings();
                auto message=added?std::wstring(L"Added ")+kindName(cfg.kind)+L". Choose Edit mode to move it.":std::wstring(kindName(cfg.kind))+L" is already on your desktop; selected.";
                SetWindowTextW(a->child(Status),message.c_str());
            }
        }
        else if(id>=TabWidgets&&id<=TabColors){a->page=id-TabWidgets;a->scroll=0;a->layoutSettings();}
        else if((id>=ThemeFirst&&id<ThemeFirst+ThemeCount)||(id==Preset&&notice==CBN_SELCHANGE)){
            int theme=id==Preset?static_cast<int>(SendMessageW(a->child(Preset),CB_GETCURSEL,0,0)):id-ThemeFirst;
            // Read old-theme fields first; the theme transaction resets appearance only.
            SendMessageW(a->child(Preset),CB_SETCURSEL,a->draft.preset,0);Config candidate;if(a->readSettings(candidate)){applyTheme(candidate,theme);a->draft=candidate;a->populateSettings();}
        }
        else if(id==Apply||id==IDOK)a->applySettings();
        else if(id==Done){if(a->applySettings())DestroyWindow(hwnd);}
        else if(id==IDCANCEL)DestroyWindow(hwnd);
        else if(id==PickText||id==TextSwatch)a->chooseColor(Color);
        else if(id==PickAccent||id==AccentSwatch)a->chooseColor(Accent);
        else if(id==ToolChoose)a->chooseColor(ToolHex);
        else if(id==ToolPick)a->startPicker();
        else if(id==UseTheme){auto& w=a->draft.widgets[a->selected];w.color=Themes[a->draft.preset].text;w.accent=Themes[a->draft.preset].accent;w.customColors=false;SetWindowTextW(a->child(Color),colorText(w.color).c_str());SetWindowTextW(a->child(Accent),colorText(w.accent).c_str());InvalidateRect(a->panel,nullptr,TRUE);}
        else if(id==FocusStart||id==FocusReset||id==FocusNext){if(a->applySettings())a->focusCommand(id==FocusStart?0:id==FocusReset?1:2);}
        else if(id==CopyHex||id==CopyRgb||id==Favorite||id==AssignText||id==AssignAccent){
            unsigned value{};if(!parseColor(controlText(a->child(ToolHex)),value)){SetWindowTextW(a->child(Status),L"Enter a six-digit HEX color first.");return 0;}a->colorTool(value);
            if(id==CopyHex||id==CopyRgb){auto text=colorText(value);if(id==CopyRgb)text=L"rgb("+std::to_wstring(value>>16&255)+L", "+std::to_wstring(value>>8&255)+L", "+std::to_wstring(value&255)+L")";SetWindowTextW(a->child(Status),copyColor(hwnd,text)?L"Copied to clipboard.":L"Clipboard busy. Try again.");}
            else if(id==Favorite){rememberColor(a->config.favorites,value,8);a->save();for(size_t i=0;i<a->config.favorites.size();++i)SetWindowTextW(a->child(FavoriteFirst+static_cast<int>(i)),colorText(a->config.favorites[i]).c_str());InvalidateRect(a->panel,nullptr,TRUE);}
            else{SetWindowTextW(a->child(id==AssignText?Color:Accent),colorText(value).c_str());SetWindowTextW(a->child(Status),L"Assigned to selected widget. Apply / Done saves it.");}
        }
        else if(id>=RecentFirst&&id<RecentFirst+12){size_t index=id-RecentFirst;if(index<a->config.recent.size())a->colorTool(a->config.recent[index]);}
        else if(id>=FavoriteFirst&&id<FavoriteFirst+8){size_t index=id-FavoriteFirst;if(index<a->config.favorites.size())a->colorTool(a->config.favorites[index]);}
        else if(id>=PaletteFirst&&id<PaletteFirst+ThemeCount*2){int index=id-PaletteFirst;auto& theme=Themes[index/2];a->colorTool(index%2?theme.accent:theme.text);a->page=2;a->scroll=0;a->layoutSettings();}
        else if(id==Reset){
            std::error_code error;auto path=a->root/L"settings.json";
            if(std::filesystem::exists(path)){std::filesystem::copy_file(path,a->root/(L"settings.before-reset-"+std::to_wstring(GetTickCount64())+L".json"),error);if(error){SetWindowTextW(a->child(Status),L"Could not back up settings. Defaults were not applied.");return 0;}}
            a->configWritable=true;a->warning.clear();a->config=Config{};a->selected=0;a->page=0;a->scroll=0;for(auto& w:a->widgets){w.surface->clearFormats();a->place(w,true);}a->draft=a->config;a->updateMedia();a->requestWallpaper();a->render(true);a->repair();a->schedule();a->save();a->populateSettings();SetWindowTextW(a->child(Status),L"Defaults restored. Previous settings were backed up.");
        }
        else if((id==Color||id==Accent)&&notice==EN_CHANGE)InvalidateRect(a->panel,nullptr,TRUE);
        return 0;
    }
    case WM_DRAWITEM:{
        auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);int savedDC=SaveDC(item->hDC);RECT r=item->rcItem;int id=static_cast<int>(item->CtlID);unsigned color=0xDDE2DE;
        if(id>=ThemeFirst&&id<ThemeFirst+ThemeCount){int index=id-ThemeFirst;auto brush=CreateSolidBrush(RGB(25,35,32));FillRect(item->hDC,&r,brush);DeleteObject(brush);RECT preview=r;preview.bottom-=30;InflateRect(&preview,-8,-8);a->themePreviews[index]->preview(item->hDC,preview);SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,RGB(245,246,242));SelectObject(item->hDC,a->uiFont);RECT text=r;text.top=text.bottom-30;DrawTextW(item->hDC,Themes[index].name,-1,&text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(index==a->draft.preset){auto pen=CreatePen(PS_SOLID,2,RGB(145,184,157));auto old=SelectObject(item->hDC,pen);auto fill=SelectObject(item->hDC,GetStockObject(HOLLOW_BRUSH));Rectangle(item->hDC,r.left+1,r.top+1,r.right-1,r.bottom-1);SelectObject(item->hDC,fill);SelectObject(item->hDC,old);DeleteObject(pen);}}
        else if(id>=WidgetFirst&&id<WidgetFirst+WidgetCount){
            int index=id-WidgetFirst;HBRUSH fill=CreateSolidBrush(RGB(25,35,32));HPEN pen=CreatePen(PS_SOLID,1,RGB(59,74,67));auto oldPen=SelectObject(item->hDC,pen);auto oldBrush=SelectObject(item->hDC,fill);RoundRect(item->hDC,r.left,r.top,r.right,r.bottom,12,12);SelectObject(item->hDC,oldBrush);SelectObject(item->hDC,oldPen);DeleteObject(fill);DeleteObject(pen);
            RECT preview=r;preview.bottom-=43;InflateRect(&preview,-6,-5);if(a->widgetPreviews[index])a->widgetPreviews[index]->preview(item->hDC,preview);
            SetBkMode(item->hDC,TRANSPARENT);SelectObject(item->hDC,a->uiFont);SetTextColor(item->hDC,RGB(245,246,242));RECT title=r;title.top=title.bottom-40;title.bottom-=21;DrawTextW(item->hDC,kindName(static_cast<Kind>(index)),-1,&title,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            SetTextColor(item->hDC,RGB(145,184,157));RECT action=r;action.top=action.bottom-21;DrawTextW(item->hDC,a->config.widgets[index].visible?L"ON DESKTOP · SELECT":L"+ ADD WIDGET",-1,&action,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            if(a->config.widgets[index].visible){HPEN selected=CreatePen(PS_SOLID,2,RGB(145,184,157));oldPen=SelectObject(item->hDC,selected);oldBrush=SelectObject(item->hDC,GetStockObject(HOLLOW_BRUSH));RoundRect(item->hDC,r.left+2,r.top+2,r.right-2,r.bottom-2,12,12);SelectObject(item->hDC,oldBrush);SelectObject(item->hDC,oldPen);DeleteObject(selected);}
        }
        else {
            bool valid=true;
            if(id==TextSwatch||id==AccentSwatch)valid=parseColor(controlText(a->child(id==TextSwatch?Color:Accent)),color);
            else if(id>=PaletteFirst&&id<PaletteFirst+ThemeCount*2){int i=id-PaletteFirst;color=i%2?Themes[i/2].accent:Themes[i/2].text;}
            else if(id>=RecentFirst&&id<RecentFirst+12){size_t i=id-RecentFirst;valid=i<a->config.recent.size();if(valid)color=a->config.recent[i];}
            else if(id>=FavoriteFirst&&id<FavoriteFirst+8){size_t i=id-FavoriteFirst;valid=i<a->config.favorites.size();if(valid)color=a->config.favorites[i];}
            if(!valid)color=0xE8EBE8;auto brush=CreateSolidBrush(RGB(color>>16&255,color>>8&255,color&255));FillRect(item->hDC,&r,brush);DeleteObject(brush);FrameRect(item->hDC,&r,static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
            if(id>=RecentFirst&&id<FavoriteFirst+8){SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,((color>>16&255)+(color>>8&255)+(color&255))>390?RGB(20,30,25):RGB(255,255,255));auto text=valid?colorText(color):L"+";DrawTextW(item->hDC,text.c_str(),-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
        }
        if(item->itemState&ODS_FOCUS){InflateRect(&r,-4,-4);DrawFocusRect(item->hDC,&r);}RestoreDC(item->hDC,savedDC);return TRUE;
    }
    case WM_VSCROLL:{SCROLLINFO info{sizeof(info),SIF_ALL,0,0,0,0,0};GetScrollInfo(a->panel,SB_VERT,&info);int delta=LOWORD(wp)==SB_LINEUP?-32:LOWORD(wp)==SB_LINEDOWN?32:LOWORD(wp)==SB_PAGEUP?-static_cast<int>(info.nPage):LOWORD(wp)==SB_PAGEDOWN?static_cast<int>(info.nPage):0;if(LOWORD(wp)==SB_THUMBTRACK)a->scroll=info.nTrackPos;else a->scroll+=delta;a->layoutSettings();return 0;}
    case WM_MOUSEWHEEL:a->scroll-=GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA*72;a->layoutSettings();return 0;
    case WM_CTLCOLORSTATIC:SetTextColor(reinterpret_cast<HDC>(wp),RGB(42,56,48));SetBkColor(reinterpret_cast<HDC>(wp),RGB(255,255,255));return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    case WM_DPICHANGED:{Config candidate;if(a->readSettings(candidate))a->draft=candidate;auto rect=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);for(HWND child=GetWindow(hwnd,GW_CHILD);child;){auto next=GetWindow(child,GW_HWNDNEXT);DestroyWindow(child);child=next;}a->panel=nullptr;a->makeSettingsControls();a->populateSettings();return 0;}
    case WM_CLOSE:if(a->picker)a->endPicker(false);DestroyWindow(hwnd);return 0;
    case WM_NCDESTROY:a->settings=nullptr;a->panel=nullptr;a->uiItems.clear();for(auto& preview:a->themePreviews)preview.reset();return DefWindowProcW(hwnd,msg,wp,lp);
    }}catch(const std::exception& e){if(a->settings)SetWindowTextW(a->child(Status),wide(e.what()).c_str());}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
