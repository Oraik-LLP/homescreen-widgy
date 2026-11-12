#include "media.hpp"
#include <roapi.h>
#include <winstring.h>
#include <shcore.h>
#include <wincodec.h>
#include <algorithm>
#include <chrono>
#include <cwctype>
#include <utility>

namespace dw {
namespace {
constexpr UINT MediaMessage = WM_APP + 6;
constexpr GUID ManagerStatics{0x2050c4ee,0x11a0,0x57de,{0xae,0xd7,0xc9,0x7c,0x70,0x33,0x82,0x45}};
constexpr GUID AsyncInfoId{0x00000036,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
constexpr GUID StreamId{0x0000000c,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

template<class R>
HRESULT call(IInspectable* object, size_t slot, R* result) {
    if (!object) return E_POINTER;
    auto vtable = *reinterpret_cast<void***>(object);
    using Function = HRESULT(STDMETHODCALLTYPE*)(IInspectable*, R*);
    return reinterpret_cast<Function>(vtable[slot])(object, result);
}

template<class T>
bool await(IInspectable* operation, T& result, DWORD timeout = 2500) {
    if (!operation) return false;
    IInspectable* info{};
    if (FAILED(operation->QueryInterface(AsyncInfoId, reinterpret_cast<void**>(&info)))) {
        operation->Release(); return false;
    }
    const ULONGLONG until = GetTickCount64() + timeout;
    INT32 status = 0;
    HRESULT hr = S_OK;
    do {
        hr = call(info, 7, &status);
        if (FAILED(hr) || status != 0) break;
        if (GetTickCount64() >= until) { hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT); break; }
        Sleep(10);
    } while (true);
    if (SUCCEEDED(hr) && status == 1) hr = call(operation, 8, &result);
    info->Release(); operation->Release();
    return SUCCEEDED(hr) && status == 1;
}

void release(IInspectable*& object) { if (object) { object->Release(); object = nullptr; } }
std::wstring hstringValue(HSTRING value) {
    UINT32 length{};
    auto text = WindowsGetStringRawBuffer(value, &length);
    std::wstring out = text ? std::wstring(text, length) : std::wstring{};
    if (value) WindowsDeleteString(value);
    return out;
}
bool property(IInspectable* object, size_t slot, std::wstring& out) {
    HSTRING value{};
    HRESULT hr = call(object, slot, &value);
    if (FAILED(hr)) return false;
    out = hstringValue(value);
    return true;
}
bool property(IInspectable* object, size_t slot, bool& out) {
    return SUCCEEDED(call(object, slot, &out));
}
std::wstring friendlySource(std::wstring id) {
    auto split = id.find_last_of(L"\\/!");
    if (split != std::wstring::npos) id.erase(0, split + 1);
    if (id.ends_with(L".exe") || id.ends_with(L".EXE")) id.resize(id.size() - 4);
    std::wstring lower = id;
    std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
    if (lower.find(L"firefox") != std::wstring::npos) return L"Firefox";
    if (lower.find(L"spotify") != std::wstring::npos) return L"Spotify";
    if (lower.find(L"chrome") != std::wstring::npos) return L"Chrome";
    if (lower.find(L"msedge") != std::wstring::npos || lower.find(L"edge") != std::wstring::npos) return L"Microsoft Edge";
    if (lower.find(L"vlc") != std::wstring::npos) return L"VLC media player";
    if (lower.find(L"zunemusic") != std::wstring::npos || lower.find(L"groove") != std::wstring::npos) return L"Media Player";
    return id.empty() ? L"Now playing" : id;
}

std::vector<unsigned char> readThumbnail(IInspectable* reference, unsigned& width, unsigned& height) {
    IInspectable* operation{};
    if (FAILED(call(reference, 6, &operation))) return {};
    IInspectable* randomAccess{};
    if (!await(operation, randomAccess)) return {};
    IStream* stream{};
    if (FAILED(CreateStreamOverRandomAccessStream(randomAccess, StreamId, reinterpret_cast<void**>(&stream)))) {
        randomAccess->Release(); return {};
    }
    randomAccess->Release();

    IWICImagingFactory* factory{}; IWICBitmapDecoder* decoder{}; IWICBitmapFrameDecode* frame{};
    IWICBitmapScaler* scaler{}; IWICFormatConverter* converter{};
    std::vector<unsigned char> pixels;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    UINT sourceWidth{}, sourceHeight{};
    if (SUCCEEDED(hr)) hr = frame->GetSize(&sourceWidth, &sourceHeight);
    if (SUCCEEDED(hr) && sourceWidth && sourceHeight) {
        const double ratio = std::min(128.0 / sourceWidth, 128.0 / sourceHeight);
        width = std::max(1u, static_cast<UINT>(sourceWidth * ratio));
        height = std::max(1u, static_cast<UINT>(sourceHeight * ratio));
        hr = factory->CreateBitmapScaler(&scaler);
        if (SUCCEEDED(hr)) hr = scaler->Initialize(frame, width, height, WICBitmapInterpolationModeFant);
        if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
        if (SUCCEEDED(hr)) hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
        if (SUCCEEDED(hr)) {
            pixels.resize(static_cast<size_t>(width) * height * 4);
            hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
        }
    } else if (SUCCEEDED(hr)) hr = E_FAIL;
    if (FAILED(hr)) { pixels.clear(); width = height = 0; }
    if (converter) converter->Release(); if (scaler) scaler->Release(); if (frame) frame->Release();
    if (decoder) decoder->Release(); if (factory) factory->Release(); stream->Release();
    return pixels;
}

class SessionApi {
    IInspectable* manager_{};
public:
    ~SessionApi() { release(manager_); }
    bool available() const { return manager_ != nullptr; }
    void reset() { release(manager_); }
    void acquire() {
        if (manager_) return;
        HSTRING className{};
        static constexpr wchar_t name[] = L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";
        if (FAILED(WindowsCreateString(name, static_cast<UINT32>(std::size(name) - 1), &className))) return;
        IInspectable* factory{};
        HRESULT hr = RoGetActivationFactory(className, ManagerStatics, reinterpret_cast<void**>(&factory));
        WindowsDeleteString(className);
        if (FAILED(hr)) return;
        IInspectable* operation{};
        hr = call(factory, 6, &operation);
        factory->Release();
        if (FAILED(hr) || !await(operation, manager_)) release(manager_);
    }
    IInspectable* current() const {
        if (!manager_) return nullptr;
        IInspectable* session{};
        if (FAILED(call(manager_, 6, &session))) return nullptr;
        return session;
    }
    MediaSnapshot query(const MediaSnapshot& previous) {
        MediaSnapshot result;
        result.apiAvailable = available();
        IInspectable* session = current();
        if (!session) return result;
        result.hasSession = true;
        property(session, 6, result.source);
        result.source = friendlySource(result.source);

        IInspectable* playback{};
        INT32 state = 0;
        if (SUCCEEDED(call(session, 9, &playback))&&playback) {
            call(playback, 7, &state);
            result.playing = state == 4;
            IInspectable* controls{};
            if (SUCCEEDED(call(playback, 6, &controls))&&controls) {
                bool value{};
                if (property(controls, 12, value)) result.canNext = value != FALSE;
                if (property(controls, 13, value)) result.canPrevious = value != FALSE;
                if (property(controls, 16, value)) result.canToggle = value != FALSE;
                if (!result.canToggle) {
                    bool play{}, pause{};
                    property(controls, 6, play); property(controls, 7, pause);
                    result.canToggle = play || pause;
                }
                controls->Release();
            }
            playback->Release();
        }

        IInspectable* operation{};
        if (SUCCEEDED(call(session, 7, &operation))) {
            IInspectable* properties{};
            if (await(operation, properties)&&properties) {
                property(properties, 6, result.title);
                property(properties, 9, result.artist);
                if (result.source == previous.source && result.title == previous.title && result.artist == previous.artist) {
                    result.thumbnail = previous.thumbnail;
                    result.thumbnailWidth = previous.thumbnailWidth;
                    result.thumbnailHeight = previous.thumbnailHeight;
                } else {
                    IInspectable* thumbnail{};
                    if (SUCCEEDED(call(properties, 15, &thumbnail))&&thumbnail) {
                        result.thumbnail = readThumbnail(thumbnail, result.thumbnailWidth, result.thumbnailHeight);
                        thumbnail->Release();
                    }
                }
                properties->Release();
            }
        }
        session->Release();
        return result;
    }
    bool execute(int command) {
        IInspectable* session = current();
        if (!session) return false;
        size_t slot = command == static_cast<int>(MediaCommand::Previous) ? 17
                    : command == static_cast<int>(MediaCommand::Next) ? 16 : 20;
        IInspectable* operation{};
        HRESULT hr = call(session, slot, &operation);
        session->Release();
        bool success{};
        return SUCCEEDED(hr) && await(operation, success, 1500) && success;
    }
};

void sendMediaKey(int command) {
    WORD key = command == static_cast<int>(MediaCommand::Previous) ? VK_MEDIA_PREV_TRACK
             : command == static_cast<int>(MediaCommand::Next) ? VK_MEDIA_NEXT_TRACK : VK_MEDIA_PLAY_PAUSE;
    INPUT events[2]{};
    events[0].type = INPUT_KEYBOARD; events[0].ki.wVk = key;
    events[1].type = INPUT_KEYBOARD; events[1].ki.wVk = key; events[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, events, sizeof(INPUT));
}
bool same(const MediaSnapshot& a, const MediaSnapshot& b) {
    return a.apiAvailable == b.apiAvailable && a.hasSession == b.hasSession && a.playing == b.playing &&
        a.canPrevious == b.canPrevious && a.canToggle == b.canToggle && a.canNext == b.canNext &&
        a.source == b.source && a.title == b.title && a.artist == b.artist &&
        a.thumbnailWidth == b.thumbnailWidth && a.thumbnailHeight == b.thumbnailHeight && a.thumbnail == b.thumbnail;
}
}

MediaController::~MediaController() { stop(); }
void MediaController::start(HWND notify) {
    std::lock_guard lock(mutex_);
    if (worker_.joinable()) return;
    notify_ = notify; stopping_ = false;
    worker_ = std::thread([this] { run(); });
}
void MediaController::stop() {
    { std::lock_guard lock(mutex_); if (!worker_.joinable()) return; stopping_ = true; }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}
void MediaController::setEnabled(bool enabled) {
    HWND notify{}; bool changed{};
    {
        std::lock_guard lock(mutex_);
        changed = enabled_ != enabled; enabled_ = enabled; notify = notify_;
        if (!enabled) { pendingCommand_ = -1; snapshot_ = {}; }
    }
    if (changed && !enabled && notify) PostMessageW(notify, MediaMessage, 0, 0);
    if (changed) wake_.notify_all();
}
void MediaController::command(MediaCommand commandValue) {
    std::lock_guard lock(mutex_);
    if (!enabled_ || stopping_) return;
    pendingCommand_ = static_cast<int>(commandValue);
    wake_.notify_all();
}
MediaSnapshot MediaController::snapshot() const { std::lock_guard lock(mutex_); return snapshot_; }
void MediaController::run() {
    const HRESULT init = RoInitialize(RO_INIT_MULTITHREADED);
    SessionApi api;
    for (;;) {
        bool enabled{}, stopping{}; int commandValue = -1;
        {
            std::lock_guard lock(mutex_);
            enabled = enabled_; stopping = stopping_; commandValue = std::exchange(pendingCommand_, -1);
        }
        if (stopping) break;
        if (enabled) {
            api.acquire();
            MediaSnapshot previous = snapshot();
            auto next = api.query(previous);
            if (commandValue >= 0 && !api.execute(commandValue)) sendMediaKey(commandValue);
            HWND notify{}; bool changed{};
            {
                std::lock_guard lock(mutex_);
                if(enabled_&&!stopping_){changed = !same(snapshot_, next); snapshot_ = std::move(next); notify = notify_;}
            }
            if (changed && notify) PostMessageW(notify, MediaMessage, 0, 0);
        } else if (api.available()) {
            // Release WinRT media-session objects while the optional widget is hidden.
            api.reset();
        }
        std::unique_lock lock(mutex_);
        if (stopping_) break;
        if(enabled_)wake_.wait_for(lock, std::chrono::seconds(1));
        else wake_.wait(lock,[this]{return stopping_||enabled_;});
    }
    api.reset();
    if (SUCCEEDED(init)) RoUninitialize();
}
}
