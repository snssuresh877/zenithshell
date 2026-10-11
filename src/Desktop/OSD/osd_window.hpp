#pragma once

#include <gtk/gtk.h>
#include <string>
#include <memory>
#include "Core/EventBus/event_bus.hpp"
#include "Core/Events/audio_events.hpp"
#include "Core/Events/brightness_events.hpp"

namespace zenith {

enum class OSDType {
    Volume,
    Brightness,
    Microphone
};

class OSDWindow {
public:
    static void init(GtkApplication* app, std::shared_ptr<EventBus> event_bus = nullptr);
    static void cleanup();

    static void show_volume(int volume_percent, bool muted);
    static void show_brightness(int brightness_percent);
    static void show_mic(int volume_percent, bool muted);
    static void show_custom(const std::string& icon, const std::string& title, int percent, bool muted = false);
    static void hide();

private:
    static void ensure_window();
    static void reset_hide_timer();

    static GtkApplication* s_app;
    static GtkWidget* s_window;
    static GtkWidget* s_card;
    static GtkWidget* s_icon_lbl;
    static GtkWidget* s_title_lbl;
    static GtkWidget* s_val_lbl;
    static GtkWidget* s_progress_bar;
    static guint s_hide_timer_id;

    static std::shared_ptr<EventBus> s_event_bus;
    static SubscriptionId s_volume_subscription_id;
    static SubscriptionId s_mic_subscription_id;
    static SubscriptionId s_brightness_subscription_id;
};

} // namespace zenith
