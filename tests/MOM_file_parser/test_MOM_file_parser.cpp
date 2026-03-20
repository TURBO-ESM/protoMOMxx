// Unit test for MOM_file_parser.cpp using GoogleTest

#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <variant>
#include <vector>

#include "MOM_file_parser.h"

using namespace MOM;

// Helper function to get the absolute path to the test data directory
std::filesystem::path get_test_data_dir() { return std::filesystem::path(__FILE__).parent_path() / "MOM_param_files"; }

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

TEST(MOMFileParserTest, ParseMOMInputSimple) {
  auto test_file_path = get_test_data_dir() / "MOM_input_simple";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());

  EXPECT_TRUE(rp.has_param("REENTRANT_X"));
  EXPECT_TRUE(rp.has_param("DT_THERM"));
  EXPECT_TRUE(rp.has_param("VERT_COORDINATE"));

  bool reentrant_x;
  rp.get("REENTRANT_X", reentrant_x);
  EXPECT_FALSE(reentrant_x);

  int dt_therm;
  rp.get("DT_THERM", dt_therm);
  EXPECT_EQ(dt_therm, 3600);

  std::string vert_coordinate;
  rp.get("VERT_COORDINATE", vert_coordinate);
  EXPECT_EQ(vert_coordinate, "ALE");

  // Check that all parameters in the file are present
  EXPECT_EQ(rp.get_num_parameters(), 5);
}

TEST(MOMFileParserTest, ParseMOMInputDirective) {
  auto test_file_path = get_test_data_dir() / "MOM_input_directive";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());

  // Check that all of the parameters in the file are present
  EXPECT_EQ(rp.get_num_parameters(), 10);

  bool Foo;
  rp.get("Foo", Foo);
  int Bar;
  rp.get("Bar", Bar);
  double PI;
  rp.get("PI", PI, {.units = ""});
  std::string Baz;
  rp.get("Baz", Baz);

  EXPECT_TRUE(Foo);   // Check that #define Foo takes effect
  EXPECT_EQ(Bar, 43); // Check that #override Bar = 43 takes effect
  EXPECT_EQ(PI, 3.14159);
  EXPECT_EQ(Baz, "Hello, World!");

  double Alpha;
  rp.get("ALPHA", Alpha, {.units = ""});
  EXPECT_EQ(Alpha, 0.1); // Check that #define ALPHA takes effect

  bool Delta;
  rp.get("DELTA", Delta);
  EXPECT_FALSE(Delta); // Check that #undef DELTA takes effect

  bool Gamma;
  rp.get("GAMMA", Gamma);
  EXPECT_FALSE(Gamma); // Check that #define GAMMA followed by #undef GAMMA results in GAMMA being false

  // Check #define with module%key syntax outside a module block
  bool Enabled;
  rp.get("ENABLED", Enabled, {.module = "MyMod"});
  EXPECT_TRUE(Enabled);

  double Scale;
  rp.get("SCALE", Scale, {.module = "MyMod", .units = ""});
  EXPECT_EQ(Scale, 2.5);
}

TEST(MOMFileParserTest, ParseMOMInputLarge1) {
  auto test_file_path = get_test_data_dir() / "MOM_input_large1";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());

  // Check that we can parse the large file without errors
  EXPECT_FALSE(rp.get_modules().empty());

  // Check several values
  bool tripolar_n;
  rp.get("TRIPOLAR_N", tripolar_n);
  EXPECT_TRUE(tripolar_n);

  // check that attempting to read niglobal as a double throws an error
  double niglobal_double;
  EXPECT_THROW(rp.get("NIGLOBAL", niglobal_double, {.units = ""}), std::runtime_error);

  int niglobal;
  rp.get("NIGLOBAL", niglobal);
  EXPECT_EQ(niglobal, 540);

  int njglobal;
  rp.get("NJGLOBAL", njglobal);
  EXPECT_EQ(njglobal, 480);

  EXPECT_TRUE(check_double_value(rp.get_variant("MLD_DECAYING_TFILTER", "MLE"), 2592000.0));

  double mld_decaying_tfilter;
  rp.get("MLD_DECAYING_TFILTER", mld_decaying_tfilter, {.module = "MLE", .units = ""});
  EXPECT_EQ(mld_decaying_tfilter, 2592000.0);

  // Check that all parameters in the file are present
  EXPECT_EQ(rp.get_num_parameters(), 243);
}

