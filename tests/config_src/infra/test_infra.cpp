// Integration test for the infrastructure layer (MOM::Infra).
//
// Infra is a thin wrapper over TIM::Runtime: its construction initializes MPI and
// hands the communicator to AMReX, which runs as a guest rather than as the
// process owner. These tests verify that, with an Infra alive, direct MPI
// calls on Infra's communicator, AMReX data structures/collectives, and TIM's
// own services all work side by side in one process.

#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include <AMReX_BoxArray.H>
#include <AMReX_MultiFab.H>

#include "MOM_infra.h"
#include "tim_coms_infra.hpp"

namespace {

// The one Infra of this test binary, created in main() and shared by all
// tests (exactly one Infra may exist per process).
const MOM::Infra* g_infra = nullptr;

// Infra exposes the communicator it owns, and direct MPI calls on it work
// alongside AMReX: a collective sum of one contribution per rank equals the
// comm size.
TEST(InfraComm, OwnedCommunicatorIsUsable) {
  ASSERT_NE(g_infra, nullptr);
  int size = -1;
  ASSERT_EQ(MPI_Comm_size(g_infra->comm(), &size), MPI_SUCCESS);
  EXPECT_GE(size, 1);
  int one = 1;
  int sum = 0;
  ASSERT_EQ(MPI_Allreduce(&one, &sum, 1, MPI_INT, MPI_SUM, g_infra->comm()),
            MPI_SUCCESS);
  EXPECT_EQ(sum, size);
}

// AMReX and TIM work side by side as guests of the runtime: fill a MultiFab,
// check AMReX's own collective reduction (MultiFab::sum) against the analytic
// answer, and run TIM::checksum over the box's data -- TIM's compiled code
// computing inside this process.
//
// The unit-test harness runs on a single rank (bare MPI_Init, no launcher), so
// this rank owns the whole single-box domain and calls the collectives once.
TEST(InfraTIMCoexistence, MultiFabReductionAndChecksum) {
  const int n = 4;
  const amrex::Box domain(amrex::IntVect(0), amrex::IntVect(n - 1));
  const amrex::BoxArray ba(domain);
  const amrex::DistributionMapping dm(ba);
  amrex::MultiFab mf(ba, dm, 1, 0);
  mf.setVal(1.5);

  // AMReX collective reduction over the field of constants.
  EXPECT_DOUBLE_EQ(mf.sum(), 1.5 * n * n * n);

  // On one rank there is exactly one local FAB, holding the whole domain.
  ASSERT_EQ(mf.local_size(), 1);
  double* data = nullptr;
  std::size_t field_size = 0;
  for (amrex::MFIter mfi(mf); mfi.isValid(); ++mfi) {
    amrex::FArrayBox& fab = mf[mfi];
    data = fab.dataPtr();
    field_size = static_cast<std::size_t>(fab.box().numPts());
  }
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(field_size, static_cast<std::size_t>(n) * n * n);

  const int64_t sum_a = TIM::checksum(data, field_size, /*mask_val=*/nullptr);
  const int64_t sum_b = TIM::checksum(data, field_size, /*mask_val=*/nullptr);
  EXPECT_EQ(sum_a, sum_b) << "checksum must be deterministic";
}

} // namespace

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer (MPI + AMReX, via MOM::Infra over TIM::Runtime), and
/// run all tests.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  g_infra = &infra;
  return RUN_ALL_TESTS();
}
