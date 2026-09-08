#include "compositors/hyprland_ipc.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <array>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace zenith {

HyprlandIPC& HyprlandIPC::instance() {
    static HyprlandIPC inst;
    return inst;
}

HyprlandIPC::~HyprlandIPC() {
    running = false;
    if (ipc_thread.joinable()) {
        ipc_thread.detach();
    }
}

void HyprlandIPC::init() {
    if (running) return;

    const char* his = std::getenv("HYPRLAND_INSTANCE_SIGNATURE");
    const char* xdg_runtime = std::getenv("XDG_RUNTIME_DIR");

    if (!his || !xdg_runtime) {
        std::cerr << "[HyprlandIPC] HYPRLAND_INSTANCE_SIGNATURE or XDG_RUNTIME_DIR not found. IPC disabled.\n";
        return;
    }

    event_socket_path = std::string(xdg_runtime) + "/hypr/" + his + "/.socket2.sock";
    req_socket_path = std::string(xdg_runtime) + "/hypr/" + his + "/.socket.sock";
    running = true;
    ipc_thread = std::thread(&HyprlandIPC::listen_loop, this);
    std::cout << "[HyprlandIPC] Connected to Hyprland event socket at " << event_socket_path << std::endl;

    // Query initial active workspace directly via request socket (zero subprocesses)
    g_idle_add([](gpointer) -> gboolean {
        int ws_id = HyprlandIPC::get_active_workspace_id();
        for (const auto& cb : HyprlandIPC::instance().workspace_cbs) {
            if (cb) cb(ws_id);
        }
        return FALSE;
    }, nullptr);
}

std::string HyprlandIPC::request(const std::string& cmd) {
    const char* his = std::getenv("HYPRLAND_INSTANCE_SIGNATURE");
    const char* xdg_runtime = std::getenv("XDG_RUNTIME_DIR");
    if (!his || !xdg_runtime) return "";

    std::string sock_p = std::string(xdg_runtime) + "/hypr/" + his + "/.socket.sock";

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, sock_p.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return "";
    }

    if (write(sock, cmd.data(), cmd.size()) < 0) {
        close(sock);
        return "";
    }

    std::string response;
    char buffer[4096];
    ssize_t bytes = 0;
    while ((bytes = read(sock, buffer, sizeof(buffer))) > 0) {
        response.append(buffer, bytes);
    }

    close(sock);
    return response;
}

std::string HyprlandIPC::query_json(const std::string& endpoint) {
    return request("j/" + endpoint);
}

bool HyprlandIPC::dispatch(const std::string& cmd) {
    std::string resp = request("dispatch " + cmd);
    return resp.find("ok") != std::string::npos;
}

void HyprlandIPC::switch_workspace(int id) {
    // 1. Try Lua dispatcher
    if (dispatch("hl.dsp.focus({workspace=\"" + std::to_string(id) + "\"})")) return;
    // 2. Try standard Hyprland dispatcher
    if (dispatch("workspace " + std::to_string(id))) return;
    // 3. Fallback
    std::string cmd = "hyprctl dispatch workspace " + std::to_string(id) + " 2>/dev/null &";
    system(cmd.c_str());
}

void HyprlandIPC::switch_workspace_relative(int delta) {
    std::string delta_str = (delta > 0) ? "e+1" : "e-1";
    // 1. Try Lua dispatcher
    if (dispatch("hl.dsp.focus({workspace=\"" + delta_str + "\"})")) return;
    // 2. Try standard Hyprland dispatcher
    if (dispatch("workspace " + delta_str)) return;
    // 3. Fallback
    std::string cmd = "hyprctl dispatch workspace " + delta_str + " 2>/dev/null &";
    system(cmd.c_str());
}

void HyprlandIPC::focus_window(const std::string& target) {
    if (target.empty()) return;
    // 1. Try Lua dispatcher
    if (dispatch("hl.dsp.focus({window=\"" + target + "\"})")) return;
    // 2. Try standard Hyprland dispatcher
    if (dispatch("focuswindow " + target)) return;
    // 3. Fallback
    std::string cmd = "hyprctl dispatch focuswindow " + target + " 2>/dev/null &";
    system(cmd.c_str());
}

void HyprlandIPC::close_window(const std::string& address) {
    if (address.empty()) return;
    // 1. Try Lua dispatcher
    if (dispatch("hl.dsp.window.close({address=\"" + address + "\"})")) return;
    // 2. Try standard Hyprland dispatcher
    if (dispatch("closewindow address:" + address)) return;
    // 3. Fallback
    std::string cmd = "hyprctl dispatch closewindow address:" + address + " 2>/dev/null &";
    system(cmd.c_str());
}

void HyprlandIPC::exit() {
    // 1. Try Lua dispatcher
    if (dispatch("hl.dsp.exit()")) return;
    // 2. Try standard Hyprland dispatcher
    if (dispatch("exit")) return;
    // 3. Fallback
    system("hyprctl dispatch exit 2>/dev/null &");
}

