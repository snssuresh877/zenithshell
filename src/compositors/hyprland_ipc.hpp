#pragma once

#include <string>
#include <functional>
#include <vector>
#include <thread>
#include <glib.h>

namespace zenith {

class HyprlandIPC {
public:
    using WorkspaceCallback = std::function<void(int active_id)>;
    using WindowTitleCallback = std::function<void(const std::string& title)>;
    using WindowEventCallback = std::function<void()>;

    static HyprlandIPC& instance();

    void init();
    void add_workspace_callback(WorkspaceCallback cb);
    void add_window_title_callback(WindowTitleCallback cb);
    void add_window_event_callback(WindowEventCallback cb);

    void set_workspace_callback(WorkspaceCallback cb) { add_workspace_callback(cb); }
    void set_window_title_callback(WindowTitleCallback cb) { add_window_title_callback(cb); }
    void set_window_event_callback(WindowEventCallback cb) { add_window_event_callback(cb); }
    
    static void switch_workspace(int workspace_id);
    static void switch_workspace_relative(int delta);

private:
    HyprlandIPC() = default;
    ~HyprlandIPC();

    std::string socket_path;
    bool running = false;
    std::thread ipc_thread;

    std::vector<WorkspaceCallback> workspace_cbs;
    std::vector<WindowTitleCallback> window_title_cbs;
    std::vector<WindowEventCallback> window_event_cbs;

    void listen_loop();
    void handle_event(const std::string& event_line);
};

} // namespace zenith
