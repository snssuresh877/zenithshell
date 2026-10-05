#pragma once

#include "Platform/Compositor/compositor_manager.hpp"
#include <string>
#include <functional>
#include <vector>
#include <thread>
#include <glib.h>

namespace zenith {

class HyprlandBackend : public ICompositorBackend {
public:
    HyprlandBackend();
    ~HyprlandBackend() override;

    void init() override;
    void switch_workspace(int id) override;
    void switch_workspace_relative(int delta) override;
    void focus_window(const std::string& target) override;
    void close_window(const std::string& address) override;
    void exit_session() override;
    int get_active_workspace_id() override;
    std::string get_clients_json() override;

    // Direct Socket IPC methods (zero subprocess overhead)
    static std::string request(const std::string& cmd);
    static std::string query_json(const std::string& endpoint);
    static bool dispatch(const std::string& cmd);
    static bool send_command(const std::string& cmd);
    static void exit_session_static();
    static int get_active_workspace_id_static();
    static std::string get_clients_json_static();

private:
    std::string event_socket_path;
    std::string req_socket_path;
    bool running = false;
    std::thread ipc_thread;

    void listen_loop();
    void handle_event(const std::string& event_line);
};

// Backward-compatible façade that routes through CompositorManager
class HyprlandIPC {
public:
    using WorkspaceCallback = CompositorManager::WorkspaceCallback;
    using WindowTitleCallback = CompositorManager::WindowTitleCallback;

    static HyprlandIPC& instance() {
        static HyprlandIPC inst;
        return inst;
    }

    void init() { CompositorManager::instance().init(); }
    void add_workspace_callback(WorkspaceCallback cb) { CompositorManager::instance().add_workspace_callback(cb); }
    void add_window_title_callback(WindowTitleCallback cb) { CompositorManager::instance().add_window_title_callback(cb); }

    void set_workspace_callback(WorkspaceCallback cb) { add_workspace_callback(cb); }
    void set_window_title_callback(WindowTitleCallback cb) { add_window_title_callback(cb); }

    static std::string request(const std::string& cmd) { return HyprlandBackend::request(cmd); }
    static std::string query_json(const std::string& endpoint) { return HyprlandBackend::query_json(endpoint); }
    static bool dispatch(const std::string& cmd) { return HyprlandBackend::dispatch(cmd); }

    static void switch_workspace(int workspace_id) { CompositorManager::instance().switch_workspace(workspace_id); }
    static void switch_workspace_relative(int delta) { CompositorManager::instance().switch_workspace_relative(delta); }
    static void focus_window(const std::string& target) { CompositorManager::instance().focus_window(target); }
    static void close_window(const std::string& address) { CompositorManager::instance().close_window(address); }
    static void exit() { CompositorManager::instance().exit_session(); }
    static bool send_command(const std::string& cmd) { return HyprlandBackend::send_command(cmd); }
    static int get_active_workspace_id() { return CompositorManager::instance().get_active_workspace_id(); }
    static std::string get_clients_json() { return CompositorManager::instance().get_clients_json(); }
};

} // namespace zenith
