#pragma once

#include "Platform/Compositor/compositor_manager.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

namespace zenith {

class SwayIPCBackend : public ICompositorBackend {
public:
    SwayIPCBackend();
    ~SwayIPCBackend() override;

    void init() override;
    void switch_workspace(int id) override;
    void switch_workspace_relative(int delta) override;
    void focus_window(const std::string& target) override;
    void close_window(const std::string& address) override;
    void exit_session() override;
    int get_active_workspace_id() override;
    std::string get_clients_json() override;

    // Direct IPC socket message sending
    static std::string send_message(const std::string& sock_path, uint32_t type, const std::string& payload);

private:
    std::string socket_path;
    std::atomic<bool> running{false};
    std::atomic<int> active_workspace{1};
    std::thread event_thread;

    void listen_loop();
    void handle_event(uint32_t event_type, const std::string& payload);
};

} // namespace zenith
