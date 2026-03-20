#pragma once
/// @file MOM_io.h
/// @brief Common I/O utilities for MOM input processing.

#include <filesystem>
#include <string>

namespace MOM::io  {

/// @brief Append ensemble number to a filename stem.
/// @param s    Input filename, e.g. "output.nc"
/// @param n    Ensemble number; negative means no-op.
/// @return     e.g. "output.3.nc"
inline std::string ensembler(const std::string &s, int n = -1) {
  if (n < 0 || s.empty())
    return s;
  std::filesystem::path p(s);
  return (p.parent_path() / (p.stem().string() + "." + std::to_string(n) + p.extension().string())).string();
}

} // namespace MOM_io
