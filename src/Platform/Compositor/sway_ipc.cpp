#include "Platform/Compositor/sway_ipc.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>
#include <glib.h>

using json = nlohmann::json;

namespace zenith {

namespace {
    const char SWAY_MAGIC[] = "i3-ipc";
    constexpr size_t MAGIC_LEN = 6;
    constexpr uint32_t IPC_RUN_COMMAND = 0;
    constexpr uint32_t IPC_GET_WORKSPACES = 1;
    constexpr uint32_t IPC_SUBSCRIBE = 2;
    constexpr uint32_t IPC_GET_TREE = 4;
}

SwayIPCBackend::SwayIPCBackend() {
    const char* sock = std::getenv("SWAYSOCK");
    if (!sock) sock = std::getenv("I3SOCK");
    if (sock) socket_path = sock;
}

SwayIPCBackend::~SwayIPCBackend() {
    running = false;
    if (event_thread.joinable()) {
        event_thread.join();
    }
}

void SwayIPCBackend::init() {
    if (socket_path.empty()) {
        std::cerr << "[SwayIPC] SWAYSOCK not found." << std::endl;
        return;
    }

    // Query initial active workspace
    std::string ws_json = send_message(socket_path, IPC_GET_WORKSPACES, "");
    if (!ws_json.empty()) {
        try {
            auto j = json::parse(ws_json);
            if (j.is_array()) {
                for (const auto& item : j) {
                    if (item.value("focused", false)) {
                        active_workspace = item.value("num", 1);
                        break;
                    }
                }
            }
        } catch (...) {}
    }

    running = true;
    event_thread = std::thread(&SwayIPCBackend::listen_loop, this);
    std::cout << "[SwayIPC] Connected to Sway IPC at " << socket_path << " (Initial WS: " << active_workspace.load() << ")" << std::endl;

    g_idle_add([](gpointer data) -> gboolean {
        auto* self = static_cast<SwayIPCBackend*>(data);
        CompositorManager::instance().notify_workspace(self->active_workspace.load());
        return G_SOURCE_REMOVE;
    }, this);
}

std::string SwayIPCBackend::send_message(const std::string& sock_path, uint32_t type, const std::string& payload) {
    if (sock_path.empty()) return "";

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return "";

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return "";
    }

    uint32_t len = static_cast<uint32_t>(payload.size());
    std::vector<uint8_t> header(MAGIC_LEN + 8);
    std::memcpy(header.data(), SWAY_MAGIC, MAGIC_LEN);
    std::memcpy(header.data() + MAGIC_LEN, &len, sizeof(len));
    std::memcpy(header.data() + MAGIC_LEN + 4, &type, sizeof(type));

    if (write(fd, header.data(), header.size()) != static_cast<ssize_t>(header.size())) {
        close(fd);
        return "";
    }

    if (len > 0) {
        if (write(fd, payload.data(), len) != static_cast<ssize_t>(len)) {
            close(fd);
            return "";
        }
    }

    // Read response header
    uint8_t resp_header[MAGIC_LEN + 8];
    ssize_t rd = read(fd, resp_header, sizeof(resp_header));
    if (rd < static_cast<ssize_t>(sizeof(resp_header))) {
        close(fd);
        return "";
    }

    uint32_t resp_len = 0;
    std::memcpy(&resp_len, resp_header + MAGIC_LEN, sizeof(resp_len));

    std::string response;
    response.resize(resp_len);
    size_t total = 0;
    while (total < resp_len) {
        ssize_t n = read(fd, &response[total], resp_len - total);
        if (n <= 0) break;
        total += n;
    }

    close(fd);
    return response;
}

