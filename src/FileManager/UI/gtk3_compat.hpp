#pragma once
#include <gtk/gtk.h>

inline void gtk_widget_add_css_class(GtkWidget* widget, const char* class_name) {
    if (widget && class_name) {
        GtkStyleContext* ctx = gtk_widget_get_style_context(widget);
        gtk_style_context_add_class(ctx, class_name);
    }
}

inline void gtk_widget_remove_css_class(GtkWidget* widget, const char* class_name) {
    if (widget && class_name) {
        GtkStyleContext* ctx = gtk_widget_get_style_context(widget);
        gtk_style_context_remove_class(ctx, class_name);
    }
}

inline bool gtk_widget_has_css_class(GtkWidget* widget, const char* class_name) {
    if (widget && class_name) {
        GtkStyleContext* ctx = gtk_widget_get_style_context(widget);
        return gtk_style_context_has_class(ctx, class_name);
    }
    return false;
}
