// Unit tests for MOM_document.cpp using GoogleTest

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

#include "MOM_document.h"
#include "MOM_logger.h"

using namespace MOM;

namespace fs = std::filesystem;

class DocFileWriterTest : public ::testing::Test {
protected:
  fs::path tmp_dir;

  void SetUp() override {
    tmp_dir =
        fs::temp_directory_path() /
        ("mom_doc_test_" +
         std::to_string(
             ::testing::UnitTest::GetInstance()->current_test_info()->line()));
    fs::create_directories(tmp_dir);
  }

  void TearDown() override { fs::remove_all(tmp_dir); }

  std::string base_path() const {
    return (tmp_dir / "MOM_parameter_doc").string();
  }

  static std::string read_file(const fs::path &p) {
    std::ifstream ifs(p);
    return {std::istreambuf_iterator<char>(ifs),
            std::istreambuf_iterator<char>()};
  }
};

// real_string: notation selection and round-trip fidelity
TEST(RealString, FormattingAndRoundTrip) {
  EXPECT_EQ(DocFileWriter::real_string(0.0), "0.0");
  EXPECT_EQ(DocFileWriter::real_string(3.14), "3.14");

  // Scientific notation for values outside [1e-3, 1e4)
  EXPECT_NE(DocFileWriter::real_string(1.0e-4).find('e'), std::string::npos);
  EXPECT_NE(DocFileWriter::real_string(1.0e5).find('e'), std::string::npos);

  // Round-trip fidelity across fixed and scientific ranges
  for (double v : {0.1, 0.123456789, 1.0e-10, 1.0e15, -42.5, 1.0 / 3.0}) {
    std::string s = DocFileWriter::real_string(v);
    EXPECT_DOUBLE_EQ(std::stod(s), v)
        << "Round-trip failed for " << v << " -> \"" << s << "\"";
  }
}

// End-to-end file routing: all param types, .short filtering, layout/debugging
// routing
TEST_F(DocFileWriterTest, FileRoutingAndParamTypes) {
  {
    DocFileWriter doc(base_path());

    // Regular params: default match (suppressed from .short) vs non-default
    // (included in .short)
    doc.doc_param<int>("NX", "x cells", "count", 100, std::optional<int>(100));
    doc.doc_param<int>("NY", "y cells", "count", 200, std::optional<int>(100));

    // Other scalar types
    doc.doc_param<double>("DT", "Time step", "s", 3600.0);
    doc.doc_param<bool>("REENTRANT_X", "Reentrant", "", true);
    doc.doc_param<std::string>("COORD", "Coord type", "", std::string("ALE"));

    // Vector param
    doc.doc_param<int>("LAYERS", "Layer counts", "",
                       std::vector<int>{10, 20, 30});

    // Layout param — should go to .layout only, not .all
    DocParamOptions layout_opts;
    layout_opts.layout_param = true;
    doc.doc_param<int>("NIGLOBAL", "Global i size", "", 360, std::nullopt,
                       layout_opts);

    // Debugging param — should go to .debugging only, not .all
    DocParamOptions debug_opts;
    debug_opts.debugging_param = true;
    doc.doc_param<int>("DEBUG_LVL", "Debug level", "", 3, std::nullopt,
                       debug_opts);

    doc.close();
  }

  std::string all = read_file(base_path() + ".all");
  std::string sht = read_file(base_path() + ".short");
  std::string lay = read_file(base_path() + ".layout");
  std::string dbg = read_file(base_path() + ".debugging");

  // .all has regular params, not layout/debugging
  EXPECT_NE(all.find("NX = 100"), std::string::npos);
  EXPECT_NE(all.find("DT = 3600.0"), std::string::npos);
  EXPECT_NE(all.find("REENTRANT_X = True"), std::string::npos);
  EXPECT_NE(all.find("COORD = \"ALE\""), std::string::npos);
  EXPECT_NE(all.find("LAYERS = 10, 20, 30"), std::string::npos);
  EXPECT_EQ(all.find("NIGLOBAL"), std::string::npos);
  EXPECT_EQ(all.find("DEBUG_LVL"), std::string::npos);

  // .short: default-matching NX suppressed, non-default NY included
  EXPECT_EQ(sht.find("NX = 100"), std::string::npos);
  EXPECT_NE(sht.find("NY = 200"), std::string::npos);

  // Layout and debugging files
  EXPECT_NE(lay.find("NIGLOBAL = 360"), std::string::npos);
  EXPECT_NE(dbg.find("DEBUG_LVL = 3"), std::string::npos);
}

// Module/block state machine: open/close, transitions, dedup, blocks
TEST_F(DocFileWriterTest, ModulesBlocksAndDedup) {
  std::ostringstream captured;
  logger::set_err_stream(captured);

  {
    DocFileWriter doc(base_path(), true, false, false, false);

    // Explicit module
    doc.doc_module("MOM_dynamics", "Dynamic core");
    doc.doc_param<int>("DT", "timestep", "s", 100);

    // Switch to new module (closes previous), then duplicate module warning
    doc.doc_module("MOM_thermo", "Thermodynamics");
    doc.doc_module("MOM_thermo",
                   "Thermodynamics"); // triggers "already open" warning

    DocParamOptions thermo_opts;
    doc.doc_param<int>("T_REF", "ref temp", "K", 300, std::nullopt,
                       thermo_opts);

    // Block open/close
    doc.open_block("KPP");
    EXPECT_EQ(doc.block_prefix(), "KPP%");
    doc.doc_param<double>("KPP_VT2", "Threshold", "", 0.01, std::nullopt,
                          thermo_opts);
    doc.close_block();
    EXPECT_EQ(doc.block_prefix(), "");

    // Duplicate param detection: same param documented twice, inconsistent
    // value warns
    doc.doc_param<int>("DUPE", "dup param", "", 1, std::nullopt, thermo_opts);
    doc.doc_param<int>("DUPE", "dup param", "", 2, std::nullopt, thermo_opts);
    EXPECT_EQ(doc.num_documented(),
              4u); // DT, T_REF, KPP_VT2, DUPE (dedup prevents second entry)

    doc.close();
  }

  logger::set_err_stream(std::cerr);

  std::string content = read_file(base_path() + ".all");

  // Module markers (comment-style headers, no closing %Module markers)
  EXPECT_NE(content.find("=== module MOM_dynamics ==="), std::string::npos);
  EXPECT_EQ(content.find("%MOM_dynamics"), std::string::npos);
  EXPECT_NE(content.find("=== module MOM_thermo ==="), std::string::npos);

  // Block markers
  EXPECT_NE(content.find("KPP%"), std::string::npos);
  EXPECT_NE(content.find("%KPP"), std::string::npos);

  // Warnings captured via logger
  EXPECT_NE(captured.str().find("already open"), std::string::npos);
  EXPECT_NE(captured.str().find("Inconsistent"), std::string::npos);
}
