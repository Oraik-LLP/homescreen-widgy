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
inline constexpr int ConfigVersion = 1;
enum class Kind { Clock, Date, Calendar };
struct WidgetConfig {
    Kind kind = Kind::Clock;
    bool visible = true;
    bool positioned = false;
    std::wstring monitor;
    int x = 0, y = 0; // DIPs relative to the monitor work area.
    int scale = 100, opacity = 100;
    std::wstring font = L"Segoe UI Light";
    unsigned color = 0xF2F4F1, accent = 0x91B89D;
};
struct Config {
    bool shown = true, locked = true, seconds = false, hour24 = true;
    int preset = 0; // 0: Thin, 1: Compact.
    std::array<WidgetConfig, 3> widgets;
    Config() { for (int i = 0; i < 3; ++i) widgets[i].kind = static_cast<Kind>(i); }
};
struct LoadResult { Config value; std::wstring warning; bool writable = true; };
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
