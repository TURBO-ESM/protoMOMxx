#pragma once
/// @file MOM.h
/// @brief Main header for the Modular Ocean Model (MOM) core.
#include "MOM_file_parser.h"

class MOM {
public:
  explicit MOM(
      int ensemble_num = -1
  );

  std::unique_ptr<RuntimeParams> params;

private:
  bool verbosity_ = false;

};