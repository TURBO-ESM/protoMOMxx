// Unit test for MOM_file_parser.cpp using GoogleTest

#include <gtest/gtest.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <unistd.h>
#include <variant>
#include <vector>

#include "MOM_file_parser.h"

// Helper function to get the absolute path to the test data directory
std::filesystem::path get_test_data_dir() {
  return std::filesystem::path(__FILE__).parent_path() / "MOM_param_files";
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


TEST(MOMFileParserTest, ParseMOMInputSimple) {
  auto test_file_path = get_test_data_dir() / "MOM_input_simple";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) 
    << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());
  
  EXPECT_TRUE(rp.has_param("REENTRANT_X"));
  EXPECT_TRUE(rp.has_param("DT_THERM"));
  EXPECT_TRUE(rp.has_param("VERT_COORDINATE"));

  EXPECT_TRUE(check_value(rp.get_as<bool>("REENTRANT_X"), false));
  EXPECT_EQ(rp.get_as<std::int64_t>("DT_THERM"), 3600);
  EXPECT_TRUE(check_value(rp.get_as<std::string>("VERT_COORDINATE"), std::string("ALE")));

  // Check that all 54 of the parameters in the file are present
  EXPECT_EQ(rp.get_num_parameters(), 5);
}


TEST(MOMFileParserTest, ParseMOMInputDirective) {
  auto test_file_path = get_test_data_dir() / "MOM_input_directive";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) 
    << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());
  
  // Check that all of the parameters in the file are present
  EXPECT_EQ(rp.get_num_parameters(), 8);

  EXPECT_TRUE(rp.get_as<bool>("Foo"));
  EXPECT_EQ(rp.get_as<std::int64_t>("Bar"), 43); // Check that #override Bar = 43 takes effect
  EXPECT_EQ(rp.get_as<double>("PI"), 3.14159);
  EXPECT_EQ(rp.get_as<std::string>("Baz"), "Hello, World!");
  EXPECT_EQ(rp.get_as<double>("ALPHA"), 0.1); // Commented out override should not take effect
  EXPECT_FALSE(rp.get_as<bool>("DELTA")); // Undefine should set to false
  EXPECT_FALSE(rp.get_as<bool>("GAMMA")); // Define-Undefine should set to false
}


TEST(MOMFileParserTest, ParseMOMInputLarge1) {
  auto test_file_path = get_test_data_dir() / "MOM_input_large1";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) 
    << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());
  
  // Check that we can parse the large file without errors
  EXPECT_FALSE(rp.get_modules().empty());

  // Check several values
  EXPECT_TRUE(check_value(rp.get_as<bool>("TRIPOLAR_N"), true));
  EXPECT_EQ(rp.get_as<std::int64_t>("NIGLOBAL"), 540);
  EXPECT_EQ(rp.get_as<std::int64_t>("NJGLOBAL"), 480);
  EXPECT_EQ(rp.get_as<double>("MLD_DECAYING_TFILTER", "MLE"), 2592000.0);

  // Expect error when trying to get a parameter with the wrong type
  EXPECT_THROW(rp.get_as<double>("NIGLOBAL"), std::runtime_error);

  // Check that all 54 of the parameters in the file are present
  EXPECT_EQ(rp.get_num_parameters(), 243);
}


TEST(MOMFileParserTest, ParseMOMInputModules) {
  auto test_file_path = get_test_data_dir() / "MOM_input_modules";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) 
    << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());
  
  // Check that we have the expected modules
  auto modules = rp.get_modules();

  // First, check the global scope parameters:
  EXPECT_EQ(rp.get_as<std::string>("B"), "fed.nc");
  EXPECT_EQ(rp.get_as<std::string>("C"), "False.nc");
  EXPECT_THROW(rp.get_as<std::string>("D"), std::runtime_error); // D should be a bool
  EXPECT_EQ(rp.get_as<bool>("D"), true);
  EXPECT_EQ(rp.get_as<double>("G"), 1e-3);
  EXPECT_EQ(rp.get_as<double>("H"), 1e-3);
  EXPECT_EQ(rp.get_as<std::string>("F"), "./t.nc");

  // Now check the parameters in module blocks
  EXPECT_THROW(rp.get_as<std::int64_t>("N_SMOOTH"), std::out_of_range); // N_SMOOTH should be in module KPP, not global
  EXPECT_EQ(rp.get_as<std::int64_t>("N_SMOOTH", "KPP"), 3);
  EXPECT_EQ(rp.get_as<bool>("USE_BODNER23", "MLE"), false);
  EXPECT_EQ(rp.get_as<std::string>("BAR", "FOO"), "t.nc");

}

