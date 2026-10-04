#include "Platform/Compositor/compositor_manager.hpp"
#include "Platform/Compositor/hyprland_ipc.hpp"
#include "Platform/Compositor/sway_ipc.hpp"
#include "Platform/Compositor/generic_compositor.hpp"
#include <iostream>
#include <cstdlib>

namespace zenith {

CompositorManager::CompositorManager() {
    detect_and_setup_backend();
}

CompositorManager::~CompositorManager() = default;

CompositorManager& CompositorManager::instance() {
    static CompositorManager inst;
    return inst;
}

void CompositorManager::detect_and_setup_backend() {
    const char* his = std::getenv("HYPRLAND_INSTANCE_SIGNATURE");
    const char* swaysock = std::getenv("SWAYSOCK");
    if (!swaysock) swaysock = std::getenv("I3SOCK");
    const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
    const char* session = std::getenv("DESKTOP_SESSION");
    const char* wayfire = std::getenv("WAYFIRE_CONFIG_FILE");
    const char* river = std::getenv("RIVERSOCK");

    if (his && *his) {
        current_type = CompositorType::Hyprland;
        backend = std::make_unique<HyprlandBackend>();
    } else if (swaysock && *swaysock) {
        current_type = CompositorType::Sway;
        backend = std::make_unique<SwayIPCBackend>();
    } else if ((desktop && (std::string(desktop).find("COSMIC") != std::string::npos || std::string(desktop).find("cosmic") != std::string::npos)) ||
               (session && std::string(session).find("cosmic") != std::string::npos)) {
        current_type = CompositorType::Cosmic;
        backend = std::make_unique<GenericCompositorBackend>();
    } else if (river && *river) {
        current_type = CompositorType::River;
        backend = std::make_unique<GenericCompositorBackend>();
    } else if (wayfire && *wayfire) {
        current_type = CompositorType::Wayfire;
        backend = std::make_unique<GenericCompositorBackend>();
    } else {
        current_type = CompositorType::Generic;
        backend = std::make_unique<GenericCompositorBackend>();
    }
}

std::string CompositorManager::get_name() const {
    switch (current_type) {
        case CompositorType::Hyprland: return "Hyprland";
        case CompositorType::Sway: return "Sway";
        case CompositorType::Cosmic: return "COSMIC";
        case CompositorType::Wayfire: return "Wayfire";
        case CompositorType::River: return "River";
        default: return "Generic Wayland";
    }
}

void CompositorManager::init() {
    std::cout << "[CompositorManager] Active Compositor: " << get_name() << std::endl;
    if (backend) {
        backend->init();
    }
}

void CompositorManager::add_workspace_callback(WorkspaceCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex);
    workspace_cbs.push_back(cb);
}

void CompositorManager::add_window_title_callback(WindowTitleCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex);
    window_title_cbs.push_back(cb);
}

void CompositorManager::add_window_event_callback(WindowEventCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex);
    window_event_cbs.push_back(cb);
}

void CompositorManager::switch_workspace(int id) {
    if (backend) backend->switch_workspace(id);
}

void CompositorManager::switch_workspace_relative(int delta) {
    if (backend) backend->switch_workspace_relative(delta);
}

void CompositorManager::focus_window(const std::string& target) {
    if (backend) backend->focus_window(target);
}

void CompositorManager::close_window(const std::string& address) {
    if (backend) backend->close_window(address);
}

void CompositorManager::exit_session() {
    if (backend) backend->exit_session();
}

int CompositorManager::get_active_workspace_id() {
    return backend ? backend->get_active_workspace_id() : 1;
}

std::string CompositorManager::get_clients_json() {
    return backend ? backend->get_clients_json() : "[]";
}

void CompositorManager::notify_workspace(int active_id) {
    std::vector<WorkspaceCallback> cbs;
    {
        std::lock_guard<std::mutex> lock(cb_mutex);
        cbs = workspace_cbs;
    }
    for (const auto& cb : cbs) {
        if (cb) cb(active_id);
    }
}

void CompositorManager::notify_window_title(const std::string& title) {
    std::vector<WindowTitleCallback> cbs;
    {
        std::lock_guard<std::mutex> lock(cb_mutex);
        cbs = window_title_cbs;
    }
    for (const auto& cb : cbs) {
        if (cb) cb(title);
    }
}

void CompositorManager::notify_window_event() {
    std::vector<WindowEventCallback> cbs;
    {
        std::lock_guard<std::mutex> lock(cb_mutex);
        cbs = window_event_cbs;
    }
    for (const auto& cb : cbs) {
        if (cb) cb();
    }
}

} // namespace zenith
