#pragma once
/// @file MOM_time_manager.hpp
/// @brief The time types from TIM, and the conversions between a time
///        and a real number of seconds.

#include "core/tim_time.hpp"

namespace MOM {

/// @brief A point on the time axis. An alias of TIM::Time.
using Time = TIM::Time;

/// @brief An exact span of whole seconds. An alias of TIM::Duration.
using Duration = TIM::Duration;

/// @brief A calendar date and time of day. An alias of TIM::Date.
using Date = TIM::Date;

/// @brief The calendar that converts between times and dates. An alias of
/// TIM::Calendar.
using Calendar = TIM::Calendar;

/// @brief Convert a real number of seconds to a Duration.
/// @param time_in The time in seconds [s].
/// @return The time as a whole number of seconds.
Duration real_to_time(double time_in);

/// @brief Convert a Duration to a real number of seconds.
/// @param time The span to convert.
/// @return The span in seconds [s].
double time_to_real(Duration time);

} // namespace MOM
