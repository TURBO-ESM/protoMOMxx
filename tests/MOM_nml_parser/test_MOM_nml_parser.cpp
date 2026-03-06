// Unit test for MOM_nml_parser.cpp using GoogleTest

#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "MOM_nml_parser.h"

// Helper function to get the absolute path to the test data directory
std::filesystem::path get_test_data_dir() { return std::filesystem::path(__FILE__).parent_path() / "nml_files"; }

// Helper function to check if a value is of a specific type and has the expected value
template <typename T> bool check_value(const ParamValue &val, const T &expected) {
  if (std::holds_alternative<T>(val)) {
    return std::get<T>(val) == expected;
  }
  return false;
}

// Helper function for double comparison with tolerance
bool check_double_value(const ParamValue &val, double expected, double tolerance = 1e-9) {
  if (std::holds_alternative<double>(val)) {
    return std::abs(std::get<double>(val) - expected) < tolerance;
  }
  return false;
}

// Helper function to check if a value is a vector of a specific type
template <typename T> bool is_vector_of(const ParamValue &val) { return std::holds_alternative<std::vector<T>>(val); }

TEST(MOMNmlParserTest, ParseNamelistSimple) {
  auto test_file_path = get_test_data_dir() / "namelist_simple.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  EXPECT_TRUE(nml.has_param("DT", "OCEAN"));
  EXPECT_TRUE(nml.has_param("REENTRANT_X", "OCEAN"));
  EXPECT_TRUE(nml.has_param("REENTRANT_Y", "OCEAN"));
  EXPECT_TRUE(nml.has_param("COORD_TYPE", "OCEAN"));
  EXPECT_TRUE(nml.has_param("GRAVITY", "OCEAN"));

  EXPECT_EQ(nml.get_as<int>("DT", "OCEAN"), 3600);
  EXPECT_FALSE(nml.get_as<bool>("REENTRANT_X", "OCEAN"));
  EXPECT_TRUE(nml.get_as<bool>("REENTRANT_Y", "OCEAN"));
  EXPECT_EQ(nml.get_as<std::string>("COORD_TYPE", "OCEAN"), "ALE");
  EXPECT_TRUE(check_double_value(nml.get("GRAVITY", "OCEAN"), 9.81));

  // Check that all 5 parameters in the file are present
  EXPECT_EQ(nml.get_num_parameters(), 5);
}

TEST(MOMNmlParserTest, ParseNamelistModules) {
  auto test_file_path = get_test_data_dir() / "namelist_modules.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  // Check that we have the expected namelists
  auto namelists = nml.get_namelists();
  EXPECT_EQ(namelists.size(), 3);

  // Check OCEAN_PARAMS namelist
  EXPECT_EQ(nml.get_as<int>("NIGLOBAL", "OCEAN_PARAMS"), 540);
  EXPECT_EQ(nml.get_as<int>("NJGLOBAL", "OCEAN_PARAMS"), 480);
  EXPECT_EQ(nml.get_as<int>("NK", "OCEAN_PARAMS"), 75);
  EXPECT_TRUE(nml.get_as<bool>("TRIPOLAR_N", "OCEAN_PARAMS"));

  // Check MLE_PARAMS namelist
  EXPECT_FALSE(nml.get_as<bool>("USE_BODNER23", "MLE_PARAMS"));
  EXPECT_TRUE(check_double_value(nml.get("MLD_DECAYING_TFILTER", "MLE_PARAMS"), 2592000.0));
  EXPECT_TRUE(nml.get_as<bool>("KHTH_USE_EBT_STRUCT", "MLE_PARAMS"));

  // Check KPP_PARAMS namelist
  EXPECT_EQ(nml.get_as<int>("N_SMOOTH", "KPP_PARAMS"), 3);
  EXPECT_TRUE(check_double_value(nml.get("RI_CRIT", "KPP_PARAMS"), 0.3));
  EXPECT_EQ(nml.get_as<int>("ENHANCE", "KPP_PARAMS"), 1);

  // Verify parameters are in the correct namelist
  EXPECT_THROW(nml.get("NIGLOBAL", "MLE_PARAMS"), std::out_of_range);
  EXPECT_THROW(nml.get("N_SMOOTH", "OCEAN_PARAMS"), std::out_of_range);

  // Check total parameter count
  EXPECT_EQ(nml.get_num_parameters(), 10);
}

