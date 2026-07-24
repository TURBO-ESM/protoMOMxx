// Integration test for the infrastructure layer (MOM::Infra).
//
// Infra is a thin wrapper over TIM::Runtime: its construction initializes MPI and
// hands the communicator to AMReX, which runs as a guest rather than as the
// process owner. These tests verify that, with an Infra alive, direct MPI
// calls on Infra's communicator, AMReX data structures/collectives, and TIM's
// own services all work side by side in one process.

#include <cstddef>
#include <cstdint>
#include <vector>

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

// Check that AMReX and TIM cooperate within the runtime: fill a MultiFab,
// check AMReX's own collective reduction (MultiFab::sum) against the analytic
// answer, and run TIM::checksum over the local data.
//
// Both reductions are collective and rank-agnostic: the domain is chopped
// into boxes that a DistributionMapping spreads across the ranks, so under a
// launcher multiple ranks contribute data (a rank may own several boxes, or
// none). The unit-test harness itself runs on a single rank (bare MPI_Init,
// no launcher), which then owns every box.
TEST(InfraTIMCoexistence, MultiFabReductionAndChecksum) {
  const int n = 4;
  const amrex::Box domain(amrex::IntVect(0), amrex::IntVect(n - 1));
  amrex::BoxArray ba(domain);
  ba.maxSize(n / 2);  // 2x2x2 boxes: 8 boxes to distribute across ranks
  const amrex::DistributionMapping dm(ba);
  amrex::MultiFab mf(ba, dm, 1, 0);
  mf.setVal(1.5);

  // AMReX collective reduction over the field of constants.
  EXPECT_DOUBLE_EQ(mf.sum(), 1.5 * n * n * n);

  // Gather this rank's FABs into one contiguous buffer: TIM::checksum takes
  // one buffer per rank, and every rank must make the same single collective
  // call however many boxes it owns.
  std::vector<double> local;
  for (amrex::MFIter mfi(mf); mfi.isValid(); ++mfi) {
    const amrex::FArrayBox& fab = mf[mfi];
    const double* p = fab.dataPtr();
    local.insert(local.end(), p, p + fab.box().numPts());
  }

  // Across ranks, the domain is held exactly once.
  unsigned long local_size = local.size();
  unsigned long global_size = 0;
  ASSERT_EQ(MPI_Allreduce(&local_size, &global_size, 1, MPI_UNSIGNED_LONG,
                          MPI_SUM, g_infra->comm()), MPI_SUCCESS);
  EXPECT_EQ(global_size, static_cast<unsigned long>(n) * n * n);

  const int64_t sum_a = TIM::checksum(local.data(), local.size(), /*mask_val=*/nullptr);
  const int64_t sum_b = TIM::checksum(local.data(), local.size(), /*mask_val=*/nullptr);
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
