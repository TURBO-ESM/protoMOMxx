/**
 * @file MOM_param_table.h
 * @brief A shared parameter storage table used by both RuntimeParams and NamelistParams.
 *
 * @details
 * ParamTable provides a two-level map (group -> key -> ParamValue) with optional
 * case-insensitive normalization. It factors out the common storage and lookup logic
 * shared by RuntimeParams and NamelistParams.
 */

#pragma once

#include "MOM_parser_utils.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace MOM {

using parser_utils::ParamValue;

/// @brief A two-level parameter storage table (group -> key -> value).
///
/// When case_insensitive is true, all group names and keys are lowercased
/// before storage and lookup.
class ParamTable {
public:
  /// @brief Construct a ParamTable.
  /// @param case_insensitive If true, normalize keys and group names to lowercase.
  explicit ParamTable(bool case_insensitive = false);

  /// @brief Insert a parameter into the table.
  ///
  /// When is_override is false (default), inserts a new entry or accepts a
  /// duplicate only if the value matches exactly. Throws on conflicting duplicates.
  /// When is_override is true, requires the key to already exist and replaces its value.
  ///
  /// @param group The group (module/namelist) name.
  /// @param key The parameter key.
  /// @param value The parameter value.
  /// @param is_override If true, override an existing key (throws if key is missing).
  void insert(const std::string &group, const std::string &key, ParamValue value, bool is_override = false);

  /// @brief Look up a parameter value.
  /// @param key The parameter key.
  /// @param group The group name (empty string for the default/global group).
  /// @return A const reference to the stored ParamValue.
  /// @throws std::out_of_range if the group or key does not exist.
  const ParamValue &get_variant(const std::string &key, const std::string &group = "") const;

  /// @brief Check if a parameter exists.
  /// @param key The parameter key.
  /// @param group The group name (empty string for the default/global group).
  /// @return true if the parameter exists.
  bool has_param(const std::string &key, const std::string &group = "") const;

  /// @brief Get all group names.
  /// @return A vector of group names.
  std::vector<std::string> get_groups() const;

  /// @brief Get the total number of parameters across all groups.
  /// @return The total parameter count.
  size_t get_num_parameters() const;

private:
  bool case_insensitive_;
  std::unordered_map<std::string, std::unordered_map<std::string, ParamValue>> table_;

  /// @brief Normalize a string (lowercase if case_insensitive_).
  std::string normalize(const std::string &s) const;
};

} // namespace MOM
