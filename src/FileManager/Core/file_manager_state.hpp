#pragma once
#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <memory>

namespace zenith {

struct PaneState {
    GtkWidget* file_view{nullptr};
    std::string current_path;
    std::vector<std::string> back_history;
    std::vector<std::string> forward_history;
    int last_total_items{0};
    int last_selected_items{0};
    uint64_t last_selected_bytes{0};
};

struct TabState {
    GtkWidget* paned{nullptr};
    PaneState left_pane;
    PaneState right_pane;
    PaneState* active_pane{&left_pane};
    bool is_dual{false};
    GtkWidget* tab_box{nullptr};
    GtkWidget* tab_label{nullptr};
};

struct FileManagerState {
    GtkWidget* window{nullptr};
    GtkWidget* btn_back{nullptr};
    GtkWidget* btn_forward{nullptr};
    GtkWidget* btn_up{nullptr};
    GtkWidget* btn_home{nullptr};
    GtkWidget* path_bar{nullptr};
    GtkWidget* btn_search{nullptr};
    GtkWidget* btn_new{nullptr};
    GtkWidget* btn_more{nullptr};
    GtkWidget* btn_view{nullptr};
    GtkWidget* btn_inspector{nullptr};
    GtkWidget* btn_settings{nullptr};
    GtkWidget* new_popover{nullptr};
    GtkWidget* more_popover{nullptr};
    GtkWidget* view_popover{nullptr};
    GtkWidget* view_grid_btn{nullptr};
    GtkWidget* view_list_btn{nullptr};
    GtkWidget* view_size_scale{nullptr};
    GtkWidget* view_size_label{nullptr};
    GtkWidget* view_sort_combo{nullptr};
    GtkWidget* view_hidden_check{nullptr};
    GtkWidget* search_revealer{nullptr};
    GtkWidget* search_entry{nullptr};
    bool show_hidden{false};
    GtkWidget* main_paned{nullptr};
    GtkWidget* sidebar{nullptr};
    GtkWidget* center_paned{nullptr};
    GtkWidget* inspector_panel{nullptr};
    bool inspector_visible{false};
    GtkWidget* notebook{nullptr};
    std::vector<std::unique_ptr<TabState>> tabs;
    TabState* active_tab{nullptr};
    GtkWidget* status_label{nullptr};
    GtkWidget* disk_label{nullptr};
};

} // namespace zenith
