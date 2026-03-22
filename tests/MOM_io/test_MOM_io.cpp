// Unit tests for MOM_io.h (ensembler utility)

#include <gtest/gtest.h>
#include <string>

#include "MOM_io.h"

using namespace MOM;

TEST(MOMIoTest, EnsemblerInsertsNumber) {
  EXPECT_EQ(io::ensembler("output.nc", 3), "output.3.nc");
}

TEST(MOMIoTest, EnsemblerNegativeIsNoop) {
  EXPECT_EQ(io::ensembler("output.nc", -1), "output.nc");
}

TEST(MOMIoTest, EnsemblerEmptyStringIsNoop) {
  EXPECT_EQ(io::ensembler("", 5), "");
}

TEST(MOMIoTest, EnsemblerNoExtension) {
  EXPECT_EQ(io::ensembler("MOM_input", 2), "MOM_input.2");
}

TEST(MOMIoTest, EnsemblerWithDirectory) {
  EXPECT_EQ(io::ensembler("RESTART/ocean.nc", 0), "RESTART/ocean.0.nc");
}
