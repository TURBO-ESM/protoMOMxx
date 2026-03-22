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

void MOM::logger::set_verbosity(LogLevel level) { 
  log_level_ = level; 
}

void MOM::logger::set_verbosity(int level) {
  if (level < 0 || level > 9) {
    warning("Invalid verbosity level: ", level, ". Valid range is 0-9. Keeping previous level: ", log_level_);
    return;
  }
  set_verbosity(static_cast<LogLevel>(level));
}

MOM::logger::LogLevel MOM::logger::get_verbosity() {
  return log_level_;
}

void MOM::logger::set_log_stream(std::ostream& log_stream) {
  log_stream_ = &log_stream;
}

void MOM::logger::set_err_stream(std::ostream& err_stream) {
  err_stream_ = &err_stream;
}