TEST(MOMNmlParserTest, ParseNamelistArrays) {
  auto test_file_path = get_test_data_dir() / "namelist_arrays.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  // Check scalar
  EXPECT_TRUE(std::holds_alternative<int>(nml.get("INT_SCALAR", "ARRAY_TEST")));
  EXPECT_EQ(nml.get_as<int>("INT_SCALAR", "ARRAY_TEST"), 42);

  // Check integer array
  EXPECT_TRUE(is_vector_of<int>(nml.get("INT_ARRAY", "ARRAY_TEST")));
  auto int_array = nml.get_as<std::vector<int>>("INT_ARRAY", "ARRAY_TEST");
  EXPECT_EQ(int_array, std::vector<int>({1, 2, 3, 4, 5}));

  // Check real array
  EXPECT_TRUE(is_vector_of<double>(nml.get("REAL_ARRAY", "ARRAY_TEST")));
  auto real_array = nml.get_as<std::vector<double>>("REAL_ARRAY", "ARRAY_TEST");
  EXPECT_EQ(real_array, std::vector<double>({1.0, 2.0, 3.0}));

  // Check string array
  EXPECT_TRUE(is_vector_of<std::string>(nml.get("STRING_ARRAY", "ARRAY_TEST")));
  auto string_array = nml.get_as<std::vector<std::string>>("STRING_ARRAY", "ARRAY_TEST");
  EXPECT_EQ(string_array, std::vector<std::string>({"file1.nc", "file2.nc", "file3.nc"}));

  // Check boolean array
  EXPECT_TRUE(is_vector_of<bool>(nml.get("BOOL_ARRAY", "ARRAY_TEST")));
  auto bool_array = nml.get_as<std::vector<bool>>("BOOL_ARRAY", "ARRAY_TEST");
  std::vector<bool> expected_bool = {true, false, true};
  EXPECT_EQ(bool_array.size(), expected_bool.size());
  for (size_t i = 0; i < bool_array.size(); ++i) {
    EXPECT_EQ(bool_array[i], expected_bool[i]);
  }

  // Check trailing comma handling
  EXPECT_TRUE(is_vector_of<int>(nml.get("TRAILING_COMMA", "ARRAY_TEST")));
  auto trailing_array = nml.get_as<std::vector<int>>("TRAILING_COMMA", "ARRAY_TEST");
  EXPECT_EQ(trailing_array, std::vector<int>({10, 20, 30}));
}

TEST(MOMNmlParserTest, ParseNamelistComments) {
  auto test_file_path = get_test_data_dir() / "namelist_comments.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  // Verify all variables are parsed correctly despite comments
  EXPECT_EQ(nml.get_as<int>("VAR1", "TEST_COMMENTS"), 100);
  EXPECT_EQ(nml.get_as<std::string>("VAR2", "TEST_COMMENTS"), "test string");
  EXPECT_TRUE(nml.get_as<bool>("VAR3", "TEST_COMMENTS"));
  EXPECT_TRUE(check_double_value(nml.get("VAR4", "TEST_COMMENTS"), 3.14159));

  EXPECT_EQ(nml.get_num_parameters(), 4);
}

TEST(MOMNmlParserTest, InvalidFileNonExistent) {
  auto test_file_path = get_test_data_dir() / "non_existent_file.nml";
  EXPECT_THROW(NamelistParams nml(test_file_path.string()), std::runtime_error);
}

TEST(MOMNmlParserTest, InvalidFileMissingClose) {
  auto test_file_path = get_test_data_dir() / "namelist_invalid1.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  EXPECT_THROW(NamelistParams nml(test_file_path.string()), std::runtime_error);
}

TEST(MOMNmlParserTest, InvalidFileOutsideNamelist) {
  auto test_file_path = get_test_data_dir() / "namelist_invalid2.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  EXPECT_THROW(NamelistParams nml(test_file_path.string()), std::runtime_error);
}

TEST(MOMNmlParserTest, InvalidFileNestedNamelists) {
  auto test_file_path = get_test_data_dir() / "namelist_invalid3.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  EXPECT_THROW(NamelistParams nml(test_file_path.string()), std::runtime_error);
}

TEST(MOMNmlParserTest, CaseInsensitivity) {
  auto test_file_path = get_test_data_dir() / "namelist_simple.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  // Fortran namelists are case-insensitive, but our parser converts to uppercase
  // Test that we can access using uppercase (as stored)
  EXPECT_TRUE(nml.has_param("DT", "OCEAN"));
  EXPECT_TRUE(nml.has_param("REENTRANT_X", "OCEAN"));
  EXPECT_TRUE(nml.has_param("COORD_TYPE", "OCEAN"));
}

