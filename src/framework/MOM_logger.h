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

namespace MOM {

/// @brief Logging class with static members for controlling message severity and verbosity.
class logger {
public:

  /// @brief Log severity levels, ordered from most to least severe.
  ///
  /// Numeric gaps are intentional to allow future levels to be inserted
  /// without renumbering existing ones.
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

  /// @brief Redirect log output streams.
  /// @param log_stream Destination for INFO/NOTE/DEBUG messages (default: std::cout).
  /// @param err_stream Destination for WARNING/FATAL messages (default: std::cerr).
  static void set_stream(std::ostream& log_stream, std::ostream& err_stream = std::cerr);

  /// @brief Set the minimum verbosity level. Messages above this level are suppressed.
  /// @param level The new verbosity threshold.
  static void set_verbosity(LogLevel level);

  /// @brief Return the current verbosity level.
  /// @return The current verbosity threshold (of type LogLevel).
  static LogLevel get_verbosity();

  /// @brief Log a message at the specified log level.
  /// @param level The log level at which to log the message.
  /// @param args The components of the message to log. These will be streamed together into a single message.
  /// @note It is preferred to use the level-specific logging functions (e.g. `fatal`, `warning`, etc.).
  template<typename... Args>
  static void log(LogLevel level, Args&&... args) {
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    log_impl(level, oss.str());
  }

  /// @brief Log a message at the FATAL log level and throw FatalError.
  /// @param args The components of the message to log. These will be streamed together into a single message.
  /// @throws FatalError Always thrown after logging the message.
  template<typename... Args>
  static void fatal(Args&&... args)   { log(LogLevel::FATAL,   std::forward<Args>(args)...); }

  /// @brief Log a message at the WARNING log level.
  /// @param args The components of the message to log. These will be streamed together into a single message.
  template<typename... Args>
  static void warning(Args&&... args) { log(LogLevel::WARNING, std::forward<Args>(args)...); }

  /// @brief Log a message at the NOTE log level.
  /// @param args The components of the message to log. These will be streamed together into a single message.
  template<typename... Args>
  static void note(Args&&... args)    { log(LogLevel::NOTE,    std::forward<Args>(args)...); }

  /// @brief Log a message at the INFO log level.
  /// @param args The components of the message to log. These will be streamed together into a single message.
  template<typename... Args>
  static void info(Args&&... args)    { log(LogLevel::INFO,    std::forward<Args>(args)...); }

  /// @brief Log a message at the DEBUG log level.
  /// @param args The components of the message to log. These will be streamed together into a single message.
  template<typename... Args>
  static void debug(Args&&... args)   { log(LogLevel::DEBUG,   std::forward<Args>(args)...); }

  /// @brief Returns true if the current verbosity level includes call tree output.
  /// @return True if call tree output is enabled, false otherwise.
  static bool callTree_showQuery();

  /// @brief Record a milestone within a subroutine in the call tree.
  /// @param mesg Description of the milestone.
  static void callTree_waypoint(std::string_view mesg);

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

private:
  /// @brief Core logging function that routes messages based on log level and current verbosity.
  static void log_impl(LogLevel level, std::string_view message);

  inline static LogLevel      log_level_       = LogLevel::CALLTREE;
  inline static std::ostream* log_stream_      = &std::cout;
  inline static std::ostream* err_stream_      = &std::cerr;
  inline static int           call_tree_depth_ = 0;
};

} // namespace MOM
