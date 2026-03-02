// Unit test for MOM_get_input.cpp using GoogleTest

#include <filesystem>
#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "MOM_get_input.h"

using namespace MOM_get_input;

// Helper function to get the absolute path to the test data directory
std::filesystem::path get_test_data_dir() { return std::filesystem::path(__FILE__).parent_path(); }

TEST(MOMGetInputTest, ReadNamelist) {
  auto test_file_path = get_test_data_dir() / "input_test.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  Directories dirs;
  std::vector<std::string> param_files;

  ASSERT_NO_THROW(dirs = get_directories(test_file_path, -1);
                  param_files = get_parameter_filenames(test_file_path, -1););

  ASSERT_TRUE(!param_files.empty());

  // Check directory values
  EXPECT_EQ(dirs.output_directory, "./output");
  EXPECT_EQ(dirs.restart_input_dir, "./RESTART");
  EXPECT_EQ(dirs.restart_output_dir, "./RESTART");
  EXPECT_EQ(dirs.input_filename, "n");

  // Check parameter filenames
  EXPECT_EQ(param_files.size(), 2);
  EXPECT_EQ(param_files[0], "MOM_input");
  EXPECT_EQ(param_files[1], "MOM_override");
}

TEST(MOMGetInputTest, DefaultInputFilename) {
  auto test_file_path = get_test_data_dir() / "input_test.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  Directories dirs;
  std::vector<std::string> param_files;

  // The namelist has INPUT_FILENAME = 'n', so it should override the default
  ASSERT_NO_THROW(dirs = get_directories(test_file_path, -1);
                  param_files = get_parameter_filenames(test_file_path, -1););

  EXPECT_EQ(dirs.input_filename, "n");
}

TEST(MOMGetInputTest, EnsembleNumber) {
  auto test_file_path = get_test_data_dir() / "input_test.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  Directories dirs;
  std::vector<std::string> param_files;

  ASSERT_NO_THROW(dirs = get_directories(test_file_path, 5); param_files = get_parameter_filenames(test_file_path, 5););

  ASSERT_TRUE(!param_files.empty());

  // With ensemble number, paths should have .5 appended
  EXPECT_EQ(dirs.output_directory, "./output.5");
  EXPECT_EQ(dirs.input_filename, "n.5");
}

TEST(MOMGetInputTest, MissingNamelistFile) {
  auto test_file_path = get_test_data_dir() / "nonexistent.nml";

  Directories dirs;
  std::vector<std::string> param_files;

  EXPECT_THROW(get_directories(test_file_path, -1), std::runtime_error);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}