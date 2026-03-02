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

/// @brief Append ensemble number to a filename stem (overload for string input).
/// @param s    Input filename as a string, e.g. "output.nc"
/// @param n    Ensemble number; negative means no-op.
/// @return     e.g. "output.3.nc"
inline std::filesystem::path ensembler(const std::string &s, int n) { return ensembler(std::filesystem::path(s), n); }

} // namespace MOM_io
