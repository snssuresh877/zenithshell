#include "undo_manager.hpp"
#include <gio/gio.h>
#include <filesystem>
#include <iostream>
#include <cstdlib>

namespace fs = std::filesystem;

namespace zenith {

std::vector<UndoAction> UndoManager::undo_stack;
std::vector<UndoAction> UndoManager::redo_stack;

void UndoManager::push_action(const UndoAction& action) {
    if (action.src_paths.empty() && action.dst_paths.empty()) return;
    undo_stack.push_back(action);
    if (undo_stack.size() > 50) undo_stack.erase(undo_stack.begin()); // Limit to 50
    redo_stack.clear();
}

bool UndoManager::can_undo() { return !undo_stack.empty(); }
bool UndoManager::can_redo() { return !redo_stack.empty(); }

bool UndoManager::undo() {
    if (undo_stack.empty()) return false;
    UndoAction action = undo_stack.back();
    undo_stack.pop_back();
    bool success = perform_action(action, true);
    if (success) redo_stack.push_back(action);
    return success;
}

bool UndoManager::redo() {
    if (redo_stack.empty()) return false;
    UndoAction action = redo_stack.back();
    redo_stack.pop_back();
    bool success = perform_action(action, false);
    if (success) undo_stack.push_back(action);
    return success;
}

bool UndoManager::perform_action(const UndoAction& action, bool is_undo) {
    bool all_ok = true;
    if (action.type == UndoOpType::RENAME || action.type == UndoOpType::MOVE) {
        const auto& from = is_undo ? action.dst_paths : action.src_paths;
        const auto& to   = is_undo ? action.src_paths : action.dst_paths;
        for (size_t i = 0; i < from.size() && i < to.size(); ++i) {
            GFile* src = g_file_parse_name(from[i].c_str());
            GFile* dst = g_file_parse_name(to[i].c_str());
            if (src && dst) {
                if (!g_file_move(src, dst, G_FILE_COPY_NONE, nullptr, nullptr, nullptr, nullptr)) all_ok = false;
                g_object_unref(src); g_object_unref(dst);
            }
        }
    } 
    else if (action.type == UndoOpType::COPY) {
        if (is_undo) {
            // Undo copy: trash the copies
            for (const auto& p : action.dst_paths) {
                GFile* f = g_file_parse_name(p.c_str());
                if (f) { g_file_trash(f, nullptr, nullptr); g_object_unref(f); }
            }
        } else {
            // Redo copy: copy src to dst again
            for (size_t i = 0; i < action.src_paths.size() && i < action.dst_paths.size(); ++i) {
                GFile* src = g_file_parse_name(action.src_paths[i].c_str());
                GFile* dst = g_file_parse_name(action.dst_paths[i].c_str());
                if (src && dst) {
                    if (!g_file_copy(src, dst, G_FILE_COPY_NONE, nullptr, nullptr, nullptr, nullptr)) all_ok = false;
                    g_object_unref(src); g_object_unref(dst);
                }
            }
        }
    }
    else if (action.type == UndoOpType::CREATE) {
        if (is_undo) {
            for (const auto& p : action.dst_paths) {
                GFile* f = g_file_parse_name(p.c_str());
                if (f) { g_file_trash(f, nullptr, nullptr); g_object_unref(f); }
            }
        } else {
            for (size_t i = 0; i < action.dst_paths.size(); ++i) {
                bool is_dir = (action.src_paths.size() > i && action.src_paths[i] == "folder");
                if (is_dir) {
                    std::error_code ec; fs::create_directory(action.dst_paths[i], ec);
                } else {
                    FILE* f = fopen(action.dst_paths[i].c_str(), "w");
                    if (f) fclose(f);
                }
            }
        }
    }
    else if (action.type == UndoOpType::TRASH) {
        if (is_undo) {
            if(system("notify-send -a 'Zenith Files' 'Undo Trash' 'To restore files, open the Trash (Trash icon in sidebar) and drag them back.' &")){};
            return false;
        } else {
            for (const auto& p : action.src_paths) {
                GFile* f = g_file_parse_name(p.c_str());
                if (f) { g_file_trash(f, nullptr, nullptr); g_object_unref(f); }
            }
        }
    }
    return all_ok;
}

} // namespace zenith
