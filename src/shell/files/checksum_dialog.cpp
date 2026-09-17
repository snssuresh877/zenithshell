#include "shell/files/checksum_dialog.hpp"
#include <thread>
#include <fstream>
#include <vector>
#include <iomanip>
#include <sstream>
#include <glib.h>
#include <iostream>

namespace zenith {

struct ChecksumContext {
    GtkWidget* window;
    GtkWidget* spinner;
    GtkWidget* md5_entry;
    GtkWidget* sha1_entry;
    GtkWidget* sha256_entry;
    GtkWidget* match_entry;
    GtkWidget* match_icon;
    std::string filepath;
    std::string md5_result;
    std::string sha1_result;
    std::string sha256_result;
};

static gboolean on_checksum_complete(gpointer user_data) {
    auto* ctx = static_cast<ChecksumContext*>(user_data);
    gtk_spinner_stop(GTK_SPINNER(ctx->spinner));
    gtk_widget_hide(ctx->spinner);
    
    gtk_entry_set_text(GTK_ENTRY(ctx->md5_entry), ctx->md5_result.empty() ? "Error reading file" : ctx->md5_result.c_str());
    gtk_entry_set_text(GTK_ENTRY(ctx->sha1_entry), ctx->sha1_result.empty() ? "Error reading file" : ctx->sha1_result.c_str());
    gtk_entry_set_text(GTK_ENTRY(ctx->sha256_entry), ctx->sha256_result.empty() ? "Error reading file" : ctx->sha256_result.c_str());
    
    return G_SOURCE_REMOVE;
}

static void calculate_hashes(ChecksumContext* ctx) {
    GChecksum* md5 = g_checksum_new(G_CHECKSUM_MD5);
    GChecksum* sha1 = g_checksum_new(G_CHECKSUM_SHA1);
    GChecksum* sha256 = g_checksum_new(G_CHECKSUM_SHA256);
    
    std::ifstream file(ctx->filepath, std::ios::binary);
    if (file) {
        char buffer[65536];
        while (file.read(buffer, sizeof(buffer))) {
            g_checksum_update(md5, (const guchar*)buffer, file.gcount());
            g_checksum_update(sha1, (const guchar*)buffer, file.gcount());
            g_checksum_update(sha256, (const guchar*)buffer, file.gcount());
        }
        if (file.gcount() > 0) {
            g_checksum_update(md5, (const guchar*)buffer, file.gcount());
            g_checksum_update(sha1, (const guchar*)buffer, file.gcount());
            g_checksum_update(sha256, (const guchar*)buffer, file.gcount());
        }
        
        ctx->md5_result = g_checksum_get_string(md5);
        ctx->sha1_result = g_checksum_get_string(sha1);
        ctx->sha256_result = g_checksum_get_string(sha256);
    } else {
        ctx->md5_result = "";
        ctx->sha1_result = "";
        ctx->sha256_result = "";
    }
    
    g_checksum_free(md5);
    g_checksum_free(sha1);
    g_checksum_free(sha256);
    
    g_idle_add(on_checksum_complete, ctx);
}

static void on_match_changed(GtkEditable* editable, gpointer user_data) {
    auto* ctx = static_cast<ChecksumContext*>(user_data);
    const char* text = gtk_entry_get_text(GTK_ENTRY(editable));
    std::string input = text;
    // trim and lowercase
    input.erase(input.find_last_not_of(" \n\r\t") + 1);
    input.erase(0, input.find_first_not_of(" \n\r\t"));
    for (auto& c : input) c = tolower(c);
    
    if (input.empty()) {
        gtk_image_set_from_icon_name(GTK_IMAGE(ctx->match_icon), "dialog-information-symbolic", GTK_ICON_SIZE_BUTTON);
        return;
    }
    
    if (ctx->md5_result.empty()) return; // still calculating or error
    
    if (input == ctx->md5_result || input == ctx->sha1_result || input == ctx->sha256_result) {
        gtk_image_set_from_icon_name(GTK_IMAGE(ctx->match_icon), "emblem-default-symbolic", GTK_ICON_SIZE_BUTTON); // A checkmark icon
    } else {
        gtk_image_set_from_icon_name(GTK_IMAGE(ctx->match_icon), "dialog-error-symbolic", GTK_ICON_SIZE_BUTTON); // An X icon
    }
}

void ChecksumDialog::show(GtkWindow* parent, const std::string& filepath) {
    auto* ctx = new ChecksumContext();
    ctx->filepath = filepath;
    
    std::string filename = g_path_get_basename(filepath.c_str());
    std::string title = "Checksums: " + filename;
    
    ctx->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ctx->window), title.c_str());
    gtk_window_set_default_size(GTK_WINDOW(ctx->window), 600, -1);
    gtk_window_set_position(GTK_WINDOW(ctx->window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_transient_for(GTK_WINDOW(ctx->window), parent);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(ctx->window), TRUE);
    
    g_signal_connect(ctx->window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
        delete static_cast<ChecksumContext*>(data);
    }), ctx);
    
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);
    gtk_container_add(GTK_CONTAINER(ctx->window), vbox);
    
    GtkWidget* header_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(vbox), header_hbox, FALSE, FALSE, 0);
    
    GtkWidget* header_lbl = gtk_label_new(("Calculating hashes for: " + filename).c_str());
    gtk_label_set_xalign(GTK_LABEL(header_lbl), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(header_lbl), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_pack_start(GTK_BOX(header_hbox), header_lbl, TRUE, TRUE, 0);
    
    ctx->spinner = gtk_spinner_new();
    gtk_spinner_start(GTK_SPINNER(ctx->spinner));
    gtk_box_pack_end(GTK_BOX(header_hbox), ctx->spinner, FALSE, FALSE, 0);
    
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);
    
    auto add_row = [&](int row, const char* label_text, GtkWidget*& entry_out) {
        GtkWidget* lbl = gtk_label_new(label_text);
        gtk_label_set_xalign(GTK_LABEL(lbl), 1.0);
        gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
        
        entry_out = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry_out), "Calculating...");
        gtk_editable_set_editable(GTK_EDITABLE(entry_out), FALSE);
        gtk_widget_set_hexpand(entry_out, TRUE);
        
        // Monospace font
        PangoFontDescription* font_desc = pango_font_description_from_string("monospace");
        gtk_widget_override_font(entry_out, font_desc);
        pango_font_description_free(font_desc);
        
        gtk_grid_attach(GTK_GRID(grid), entry_out, 1, row, 1, 1);
    };
    
    add_row(0, "MD5:", ctx->md5_entry);
    add_row(1, "SHA-1:", ctx->sha1_entry);
    add_row(2, "SHA-256:", ctx->sha256_entry);
    
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 8);
    
    GtkWidget* match_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(vbox), match_hbox, FALSE, FALSE, 0);
    
    GtkWidget* match_lbl = gtk_label_new("Verify:");
    gtk_box_pack_start(GTK_BOX(match_hbox), match_lbl, FALSE, FALSE, 0);
    
    ctx->match_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx->match_entry), "Paste a hash here to verify...");
    gtk_widget_set_hexpand(ctx->match_entry, TRUE);
    gtk_box_pack_start(GTK_BOX(match_hbox), ctx->match_entry, TRUE, TRUE, 0);
    
    ctx->match_icon = gtk_image_new_from_icon_name("dialog-information-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(match_hbox), ctx->match_icon, FALSE, FALSE, 0);
    
    g_signal_connect(ctx->match_entry, "changed", G_CALLBACK(on_match_changed), ctx);
    
    gtk_widget_show_all(ctx->window);
    
    std::thread t(calculate_hashes, ctx);
    t.detach();
}

} // namespace zenith
