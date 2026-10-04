#pragma once

#include <string>
#include <vector>

namespace zenith {

struct CustomAction {
    std::string id;
    std::string name;
    std::string icon;
    std::string command;
    std::vector<std::string> patterns;
};

class CustomActionsManager {
public:
    static void init();
    static std::vector<CustomAction> get_actions_for(const std::vector<std::string>& selected_paths);
    static void execute_action(const CustomAction& action, const std::vector<std::string>& selected_paths);
    static void reload();
    
private:
    static std::vector<CustomAction> actions;
    static std::string get_config_path();
    static void write_default_config();
};

} // namespace zenith