TEST(MOMFileParserTest, ParseMOMInputLists) {
  auto test_file_path = get_test_data_dir() / "MOM_input_lists";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) 
    << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());
  
  // Check that A, B, C are parsed as lists of ints
  EXPECT_TRUE(is_vector_of<std::int64_t>(rp.get("A")));
  EXPECT_TRUE(is_vector_of<std::int64_t>(rp.get("B")));
  EXPECT_TRUE(is_vector_of<std::int64_t>(rp.get("C")));
  EXPECT_EQ(rp.get_as<std::vector<std::int64_t>>("A"), std::vector<std::int64_t>({1, 2}));
  EXPECT_EQ(rp.get_as<std::vector<std::int64_t>>("B"), std::vector<std::int64_t>({3, 4}));
  EXPECT_EQ(rp.get_as<std::vector<std::int64_t>>("C"), std::vector<std::int64_t>({5, 6, 7, 8}));

  // Check that D is parsed as a list of doubles
  EXPECT_TRUE(is_vector_of<double>(rp.get("D")));
  EXPECT_EQ(rp.get_as<std::vector<double>>("D"), std::vector<double>({1.0, 2.0, 3.0}));

  // Check that my_str_list is parsed as a list of strings
  EXPECT_TRUE(is_vector_of<std::string>(rp.get("my_str_list")));
  EXPECT_EQ(rp.get_as<std::vector<std::string>>("my_str_list"), std::vector<std::string>({"sd.nc", "foo.nc"}));

}

TEST(MOMFileParserTest, ParseMOMInputScalars) {
  auto test_file_path = get_test_data_dir() / "MOM_input_lists";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) 
    << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());
  
  // Check that X and Y are parsed as scalar ints
  EXPECT_TRUE(std::holds_alternative<std::int64_t>(rp.get("X")));
  EXPECT_TRUE(std::holds_alternative<std::int64_t>(rp.get("Y")));
  EXPECT_EQ(rp.get_as<std::int64_t>("X"), 1);
  EXPECT_EQ(rp.get_as<std::int64_t>("Y"), 3);

  // Check that A, B, C are parsed as lists of ints
  EXPECT_TRUE(is_vector_of<std::int64_t>(rp.get("A")));
  EXPECT_TRUE(is_vector_of<std::int64_t>(rp.get("B")));
  EXPECT_TRUE(is_vector_of<std::int64_t>(rp.get("C")));
  EXPECT_EQ(rp.get_as<std::vector<std::int64_t>>("A"), std::vector<std::int64_t>({1, 2}));
  EXPECT_EQ(rp.get_as<std::vector<std::int64_t>>("B"), std::vector<std::int64_t>({3, 4}));
  EXPECT_EQ(rp.get_as<std::vector<std::int64_t>>("C"), std::vector<std::int64_t>({5, 6, 7, 8}));

  // Check that D is parsed as a list of doubles
  EXPECT_TRUE(is_vector_of<double>(rp.get("D")));
  EXPECT_EQ(rp.get_as<std::vector<double>>("D"), std::vector<double>({1.0, 2.0, 3.0}));

  // Check that my_str_list is parsed as a list of strings
  EXPECT_TRUE(is_vector_of<std::string>(rp.get("my_str_list")));
  EXPECT_EQ(rp.get_as<std::vector<std::string>>("my_str_list"), std::vector<std::string>({"sd.nc", "foo.nc"})); 

  // Check that not_a_list and also_not_a_list are parsed as scalar strings, not lists
  EXPECT_TRUE(std::holds_alternative<std::string>(rp.get("not_a_list")));
  EXPECT_TRUE(std::holds_alternative<std::string>(rp.get("also_not_a_list")));
  EXPECT_EQ(rp.get_as<std::string>("not_a_list"), "sd.nc, foo.nc");
  EXPECT_EQ(rp.get_as<std::string>("also_not_a_list"), "\"sd.nc\" , \"foo.nc\" , \"bar.nc\"");

  // Check that foo is parsed as a scalar string, not a list
  EXPECT_TRUE(std::holds_alternative<std::string>(rp.get("foo")));
  EXPECT_EQ(rp.get_as<std::string>("foo"), "\"FILE:fed.nc,dz\"");
}


TEST(MOMFileParserTest, InvalidFile0) {
  auto test_file_path = get_test_data_dir() / "non_existent_file";
  EXPECT_THROW(RuntimeParams rp(test_file_path.string()), std::runtime_error);
}

