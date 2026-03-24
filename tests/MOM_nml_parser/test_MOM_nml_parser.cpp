// Unit test for MOM_nml_parser.cpp using GoogleTest

#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "MOM_nml_parser.h"

using namespace MOM;

// Helper function to get the absolute path to the test data directory
std::filesystem::path get_test_data_dir() {
  return std::filesystem::path(__FILE__).parent_path() / "nml_files";
}

// Helper function to check if a value is of a specific type and has the
// expected value
template <typename T>
bool check_value(const ParamValue &val, const T &expected) {
  if (std::holds_alternative<T>(val)) {
    return std::get<T>(val) == expected;
  }
  return false;
}

// Helper function for double comparison with tolerance
bool check_double_value(const ParamValue &val, double expected,
                        double tolerance = 1e-9) {
  if (std::holds_alternative<double>(val)) {
    return std::abs(std::get<double>(val) - expected) < tolerance;
  }
  return false;
}

// Helper function to check if a value is a vector of a specific type
template <typename T> bool is_vector_of(const ParamValue &val) {
  return std::holds_alternative<std::vector<T>>(val);
}

TEST(MOMNmlParserTest, ParseNamelistSimple) {
  auto test_file_path = get_test_data_dir() / "namelist_simple.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path))
      << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  EXPECT_TRUE(nml.has_param("DT", "OCEAN"));
  EXPECT_TRUE(nml.has_param("REENTRANT_X", "OCEAN"));
  EXPECT_TRUE(nml.has_param("REENTRANT_Y", "OCEAN"));
  EXPECT_TRUE(nml.has_param("COORD_TYPE", "OCEAN"));
  EXPECT_TRUE(nml.has_param("GRAVITY", "OCEAN"));

  int dt;
  nml.get("DT", dt, "OCEAN");
  EXPECT_EQ(dt, 3600);
  bool reentrant_x;
  nml.get("REENTRANT_X", reentrant_x, "OCEAN");
  EXPECT_FALSE(reentrant_x);
  bool reentrant_y;
  nml.get("REENTRANT_Y", reentrant_y, "OCEAN");
  EXPECT_TRUE(reentrant_y);
  std::string coord_type;
  nml.get("COORD_TYPE", coord_type, "OCEAN");
  EXPECT_EQ(coord_type, "ALE");
  EXPECT_TRUE(check_double_value(nml.get_variant("GRAVITY", "OCEAN"), 9.81));

  // Check that all 5 parameters in the file are present
  EXPECT_EQ(nml.get_num_parameters(), 5);
}

TEST(MOMNmlParserTest, ParseNamelistModules) {
  auto test_file_path = get_test_data_dir() / "namelist_modules.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path))
      << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  // Check that we have the expected namelists
  auto namelists = nml.get_namelists();
  EXPECT_EQ(namelists.size(), 3);

  // Check OCEAN_PARAMS namelist
  int niglobal;
  nml.get("NIGLOBAL", niglobal, "OCEAN_PARAMS");
  EXPECT_EQ(niglobal, 540);
  int njglobal;
  nml.get("NJGLOBAL", njglobal, "OCEAN_PARAMS");
  EXPECT_EQ(njglobal, 480);
  int nk;
  nml.get("NK", nk, "OCEAN_PARAMS");
  EXPECT_EQ(nk, 75);
  bool tripolar_n;
  nml.get("TRIPOLAR_N", tripolar_n, "OCEAN_PARAMS");
  EXPECT_TRUE(tripolar_n);

  // Check MLE_PARAMS namelist
  bool use_bodner23;
  nml.get("USE_BODNER23", use_bodner23, "MLE_PARAMS");
  EXPECT_FALSE(use_bodner23);
  EXPECT_TRUE(check_double_value(
      nml.get_variant("MLD_DECAYING_TFILTER", "MLE_PARAMS"), 2592000.0));
  bool khth_use_ebt_struct;
  nml.get("KHTH_USE_EBT_STRUCT", khth_use_ebt_struct, "MLE_PARAMS");
  EXPECT_TRUE(khth_use_ebt_struct);

  // Check KPP_PARAMS namelist
  int n_smooth;
  nml.get("N_SMOOTH", n_smooth, "KPP_PARAMS");
  EXPECT_EQ(n_smooth, 3);
  EXPECT_TRUE(
      check_double_value(nml.get_variant("RI_CRIT", "KPP_PARAMS"), 0.3));
  int enhance;
  nml.get("ENHANCE", enhance, "KPP_PARAMS");
  EXPECT_EQ(enhance, 1);

  // Verify parameters are in the correct namelist
  EXPECT_THROW(nml.get_variant("NIGLOBAL", "MLE_PARAMS"), std::out_of_range);
  EXPECT_THROW(nml.get_variant("N_SMOOTH", "OCEAN_PARAMS"), std::out_of_range);

  // Check total parameter count
  EXPECT_EQ(nml.get_num_parameters(), 10);
}