TEST(MOMFileParserTest, ParseMOMInputModules) {
  auto test_file_path = get_test_data_dir() / "MOM_input_modules";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());

  // Check that we have the expected modules
  auto modules = rp.get_modules();

  // Attemping to read D as a double should throw an error since D is a bool
  double D_double;
  try {
    rp.get("D", D_double, {.units = ""});
    FAIL() << "Expected std::runtime_error when trying to read D as double";
  } catch (const std::runtime_error &e) {
    EXPECT_TRUE(std::string(e.what()).find("not of the requested type") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::runtime_error when trying to read D as double, but caught different exception";
  }

  // First, check the global scope parameters:
  EXPECT_EQ(
      [&] {
        std::string B;
        rp.get("B", B);
        return B;
      }(),
      "fed.nc");
  EXPECT_EQ(
      [&] {
        std::string C;
        rp.get("C", C);
        return C;
      }(),
      "False.nc");
  EXPECT_EQ(
      [&] {
        bool D;
        rp.get("D", D);
        return D;
      }(),
      true);
  EXPECT_EQ(
      [&] {
        double G;
        rp.get("G", G, {.units = ""});
        return G;
      }(),
      1e-3);
  EXPECT_EQ(
      [&] {
        double H;
        rp.get("H", H, {.units = ""});
        return H;
      }(),
      1e-3);
  EXPECT_EQ(
      [&] {
        std::string F;
        rp.get("F", F);
        return F;
      }(),
      "./t.nc");

  // Now check the parameters in module blocks
  try {
    int N_SMOOTH;
    rp.get("N_SMOOTH", N_SMOOTH, {.fail_if_missing = true});
    FAIL() << "Expected std::out_of_range when trying to read N_SMOOTH without specifying module, but no exception was "
              "thrown";
  } catch (const std::out_of_range &e) {
    EXPECT_TRUE(std::string(e.what()).find("Key not found in module ") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::out_of_range when trying to read N_SMOOTH without specifying module, but caught different "
              "exception";
  }
  EXPECT_EQ(
      [&] {
        int N_SMOOTH;
        rp.get("N_SMOOTH", N_SMOOTH, {.module = "KPP"});
        return N_SMOOTH;
      }(),
      3);
  EXPECT_EQ(
      [&] {
        bool USE_BODNER23;
        rp.get("USE_BODNER23", USE_BODNER23, {.module = "MLE"});
        return USE_BODNER23;
      }(),
      false);
  EXPECT_EQ(
      [&] {
        std::string BAR;
        rp.get("BAR", BAR, {.module = "FOO"});
        return BAR;
      }(),
      "t.nc");
}

TEST(MOMFileParserTest, ParseMOMInputLists) {
  auto test_file_path = get_test_data_dir() / "MOM_input_lists";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());

  // Check that A, B, C are parsed as lists of ints
  EXPECT_TRUE(is_vector_of<int>(rp.get_variant("A")));
  EXPECT_TRUE(is_vector_of<int>(rp.get_variant("B")));
  EXPECT_TRUE(is_vector_of<int>(rp.get_variant("C")));
  EXPECT_EQ(
      [&] {
        std::vector<int> A;
        rp.get("A", A);
        return A;
      }(),
      std::vector<int>({1, 2}));
  EXPECT_EQ(
      [&] {
        std::vector<int> B;
        rp.get("B", B);
        return B;
      }(),
      std::vector<int>({3, 4}));
  EXPECT_EQ(
      [&] {
        std::vector<int> C;
        rp.get("C", C);
        return C;
      }(),
      std::vector<int>({5, 6, 7, 8}));

  // Check that D is parsed as a list of doubles
  EXPECT_TRUE(is_vector_of<double>(rp.get_variant("D")));
  EXPECT_EQ(
      [&] {
        std::vector<double> D;
        rp.get("D", D, {.units = ""});
        return D;
      }(),
      std::vector<double>({1.0, 2.0, 3.0}));

  // Check that my_str_list is parsed as a list of strings
  EXPECT_TRUE(is_vector_of<std::string>(rp.get_variant("my_str_list")));
  EXPECT_EQ(
      [&] {
        std::vector<std::string> my_str_list;
        rp.get("my_str_list", my_str_list);
        return my_str_list;
      }(),
      std::vector<std::string>({"sd.nc", "foo.nc"}));
}

