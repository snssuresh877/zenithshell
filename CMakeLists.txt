cmake_minimum_required(VERSION 3.20)
project(zenithshell VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler Warning Flags & Optimizations
add_compile_options(-O3 -Wall -Wextra)

# Find Required Packages via PkgConfig
find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(GTK_LAYER_SHELL REQUIRED gtk-layer-shell-0)
pkg_check_modules(GLIB REQUIRED glib-2.0 gio-2.0 gobject-2.0)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(PANGO REQUIRED pango pangocairo)

include_directories(
    src
    ${GTK3_INCLUDE_DIRS}
    ${GTK_LAYER_SHELL_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${PANGO_INCLUDE_DIRS}
)

link_directories(
    ${GTK3_LIBRARY_DIRS}
    ${GTK_LAYER_SHELL_LIBRARY_DIRS}
    ${GLIB_LIBRARY_DIRS}
    ${CAIRO_LIBRARY_DIRS}
    ${PANGO_LIBRARY_DIRS}
)

set(SOURCES
    src/main.cpp
    src/app/app.cpp
    src/config/config.cpp
    src/compositors/hyprland_ipc.cpp
    src/dbus/dbus_service.cpp
    src/dbus/notification_manager.cpp
    src/dbus/mpris_player.cpp
    src/pipewire/audio_manager.cpp
    src/system/sys_monitor.cpp
    src/theme/theme_engine.cpp
    src/theme/theme_loader.cpp
    src/theme/pywal_importer.cpp
    src/theme/css_manager.cpp
    src/shell/bar/bar_window.cpp
    src/shell/bar/workspace_widget.cpp
    src/shell/bar/sys_info_widget.cpp
    src/shell/bar/clock_widget.cpp
    src/shell/launcher/spotlight_search.cpp
    src/shell/launcher/launcher_widget.cpp
    src/shell/control_center/control_center.cpp
    src/shell/control_center/wifi_manager.cpp
    src/shell/clipboard/clipboard_manager.cpp
    src/shell/reminder/reminder_manager.cpp
    src/shell/notification/notification_panel.cpp
    src/shell/task/active_apps_drawer.cpp
    src/shell/tray/system_tray_manager.cpp
    src/shell/keybinds/keybinds_overlay.cpp
    src/shell/power/power_menu.cpp
)

add_executable(zenithshell ${SOURCES})

target_link_libraries(zenithshell PRIVATE
    ${GTK3_LIBRARIES}
    ${GTK_LAYER_SHELL_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${PANGO_LIBRARIES}
)

# Standard Install Target
include(GNUInstallDirs)
install(TARGETS zenithshell DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config.json style.css DESTINATION ${CMAKE_INSTALL_DATADIR}/zenithshell)
