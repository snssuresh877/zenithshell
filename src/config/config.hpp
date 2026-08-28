#pragma once

#include <string>
#include <vector>

namespace zenith {

struct Config {
  std::string position = "top";
  int height = 30;
  int margin_top = 8;
  int margin_bottom = 0;
  int margin_left = 12;
  int margin_right = 12;
  bool exclusive_zone = true;

  int workspace_count = 10;
  std::string clock_format = "%a %b %d  %H:%M:%S";
  int sys_update_interval_ms = 1500;
  std::string wallpaper_dir = "";

  static Config load(const std::string &path);
};

} // namespace zenith
