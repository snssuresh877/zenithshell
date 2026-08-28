#include "shell/bar/sys_info_widget.hpp"
#include "gtk3_compat.hpp"
#include "system/sys_monitor.hpp"
#include <cstdio>
#include <cstdlib>

namespace zenith {

GtkWidget* SysInfoWidget::cpu_label = nullptr;
GtkWidget* SysInfoWidget::ram_label = nullptr;
GtkWidget* SysInfoWidget::bat_label = nullptr;

GtkWidget* SysInfoWidget::create(int update_interval_ms) {
    GtkWidget* btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "pill-widget");
    gtk_widget_add_css_class(btn, "sys-stats-btn");

    GtkWidget* container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    // CPU (Waybar / QuickShell Icon: )
    GtkWidget* cpu_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* cpu_icon = gtk_label_new("");
    gtk_widget_add_css_class(cpu_icon, "sys-cpu-icon");

    cpu_label = gtk_label_new("0%");
    gtk_widget_add_css_class(cpu_label, "sys-cpu-text");

    gtk_box_pack_start(GTK_BOX(cpu_box), cpu_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cpu_box), cpu_label, FALSE, FALSE, 0);

    // Separator ("|")
    GtkWidget* sep = gtk_label_new("|");
    gtk_widget_add_css_class(sep, "sys-vsep-lbl");

    // RAM (Waybar / QuickShell Icon: )
    GtkWidget* ram_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* ram_icon = gtk_label_new("");
    gtk_widget_add_css_class(ram_icon, "sys-ram-icon");

    ram_label = gtk_label_new("0%");
    gtk_widget_add_css_class(ram_label, "sys-ram-text");

    gtk_box_pack_start(GTK_BOX(ram_box), ram_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ram_box), ram_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(container), cpu_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), sep, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), ram_box, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(btn), container);

    g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        system("foot --app-id=btop_float -e btop 2>/dev/null &");
    }), nullptr);

    g_timeout_add(update_interval_ms, update_stats, nullptr);
    update_stats(nullptr);

    return btn;
}

gboolean SysInfoWidget::update_stats(gpointer) {
    SysStats stats = SysMonitor::get_stats();

    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f%%", stats.cpu_usage);
    gtk_label_set_text(GTK_LABEL(cpu_label), buf);

    if (stats.cpu_usage > 85) {
        gtk_widget_remove_css_class(cpu_label, "stat-warning");
        gtk_widget_add_css_class(cpu_label, "stat-urgent");
    } else if (stats.cpu_usage > 65) {
        gtk_widget_remove_css_class(cpu_label, "stat-urgent");
        gtk_widget_add_css_class(cpu_label, "stat-warning");
    } else {
        gtk_widget_remove_css_class(cpu_label, "stat-urgent");
        gtk_widget_remove_css_class(cpu_label, "stat-warning");
    }

    snprintf(buf, sizeof(buf), "%.0f%%", stats.ram_usage);
    gtk_label_set_text(GTK_LABEL(ram_label), buf);

    if (stats.ram_usage > 85) {
        gtk_widget_remove_css_class(ram_label, "stat-warning");
        gtk_widget_add_css_class(ram_label, "stat-urgent");
    } else if (stats.ram_usage > 70) {
        gtk_widget_remove_css_class(ram_label, "stat-urgent");
        gtk_widget_add_css_class(ram_label, "stat-warning");
    } else {
        gtk_widget_remove_css_class(ram_label, "stat-urgent");
        gtk_widget_remove_css_class(ram_label, "stat-warning");
    }

    return TRUE;
}

} // namespace zenith
