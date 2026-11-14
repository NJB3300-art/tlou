// Wi-Fi transfer: a small HTTP server on the console that takes the game folders from a PC on
// the same network, from a browser page it serves itself or from tools/xbox_setup.py.
//
//   GET  /                         the upload page
//   GET  /api/status?code=C        free space and where files go
//   GET  /api/info?code=C&path=P   size of P on the console (-1: missing), for resuming
//   PUT  /api/file?code=C&path=P&offset=O&total=T   bytes of P from O; T is P's full size
//   POST /api/done?code=C          the PC is done: the launcher looks at the folders again
//
// P is relative to the TLOU folder and starts with CUSA00552/, CUSA00552-patch/ or stage/.
// C is the pairing code shown on the TV; requests without it are refused.
#pragma once
#include "pch.h"

namespace tlou {

class Receiver {
public:
    struct Status {
        bool running = false;
        std::string address;   // http://192.168.1.20:8077
        std::string code;      // six digits
        std::string current;   // file being received
        uint64_t files_done = 0, bytes = 0;
        std::string message;   // last event or error
    };

    explicit Receiver(std::filesystem::path root) : root(std::move(root)) {}
    ~Receiver();
    void Start();
    void Stop();
    Status Snapshot();
    // True once after a transfer finished (POST /api/done) or a file completed: rescan.
    bool TakeChanged() { return changed.exchange(false); }

    static constexpr uint16_t Port = 8077;

private:
    winrt::fire_and_forget Listen();
    winrt::fire_and_forget Serve(winrt::Windows::Networking::Sockets::StreamSocket socket);
    void Set(std::function<void(Status&)> change);

    std::filesystem::path root;
    winrt::Windows::Networking::Sockets::StreamSocketListener listener{nullptr};
    std::mutex mutex;
    Status status;
    std::atomic<bool> changed{false};
};

} // namespace tlou
