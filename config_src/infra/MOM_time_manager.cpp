/// @file MOM_time_manager.cpp
/// @brief Implementation of the conversions between a time and seconds (real).

#include "MOM_time_manager.hpp"

#include <cmath>
#include <cstdint>

namespace MOM {

Duration real_to_time(double time_in) {
  // Instead of this function, std::chrono::round<Duration> would
  // be shorter, but it rounds an exact half second to the even neighbour
  // (0.5 s -> 0 s), where MOM6 rounds it away from zero (0.5 s -> 1 s).
  const double x = time_in;

  const auto days = static_cast<std::int64_t>(std::floor(x / 86400.));
  const auto seconds = static_cast<std::int64_t>(std::floor(x - 86400. * days));
  const double real_subsecond_remainder = x - (days * 86400. + seconds);
  // std::llround rounds halves away from zero, as Fortran's nint does.
  const std::int64_t ticks = std::llround(real_subsecond_remainder);

  return Duration{days * 86400 + seconds + ticks};
}

double time_to_real(Duration time) {
  return static_cast<double>(time.count());
}

} // namespace MOM
