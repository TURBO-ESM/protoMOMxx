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
    MOM_logger::set_stream(log_out, err_out);
    MOM_logger::set_verbosity(MOM_logger::LogLevel::DEBUG);
  }

  void TearDown() override {
    MOM_logger::set_stream(std::cout, std::cerr);
    MOM_logger::set_verbosity(MOM_logger::LogLevel::CALLTREE);
  }
};

// 1. Verbosity get/set round-trip
TEST_F(MOMLoggerTest, VerbosityGetSet) {
  MOM_logger::set_verbosity(MOM_logger::LogLevel::NOTE);
  EXPECT_EQ(MOM_logger::get_verbosity(), MOM_logger::LogLevel::NOTE);

  MOM_logger::set_verbosity(MOM_logger::LogLevel::DEBUG);
  EXPECT_EQ(MOM_logger::get_verbosity(), MOM_logger::LogLevel::DEBUG);
}

// 2. INFO writes to log_stream
TEST_F(MOMLoggerTest, InfoWritesToLogStream) {
  MOM_logger::info("hello world");
  EXPECT_NE(log_out.str().find("hello world"), std::string::npos);
  EXPECT_TRUE(err_out.str().empty());
}

// 3. WARNING writes to err_stream
TEST_F(MOMLoggerTest, WarningWritesToErrStream) {
  MOM_logger::warning("bad thing");
  EXPECT_NE(err_out.str().find("bad thing"), std::string::npos);
  EXPECT_TRUE(log_out.str().empty());
}

// 4. Verbosity filtering — DEBUG suppressed at NOTE level
TEST_F(MOMLoggerTest, VerbosityFiltering) {
  MOM_logger::set_verbosity(MOM_logger::LogLevel::NOTE);
  MOM_logger::debug("should not appear");
  EXPECT_TRUE(log_out.str().empty());
}

// 5. WARNING bypasses verbosity
TEST_F(MOMLoggerTest, WarningBypassesVerbosity) {
  MOM_logger::set_verbosity(MOM_logger::LogLevel::FATAL);
  MOM_logger::warning("still visible");
  EXPECT_NE(err_out.str().find("still visible"), std::string::npos);
}

// 6. Multi-arg message concatenation
TEST_F(MOMLoggerTest, MultiArgConcatenation) {
  MOM_logger::info("a", 1, "b");
  EXPECT_NE(log_out.str().find("a1b"), std::string::npos);
}

// 7. CallTree entry/exit markers
TEST_F(MOMLoggerTest, CallTreeEntryExit) {
  {
    MOM_logger::CallTree scope("myFunc");
    // After construction, should have ---> marker
    EXPECT_NE(log_out.str().find("---> myFunc"), std::string::npos);
  }
  // After destruction, should have <--- marker
  EXPECT_NE(log_out.str().find("<--- myFunc"), std::string::npos);
}

// 8. callTree_waypoint marker
TEST_F(MOMLoggerTest, CallTreeWaypoint) {
  MOM_logger::callTree_waypoint("checkpoint");
  EXPECT_NE(log_out.str().find("o checkpoint"), std::string::npos);
}

// 9. callTree_showQuery based on verbosity
TEST_F(MOMLoggerTest, CallTreeShowQuery) {
  MOM_logger::set_verbosity(MOM_logger::LogLevel::CALLTREE);
  EXPECT_TRUE(MOM_logger::callTree_showQuery());

  MOM_logger::set_verbosity(MOM_logger::LogLevel::DEBUG);
  EXPECT_TRUE(MOM_logger::callTree_showQuery());

  MOM_logger::set_verbosity(MOM_logger::LogLevel::INFO);
  EXPECT_FALSE(MOM_logger::callTree_showQuery());
}

// 10. FATAL calls std::exit
TEST_F(MOMLoggerTest, FatalExits) {
  EXPECT_EXIT(
    MOM_logger::fatal("crash now"),
    ::testing::ExitedWithCode(EXIT_FAILURE),
    ""
  );
}
