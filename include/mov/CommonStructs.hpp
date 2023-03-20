#pragma once

namespace mov {

struct ApplicationCreateInfo {
  const char *app_name;
  const uint16_t app_major;
  const uint16_t app_minor;
  const uint16_t app_patch;
  const int32_t width;
  const int32_t height;
};

} // namespace mov