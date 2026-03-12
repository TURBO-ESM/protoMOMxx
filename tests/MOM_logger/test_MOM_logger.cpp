#include <gtest/gtest.h>
#include <sstream>
#include "MOM_logger.h"

class MOMLoggerTest : public ::testing::Test {
protected:
  std::ostringstream log_out;
  std::ostringstream err_out;

  void SetUp() override {
    log_out.str("");
    err_out.str("");
    MOM::logger::set_stream(log_out, err_out);
    MOM::logger::set_verbosity(MOM::logger::LogLevel::DEBUG);
  }

  void TearDown() override {
    MOM::logger::set_stream(std::cout, std::cerr);
    MOM::logger::set_verbosity(MOM::logger::LogLevel::DEBUG);
  }
};

// 1. Verbosity get/set round-trip
TEST_F(MOMLoggerTest, VerbosityGetSet) {
  MOM::logger::set_verbosity(MOM::logger::LogLevel::NOTE);
  EXPECT_EQ(MOM::logger::get_verbosity(), MOM::logger::LogLevel::NOTE);

  MOM::logger::set_verbosity(MOM::logger::LogLevel::DEBUG);
  EXPECT_EQ(MOM::logger::get_verbosity(), MOM::logger::LogLevel::DEBUG);
}

// 2. INFO writes to log_stream
TEST_F(MOMLoggerTest, InfoWritesToLogStream) {
  MOM::logger::info("hello world");
  EXPECT_NE(log_out.str().find("hello world"), std::string::npos);
  EXPECT_TRUE(err_out.str().empty());
}

// 3. WARNING writes to err_stream
TEST_F(MOMLoggerTest, WarningWritesToErrStream) {
  MOM::logger::warning("bad thing");
  EXPECT_NE(err_out.str().find("bad thing"), std::string::npos);
  EXPECT_TRUE(log_out.str().empty());
}

// 4. Verbosity filtering — DEBUG suppressed at NOTE level
TEST_F(MOMLoggerTest, VerbosityFiltering) {
  MOM::logger::set_verbosity(MOM::logger::LogLevel::NOTE);
  MOM::logger::debug("should not appear");
  EXPECT_TRUE(log_out.str().empty());
}

// 5. WARNING bypasses verbosity
TEST_F(MOMLoggerTest, WarningBypassesVerbosity) {
  MOM::logger::set_verbosity(MOM::logger::LogLevel::FATAL);
  MOM::logger::warning("still visible");
  EXPECT_NE(err_out.str().find("still visible"), std::string::npos);
}

// 6. Multi-arg message concatenation
TEST_F(MOMLoggerTest, MultiArgConcatenation) {
  MOM::logger::info("a", 1, "b");
  EXPECT_NE(log_out.str().find("a1b"), std::string::npos);
}

// 7. FATAL throws FatalError
TEST_F(MOMLoggerTest, FatalThrowsFatalError) {
  EXPECT_THROW(MOM::logger::fatal("crash now"), MOM::logger::FatalError);
}
