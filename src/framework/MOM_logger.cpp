#include "MOM_logger.h"
#include <cstdlib>

namespace MOM::logger::detail {

  LogLevel      log_level  = LogLevel::CALLTREE;
  std::ostream* log_stream = &std::cout;
  std::ostream* err_stream = &std::cerr;
  int           call_tree_depth = 0;

  void log(LogLevel level, std::string_view message) {
    if (level == LogLevel::FATAL || level == LogLevel::WARNING) {
      (*err_stream) << message << '\n';
    } else if (level <= log_level) {
      (*log_stream) << message << '\n';
    }
    if (level == LogLevel::FATAL) {
      err_stream->flush();
      log_stream->flush();
      throw FatalError(std::string(message));
    }
  }

} // namespace MOM::logger::detail

namespace MOM::logger {

  void set_verbosity(LogLevel level) { detail::log_level = level; }
  LogLevel get_verbosity()           { return detail::log_level;  }

  void set_stream(std::ostream& log_stream, std::ostream& err_stream) {
    detail::log_stream = &log_stream;
    detail::err_stream = &err_stream;
  }

  bool callTree_showQuery() {
    return detail::log_level >= LogLevel::CALLTREE;
  }

  void callTree_waypoint(std::string_view mesg) {
    if (detail::log_level < LogLevel::CALLTREE) return;
    std::string indent(3 * detail::call_tree_depth, ' ');
    detail::log(LogLevel::CALLTREE, "callTree: " + indent + "o " + std::string(mesg));
  }

  CallTree::CallTree(std::string_view mesg) : mesg_(mesg) {
    detail::call_tree_depth++;
    if (detail::log_level < LogLevel::CALLTREE) return;
    std::string indent(3 * (detail::call_tree_depth - 1), ' ');
    detail::log(LogLevel::CALLTREE, "callTree: " + indent + "---> " + mesg_);
  }

  CallTree::~CallTree() {
    if (detail::call_tree_depth < 1) {
      (*detail::err_stream) << "FATAL: callTree depth underflow at " << mesg_ << '\n';
      detail::err_stream->flush();
      detail::log_stream->flush();
      std::abort();
    }
    detail::call_tree_depth--;
    if (detail::log_level < LogLevel::CALLTREE) return;
    std::string indent(3 * detail::call_tree_depth, ' ');
    detail::log(LogLevel::CALLTREE, "callTree: " + indent + "<--- " + mesg_);
  }

} // namespace MOM::logger