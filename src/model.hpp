#pragma once
#include <windows.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

namespace dw {
inline constexpr wchar_t AppName[] = L"panes";
inline constexpr int ConfigVersion = 2;
inline constexpr int WidgetCount = 8, ThemeCount = 5;
inline constexpr std::array<const wchar_t*, 7> BundledFontFamilies{{
    L"Inter", L"Manrope", L"DM Sans", L"Space Grotesk", L"Plus Jakarta Sans", L"Outfit", L"Google Sans Flex"
}};
enum class Kind { Clock, Date, Calendar, Focus, Countdown, Battery, WorldClock, Music };
struct WidgetConfig {
    Kind kind = Kind::Clock;
    bool visible = true;
    bool positioned = false;
    std::wstring monitor;
    int x = 0, y = 0; // DIPs relative to the monitor work area.
    int scale = 100, opacity = 100;
    std::wstring font = L"Segoe UI Light";
    unsigned color = 0xF2F4F1, accent = 0x91B89D;
    bool customColors = false, backing = false; // backing is a runtime contrast aid.
};
struct FocusState {
    int focusMinutes = 25, breakMinutes = 5;
    bool isBreak = false, running = false, pending = false;
    long long deadline = 0, remaining = 25 * 60;
};
struct Config {
    bool shown = true, locked = true, seconds = false, hour24 = true;
    int preset = 0; // Stable theme IDs: Minimal, Compact, Editorial, Stacked, Soft.
    std::array<bool, ThemeCount> wallpaperMatch{};
    std::array<WidgetConfig, WidgetCount> widgets;
    FocusState focus;
    std::wstring eventTitle = L"Your next moment", zone = L"UTC", city = L"UTC";
    long long eventUtc = 0;
    std::vector<unsigned> recent, favorites;
    Config() { for (int i = 0; i < WidgetCount; ++i) { widgets[i].kind = static_cast<Kind>(i); widgets[i].visible = i < 3; } }
};
struct LoadResult { Config value; std::wstring warning; bool writable = true, migrated = false; };
struct Theme { const wchar_t* name; const wchar_t* font; unsigned text, accent; };
extern const std::array<Theme, ThemeCount> Themes;
void applyTheme(Config& config, int theme);
long long utcNow();
long long systemSeconds(const SYSTEMTIME& time);
SYSTEMTIME secondsSystem(long long seconds);
long long focusRemaining(const FocusState& focus, long long now);
bool finishFocus(FocusState& focus, long long now);
void focusAction(FocusState& focus, int action, long long now); // 0 start/pause, 1 reset, 2 next.
std::wstring countdownText(long long target, long long now);
std::wstring batteryText(const SYSTEM_POWER_STATUS& power, bool available);
std::vector<DYNAMIC_TIME_ZONE_INFORMATION> timeZones();
bool zoneTime(const std::wstring& key, const SYSTEMTIME& utc, SYSTEMTIME& local);
bool localTarget(const std::wstring& value, long long& utc);
std::wstring targetText(long long utc);
void rememberColor(std::vector<unsigned>& colors, unsigned color, size_t limit);
struct Monitor { HMONITOR handle{}; std::wstring device; RECT work{}; UINT dpi = 96; };
std::string utf8(const std::wstring& s);
std::wstring wide(const std::string& s);
std::filesystem::path defaultRoot();
LoadResult loadConfig(const std::filesystem::path& path);
void saveConfig(const std::filesystem::path& path, const Config& config);
bool leapYear(int year);
int daysInMonth(int year, int month);
int firstWeekday(int year, int month); // Sunday = 0.
std::wstring clockText(const SYSTEMTIME& now, bool hour24, bool seconds);
UINT nextTick(const SYSTEMTIME& now, bool seconds);
SIZE logicalSize(Kind kind, int preset, int scale, bool seconds);
POINT clampPosition(POINT point, SIZE size, RECT work);
bool parseColor(const std::wstring& text, unsigned& result);
std::wstring colorText(unsigned value);
const wchar_t* kindName(Kind kind);
}