void SwayIPCBackend::listen_loop() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return;
    }

    // Send subscribe to workspace and window events
    std::string sub_payload = "[\"workspace\", \"window\"]";
    uint32_t len = static_cast<uint32_t>(sub_payload.size());
    uint32_t type = IPC_SUBSCRIBE;

    std::vector<uint8_t> header(MAGIC_LEN + 8);
    std::memcpy(header.data(), SWAY_MAGIC, MAGIC_LEN);
    std::memcpy(header.data() + MAGIC_LEN, &len, sizeof(len));
    std::memcpy(header.data() + MAGIC_LEN + 4, &type, sizeof(type));

    if (write(fd, header.data(), header.size()) < 0 || write(fd, sub_payload.data(), len) < 0) {
        close(fd);
        return;
    }

    // Read subscription ack
    uint8_t ack_hdr[MAGIC_LEN + 8];
    if (read(fd, ack_hdr, sizeof(ack_hdr)) > 0) {
        uint32_t ack_len = 0;
        std::memcpy(&ack_len, ack_hdr + MAGIC_LEN, sizeof(ack_len));
        std::string ack_body(ack_len, '\0');
        if (ack_len > 0) {
            ssize_t ack_read = read(fd, &ack_body[0], ack_len);
            (void)ack_read;
        }
    }

    // Event listening loop
    while (running) {
        uint8_t ev_hdr[MAGIC_LEN + 8];
        ssize_t n = read(fd, ev_hdr, sizeof(ev_hdr));
        if (n <= 0) break;
        if (n < static_cast<ssize_t>(sizeof(ev_hdr))) continue;

        uint32_t ev_len = 0;
        uint32_t ev_type = 0;
        std::memcpy(&ev_len, ev_hdr + MAGIC_LEN, sizeof(ev_len));
        std::memcpy(&ev_type, ev_hdr + MAGIC_LEN + 4, sizeof(ev_type));

        std::string ev_payload;
        ev_payload.resize(ev_len);
        size_t total = 0;
        while (total < ev_len && running) {
            ssize_t r = read(fd, &ev_payload[total], ev_len - total);
            if (r <= 0) break;
            total += r;
        }

        if (total == ev_len) {
            handle_event(ev_type, ev_payload);
        }
    }

    close(fd);
}

void SwayIPCBackend::handle_event(uint32_t ev_type, const std::string& payload) {
    try {
        auto j = json::parse(payload);
        uint32_t clean_type = ev_type & 0x7FFFFFFF; // Clear high bit

        if (clean_type == 0) { // Workspace event
            std::string change = j.value("change", "");
            if (change == "focus" || change == "init") {
                if (j.contains("current") && j["current"].is_object()) {
                    int num = j["current"].value("num", -1);
                    if (num > 0) {
                        active_workspace = num;
                        g_idle_add([](gpointer data) -> gboolean {
                            int ws = GPOINTER_TO_INT(data);
                            CompositorManager::instance().notify_workspace(ws);
                            return G_SOURCE_REMOVE;
                        }, GINT_TO_POINTER(num));
                    }
                }
            }
        } else if (clean_type == 3) { // Window event
            std::string change = j.value("change", "");
            if (change == "focus" || change == "title") {
                if (j.contains("container") && j["container"].is_object()) {
                    std::string title = j["container"].value("name", "");
                    g_idle_add([](gpointer data) -> gboolean {
                        auto* t = static_cast<std::string*>(data);
                        CompositorManager::instance().notify_window_title(*t);
                        delete t;
                        return G_SOURCE_REMOVE;
                    }, new std::string(title));
                }
            }
            g_idle_add([](gpointer) -> gboolean {
                CompositorManager::instance().notify_window_event();
                return G_SOURCE_REMOVE;
            }, nullptr);
        }
    } catch (...) {}
}

void SwayIPCBackend::switch_workspace(int id) {
    std::string cmd = "workspace number " + std::to_string(id);
    send_message(socket_path, IPC_RUN_COMMAND, cmd);
}

void SwayIPCBackend::switch_workspace_relative(int delta) {
    std::string cmd = (delta > 0) ? "workspace next" : "workspace prev";
    send_message(socket_path, IPC_RUN_COMMAND, cmd);
}

void SwayIPCBackend::focus_window(const std::string& target) {
    std::string cmd = "[con_id=" + target + "] focus";
    send_message(socket_path, IPC_RUN_COMMAND, cmd);
}

void SwayIPCBackend::close_window(const std::string& address) {
    std::string cmd = "[con_id=" + address + "] kill";
    send_message(socket_path, IPC_RUN_COMMAND, cmd);
}

void SwayIPCBackend::exit_session() {
    send_message(socket_path, IPC_RUN_COMMAND, "exit");
}

int SwayIPCBackend::get_active_workspace_id() {
    return active_workspace.load();
}

std::string SwayIPCBackend::get_clients_json() {
    return send_message(socket_path, IPC_GET_TREE, "");
}

} // namespace zenith
