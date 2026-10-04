#pragma once

#include <string>
#include <vector>

namespace zenith {

enum class UndoOpType {
    CREATE,
    RENAME,
    MOVE,
    COPY,
    TRASH
};

struct UndoAction {
    UndoOpType type;
    std::vector<std::string> src_paths; // Original paths
    std::vector<std::string> dst_paths; // New paths (where they ended up)
};

class UndoManager {
public:
    static void push_action(const UndoAction& action);
    static bool can_undo();
    static bool can_redo();
    static bool undo();
    static bool redo();

private:
    static std::vector<UndoAction> undo_stack;
    static std::vector<UndoAction> redo_stack;

    static bool perform_action(const UndoAction& action, bool is_undo);
};

} // namespace zenith
