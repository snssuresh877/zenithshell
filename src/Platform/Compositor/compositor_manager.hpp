#pragma once

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <mutex>

namespace zenith {

enum class CompositorType {
    Hyprland,
    Sway,
    Cosmic,
    Wayfire,
    River,
    Generic
};

class ICompositorBackend {
public:
    virtual ~ICompositorBackend() = default;
    virtual void init() = 0;
    virtual void switch_workspace(int id) = 0;
    virtual void switch_workspace_relative(int delta) = 0;
    virtual void focus_window(const std::string& target) = 0;
    virtual void close_window(const std::string& address) = 0;
    virtual void exit_session() = 0;
    virtual int get_active_workspace_id() = 0;
    virtual std::string get_clients_json() = 0;
};

class CompositorManager {
public:
    using WorkspaceCallback = std::function<void(int active_id)>;
    using WindowTitleCallback = std::function<void(const std::string& title)>;
    using WindowEventCallback = std::function<void()>;

    static CompositorManager& instance();

    void init();
    CompositorType get_type() const { return current_type; }
    std::string get_name() const;

    void add_workspace_callback(WorkspaceCallback cb);
    void add_window_title_callback(WindowTitleCallback cb);
    void add_window_event_callback(WindowEventCallback cb);

    void set_workspace_callback(WorkspaceCallback cb) { add_workspace_callback(cb); }
    void set_window_title_callback(WindowTitleCallback cb) { add_window_title_callback(cb); }
    void set_window_event_callback(WindowEventCallback cb) { add_window_event_callback(cb); }

    // Unified actions
    void switch_workspace(int id);
    void switch_workspace_relative(int delta);
    void focus_window(const std::string& target);
    void close_window(const std::string& address);
    void exit_session();

    int get_active_workspace_id();
    std::string get_clients_json();

    // Event notification dispatchers for backends
    void notify_workspace(int active_id);
    void notify_window_title(const std::string& title);
    void notify_window_event();

private:
    CompositorManager();
    ~CompositorManager();

    CompositorType current_type = CompositorType::Generic;
    std::unique_ptr<ICompositorBackend> backend;
    std::mutex cb_mutex;

    std::vector<WorkspaceCallback> workspace_cbs;
    std::vector<WindowTitleCallback> window_title_cbs;
    std::vector<WindowEventCallback> window_event_cbs;

    void detect_and_setup_backend();
};

} // namespace zenith
