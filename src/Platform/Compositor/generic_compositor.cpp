#include "Platform/Compositor/generic_compositor.hpp"
#include <iostream>
#include <cstdlib>
#include <glib.h>

namespace zenith {

GenericCompositorBackend::GenericCompositorBackend() : current_workspace(1) {}

void GenericCompositorBackend::init() {
    std::cout << "[Compositor] Generic/COSMIC Wayland backend initialized." << std::endl;
    // Initial workspace broadcast on main thread
    g_idle_add([](gpointer data) -> gboolean {
        auto* self = static_cast<GenericCompositorBackend*>(data);
        CompositorManager::instance().notify_workspace(self->current_workspace.load());
        return G_SOURCE_REMOVE;
    }, this);
}

void GenericCompositorBackend::switch_workspace(int id) {
    if (id < 1) id = 1;
    current_workspace.store(id);
    std::cout << "[Compositor] Generic workspace switched to: " << id << std::endl;

    g_idle_add([](gpointer data) -> gboolean {
        int ws = GPOINTER_TO_INT(data);
        CompositorManager::instance().notify_workspace(ws);
        return G_SOURCE_REMOVE;
    }, GINT_TO_POINTER(id));
}

void GenericCompositorBackend::switch_workspace_relative(int delta) {
    int cur = current_workspace.load();
    int next = std::max(1, cur + delta);
    switch_workspace(next);
}

void GenericCompositorBackend::focus_window(const std::string& target) {
    std::cout << "[Compositor] Generic focus window requested for: " << target << std::endl;
}

void GenericCompositorBackend::close_window(const std::string& address) {
    std::cout << "[Compositor] Generic close window requested for: " << address << std::endl;
}

void GenericCompositorBackend::exit_session() {
    std::cout << "[Compositor] Terminating Wayland session via loginctl/systemd..." << std::endl;
    g_spawn_command_line_async("loginctl terminate-session self", nullptr);
}

int GenericCompositorBackend::get_active_workspace_id() {
    return current_workspace.load();
}

std::string GenericCompositorBackend::get_clients_json() {
    return "[]";
}

} // namespace zenith
