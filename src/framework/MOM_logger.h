/**
 * @file MOM_logger.h
 * @brief Logging utility for protoMOMxx.
 *
 * Provides logging and error handling. Intended to mirror the behavior
 * of MOM_error_handler.F90 from MOM6.
 *
 * ## Usage
 * @code
 *   MOM::logger::set_verbosity(MOM::logger::LogLevel::DEBUG);
 *   MOM::logger::info("Starting timestep ", n, " dt=", dt);
 *   MOM::logger::warning("CFL condition approaching limit: dt=", dt);
 * @endcode
 *
 * ## Log routing
 *   - FATAL and WARNING always write to err_stream (default: std::cerr)
 *   - All other levels write to log_stream (default: std::cout) if they
 *     are at or below the current verbosity level
 *   - FATAL logs to err_stream and throws MOM::logger::FatalError
 */


#pragma once
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>

/// @brief Log severity levels, ordered from most to least severe.
///
/// Numeric gaps are intentional to allow future levels to be inserted
/// without renumbering existing ones. CALLTREE logging is not yet implemented.
namespace MOM::logger {

/// @brief Log levels for controlling message severity and verbosity.
enum class LogLevel {
  FATAL     = 0,  ///< Unrecoverable error. Logs to err_stream then throws FatalError.
  WARNING   = 1,  ///< Important warning. Always logs to err_stream.
  NOTE      = 2,  ///< Notable but non-critical information.
  INFO      = 3,  ///< General progress information.
  CALLTREE  = 6,  ///< Call tree tracing.
  DEBUG     = 9   ///< Detailed diagnostic output.
};

/// @brief Exception thrown by fatal() to signal an unrecoverable error.
class FatalError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

namespace detail {
  /// @brief Core logging function that routes messages based on log level and current verbosity.
  /// @param level The severity level of the message being logged.
  /// @param message The complete message to log, already formatted as a single string.
  void log(LogLevel level, std::string_view message);
} // namespace detail

/// @brief Redirect log output streams.
/// @param log_stream Destination for INFO/NOTE/DEBUG messages (default: std::cout).
/// @param err_stream Destination for WARNING/FATAL messages (default: std::cerr).
void set_stream(std::ostream& log_stream, std::ostream& err_stream = std::cerr);

/// @brief Set the minimum verbosity level. Messages above this level are suppressed.
/// @param level The new verbosity threshold.
void set_verbosity(LogLevel level);

/// @brief Return the current verbosity level.
/// @return The current verbosity threshold (of type LogLevel).
LogLevel get_verbosity();

/// @brief Log a message at the specified log level.
/// @param level The log level at which to log the message.
/// @param args The components of the message to log. These will be streamed together into a single message.
/// @note It is preferred to use the level-specific logging functions (e.g. `fatal`, `warning`, etc.).
template<typename... Args>
inline void log(LogLevel level, Args&&... args) {
  std::ostringstream oss;
  (oss << ... << std::forward<Args>(args));
  detail::log(level, oss.str());
}

/// @brief Log a message at the FATAL log level and throw FatalError.
/// @param args The components of the message to log. These will be streamed together into a single message.
/// @throws FatalError Always thrown after logging the message.
template<typename... Args>
inline void fatal(Args&&... args)   { log(LogLevel::FATAL,   std::forward<Args>(args)...); }

/// @brief Log a message at the WARNING log level.
/// @param args The components of the message to log. These will be streamed together into a single message.
template<typename... Args>
inline void warning(Args&&... args) { log(LogLevel::WARNING, std::forward<Args>(args)...); }

/// @brief Log a message at the NOTE log level.
/// @param args The components of the message to log. These will be streamed together into a single message.
template<typename... Args>
inline void note(Args&&... args)    { log(LogLevel::NOTE,    std::forward<Args>(args)...); }

/// @brief Log a message at the INFO log level.
/// @param args The components of the message to log. These will be streamed together into a single message.
template<typename... Args>
inline void info(Args&&... args)    { log(LogLevel::INFO,    std::forward<Args>(args)...); }

/// @brief Log a message at the DEBUG log level.
/// @param args The components of the message to log. These will be streamed together into a single message.
template<typename... Args>
inline void debug(Args&&... args)   { log(LogLevel::DEBUG,   std::forward<Args>(args)...); }

/// @brief Returns true if the current verbosity level includes call tree output.
/// @return True if call tree output is enabled, false otherwise.
bool callTree_showQuery();

/// @brief Record a milestone within a subroutine in the call tree.
/// @param mesg Description of the milestone.
void callTree_waypoint(std::string_view mesg);

/// @brief RAII guard for call tree tracing.
/// 
/// Logs entry on construction and exit on destruction.
/// 
/// @code
///   void MOM_step() {
///     MOM::logger::CallTree scope("MOM_step");
///     MOM::logger::callTree_waypoint("before btstep");
///     // ... scope exits and logs "<--- MOM_step" automatically
///   }
/// @endcode
class CallTree {
public:
  /// @brief Construct a CallTree scope with the given message. Logs entry if CALLTREE level is enabled.
  /// @param mesg Description of the scope (e.g. function name).
  explicit CallTree(std::string_view mesg);
  ~CallTree();
  CallTree(const CallTree&)            = delete;
  CallTree& operator=(const CallTree&) = delete;
private:
  std::string mesg_;
};



} // namespace MOM::logger