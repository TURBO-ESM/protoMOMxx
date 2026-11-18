// Unit tests for the MOM_domains module (src/framework/MOM_domains.cpp),
// which implicitly depends on the infra (AMReX) layer being initialized.
// (Hence the main() below, which instantiates a MOM::Infra instance.)

#include <filesystem>
#include <gtest/gtest.h>

#include <AMReX_MultiFab.H>

#include "MOM_domains.hpp"
#include "MOM_infra.hpp"
#include "MOM_logger.hpp"

// Helper function to get the absolute path to the test data directory
std::filesystem::path get_test_data_dir() {
  return std::filesystem::path(__FILE__).parent_path() / "MOM_param_files";
}

// Check that make_domain constructs a Domain from the parameter file,
// with defaults applied for the parameters the file omits.
TEST(MOMDomainsTest, MakeDomainFromParamFile) {
  auto test_file_path = get_test_data_dir() / "MOM_input_test";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  MOM::RuntimeParams params(test_file_path.string());
  const MOM::Domain domain = MOM::make_domain(params);

  EXPECT_EQ(domain.ni_global(), 44);
  EXPECT_EQ(domain.nj_global(), 40);
  EXPECT_EQ(domain.ni_halo(), 4);      // default
  EXPECT_EQ(domain.nj_halo(), 4);      // default
  EXPECT_FALSE(domain.reentrant_x());  // REENTRANT_X = False in the file
  EXPECT_FALSE(domain.reentrant_y());  // default
  EXPECT_FALSE(domain.tripolar_n());   // default
}

// Nonsensical configuration values are rejected through logger::fatal
// (NIGLOBAL = 0 standing in for the make_domain value checks).
TEST(MOMDomainsTest, NonPositiveExtentIsFatal) {
  auto test_file_path = get_test_data_dir() / "MOM_input_bad_extent";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  MOM::RuntimeParams params(test_file_path.string());
  EXPECT_THROW(MOM::make_domain(params), MOM::logger::FatalError);
}

// (Missing mandatory parameters -- NIGLOBAL, NJGLOBAL -- throwing is the
// parameter table's contract, tested in test_MOM_file_parser; it is
// deliberately not re-verified here.)

// The h/u/v/q field factories map onto the right staggerings.
TEST(MOMDomainsTest, PointVocabularyFactories) {
  const MOM::Domain domain({.ni_global = 8, .nj_global = 6});

  EXPECT_EQ(domain.make_h_field({.nk = 1}).ixType(), amrex::IndexType(TIM::nodality(MOM::Stagger::Cell)));
  EXPECT_EQ(domain.make_u_field({.nk = 1}).ixType(), amrex::IndexType(TIM::nodality(MOM::Stagger::XFace)));
  EXPECT_EQ(domain.make_v_field({.nk = 1}).ixType(), amrex::IndexType(TIM::nodality(MOM::Stagger::YFace)));
  EXPECT_EQ(domain.make_q_field({.nk = 1}).ixType(), amrex::IndexType(TIM::nodality(MOM::Stagger::Node)));
}

// Through mom_view, u(i) is storage point i+1, the east face of cell i, as in
// MOM6. v points shift in j, q points in both, and h points not at all.
TEST(MOMDomainsTest, MomViewLabelsFacesLikeMOM6) {
  const MOM::Domain domain({.ni_global = 8, .nj_global = 6, .ni_halo = 2, .nj_halo = 2,
                            .n_boxes = 2});
  const auto check = [](amrex::MultiFab mf, const int si, const int sj) {
    for (amrex::MFIter mfi(mf); mfi.isValid(); ++mfi) {
      const auto storage = mf.array(mfi);
      const auto view = MOM::mom_array(mf, mfi);
      EXPECT_EQ(amrex::lbound(view).x, amrex::lbound(storage).x - si);
      EXPECT_EQ(amrex::lbound(view).y, amrex::lbound(storage).y - sj);
      EXPECT_EQ(amrex::ubound(view).x, amrex::ubound(storage).x - si);
      EXPECT_EQ(amrex::ubound(view).y, amrex::ubound(storage).y - sj);
      const amrex::Box b = mfi.validbox();
      for (int j = b.smallEnd(1); j <= b.bigEnd(1); ++j) {
        for (int i = b.smallEnd(0); i <= b.bigEnd(0); ++i) {
          EXPECT_EQ(view.ptr(i - si, j - sj, 0), storage.ptr(i, j, 0));
        }
      }
      // mom_box shifts the box the same way and returns a cell-centered box.
      const amrex::Box vb = MOM::mom_box(b);
      EXPECT_TRUE(vb.ixType().cellCentered());
      EXPECT_EQ(view.ptr(vb.smallEnd(0), vb.smallEnd(1), 0),
                storage.ptr(b.smallEnd(0), b.smallEnd(1), 0));
      EXPECT_EQ(view.ptr(vb.bigEnd(0), vb.bigEnd(1), 0),
                storage.ptr(b.bigEnd(0), b.bigEnd(1), 0));
    }
  };
  check(domain.make_h_field({.nk = 1}), 0, 0);
  check(domain.make_u_field({.nk = 1}), 1, 0);
  check(domain.make_v_field({.nk = 1}), 0, 1);
  check(domain.make_q_field({.nk = 1}), 1, 1);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