TEST(MOMNmlParserTest, TypeMismatch) {
  auto test_file_path = get_test_data_dir() / "namelist_simple.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  // DT is an integer, trying to get as double should throw
  EXPECT_THROW(nml.get_as<double>("DT", "OCEAN"), std::runtime_error);

  // REENTRANT_X is a bool, trying to get as int should throw
  EXPECT_THROW(nml.get_as<int>("REENTRANT_X", "OCEAN"), std::runtime_error);
}

// Read in double_gyre_input.nml and check that the expected parameters are present and correct:
TEST(MOMNmlParserTest, ParseDoubleGyreInput) {
  auto test_file_path = get_test_data_dir() / "double_gyre_input.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";
  NamelistParams nml(test_file_path.string());

  EXPECT_EQ(nml.get_as<std::string>("output_directory", "MOM_input_nml"), "./");
  EXPECT_EQ(nml.get_as<std::string>("input_filename", "MOM_INPUT_NML"), "n");
  EXPECT_EQ(nml.get_as<std::string>("restart_input_dir", "MOM_INPUT_NML"), "INPUT/");
  EXPECT_EQ(nml.get_as<std::string>("restart_output_dir", "MOM_INPUT_NML"), "RESTART/");
  EXPECT_TRUE(nml.has_param("parameter_filename", "MOM_INPUT_NML"));
  const auto &param_val = nml.get("parameter_filename", "MOM_INPUT_NML");
  EXPECT_TRUE(std::holds_alternative<std::vector<std::string>>(param_val));
  auto param_files = std::get<std::vector<std::string>>(param_val);
  EXPECT_EQ(param_files.size(), 2);
  EXPECT_EQ(param_files[0], "MOM_input");
  EXPECT_EQ(param_files[1], "MOM_override");
}

// ============================================================================
// Tests for tests_nml/ input files
// ============================================================================

// Helper function to get the path to tests_nml test data directory
std::filesystem::path get_tests_nml_dir() { return std::filesystem::path(__FILE__).parent_path() / "tests_nml"; }

// Helper to validate the common MOM_input_nml 5-parameter structure shared by
// input.0, input.1, input.2, input.9, input.15, input.16.
void validate_mom_input_nml_5params(const NamelistParams &nml) {
  EXPECT_EQ(nml.get_namelists().size(), 1) << "Only mom_input_nml should be present (empty namelists are not stored)";
  EXPECT_EQ(nml.get_num_parameters(), 5);

  EXPECT_EQ(nml.get_as<std::string>("output_directory", "MOM_INPUT_NML"), "./");
  EXPECT_EQ(nml.get_as<std::string>("input_filename", "MOM_INPUT_NML"), "n");
  EXPECT_EQ(nml.get_as<std::string>("restart_input_dir", "MOM_INPUT_NML"), "INPUT/");
  EXPECT_EQ(nml.get_as<std::string>("restart_output_dir", "MOM_INPUT_NML"), "RESTART/");

  EXPECT_TRUE(nml.has_param("parameter_filename", "MOM_INPUT_NML"));
  const auto &pf = nml.get("parameter_filename", "MOM_INPUT_NML");
  ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(pf));
  auto files = std::get<std::vector<std::string>>(pf);
  ASSERT_EQ(files.size(), 2);
  EXPECT_EQ(files[0], "MOM_input");
  EXPECT_EQ(files[1], "MOM_override");
}

// input.0 – MOM_input_nml with multiline parameter_filename array (/ on same line),
//           plus empty diag_manager_nml.
TEST(TestsNml, Input0) {
  auto path = get_tests_nml_dir() / "input.0";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  validate_mom_input_nml_5params(nml);
}

// input.1 – Same structure as input.0 but parameter_filename values on one line,
//           / on the next line.
TEST(TestsNml, Input1) {
  auto path = get_tests_nml_dir() / "input.1";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  validate_mom_input_nml_5params(nml);
}

// input.2 – Same as input.1 with a leading Fortran comment.
TEST(TestsNml, Input2) {
  auto path = get_tests_nml_dir() / "input.2";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  validate_mom_input_nml_5params(nml);
}

// input.3 – Simple &foo with integer and string.
TEST(TestsNml, Input3) {
  auto path = get_tests_nml_dir() / "input.3";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  EXPECT_EQ(nml.get_as<int>("a", "FOO"), 4);
  EXPECT_EQ(nml.get_as<std::string>("b", "FOO"), "a string");
}

