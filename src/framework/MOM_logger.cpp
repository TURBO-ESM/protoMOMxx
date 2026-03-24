#include "MOM_logger.h"

void MOM::logger::log_impl(LogLevel level, std::string_view message) {
  if (level == LogLevel::FATAL || level == LogLevel::WARNING) {
    (*err_stream_) << message << '\n';
  } else if (level <= log_level_) {
    (*log_stream_) << message << '\n';
  }
  if (level == LogLevel::FATAL) {
    err_stream_->flush();
    log_stream_->flush();
    throw FatalError(std::string(message));
  }
}

void MOM::logger::set_verbosity(LogLevel level) { log_level_ = level; }

void MOM::logger::set_verbosity(int level) {
  // Ensure enum values match the hardcoded cases below
  static_assert(static_cast<int>(LogLevel::FATAL) == 0);
  static_assert(static_cast<int>(LogLevel::WARNING) == 1);
  static_assert(static_cast<int>(LogLevel::NOTE) == 2);
  static_assert(static_cast<int>(LogLevel::INFO) == 3);
  static_assert(static_cast<int>(LogLevel::DEBUG) == 9);
  switch (level) {
  case 0:
  case 1:
  case 2:
  case 3:
  case 9:
    set_verbosity(static_cast<LogLevel>(level));
    break;
  default:
    throw std::invalid_argument(
        "Invalid verbosity level: " + std::to_string(level) +
        ". Valid values are 0 (FATAL), 1 (WARNING), 2 (NOTE), 3 (INFO), 9 "
        "(DEBUG).");
  }
}

MOM::logger::LogLevel MOM::logger::get_verbosity() { return log_level_; }

void MOM::logger::set_log_stream(std::ostream &log_stream) {
  log_stream_ = &log_stream;
}

void MOM::logger::set_err_stream(std::ostream &err_stream) {
  err_stream_ = &err_stream;
}
