// Unit test for MOM_nml_parser.cpp using GoogleTest

#include <gtest/gtest.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <variant>
#include <vector>

#include "MOM_nml_parser.h"

// Helper function to get the absolute path to the test data directory
std::filesystem::path get_test_data_dir() {
  return std::filesystem::path(__FILE__).parent_path() / "nml_files";
}

// Helper function to check if a value is of a specific type and has the expected value
template<typename T>
bool check_value(const ParamValue& val, const T& expected) {
  if (std::holds_alternative<T>(val)) {
    return std::get<T>(val) == expected;
  }
  return false;
}

// Helper function for double comparison with tolerance
bool check_double_value(const ParamValue& val, double expected, double tolerance = 1e-9) {
  if (std::holds_alternative<double>(val)) {
    return std::abs(std::get<double>(val) - expected) < tolerance;
  }
  return false;
}

// Helper function to check if a value is a vector of a specific type
template<typename T>
bool is_vector_of(const ParamValue& val) {
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

  EXPECT_EQ(nml.get_as<std::int64_t>("DT", "OCEAN"), 3600);
  EXPECT_FALSE(nml.get_as<bool>("REENTRANT_X", "OCEAN"));
  EXPECT_TRUE(nml.get_as<bool>("REENTRANT_Y", "OCEAN"));
  EXPECT_EQ(nml.get_as<std::string>("COORD_TYPE", "OCEAN"), "ALE");
  EXPECT_TRUE(check_double_value(nml.get("GRAVITY", "OCEAN"), 9.81));

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
  EXPECT_EQ(nml.get_as<std::int64_t>("NIGLOBAL", "OCEAN_PARAMS"), 540);
  EXPECT_EQ(nml.get_as<std::int64_t>("NJGLOBAL", "OCEAN_PARAMS"), 480);
  EXPECT_EQ(nml.get_as<std::int64_t>("NK", "OCEAN_PARAMS"), 75);
  EXPECT_TRUE(nml.get_as<bool>("TRIPOLAR_N", "OCEAN_PARAMS"));

  // Check MLE_PARAMS namelist
  EXPECT_FALSE(nml.get_as<bool>("USE_BODNER23", "MLE_PARAMS"));
  EXPECT_TRUE(check_double_value(nml.get("MLD_DECAYING_TFILTER", "MLE_PARAMS"), 2592000.0));
  EXPECT_TRUE(nml.get_as<bool>("KHTH_USE_EBT_STRUCT", "MLE_PARAMS"));

  // Check KPP_PARAMS namelist
  EXPECT_EQ(nml.get_as<std::int64_t>("N_SMOOTH", "KPP_PARAMS"), 3);
  EXPECT_TRUE(check_double_value(nml.get("RI_CRIT", "KPP_PARAMS"), 0.3));
  EXPECT_EQ(nml.get_as<std::int64_t>("ENHANCE", "KPP_PARAMS"), 1);

  // Verify parameters are in the correct namelist
  EXPECT_THROW(nml.get("NIGLOBAL", "MLE_PARAMS"), std::out_of_range);
  EXPECT_THROW(nml.get("N_SMOOTH", "OCEAN_PARAMS"), std::out_of_range);

  // Check total parameter count
  EXPECT_EQ(nml.get_num_parameters(), 10);
}


TEST(MOMNmlParserTest, ParseNamelistArrays) {
  auto test_file_path = get_test_data_dir() / "namelist_arrays.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) 
    << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());
  
  // Check scalar
  EXPECT_TRUE(std::holds_alternative<std::int64_t>(nml.get("INT_SCALAR", "ARRAY_TEST")));
  EXPECT_EQ(nml.get_as<std::int64_t>("INT_SCALAR", "ARRAY_TEST"), 42);

  // Check integer array
  EXPECT_TRUE(is_vector_of<std::int64_t>(nml.get("INT_ARRAY", "ARRAY_TEST")));
  auto int_array = nml.get_as<std::vector<std::int64_t>>("INT_ARRAY", "ARRAY_TEST");
  EXPECT_EQ(int_array, std::vector<std::int64_t>({1, 2, 3, 4, 5}));

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
  EXPECT_TRUE(is_vector_of<std::int64_t>(nml.get("TRAILING_COMMA", "ARRAY_TEST")));
  auto trailing_array = nml.get_as<std::vector<std::int64_t>>("TRAILING_COMMA", "ARRAY_TEST");
  EXPECT_EQ(trailing_array, std::vector<std::int64_t>({10, 20, 30}));
}


TEST(MOMNmlParserTest, ParseNamelistComments) {
  auto test_file_path = get_test_data_dir() / "namelist_comments.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) 
    << "Test file " << test_file_path << " does not exist";

  NamelistParams nml(test_file_path.string());
  
  // Verify all variables are parsed correctly despite comments
  EXPECT_EQ(nml.get_as<std::int64_t>("VAR1", "TEST_COMMENTS"), 100);
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
  
  // Fortran namelists are case-insensitive, but our parser converts to uppercase
  // Test that we can access using uppercase (as stored)
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
  EXPECT_THROW(nml.get_as<double>("DT", "OCEAN"), std::runtime_error);
  
  // REENTRANT_X is a bool, trying to get as int should throw
  EXPECT_THROW(nml.get_as<std::int64_t>("REENTRANT_X", "OCEAN"), std::runtime_error);
}


// Read in double_gyre_input.nml and check that the expected parameters are present and correct:
TEST(MOMNmlParserTest, ParseDoubleGyreInput) {
  auto test_file_path = get_test_data_dir() / "double_gyre_input.nml";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) 
    << "Test file " << test_file_path << " does not exist";
  NamelistParams nml(test_file_path.string());

  EXPECT_EQ(nml.get_as<std::string>("output_directory", "MOM_input_nml"), "./");
  EXPECT_EQ(nml.get_as<std::string>("input_filename", "MOM_INPUT_NML"), "n");
  EXPECT_EQ(nml.get_as<std::string>("restart_input_dir", "MOM_INPUT_NML"), "INPUT/");
  EXPECT_EQ(nml.get_as<std::string>("restart_output_dir", "MOM_INPUT_NML"), "RESTART/");
  EXPECT_TRUE(nml.has_param("parameter_filename", "MOM_INPUT_NML"));
  const auto& param_val = nml.get("parameter_filename", "MOM_INPUT_NML");
  EXPECT_TRUE(std::holds_alternative<std::vector<std::string>>(param_val));
  auto param_files = std::get<std::vector<std::string>>(param_val);
  EXPECT_EQ(param_files.size(), 2);
  EXPECT_EQ(param_files[0], "MOM_input");
  EXPECT_EQ(param_files[1], "MOM_override");

}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