// input.4 – &foo with different variable names (c, d).
TEST(TestsNml, Input4) {
  auto path = get_tests_nml_dir() / "input.4";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  EXPECT_EQ(nml.get_as<int>("c", "FOO"), 4);
  EXPECT_EQ(nml.get_as<std::string>("d", "FOO"), "a string");
}

// input.5 – &foo with integer and file-path string containing slashes in quotes.
TEST(TestsNml, Input5) {
  auto path = get_tests_nml_dir() / "input.5";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  EXPECT_EQ(nml.get_as<int>("a", "FOO"), 4);
  EXPECT_EQ(nml.get_as<std::string>("b", "FOO"), "/scratch/dennis/foo.nc");
}

// input.6 – Same as input.5 with leading whitespace before &foo.
TEST(TestsNml, Input6) {
  auto path = get_tests_nml_dir() / "input.6";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  EXPECT_EQ(nml.get_as<int>("a", "FOO"), 4);
  EXPECT_EQ(nml.get_as<std::string>("b", "FOO"), "/scratch/dennis/foo.nc");
}

// input.7 – &foo with trailing comma on integer, file-path string; plus empty &bar.
TEST(TestsNml, Input7) {
  auto path = get_tests_nml_dir() / "input.7";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  // Only foo has params; bar is empty and not stored.
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  EXPECT_EQ(nml.get_as<int>("a", "FOO"), 4);
  EXPECT_EQ(nml.get_as<std::string>("b", "FOO"), "/scratch/dennis/foo.nc");
}

// input.8 – &foo_nml with integer and file-path; plus empty &bar.
TEST(TestsNml, Input8) {
  auto path = get_tests_nml_dir() / "input.8";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  EXPECT_EQ(nml.get_as<int>("a", "FOO_NML"), 4);
  EXPECT_EQ(nml.get_as<std::string>("b", "FOO_NML"), "/scratch/dennis/foo.nc");
}

// input.9 – MOM_input_nml without trailing commas; parameter_filename array on
//           one line, empty diag_manager_nml.
TEST(TestsNml, Input9) {
  auto path = get_tests_nml_dir() / "input.9";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  validate_mom_input_nml_5params(nml);
}

// input.10 – &foo with inline closing / on last assignment line.
TEST(TestsNml, Input10) {
  auto path = get_tests_nml_dir() / "input.10";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  EXPECT_EQ(nml.get_as<int>("a", "FOO"), 4);
  EXPECT_EQ(nml.get_as<std::string>("b", "FOO"), "a string");
}

// input.11 – &foo with / on separate line after last assignment.
TEST(TestsNml, Input11) {
  auto path = get_tests_nml_dir() / "input.11";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  EXPECT_EQ(nml.get_as<int>("a", "FOO"), 4);
  EXPECT_EQ(nml.get_as<std::string>("b", "FOO"), "a string");
}

// input.12 – Multiple namelists: &bar (scalar), &bart (int array inline /),
//            &var (multiline int array), &varn (multiline int array with inline comments).
TEST(TestsNml, Input12) {
  auto path = get_tests_nml_dir() / "input.12";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 4);
  EXPECT_EQ(nml.get_num_parameters(), 4);

  // &bar  b = 4
  EXPECT_EQ(nml.get_as<int>("b", "BAR"), 4);

  // &bart  b = 2,3,4
  {
    auto arr = nml.get_as<std::vector<int>>("b", "BART");
    EXPECT_EQ(arr, std::vector<int>({2, 3, 4}));
  }

  // &var  a = 3, (next line) 9
  {
    auto arr = nml.get_as<std::vector<int>>("a", "VAR");
    EXPECT_EQ(arr, std::vector<int>({3, 9}));
  }

  // &varn  a = 3, (next line) 9  (with inline comments)
  {
    auto arr = nml.get_as<std::vector<int>>("a", "VARN");
    EXPECT_EQ(arr, std::vector<int>({3, 9}));
  }
}

// input.13 – &foo with integer array and string.
TEST(TestsNml, Input13) {
  auto path = get_tests_nml_dir() / "input.13";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  {
    auto arr = nml.get_as<std::vector<int>>("a", "FOO");
    EXPECT_EQ(arr, std::vector<int>({4, 1, 3}));
  }
  EXPECT_EQ(nml.get_as<std::string>("b", "FOO"), "a string");
}

