#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <string>
#include "Engine/Config/config.hpp"
#include "Core/EventBus/event_bus.hpp"
#include "Core/Events/compositor_events.hpp"

namespace zenith {

class BarWindow {
public:
    static GtkWidget* create(GtkApplication* app, const Config& config, std::shared_ptr<EventBus> event_bus = nullptr);
    static void toggle();
    static void show();
    static void hide();

    static void update_title(const std::string& title);
    static void cleanup();

private:
    static GtkWidget* window;
    static GtkWidget* active_app_icon;
    static GtkWidget* active_title_label;

    static std::shared_ptr<EventBus> event_bus_;
    static SubscriptionId title_subscription_id_;
};

} // namespace zenith
