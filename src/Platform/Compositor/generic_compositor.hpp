#pragma once

#include "Platform/Compositor/compositor_manager.hpp"
#include <atomic>

namespace zenith {

class GenericCompositorBackend : public ICompositorBackend {
public:
    GenericCompositorBackend();
    ~GenericCompositorBackend() override = default;

    void init() override;
    void switch_workspace(int id) override;
    void switch_workspace_relative(int delta) override;
    void focus_window(const std::string& target) override;
    void close_window(const std::string& address) override;
    void exit_session() override;
    int get_active_workspace_id() override;
    std::string get_clients_json() override;

private:
    std::atomic<int> current_workspace{1};
};

} // namespace zenith