TEST(MOMNmlParserTest, ParseNamelistArrays) {
  auto test_file_path = get_test_data_dir() / "namelist_arrays.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path))
      << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  // Check scalar
  EXPECT_TRUE(
      std::holds_alternative<int>(nml.get_variant("INT_SCALAR", "ARRAY_TEST")));
  int int_scalar;
  nml.get("INT_SCALAR", int_scalar, "ARRAY_TEST");
  EXPECT_EQ(int_scalar, 42);

  // Check integer array
  EXPECT_TRUE(is_vector_of<int>(nml.get_variant("INT_ARRAY", "ARRAY_TEST")));
  std::vector<int> int_array;
  nml.get("INT_ARRAY", int_array, "ARRAY_TEST");
  EXPECT_EQ(int_array, std::vector<int>({1, 2, 3, 4, 5}));

  // Check real array
  EXPECT_TRUE(
      is_vector_of<double>(nml.get_variant("REAL_ARRAY", "ARRAY_TEST")));
  std::vector<double> real_array;
  nml.get("REAL_ARRAY", real_array, "ARRAY_TEST");
  EXPECT_EQ(real_array, std::vector<double>({1.0, 2.0, 3.0}));

  // Check string array
  EXPECT_TRUE(
      is_vector_of<std::string>(nml.get_variant("STRING_ARRAY", "ARRAY_TEST")));
  std::vector<std::string> string_array;
  nml.get("STRING_ARRAY", string_array, "ARRAY_TEST");
  EXPECT_EQ(string_array,
            std::vector<std::string>({"file1.nc", "file2.nc", "file3.nc"}));

  // Check boolean array
  EXPECT_TRUE(is_vector_of<bool>(nml.get_variant("BOOL_ARRAY", "ARRAY_TEST")));
  std::vector<bool> bool_array;
  nml.get("BOOL_ARRAY", bool_array, "ARRAY_TEST");
  std::vector<bool> expected_bool = {true, false, true};
  EXPECT_EQ(bool_array.size(), expected_bool.size());
  for (size_t i = 0; i < bool_array.size(); ++i) {
    EXPECT_EQ(bool_array[i], expected_bool[i]);
  }

  // Check trailing comma handling
  EXPECT_TRUE(
      is_vector_of<int>(nml.get_variant("TRAILING_COMMA", "ARRAY_TEST")));
  std::vector<int> trailing_array;
  nml.get("TRAILING_COMMA", trailing_array, "ARRAY_TEST");
  EXPECT_EQ(trailing_array, std::vector<int>({10, 20, 30}));
}

TEST(MOMNmlParserTest, ParseNamelistComments) {
  auto test_file_path = get_test_data_dir() / "namelist_comments.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path))
      << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  // Verify all variables are parsed correctly despite comments
  int var1;
  nml.get("VAR1", var1, "TEST_COMMENTS");
  EXPECT_EQ(var1, 100);
  std::string var2;
  nml.get("VAR2", var2, "TEST_COMMENTS");
  EXPECT_EQ(var2, "test string");
  bool var3;
  nml.get("VAR3", var3, "TEST_COMMENTS");
  EXPECT_TRUE(var3);
  EXPECT_TRUE(
      check_double_value(nml.get_variant("VAR4", "TEST_COMMENTS"), 3.14159));

  EXPECT_EQ(nml.get_num_parameters(), 4);
}