// input.14 – &foo with trailing-comma scalar (comment stripped) and string.
TEST(TestsNml, Input14) {
  auto path = get_tests_nml_dir() / "input.14";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  // "a = 4," after comment strip; trailing comma with single value → scalar 4
  EXPECT_EQ(nml.get_as<int>("a", "FOO"), 4);
  EXPECT_EQ(nml.get_as<std::string>("b", "FOO"), "a string");
}

// input.15 – MOM_input_nml with inline comments throughout.
TEST(TestsNml, Input15) {
  auto path = get_tests_nml_dir() / "input.15";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  validate_mom_input_nml_5params(nml);
}

// input.16 – MOM_input_nml with / on a separate line for both namelists.
TEST(TestsNml, Input16) {
  auto path = get_tests_nml_dir() / "input.16";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  validate_mom_input_nml_5params(nml);
}

// input.17 – MOM_input_nml with only parameter_filename (multiline array).
TEST(TestsNml, Input17) {
  auto path = get_tests_nml_dir() / "input.17";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  const auto &pf = nml.get("parameter_filename", "MOM_INPUT_NML");
  ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(pf));
  auto files = std::get<std::vector<std::string>>(pf);
  ASSERT_EQ(files.size(), 2);
  EXPECT_EQ(files[0], "MOM_input");
  EXPECT_EQ(files[1], "MOM_override");
}

// input.18 – &bar_nml with trailing-comma integer array, / on next line.
TEST(TestsNml, Input18) {
  auto path = get_tests_nml_dir() / "input.18";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  auto arr = nml.get_as<std::vector<int>>("bar", "BAR_NML");
  EXPECT_EQ(arr, std::vector<int>({2, 4}));
}

// input.19 – &foo with comments and blank lines, then "&var /" on the last line.
//            The parser treats "&var /" as starting a namelist named "var /" (the
//            closing slash is consumed as part of the name), so parsing throws at
//            EOF with an unterminated namelist.
TEST(TestsNml, Input19) {
  auto path = get_tests_nml_dir() / "input.19";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  EXPECT_THROW(NamelistParams nml(path.string()), std::runtime_error);
}

// input.20 – &foo with integer array, inline /.
TEST(TestsNml, Input20) {
  auto path = get_tests_nml_dir() / "input.20";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  auto arr = nml.get_as<std::vector<int>>("c", "FOO");
  EXPECT_EQ(arr, std::vector<int>({1, 3}));
}

// input.21 – &bar with single scalar, inline /.
TEST(TestsNml, Input21) {
  auto path = get_tests_nml_dir() / "input.21";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  EXPECT_EQ(nml.get_as<int>("b", "BAR"), 4);
}

// input.22 – &var_nml with two-element integer array, inline /.
TEST(TestsNml, Input22) {
  auto path = get_tests_nml_dir() / "input.22";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  auto arr = nml.get_as<std::vector<int>>("a", "VAR_NML");
  EXPECT_EQ(arr, std::vector<int>({3, 4}));
}

// input.23 – &var_nml with trailing-comma scalar, / on next line.
TEST(TestsNml, Input23) {
  auto path = get_tests_nml_dir() / "input.23";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  // "a = 3," with trailing comma; single value after stripping empty → scalar 3
  EXPECT_EQ(nml.get_as<int>("a", "VAR_NML"), 3);
}

// input.24 – &var_nml with multiline integer array (trailing comma continuation).
TEST(TestsNml, Input24) {
  auto path = get_tests_nml_dir() / "input.24";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  auto arr = nml.get_as<std::vector<int>>("a", "VAR_NML");
  EXPECT_EQ(arr, std::vector<int>({3, 4}));
}

// input.25 – &bar with two-element integer array, inline /.
TEST(TestsNml, Input25) {
  auto path = get_tests_nml_dir() / "input.25";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  auto arr = nml.get_as<std::vector<int>>("b", "BAR");
  EXPECT_EQ(arr, std::vector<int>({4, 3}));
}

// input.26 – &bart with three-element integer array, inline /.
TEST(TestsNml, Input26) {
  auto path = get_tests_nml_dir() / "input.26";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  auto arr = nml.get_as<std::vector<int>>("b", "BART");
  EXPECT_EQ(arr, std::vector<int>({2, 3, 4}));
}

// input.27 – &bar (scalar) followed by &var (multiline int array with inline comment).
TEST(TestsNml, Input27) {
  auto path = get_tests_nml_dir() / "input.27";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 2);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  EXPECT_EQ(nml.get_as<int>("b", "BAR"), 4);

  auto arr = nml.get_as<std::vector<int>>("a", "VAR");
  EXPECT_EQ(arr, std::vector<int>({3, 9}));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
