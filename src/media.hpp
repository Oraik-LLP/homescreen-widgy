#pragma once
#include <windows.h>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace dw {
enum class MediaCommand { Previous, Toggle, Next };
struct MediaSnapshot {
    bool apiAvailable = false, hasSession = false, playing = false;
    bool canPrevious = false, canToggle = false, canNext = false;
    std::wstring source, title, artist;
    unsigned thumbnailWidth = 0, thumbnailHeight = 0;
    std::vector<unsigned char> thumbnail;
};

class MediaController {
    mutable std::mutex mutex_;
    std::thread worker_;
    std::condition_variable wake_;
    HWND notify_{};
    bool stopping_ = false, enabled_ = false;
    int pendingCommand_ = -1;
    MediaSnapshot snapshot_;
    void run();
public:
    MediaController() = default;
    ~MediaController();
    MediaController(const MediaController&) = delete;
    MediaController& operator=(const MediaController&) = delete;
    void start(HWND notify);
    void stop();
    void setEnabled(bool enabled);
    void command(MediaCommand command);
    MediaSnapshot snapshot() const;
};
}