TEST(MOMNmlParserTest, InvalidFileNonExistent) {
  auto test_file_path = get_test_data_dir() / "non_existent_file.nml";
  EXPECT_THROW(NamelistParams nml(test_file_path.string()), std::runtime_error);
}

TEST(MOMNmlParserTest, InvalidFileMissingClose) {
  auto test_file_path = get_test_data_dir() / "namelist_invalid1.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path))
      << "Test file " << test_file_path << " does not exist";

  EXPECT_THROW(NamelistParams nml(test_file_path.string()), std::runtime_error);
}

TEST(MOMNmlParserTest, InvalidFileOutsideNamelist) {
  auto test_file_path = get_test_data_dir() / "namelist_invalid2.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path))
      << "Test file " << test_file_path << " does not exist";

  EXPECT_THROW(NamelistParams nml(test_file_path.string()), std::runtime_error);
}

TEST(MOMNmlParserTest, InvalidFileNestedNamelists) {
  auto test_file_path = get_test_data_dir() / "namelist_invalid3.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path))
      << "Test file " << test_file_path << " does not exist";

  EXPECT_THROW(NamelistParams nml(test_file_path.string()), std::runtime_error);
}

TEST(MOMNmlParserTest, CaseInsensitivity) {
  auto test_file_path = get_test_data_dir() / "namelist_simple.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path))
      << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  // Fortran namelists are case-insensitive, but our parser converts to
  // uppercase Test that we can access using uppercase (as stored)
  EXPECT_TRUE(nml.has_param("DT", "OCEAN"));
  EXPECT_TRUE(nml.has_param("REENTRANT_X", "OCEAN"));
  EXPECT_TRUE(nml.has_param("COORD_TYPE", "OCEAN"));
}

TEST(MOMNmlParserTest, TypeMismatch) {
  auto test_file_path = get_test_data_dir() / "namelist_simple.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path))
      << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());

  // DT is an integer, trying to get as double should throw
  double tmp_double;
  EXPECT_THROW(nml.get("DT", tmp_double, "OCEAN"), std::runtime_error);

  // REENTRANT_X is a bool, trying to get as int should throw
  int tmp_int;
  EXPECT_THROW(nml.get("REENTRANT_X", tmp_int, "OCEAN"), std::runtime_error);
}

// Read in double_gyre_input.nml and check that the expected parameters are
// present and correct:
TEST(MOMNmlParserTest, ParseDoubleGyreInput) {
  auto test_file_path = get_test_data_dir() / "double_gyre_input.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path))
      << "Test file " << test_file_path << " does not exist";
  NamelistParams nml(test_file_path.string());

  std::string output_directory;
  nml.get("output_directory", output_directory, "MOM_input_nml");
  EXPECT_EQ(output_directory, "./");
  std::string input_filename;
  nml.get("input_filename", input_filename, "MOM_INPUT_NML");
  EXPECT_EQ(input_filename, "n");
  std::string restart_input_dir;
  nml.get("restart_input_dir", restart_input_dir, "MOM_INPUT_NML");
  EXPECT_EQ(restart_input_dir, "INPUT/");
  std::string restart_output_dir;
  nml.get("restart_output_dir", restart_output_dir, "MOM_INPUT_NML");
  EXPECT_EQ(restart_output_dir, "RESTART/");
  EXPECT_TRUE(nml.has_param("parameter_filename", "MOM_INPUT_NML"));
  const auto &param_val =
      nml.get_variant("parameter_filename", "MOM_INPUT_NML");
  EXPECT_TRUE(std::holds_alternative<std::vector<std::string>>(param_val));
  auto param_files = std::get<std::vector<std::string>>(param_val);
  EXPECT_EQ(param_files.size(), 2);
  EXPECT_EQ(param_files[0], "MOM_input");
  EXPECT_EQ(param_files[1], "MOM_override");
}