bool HyprlandIPC::send_command(const std::string& cmd) {
    std::string c = cmd;
    // Strip leading "hyprctl " if present
    if (c.rfind("hyprctl ", 0) == 0) {
        c = c.substr(8);
    }
    // Trim trailing background & redirects
    size_t amp = c.find('&');
    if (amp != std::string::npos) c = c.substr(0, amp);
    while (!c.empty() && (c.back() == ' ' || c.back() == '\t')) c.pop_back();
    if (c.rfind("2>/dev/null") != std::string::npos) {
        c = c.substr(0, c.rfind("2>/dev/null"));
        while (!c.empty() && (c.back() == ' ' || c.back() == '\t')) c.pop_back();
    }

    if (c == "dispatch exit" || c == "exit") {
        exit();
        return true;
    }

    if (c.rfind("dispatch ", 0) == 0) {
        std::string sub = c.substr(9);
        return dispatch(sub);
    }

    std::string resp = request(c);
    return !resp.empty();
}

int HyprlandIPC::get_active_workspace_id() {
    std::string res = query_json("activeworkspace");
    if (!res.empty()) {
        try {
            auto j = json::parse(res);
            if (j.contains("id")) return j["id"].get<int>();
        } catch (...) {}
    }
    return 1;
}

std::string HyprlandIPC::get_clients_json() {
    return query_json("clients");
}

void HyprlandIPC::add_workspace_callback(WorkspaceCallback cb) {
    workspace_cbs.push_back(cb);
}

void HyprlandIPC::add_window_title_callback(WindowTitleCallback cb) {
    window_title_cbs.push_back(cb);
}

void HyprlandIPC::add_window_event_callback(WindowEventCallback cb) {
    window_event_cbs.push_back(cb);
}

void HyprlandIPC::listen_loop() {
    while (running) {
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, event_socket_path.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        char buffer[1024];
        std::string stream_acc;

        while (running) {
            ssize_t bytes = read(sock, buffer, sizeof(buffer) - 1);
            if (bytes <= 0) break;

            buffer[bytes] = '\0';
            stream_acc += buffer;

            size_t pos = 0;
            while ((pos = stream_acc.find('\n')) != std::string::npos) {
                std::string event = stream_acc.substr(0, pos);
                stream_acc.erase(0, pos + 1);
                handle_event(event);
            }
        }

        close(sock);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

static guint window_event_debounce_source = 0;

void HyprlandIPC::handle_event(const std::string& event_line) {
    bool is_window_or_ws_event = false;

    if (event_line.rfind("workspace>>", 0) == 0 || event_line.rfind("focusedmon>>", 0) == 0) {
        is_window_or_ws_event = true;
        std::string val = "";
        if (event_line.rfind("workspace>>", 0) == 0) {
            val = event_line.substr(11);
        } else {
            size_t comma = event_line.find(',');
            if (comma != std::string::npos) {
                val = event_line.substr(comma + 1);
            }
        }
        try {
            int ws = std::stoi(val);
            g_idle_add([](gpointer data) -> gboolean {
                int id = GPOINTER_TO_INT(data);
                for (const auto& cb : HyprlandIPC::instance().workspace_cbs) {
                    if (cb) cb(id);
                }
                return FALSE;
            }, GINT_TO_POINTER(ws));
        } catch (...) {}
    } else if (event_line.rfind("activewindow>>", 0) == 0) {
        is_window_or_ws_event = true;
        std::string content = event_line.substr(14);
        size_t comma = content.find(',');
        std::string title = (comma != std::string::npos) ? content.substr(comma + 1) : content;

        std::string* title_copy = new std::string(title);
        g_idle_add([](gpointer data) -> gboolean {
            auto* t = static_cast<std::string*>(data);
            for (const auto& cb : HyprlandIPC::instance().window_title_cbs) {
                if (cb) cb(*t);
            }
            delete t;
            return FALSE;
        }, title_copy);
    } else if (event_line.rfind("openwindow>>", 0) == 0 ||
               event_line.rfind("closewindow>>", 0) == 0 ||
               event_line.rfind("movewindow>>", 0) == 0 ||
               event_line.rfind("windowtitle>>", 0) == 0 ||
               event_line.rfind("activewindowv2>>", 0) == 0 ||
               event_line.rfind("fullscreen>>", 0) == 0 ||
               event_line.rfind("changefloatingmode>>", 0) == 0 ||
               event_line.rfind("destroyworkspace>>", 0) == 0 ||
               event_line.rfind("createworkspace>>", 0) == 0) {
        is_window_or_ws_event = true;
    }

    if (is_window_or_ws_event && !window_event_cbs.empty()) {
        if (window_event_debounce_source == 0) {
            window_event_debounce_source = g_timeout_add(30, [](gpointer) -> gboolean {
                window_event_debounce_source = 0;
                for (const auto& cb : HyprlandIPC::instance().window_event_cbs) {
                    if (cb) cb();
                }
                return FALSE;
            }, nullptr);
        }
    }
}

} // namespace zenith
