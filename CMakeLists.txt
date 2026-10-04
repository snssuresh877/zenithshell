cmake_minimum_required(VERSION 3.20)
project(zenithshell VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler Warning Flags, Optimizations & Security Hardening
add_compile_options(
    -O3
    -Wall
    -Wextra
    -Wformat
    -Wformat-security
    -fstack-protector-strong
    -fPIE
    -D_FORTIFY_SOURCE=2
)

# Hardened Linker Flags (Full RELRO, Immediate Binding, No-Exec Stack)
add_link_options(
    -pie
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
)

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
    src/FileManager/Core/file_item.cpp 
    src/FileManager/Core/undo_manager.cpp
    src/FileManager/Features/transfer_history_dialog.cpp
    src/FileManager/Core/file_operations.cpp
    src/FileManager/Features/checksum_dialog.cpp
    src/FileManager/Features/duplicate_finder_dialog.cpp
    src/FileManager/Features/disk_analyzer_dialog.cpp 
    src/FileManager/Core/file_shortcuts.cpp 
    src/FileManager/Core/custom_actions.cpp 
    src/FileManager/Features/command_palette.cpp 
    src/FileManager/Features/bulk_rename_dialog.cpp 
    src/FileManager/UI/inspector_panel.cpp 
    src/FileManager/Features/quick_preview.cpp 
    src/FileManager/Features/file_preferences_dialog.cpp
    src/FileManager/UI/theme_settings.cpp 
    src/FileManager/UI/path_bar_widget.cpp 
    src/FileManager/UI/places_sidebar.cpp 
    src/FileManager/UI/file_view_widget.cpp 
    src/FileManager/UI/file_manager_window.cpp 
    src/FileManager/UI/tree_sidebar.cpp
    src/Engine/Config/config.cpp
    src/Core/EventBus/event_bus.cpp
    src/Platform/Compositor/compositor_manager.cpp
    src/Platform/Compositor/hyprland_ipc.cpp
    src/Platform/Compositor/sway_ipc.cpp
    src/Platform/Compositor/generic_compositor.cpp
    src/Platform/DBus/dbus_service.cpp
    src/Services/Notifications/notification_manager.cpp
    src/Services/Media/mpris_player.cpp
    src/Services/Audio/audio_manager.cpp
    src/Services/Monitor/sys_monitor.cpp
    src/Services/Brightness/backlight_manager.cpp
    src/Services/Network/wifi_manager.cpp
    src/Engine/Theme/theme_engine.cpp
    src/Engine/Theme/theme_loader.cpp
    src/Engine/Theme/pywal_importer.cpp
    src/Engine/Theme/palette_extractor.cpp
    src/Engine/Theme/css_manager.cpp
    src/Desktop/TopBar/bar_window.cpp
    src/Desktop/TopBar/workspace_widget.cpp
    src/Desktop/TopBar/sys_info_widget.cpp
    src/Desktop/TopBar/clock_widget.cpp
    src/Desktop/Launcher/spotlight_search.cpp
    src/Desktop/Launcher/launcher_widget.cpp
    src/Desktop/ControlCenter/control_center.cpp
    src/Desktop/Clipboard/clipboard_manager.cpp
    src/Desktop/Reminders/reminder_manager.cpp
    src/Desktop/NotificationCenter/notification_panel.cpp
    src/Desktop/ActiveApps/active_apps_drawer.cpp
    src/Desktop/SystemTray/system_tray_manager.cpp
    src/Desktop/Keybinds/keybinds_overlay.cpp
    src/Desktop/PowerMenu/power_menu.cpp
    src/Desktop/Wallpaper/wallpaper_engine.cpp
    src/Desktop/OSD/osd_window.cpp
    src/Platform/CLI/zenithctl.cpp
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
install(CODE "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink zenithshell \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/zenithctl)")
install(FILES config.json style.css DESTINATION ${CMAKE_INSTALL_DATADIR}/zenithshell)
install(FILES docs/CONFIGURATION.md DESTINATION ${CMAKE_INSTALL_DOCDIR}/zenithshell)
install(FILES zenithshell.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Shell Completions
install(FILES completions/fish/zenithctl.fish DESTINATION ${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d)
install(CODE "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink zenithctl.fish \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d/zenithshell.fish)")

install(FILES completions/bash/zenithctl DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions)
install(CODE "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink zenithctl \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/bash-completion/completions/zenithshell)")

install(FILES completions/zsh/_zenithctl DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions)
install(CODE "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink _zenithctl \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/zsh/site-functions/_zenithshell)")