// Helper to validate the common MOM_input_nml 5-parameter structure shared by
// mom_input_nml_string_array.nml and mom_input_nml_inline_comments.nml.
void validate_mom_input_nml_5params(const NamelistParams &nml) {
  EXPECT_EQ(nml.get_namelists().size(), 1)
      << "Only mom_input_nml should be present (empty namelists are not "
         "stored)";
  EXPECT_EQ(nml.get_num_parameters(), 5);

  std::string output_directory;
  nml.get("output_directory", output_directory, "MOM_INPUT_NML");
  EXPECT_EQ(output_directory, "./");
  std::string input_filename;
  nml.get("input_filename", input_filename, "MOM_INPUT_NML");
  EXPECT_EQ(input_filename, "n");
  std::string restart_input_dir;
  nml.get("restart_input_dir", restart_input_dir, "MOM_INPUT_NML");
  EXPECT_EQ(restart_input_dir, "INPUT/");
  std::string restart_output_dir;
  nml.get("restart_output_dir", restart_output_dir, "MOM_INPUT_NML");
  EXPECT_EQ(restart_output_dir, "RESTART/");

  EXPECT_TRUE(nml.has_param("parameter_filename", "MOM_INPUT_NML"));
  const auto &pf = nml.get_variant("parameter_filename", "MOM_INPUT_NML");
  ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(pf));
  auto files = std::get<std::vector<std::string>>(pf);
  ASSERT_EQ(files.size(), 2);
  EXPECT_EQ(files[0], "MOM_input");
  EXPECT_EQ(files[1], "MOM_override");
}

TEST(MOMNmlParserTest, ParseMomInputNmlStringArray) {
  auto path = get_test_data_dir() / "mom_input_nml_string_array.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  validate_mom_input_nml_5params(nml);
}

TEST(MOMNmlParserTest, ParseScalarIntAndString) {
  auto path = get_test_data_dir() / "scalar_int_and_string.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  int c;
  nml.get("c", c, "FOO");
  EXPECT_EQ(c, 4);
  std::string d;
  nml.get("d", d, "FOO");
  EXPECT_EQ(d, "a string");
}

TEST(MOMNmlParserTest, ParseFilepathStringEmptyNml) {
  auto path = get_test_data_dir() / "filepath_string_empty_nml.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  int a;
  nml.get("a", a, "FOO_NML");
  EXPECT_EQ(a, 4);
  std::string b;
  nml.get("b", b, "FOO_NML");
  EXPECT_EQ(b, "/foo/bar/baz.nc");
}

TEST(MOMNmlParserTest, ParseInlineClose) {
  auto path = get_test_data_dir() / "inline_close.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  int a_val;
  nml.get("a", a_val, "FOO");
  EXPECT_EQ(a_val, 4);
  std::string b_val;
  nml.get("b", b_val, "FOO");
  EXPECT_EQ(b_val, "a string");
}

TEST(MOMNmlParserTest, ParseSeparateClose) {
  auto path = get_test_data_dir() / "separate_close.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  int a_val;
  nml.get("a", a_val, "FOO");
  EXPECT_EQ(a_val, 4);
  std::string b_val;
  nml.get("b", b_val, "FOO");
  EXPECT_EQ(b_val, "a string");
}

TEST(MOMNmlParserTest, ParseMultiNmlArraysComments) {
  auto path = get_test_data_dir() / "multi_nml_arrays_comments.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 4);
  EXPECT_EQ(nml.get_num_parameters(), 4);

  // &bar  b = 4
  int b_bar;
  nml.get("b", b_bar, "BAR");
  EXPECT_EQ(b_bar, 4);

  // &bart  b = 2,3,4
  {
    std::vector<int> arr;
    nml.get("b", arr, "BART");
    EXPECT_EQ(arr, std::vector<int>({2, 3, 4}));
  }

  // &var  a = 3, (next line) 9
  {
    std::vector<int> arr;
    nml.get("a", arr, "VAR");
    EXPECT_EQ(arr, std::vector<int>({3, 9}));
  }

  // &varn  a = 3, (next line) 9  (with inline comments)
  {
    std::vector<int> arr;
    nml.get("a", arr, "VARN");
    EXPECT_EQ(arr, std::vector<int>({3, 9}));
  }
}

