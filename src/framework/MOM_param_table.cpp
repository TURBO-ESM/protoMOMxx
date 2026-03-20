#include "MOM_param_table.h"
#include "MOM_string_utils.h"
#include <stdexcept>

namespace MOM {

using string_utils::lowercase;

ParamTable::ParamTable(bool case_insensitive) : case_insensitive_(case_insensitive) {}

std::string ParamTable::normalize(const std::string &s) const {
  return case_insensitive_ ? lowercase(s) : s;
}

void ParamTable::insert(const std::string &group, const std::string &key, ParamValue value, bool is_override) {
  auto norm_group = normalize(group);
  auto norm_key = normalize(key);

  if (is_override) {
    auto grp_it = table_.find(norm_group);
    if (grp_it == table_.end() || grp_it->second.find(norm_key) == grp_it->second.end()) {
      throw std::runtime_error("#override requires an existing key: '" +
                               (group.empty() ? key : group + "%" + key) + "'");
    }
    grp_it->second[norm_key] = std::move(value);
    return;
  }

  auto &grp_table = table_[norm_group];
  auto it = grp_table.find(norm_key);

  if (it == grp_table.end()) {
    grp_table.emplace(std::move(norm_key), std::move(value));
    return;
  }

  if (it->second != value) {
    throw std::runtime_error("duplicate assignment for '" +
                             (group.empty() ? key : group + "%" + key) +
                             "' with a different value");
  }
}

const ParamValue &ParamTable::get_variant(const std::string &key, const std::string &group) const {
  auto norm_group = normalize(group);
  auto norm_key = normalize(key);

  auto grp_it = table_.find(norm_group);
  if (grp_it == table_.end()) {
    throw std::out_of_range("Group not found: " + group);
  }

  auto key_it = grp_it->second.find(norm_key);
  if (key_it == grp_it->second.end()) {
    throw std::out_of_range("Key not found in group " + group + ": " + key);
  }

  return key_it->second;
}

bool ParamTable::has_param(const std::string &key, const std::string &group) const {
  auto grp_it = table_.find(normalize(group));
  if (grp_it == table_.end()) {
    return false;
  }
  return grp_it->second.find(normalize(key)) != grp_it->second.end();
}

std::vector<std::string> ParamTable::get_groups() const {
  std::vector<std::string> groups;
  groups.reserve(table_.size());
  for (const auto &pair : table_) {
    groups.push_back(pair.first);
  }
  return groups;
}

size_t ParamTable::get_num_parameters() const {
  size_t count = 0;
  for (const auto &grp_pair : table_) {
    count += grp_pair.second.size();
  }
  return count;
}

} // namespace MOM
