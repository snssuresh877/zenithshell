#include "transfer_history_dialog.hpp"
#include "gtk3_compat.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace zenith {

static std::string format_size(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024.0)) << " MB";
    return out.str();
}

void TransferHistoryDialog::show(GtkWindow* parent) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Wi-Fi Transfer History",
        parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Close", GTK_RESPONSE_CLOSE,
        nullptr
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 0);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);

    GtkWidget* listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scroll), listbox);

    std::string history_path = std::string(getenv("HOME")) + "/.config/zenithshell/transfer_history.json";
    bool has_entries = false;

    if (fs::exists(history_path)) {
        try {
            std::ifstream f(history_path);
            json j = json::parse(f);
            
            for (const auto& item : j) {
                has_entries = true;
                std::string time = item.value("timestamp", "");
                std::string dir = item.value("direction", "");
                std::string fname = item.value("filename", "");
                uint64_t size = item.value("size_bytes", 0);

                GtkWidget* row = gtk_list_box_row_new();
                GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
                gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);

                // Icon (Upload or Download)
                const char* icon_name = (dir == "Sent") ? "network-transmit-symbolic" : "network-receive-symbolic";
                GtkWidget* icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
                gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);

                // Text info
                GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
                
                std::string title = (dir == "Sent" ? "Uploaded: " : "Downloaded: ") + fname;
                GtkWidget* lbl_title = gtk_label_new(title.c_str());
                gtk_label_set_xalign(GTK_LABEL(lbl_title), 0.0);
                // Make title bold
                std::string markup = "<b>" + title + "</b>";
                gtk_label_set_markup(GTK_LABEL(lbl_title), markup.c_str());
                gtk_label_set_ellipsize(GTK_LABEL(lbl_title), PANGO_ELLIPSIZE_END);
                
                std::string sub = time + " • " + format_size(size);
                GtkWidget* lbl_sub = gtk_label_new(sub.c_str());
                gtk_label_set_xalign(GTK_LABEL(lbl_sub), 0.0);
                gtk_widget_add_css_class(lbl_sub, "dim-label");

                gtk_box_pack_start(GTK_BOX(vbox), lbl_title, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), lbl_sub, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

                gtk_container_add(GTK_CONTAINER(row), hbox);
                gtk_list_box_insert(GTK_LIST_BOX(listbox), row, -1);
            }
        } catch (...) {
            // parse error
        }
    }

    if (!has_entries) {
        GtkWidget* empty_lbl = gtk_label_new("No transfers recorded yet.\nFiles sent or received via Zenith Share will appear here.");
        gtk_label_set_justify(GTK_LABEL(empty_lbl), GTK_JUSTIFY_CENTER);
        gtk_widget_set_margin_top(empty_lbl, 50);
        gtk_widget_set_margin_bottom(empty_lbl, 50);
        gtk_container_add(GTK_CONTAINER(listbox), empty_lbl);
    }

    gtk_widget_show_all(dialog);
    g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* d, gint, gpointer) {
        gtk_widget_destroy(GTK_WIDGET(d));
    }), nullptr);
}

} // namespace zenith
