#include "MOM_logger.h"
#include <cstdlib>

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

bool MOM::logger::callTree_showQuery() {
  return log_level_ >= LogLevel::CALLTREE;
}

void MOM::logger::callTree_waypoint(std::string_view mesg) {
  if (log_level_ < LogLevel::CALLTREE) return;
  std::string indent(3 * call_tree_depth_, ' ');
  log_impl(LogLevel::CALLTREE, "callTree: " + indent + "o " + std::string(mesg));
}

MOM::logger::CallTree::CallTree(std::string_view mesg) : mesg_(mesg) {
  call_tree_depth_++;
  if (log_level_ < LogLevel::CALLTREE) return;
  std::string indent(3 * (call_tree_depth_ - 1), ' ');
  log_impl(LogLevel::CALLTREE, "callTree: " + indent + "---> " + mesg_);
}

MOM::logger::CallTree::~CallTree() {
  if (call_tree_depth_ < 1) {
    (*err_stream_) << "FATAL: callTree depth underflow at " << mesg_ << '\n';
    err_stream_->flush();
    log_stream_->flush();
    std::abort();
  }
  call_tree_depth_--;
  if (log_level_ < LogLevel::CALLTREE) return;
  std::string indent(3 * call_tree_depth_, ' ');
  log_impl(LogLevel::CALLTREE, "callTree: " + indent + "<--- " + mesg_);
}
