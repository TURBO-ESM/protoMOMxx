#include "MOM_logger.hpp"
#include <cstdlib>
#include <iostream>

namespace MOM_logger::detail {

  LogLevel      log_level  = LogLevel::CALLTREE;
  std::ostream* log_stream = &std::cout;
  std::ostream* err_stream = &std::cerr;

  void log(LogLevel level, const std::string& message) {
    if (level == LogLevel::FATAL || level == LogLevel::WARNING) {
      (*err_stream) << message << '\n';
    } else if (level <= log_level) {
      (*log_stream) << message << '\n';
    }
    if (level == LogLevel::FATAL) {
      err_stream->flush();
      std::exit(EXIT_FAILURE);
    }
  }

} // namespace MOM_logger::detail

namespace MOM_logger {

  void set_verbosity(LogLevel level) { detail::log_level = level; }
  LogLevel get_verbosity()           { return detail::log_level;  }

  void set_stream(std::ostream& log_stream, std::ostream& err_stream) {
    detail::log_stream = &log_stream;
    detail::err_stream = &err_stream;
  }

} // namespace MOM_logger