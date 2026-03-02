#pragma once
/// @file MOM_io.h
/// @brief Common I/O utilities for MOM input processing.

#include <filesystem>
#include <string>

namespace MOM_io {

/// @brief Append ensemble number to a filename stem.
/// @param p    Input path, e.g. "output.nc"
/// @param n    Ensemble number; negative means no-op.
/// @return     e.g. "output.3.nc"
inline std::filesystem::path ensembler(std::filesystem::path p, int n = -1) {
  if (n < 0 || p.empty())
    return p;
  return p.parent_path() / (p.stem().string() + "." + std::to_string(n) + p.extension().string());
}

inline std::filesystem::path ensembler(const std::string &s, int ensemble_num) {
  return ensembler(std::filesystem::path(s), ensemble_num);
}

} // namespace MOM_io
