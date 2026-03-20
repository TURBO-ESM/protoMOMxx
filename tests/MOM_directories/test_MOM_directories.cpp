// Unit tests for MOM_directories.cpp

#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "MOM_directories.h"

using namespace MOM;

// The Directories constructor reads "input.nml" from the current working directory,
// so we chdir into the test source directory that contains our fixture input.nml.

class MOMDirectoriesTest : public ::testing::Test {
protected:
  std::filesystem::path original_cwd_;

  void SetUp() override {
    original_cwd_ = std::filesystem::current_path();
    auto test_dir = std::filesystem::path(__FILE__).parent_path();
    std::filesystem::current_path(test_dir);
    ASSERT_TRUE(std::filesystem::exists("input.nml"))
        << "input.nml not found in: " << test_dir;
  }

  void TearDown() override {
    std::filesystem::current_path(original_cwd_);
  }
};

TEST_F(MOMDirectoriesTest, ReadsDirectoriesFromNml) {
  Directories dirs;

  EXPECT_EQ(dirs.output_directory(), "./");
  EXPECT_EQ(dirs.input_filename(), "n");
  EXPECT_EQ(dirs.restart_input_dir(), "INPUT/");
  EXPECT_EQ(dirs.restart_output_dir(), "RESTART/");
}

TEST_F(MOMDirectoriesTest, ReadsParameterFilenames) {
  Directories dirs;

  const auto& pfiles = dirs.parameter_filenames();
  ASSERT_EQ(pfiles.size(), 2);
  EXPECT_EQ(pfiles[0], "MOM_input");
  EXPECT_EQ(pfiles[1], "MOM_override");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
