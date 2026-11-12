#include "model.hpp"
#include "../third_party/json.hpp"
#include <shlobj.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <limits>

namespace dw {
using json = nlohmann::json;
std::string utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (!n) throw std::runtime_error("Invalid Unicode string");
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}
std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Invalid UTF-8 string");
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}
std::filesystem::path defaultRoot() {
    PWSTR p{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &p)))
        throw std::runtime_error("Cannot find Local AppData");
    std::filesystem::path result = std::filesystem::path(p) / L"panes";
    CoTaskMemFree(p);
    return result;
}
const wchar_t* kindName(Kind k) {
    return k == Kind::Clock ? L"Clock" : k == Kind::Date ? L"Date" : L"Calendar";
}
bool leapYear(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
int daysInMonth(int y, int m) {
    static constexpr int days[]{31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1 || m > 12) throw std::invalid_argument("Month out of range");
    return days[m-1] + (m == 2 && leapYear(y) ? 1 : 0);
}
int firstWeekday(int y, int m) {
    if (m < 1 || m > 12 || y < 1601) throw std::invalid_argument("Invalid calendar date");
    static constexpr int offsets[]{0,3,2,5,0,3,5,1,4,6,2,4};
    if (m < 3) --y;
    return (y + y/4 - y/100 + y/400 + offsets[m-1] + 1) % 7;
}
std::wstring clockText(const SYSTEMTIME& now, bool h24, bool seconds) {
    wchar_t buffer[40]{};
    int h = h24 ? now.wHour : (now.wHour % 12 ? now.wHour % 12 : 12);
    if (seconds) swprintf_s(buffer, L"%02d:%02u:%02u", h, now.wMinute, now.wSecond);
    else swprintf_s(buffer, L"%02d:%02u", h, now.wMinute);
    return buffer;
}
UINT nextTick(const SYSTEMTIME& now, bool seconds) {
    return seconds ? 1000 - now.wMilliseconds : 60000 - now.wSecond * 1000 - now.wMilliseconds;
}
SIZE logicalSize(Kind k, int preset, int scale, bool seconds) {
    SIZE base = k == Kind::Clock ? SIZE{seconds ? 660L : 490L, preset ? 140L : 180L}
              : k == Kind::Date ? SIZE{600,75} : SIZE{330,340};
    return {MulDiv(base.cx, scale, 100), MulDiv(base.cy, scale, 100)};
}
POINT clampPosition(POINT p, SIZE s, RECT work) {
    return {std::clamp(p.x, work.left, std::max(work.left, work.right-s.cx)),
            std::clamp(p.y, work.top, std::max(work.top, work.bottom-s.cy))};
}
bool parseColor(const std::wstring& text, unsigned& result) {
    auto s = text;
    if (!s.empty() && s[0] == L'#') s.erase(0,1);
    if (s.size() != 6 || s.find_first_not_of(L"0123456789abcdefABCDEF") != std::wstring::npos) return false;
    result = static_cast<unsigned>(std::stoul(s, nullptr, 16));
    return true;
}
std::wstring colorText(unsigned v) {
    wchar_t s[8]{}; swprintf_s(s, L"#%06X", v & 0xffffff); return s;
}
static int bounded(const json& obj, const char* key, int fallback, int min, int max) {
    if (!obj.contains(key) || !obj.at(key).is_number_integer()) return fallback;
    auto n = obj.at(key).get<std::int64_t>();
    return static_cast<int>(std::clamp(n, static_cast<std::int64_t>(min), static_cast<std::int64_t>(max)));
}
static bool boolean(const json& obj, const char* key, bool fallback) {
    return obj.contains(key) && obj.at(key).is_boolean() ? obj.at(key).get<bool>() : fallback;
}
static std::wstring stringValue(const json& obj, const char* key, const std::wstring& fallback, size_t maxLen) {
    if (!obj.contains(key) || !obj.at(key).is_string()) return fallback;
    auto s = wide(obj.at(key).get<std::string>());
    return s.empty() || s.size() > maxLen ? fallback : s;
}
LoadResult loadConfig(const std::filesystem::path& path) {
    LoadResult result;
    try {
        if (!std::filesystem::exists(path)) return result;
        if (std::filesystem::file_size(path) > 256 * 1024) throw std::runtime_error("Settings file too large");
        std::ifstream stream(path);
        if (!stream) throw std::runtime_error("Cannot read settings");
        const auto obj = json::parse(stream);
        if (!obj.is_object() || !obj.contains("version") || obj.at("version") != ConfigVersion)
            throw std::runtime_error("Unsupported settings version");
        Config& c = result.value;
        c.shown = boolean(obj, "shown", true); c.locked = boolean(obj, "locked", true);
        c.seconds = boolean(obj, "seconds", false); c.hour24 = boolean(obj, "hour24", true);
        c.preset = bounded(obj, "preset", 0, 0, 1);
        if (obj.contains("widgets") && obj.at("widgets").is_array()) {
            for (const auto& entry : obj.at("widgets")) {
                if (!entry.is_object()) continue;
                int id = bounded(entry, "id", -1, -1, 2);
                if (id < 0) continue;
                auto& w = c.widgets[id];
                w.visible = boolean(entry, "visible", true);
                w.positioned = boolean(entry, "positioned", false);
                w.monitor = stringValue(entry, "monitor", L"", 128);
                w.x = bounded(entry, "x", 0, -50000, 50000); w.y = bounded(entry, "y", 0, -50000, 50000);
                w.scale = bounded(entry, "scale", 100, 50, 200); w.opacity = bounded(entry, "opacity", 100, 10, 100);
                w.font = stringValue(entry, "font", L"Segoe UI Light", 100);
                unsigned color;
                if (parseColor(stringValue(entry, "color", L"", 7), color)) w.color = color;
                if (parseColor(stringValue(entry, "accent", L"", 7), color)) w.accent = color;
            }
        }
    } catch (const std::exception& e) {
        result.value = Config{};
        result.warning = L"Settings could not be loaded. Defaults are active. Original file preserved.\n" + wide(e.what());
        result.writable = false;
    }
    return result;
}
void saveConfig(const std::filesystem::path& path, const Config& c) {
    json obj{{"version",ConfigVersion},{"shown",c.shown},{"locked",c.locked},{"seconds",c.seconds},{"hour24",c.hour24},{"preset",c.preset}};
    obj["widgets"] = json::array();
    for (int i = 0; i < 3; ++i) {
        const auto& w = c.widgets[i];
        obj["widgets"].push_back({{"id",i},{"visible",w.visible},{"positioned",w.positioned},{"monitor",utf8(w.monitor)},
            {"x",w.x},{"y",w.y},{"scale",w.scale},{"opacity",w.opacity},{"font",utf8(w.font)},
            {"color",utf8(colorText(w.color))},{"accent",utf8(colorText(w.accent))}});
    }
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path; temporary += L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << obj.dump(2) << '\n'; stream.flush();
        if (!stream) throw std::runtime_error("Cannot write settings");
    }
    // Write-through replacement keeps the previous settings if the rename fails.
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace settings file");
}
}