TEST(MOMFileParserTest, ParseMOMInputScalars) {
  auto test_file_path = get_test_data_dir() / "MOM_input_lists";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  RuntimeParams rp(test_file_path.string());

  // Check that X and Y are parsed as scalar ints
  EXPECT_TRUE(std::holds_alternative<int>(rp.get_variant("X")));
  EXPECT_TRUE(std::holds_alternative<int>(rp.get_variant("Y")));
  EXPECT_EQ(
      [&] {
        int X;
        rp.get("X", X);
        return X;
      }(),
      1);
  EXPECT_EQ(
      [&] {
        int Y;
        rp.get("Y", Y);
        return Y;
      }(),
      3);

  // Check that not_a_list and also_not_a_list are parsed as scalar strings, not lists
  EXPECT_TRUE(std::holds_alternative<std::string>(rp.get_variant("not_a_list")));
  EXPECT_TRUE(std::holds_alternative<std::string>(rp.get_variant("also_not_a_list")));
  EXPECT_EQ(
      [&] {
        std::string not_a_list;
        rp.get("not_a_list", not_a_list);
        return not_a_list;
      }(),
      "sd.nc, foo.nc");
  EXPECT_EQ(
      [&] {
        std::string also_not_a_list;
        rp.get("also_not_a_list", also_not_a_list);
        return also_not_a_list;
      }(),
      "\"sd.nc\" , \"foo.nc\" , \"bar.nc\"");

  // Check that foo is parsed as a scalar string, not a list
  EXPECT_TRUE(std::holds_alternative<std::string>(rp.get_variant("foo")));
  EXPECT_EQ(
      [&] {
        std::string foo;
        rp.get("foo", foo);
        return foo;
      }(),
      "\"FILE:fed.nc,dz\"");
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
  } catch (const std::runtime_error &e) {
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
  } catch (const std::runtime_error &e) {
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
  } catch (const std::runtime_error &e) {
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
  } catch (const std::runtime_error &e) {
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
  } catch (const std::runtime_error &e) {
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
  } catch (const std::runtime_error &e) {
    EXPECT_TRUE(std::string(e.what()).find("only one '%' allowed") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::runtime_error due to invalid module%key syntax, but caught different exception";
  }
}

TEST(MOMFileParserTest, OverrideModules) {
  std::vector<std::string> paths = {(get_test_data_dir() / "MOM_input_modules").string(),
                                    (get_test_data_dir() / "MOM_override_modules").string()};

  RuntimeParams rp(paths);
  // Check that the override file successfully overrides some values from the first file
  double H;
  rp.get("H", H, {.units = ""});
  EXPECT_EQ(H, 1e-5); // H should be overridden to 1e-5

  int N_SMOOTH;
  rp.get("N_SMOOTH", N_SMOOTH, {.module = "KPP"});
  EXPECT_EQ(N_SMOOTH, 5); // N_SMOOTH in KPP should be overridden to 5

  // Newly defined variable in the override file should be present
  EXPECT_TRUE(rp.has_param("NEW_VAR"));
  std::string NEW_VAR;
  rp.get("NEW_VAR", NEW_VAR);
  EXPECT_EQ(NEW_VAR, "xyz");

  // Those that are not overridden should retain their original values
  std::string B;
  rp.get("B", B);
  EXPECT_EQ(B, "fed.nc");
  std::string C;
  rp.get("C", C);
  EXPECT_EQ(C, "False.nc");
  bool D;
  rp.get("D", D);
  EXPECT_EQ(D, true);
  double G;
  rp.get("G", G, {.units = ""});
  EXPECT_EQ(G, 1e-3);
  std::string F;
  rp.get("F", F);
  EXPECT_EQ(F, "./t.nc");
  bool USE_BODNER23;
  rp.get("USE_BODNER23", USE_BODNER23, {.module = "MLE"});
  EXPECT_EQ(USE_BODNER23, false);
  std::string BAR;
  rp.get("BAR", BAR, {.module = "FOO"});
  EXPECT_EQ(BAR, "t.nc");
}

TEST(MOMNmlParserTest, InvalidOverride) {
  std::vector<std::string> paths = {(get_test_data_dir() / "MOM_input_modules").string(),
                                    (get_test_data_dir() / "MOM_override_invalid").string()};

  // Parsing should throw an error due to a missing override directive
  try {
    RuntimeParams rp(paths);
    FAIL() << "Expected std::runtime_error due to invalid override file";
  } catch (const std::runtime_error &e) {
    EXPECT_TRUE(std::string(e.what()).find("duplicate assignment") != std::string::npos);
  } catch (...) {
    FAIL() << "Expected std::runtime_error due to invalid override file, but caught different exception";
  }
}