TEST(MOMNmlParserTest, ParseTrailingCommaWithComments) {
  auto path = get_test_data_dir() / "trailing_comma_with_comments.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  // "a = 4," after comment strip; trailing comma with single value → scalar 4
  int a_val;
  nml.get("a", a_val, "FOO");
  EXPECT_EQ(a_val, 4);
  std::string b_val;
  nml.get("b", b_val, "FOO");
  EXPECT_EQ(b_val, "a string");
}

TEST(MOMNmlParserTest, ParseMomInputNmlInlineComments) {
  auto path = get_test_data_dir() / "mom_input_nml_inline_comments.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  validate_mom_input_nml_5params(nml);
}

TEST(MOMNmlParserTest, ParseArrayTrailingComma) {
  auto path = get_test_data_dir() / "array_trailing_comma.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  std::vector<int> arr;
  nml.get("bar", arr, "BAR_NML");
  EXPECT_EQ(arr, std::vector<int>({2, 4}));
}

TEST(MOMNmlParserTest, ParseInlineCloseNml) {
  auto path = get_test_data_dir() / "invalid_inline_close_nml.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  // &var / on a single line is a valid empty namelist in Fortran.
  NamelistParams nml(path.string());
  // &var / is an empty namelist, so only &foo is stored
  EXPECT_EQ(nml.get_namelists().size(), 1);

  // &foo namelist has 3 parameters
  int b_foo;
  nml.get("b", b_foo, "FOO");
  EXPECT_EQ(b_foo, 3);
  std::vector<int> a_arr;
  nml.get("a", a_arr, "FOO");
  EXPECT_EQ(a_arr, std::vector<int>({4, 4}));
  std::vector<int> c_arr;
  nml.get("c", c_arr, "FOO");
  EXPECT_EQ(c_arr, std::vector<int>({1, 3}));
}

TEST(MOMNmlParserTest, ParseArrayInlineClose) {
  auto path = get_test_data_dir() / "array_inline_close.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  std::vector<int> arr;
  nml.get("a", arr, "VAR_NML");
  EXPECT_EQ(arr, std::vector<int>({3, 4}));
}

TEST(MOMNmlParserTest, ParseScalarTrailingComma) {
  auto path = get_test_data_dir() / "scalar_trailing_comma.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  // "a = 3," with trailing comma; single value after stripping empty → scalar 3
  int a_val;
  nml.get("a", a_val, "VAR_NML");
  EXPECT_EQ(a_val, 3);
}

TEST(MOMNmlParserTest, ParseArrayMultiline) {
  auto path = get_test_data_dir() / "array_multiline.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  std::vector<int> arr;
  nml.get("a", arr, "VAR_NML");
  EXPECT_EQ(arr, std::vector<int>({3, 4}));
}

TEST(MOMNmlParserTest, ParseOneline) {
  auto path = get_test_data_dir() / "oneline.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 2);

  int a_val;
  nml.get("a", a_val, "FOO");
  EXPECT_EQ(a_val, 3);
  std::string b_val;
  nml.get("b", b_val, "FOO");
  EXPECT_EQ(b_val, "bar");
}

TEST(MOMNmlParserTest, ParseArrayTwoElement) {
  auto path = get_test_data_dir() / "array_two_element.nml";
  ASSERT_TRUE(std::filesystem::exists(path)) << path;

  NamelistParams nml(path.string());
  EXPECT_EQ(nml.get_namelists().size(), 1);
  EXPECT_EQ(nml.get_num_parameters(), 1);

  std::vector<int> arr;
  nml.get("b", arr, "BAR");
  EXPECT_EQ(arr, std::vector<int>({4, 3}));
}