TEST(MOMFileParserTest, InvalidFile1) {
  auto test_file_path = get_test_data_dir() / "MOM_input_invalid1";
  // Parsing this file should throw a "missing value" error:
  try {
    RuntimeParams rp(test_file_path.string());
    FAIL() << "Expected std::runtime_error due to missing value";
  } catch (const std::runtime_error& e) {
    EXPECT_TRUE(std::string(e.what()).find("missing value") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::runtime_error due to missing value, but caught different exception";
  }
}

TEST(MOMFileParserTest, InvalidFile2) {
  auto test_file_path = get_test_data_dir() / "MOM_input_invalid2";
  // Parsing this file should throw an "invalid key name" error:
  try {
    RuntimeParams rp(test_file_path.string());
    FAIL() << "Expected std::runtime_error due to invalid key name";
  } catch (const std::runtime_error& e) {
    EXPECT_TRUE(std::string(e.what()).find("already-open module") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::runtime_error due to invalid key name, but caught different exception";
  }
}

TEST(MOMFileParserTest, InvalidFile3) {
  auto test_file_path = get_test_data_dir() / "MOM_input_invalid3";
  // Parsing this file should throw an "unterminated module" error:
  try {
    RuntimeParams rp(test_file_path.string());
    FAIL() << "Expected std::runtime_error due to unterminated module";
  } catch (const std::runtime_error& e) {
    EXPECT_TRUE(std::string(e.what()).find("unterminated module") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::runtime_error due to unterminated module, but caught different exception";
  }
}

TEST(MOMFileParserTest, InvalidFile4) {
  auto test_file_path = get_test_data_dir() / "MOM_input_invalid4";
  // Parsing this file should throw an "unterminated block comment" error:
  try {
    RuntimeParams rp(test_file_path.string());
    FAIL() << "Expected std::runtime_error due to unterminated block comment";
  } catch (const std::runtime_error& e) {
    EXPECT_TRUE(std::string(e.what()).find("unterminated block comment") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::runtime_error due to unterminated block comment, but caught different exception";
  }
}

TEST(MOMFileParserTest, InvalidFile5) {
  auto test_file_path = get_test_data_dir() / "MOM_input_invalid5";
  // Parsing this file should throw an "invalid key name" error:
  try {
    RuntimeParams rp(test_file_path.string());
    FAIL() << "Expected std::runtime_error due to invalid key name";
  } catch (const std::runtime_error& e) {
    EXPECT_TRUE(std::string(e.what()).find("invalid key name") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::runtime_error due to invalid key name, but caught different exception";
  }
}

TEST(MOMFileParserTest, InvalidFile6) {
  auto test_file_path = get_test_data_dir() / "MOM_input_invalid6";
  // Parsing this file should throw an "only one '%' allowed" error:
  try {
    RuntimeParams rp(test_file_path.string());
    FAIL() << "Expected std::runtime_error due to invalid module%key syntax";
  } catch (const std::runtime_error& e) {
    EXPECT_TRUE(std::string(e.what()).find("only one '%' allowed") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::runtime_error due to invalid module%key syntax, but caught different exception";
  }
}

TEST(MOMFileParserTest, OverrideModules) {
  std::vector<std::string> paths = {
    (get_test_data_dir() / "MOM_input_modules").string(),
    (get_test_data_dir() / "MOM_override_modules").string()
  };

  RuntimeParams rp(paths);
  // Check that the override file successfully overrides some values from the first file
  EXPECT_EQ(rp.get_as<double>("H"), 1e-5); // H should be overridden to 1e-5
  EXPECT_EQ(rp.get_as<std::int64_t>("N_SMOOTH", "KPP"), 5); // N_SMOOTH in KPP should be overridden to 5

  // Newly defined variable in the override file should be present
  EXPECT_TRUE(rp.has_param("NEW_VAR"));
  EXPECT_EQ(rp.get_as<std::string>("NEW_VAR"), "xyz");

  // Those that are not overridden should retain their original values
  EXPECT_EQ(rp.get_as<std::string>("B"), "fed.nc");
  EXPECT_EQ(rp.get_as<std::string>("C"), "False.nc");
  EXPECT_EQ(rp.get_as<bool>("D"), true);
  EXPECT_EQ(rp.get_as<double>("G"), 1e-3);
  EXPECT_EQ(rp.get_as<std::string>("F"), "./t.nc");
  EXPECT_EQ(rp.get_as<bool>("USE_BODNER23", "MLE"), false);
  EXPECT_EQ(rp.get_as<std::string>("BAR", "FOO"), "t.nc");

}

TEST(MOMNmlParserTest, InvalidOverride) {
  std::vector<std::string> paths = {
    (get_test_data_dir() / "MOM_input_modules").string(),
    (get_test_data_dir() / "MOM_override_invalid").string()
  };
  
  // Parsing should throw an error due to a missing override directive
  try {
    RuntimeParams rp(paths);
    FAIL() << "Expected std::runtime_error due to invalid override file";
  } catch (const std::runtime_error& e) {
    EXPECT_TRUE(std::string(e.what()).find("duplicate assignment") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::runtime_error due to invalid override file, but caught different exception";
  }
}
