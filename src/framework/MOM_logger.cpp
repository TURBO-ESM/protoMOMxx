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
MOM::logger::LogLevel MOM::logger::get_verbosity() { return log_level_; }

void MOM::logger::set_stream(std::ostream& log_stream, std::ostream& err_stream) {
  log_stream_ = &log_stream;
  err_stream_ = &err_stream;
}
