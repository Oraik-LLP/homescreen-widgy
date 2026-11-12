#pragma once
#include "model.hpp"
#include "media.hpp"
#include <d2d1.h>
#include <dwrite_3.h>
#include <map>
#include <memory>

namespace dw {
template<class T> void release(T*& p) { if(p) { p->Release(); p = nullptr; } }
class Graphics {
public:
    ID2D1Factory* d2d{};
    IDWriteFactory* text{};
    IDWriteFontCollection* fonts{};
    IDWriteFactory5* modern{};
    IDWriteInMemoryFontFileLoader* memoryLoader{};
    IDWriteFontCollection1* bundled{};
    Graphics();
    ~Graphics();
    std::wstring resolveFont(const std::wstring& requested) const;
};
class Surface {
    Graphics& graphics;
    HDC dc{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    ID2D1DCRenderTarget* target{};
    ID2D1SolidColorBrush* brush{};
    SIZE pixels{};
    std::map<std::wstring, IDWriteTextFormat*> formats;
    void ensure(SIZE size);
    void text(const std::wstring& value, const std::wstring& font, float size, D2D1_RECT_F box, unsigned color, bool centered = true);
    void setColor(unsigned color, float alpha = 1.0f);
public:
    explicit Surface(Graphics& graphics);
    ~Surface();
    Surface(const Surface&) = delete;
    void clearFormats();
    void draw(HWND window, const WidgetConfig& widget, const Config& config, const SYSTEMTIME& now, bool editing, bool present = true, UINT forcedDpi = 0, const MediaSnapshot* media = nullptr);
    void preview(HDC destination, RECT box) const;
    void saveBmp(const std::filesystem::path& path) const;
    SIZE size() const { return pixels; }
};
}
