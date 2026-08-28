#include "shell/launcher/launcher_widget.hpp"
#include "shell/launcher/spotlight_search.hpp"
#include "gtk3_compat.hpp"
#include <iostream>

namespace zenith {

GtkWidget* LauncherWidget::create() {
    GtkWidget* btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "pill-widget");
    gtk_widget_add_css_class(btn, "launcher-pill");
    gtk_widget_set_size_request(btn, -1, 32);

    GtkWidget* lbl = gtk_label_new("󰣇");
    gtk_widget_add_css_class(lbl, "launcher-icon");
    gtk_container_add(GTK_CONTAINER(btn), lbl);

    g_signal_connect(btn, "clicked", G_CALLBACK(on_launcher_clicked), nullptr);

    return btn;
}

void LauncherWidget::on_launcher_clicked(GtkButton*, gpointer) {
    SpotlightSearch::toggle();
}

} // namespace zenith
