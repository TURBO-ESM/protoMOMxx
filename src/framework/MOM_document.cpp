/**
 * @file MOM_document.cpp
 * @brief Implementation of the MOM parameter documentation system.
 *
 * Reimplements the Fortran MOM_document module in C++.
 */

#include "MOM_document.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <type_traits>
#include <stdexcept>

#include "MOM_logger.h"

namespace MOM {

namespace {
// Thresholds for choosing fixed vs scientific notation in real_string
constexpr double fixed_lower_bound = 1.0e-3;
constexpr double fixed_upper_bound = 1.0e4;

// Precision sweep bounds for real_string round-trip formatting
constexpr int fixed_min_precision = 1;
constexpr int sci_min_precision = 1;
constexpr int max_precision = 16;
constexpr int fallback_precision = 15;

/// @brief Trim trailing zeros from a fixed-notation string, keeping at least "x.0".
void trim_fixed_zeros(std::string &s) {
  auto dot = s.find('.');
  if (dot == std::string::npos)
    return;
  auto last_nonzero = s.find_last_not_of('0');
  if (last_nonzero != std::string::npos && last_nonzero > dot)
    s.erase(last_nonzero + 1);
  else
    s.erase(dot + 2); // keep at least "x.0"
}

/// @brief Trim trailing zeros before the exponent in a scientific-notation string.
void trim_sci_zeros(std::string &s) {
  auto e_pos = s.find('e');
  if (e_pos == std::string::npos)
    e_pos = s.find('E');
  if (e_pos == std::string::npos)
    return;
  auto dot = s.find('.');
  if (dot != std::string::npos && dot < e_pos) {
    auto last_nonzero = s.find_last_not_of('0', e_pos - 1);
    if (last_nonzero != std::string::npos && last_nonzero > dot)
      s.erase(last_nonzero + 1, e_pos - last_nonzero - 1);
  }
}
} // namespace

// ---------------------------------------------------------------------------
// DocFile
// ---------------------------------------------------------------------------

void DocFile::open(const std::string &path, std::string_view header) {
  if (!enabled || stream.is_open())
    return;
  stream.open(path);
  if (!stream)
    throw std::runtime_error("Failed to open doc file: " + path);
  stream << header;
}

void DocFile::close() noexcept {
  if (stream.is_open())
    stream.close();
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DocFileWriter::~DocFileWriter() noexcept {
  try {
    close();
  } catch (...) {
    // Swallow errors — callers should use close() explicitly to detect I/O failures.
    // This is just a safety net to ensure files are closed on destruction, but we don't want
    // exceptions escaping from the destructor.
  }
}

DocFileWriter::DocFileWriter(const std::string &doc_file_base, bool complete, bool minimal, bool layout, bool debugging)
    : doc_file_base_(doc_file_base) {
  file_all_.enabled = complete;
  file_short_.enabled = minimal;
  file_layout_.enabled = layout;
  file_debugging_.enabled = debugging;
}

void DocFileWriter::close() {
  close_module();

  // Flush and check for I/O errors before closing, so callers can detect
  // write failures (the DocFile::close() noexcept path cannot report them).
  for (auto *f : {&file_all_, &file_short_, &file_layout_, &file_debugging_}) {
    if (f->stream.is_open()) {
      f->stream.flush();
      if (!f->stream)
        throw std::runtime_error("I/O error writing doc file");
      f->close();
    }
  }
  files_are_open_ = false;
}

// ---------------------------------------------------------------------------
// File management
// ---------------------------------------------------------------------------

void DocFileWriter::open_files() {
  if (files_are_open_ || doc_file_base_.empty())
    return;

  file_all_.open(doc_file_base_ + ".all",
                 "! This file was written by the model and records all non-layout or debugging parameters used at run-time.\n");
  file_short_.open(doc_file_base_ + ".short",
                   "! This file was written by the model and records the non-default parameters used at run-time.\n");
  file_layout_.open(doc_file_base_ + ".layout",
                    "! This file was written by the model and records the layout parameters used at run-time.\n");
  file_debugging_.open(doc_file_base_ + ".debugging",
                       "! This file was written by the model and records the debugging parameters used at run-time.\n");

  files_are_open_ = true;
}

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------

std::string DocFileWriter::real_string(double val) {
  if (val == 0.0)
    return "0.0";

  // Stack buffer large enough for any double representation
  std::array<char, 64> buf;
  double absval = std::abs(val);

  if (absval >= fixed_lower_bound && absval < fixed_upper_bound) {
    // Fixed notation — try increasing precision until round-trip matches
    for (int prec = fixed_min_precision; prec <= max_precision; ++prec) {
      auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), val, std::chars_format::fixed, prec);
      if (ec != std::errc{})
        continue;
      std::string s(buf.data(), end);
      double reparsed = 0.0;
      auto [rptr, rec] = std::from_chars(s.data(), s.data() + s.size(), reparsed);
      if (rec == std::errc{} && reparsed == val) {
        trim_fixed_zeros(s);
        return s;
      }
    }
    // Fallback
    auto [end, ec] =
        std::to_chars(buf.data(), buf.data() + buf.size(), val, std::chars_format::fixed, fallback_precision);
    std::string s(buf.data(), (ec == std::errc{}) ? end : buf.data());
    trim_fixed_zeros(s);
    return s;
  }

  // Scientific notation
  for (int prec = sci_min_precision; prec <= max_precision; ++prec) {
    auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), val, std::chars_format::scientific, prec);
    if (ec != std::errc{})
      continue;
    std::string s(buf.data(), end);
    double reparsed = 0.0;
    auto [rptr, rec] = std::from_chars(s.data(), s.data() + s.size(), reparsed);
    if (rec == std::errc{} && reparsed == val) {
      trim_sci_zeros(s);
      return s;
    }
  }
  auto [end, ec] =
      std::to_chars(buf.data(), buf.data() + buf.size(), val, std::chars_format::scientific, fallback_precision);
  std::string s(buf.data(), (ec == std::errc{}) ? end : buf.data());
  trim_sci_zeros(s);
  return s;
}

std::string DocFileWriter::define_string(std::string_view varname, std::string_view valstring,
                                         std::string_view units) const {
  std::string result;
  result.reserve(varname.size() + 3 + valstring.size() + comment_column_ + units.size() + 8);
  result += varname;
  result += " = ";
  result += valstring;
  int num_spaces = std::max(1, comment_column_ - static_cast<int>(result.size()));
  result.append(num_spaces, ' ');
  result += '!';
  if (!units.empty()) {
    result += "   [";
    result += units;
    result += ']';
  }
  return result;
}

std::string DocFileWriter::undef_string(std::string_view varname, std::string_view units) const {
  std::string result;
  result.reserve(varname.size() + 8 + comment_column_ + units.size() + 8);
  result += varname;
  result += " = False";
  int num_spaces = std::max(1, comment_column_ - static_cast<int>(result.size()));
  result.append(num_spaces, ' ');
  result += '!';
  if (!units.empty()) {
    result += "   [";
    result += units;
    result += ']';
  }
  return result;
}

// ---------------------------------------------------------------------------
// Message writing
// ---------------------------------------------------------------------------

void DocFileWriter::write_message_and_desc(std::string_view mesg, std::string_view desc, bool value_was_default,
                                           bool layout_param, bool debugging_param) {
  // Determine which files to write to
  bool write_all = file_all_.is_open() && !layout_param && !debugging_param;
  bool write_short = file_short_.is_open() && !layout_param && !debugging_param && !value_was_default;
  bool write_lay = layout_param && file_layout_.is_open();
  bool write_dbg = debugging_param && file_debugging_.is_open();

  // Write the main message line
  if (write_all)
    file_all_.stream << mesg << "\n";
  if (write_short)
    file_short_.stream << mesg << "\n";
  if (write_lay)
    file_layout_.stream << mesg << "\n";
  if (write_dbg)
    file_debugging_.stream << mesg << "\n";

  auto check_io = [&] {
    if ((write_all && !file_all_.stream) || (write_short && !file_short_.stream) ||
        (write_lay && !file_layout_.stream) || (write_dbg && !file_debugging_.stream))
      throw std::runtime_error("I/O error while writing documentation file");
  };

  if (desc.empty()) {
    check_io();
    return;
  }

  // Process description: convert escape sequences (\n -> newline, \t -> spaces)
  // and convert actual tab characters to spaces
  std::string processed_desc;
  for (std::size_t i = 0; i < desc.size(); ++i) {
    if (desc[i] == '\t') {
      // Actual tab character -> 2 spaces
      processed_desc += "  ";
    } else if (desc[i] == '\\' && i + 1 < desc.size()) {
      if (desc[i + 1] == 'n') {
        processed_desc += '\n';
        ++i; // skip the 'n'
      } else if (desc[i + 1] == 't') {
        processed_desc += "  "; // tab -> 2 spaces
        ++i;                    // skip the 't'
      } else {
        processed_desc += desc[i];
      }
    } else {
      processed_desc += desc[i];
    }
  }

  // Word-wrap the description and write as comment lines
  std::string indent(comment_column_, ' ');
  int text_width = max_line_len_ - comment_column_ - comment_prefix_len_;

  std::size_t start = 0;
  while (start < processed_desc.size()) {
    std::size_t remaining = processed_desc.size() - start;
    std::string line_text;

    // Check for explicit newline in the processed description
    auto nl_pos = processed_desc.find('\n', start);

    if (nl_pos != std::string::npos && nl_pos - start <= static_cast<std::size_t>(text_width)) {
      // There's a newline within text_width, use it
      line_text = processed_desc.substr(start, nl_pos - start);
      start = nl_pos + 1; // skip the newline
    } else if (remaining <= static_cast<std::size_t>(text_width)) {
      // Remaining text fits on one line
      line_text = processed_desc.substr(start);
      start = processed_desc.size();
    } else {
      // Find a space to break at
      std::size_t end = start + text_width;
      if (end >= processed_desc.size())
        end = processed_desc.size();
      auto break_pos = processed_desc.rfind(' ', end);
      if (break_pos != std::string::npos && break_pos > start) {
        line_text = processed_desc.substr(start, break_pos - start);
        start = break_pos + 1;
      } else {
        line_text = processed_desc.substr(start, text_width);
        start += text_width;
      }
    }

    std::string comment_line = indent + "! " + line_text;

    if (write_all)
      file_all_.stream << comment_line << "\n";
    if (write_short)
      file_short_.stream << comment_line << "\n";
    if (write_lay)
      file_layout_.stream << comment_line << "\n";
    if (write_dbg)
      file_debugging_.stream << comment_line << "\n";

    check_io();
  }
}

// ---------------------------------------------------------------------------
// Duplicate detection
// ---------------------------------------------------------------------------

bool DocFileWriter::mesg_has_been_documented(std::string_view varname, std::string_view mesg) {
  std::string full_name = block_prefix_;
  full_name += varname;

  auto it = documented_.find(full_name);
  if (it != documented_.end()) {
    if (it->second != mesg) {
      logger::warning("Inconsistent documentation for parameter ", full_name,
                           "\n  Previous: ", it->second, "\n  New:      ", mesg);
    }
    return true;
  }

  documented_[full_name] = std::string(mesg);
  return false;
}

// ---------------------------------------------------------------------------
// doc_param helpers
// ---------------------------------------------------------------------------

bool DocFileWriter::prepare_doc(const DocParamOptions &opts) {
  open_files();
  if (!files_are_open_)
    return false;
  return true;
}

void DocFileWriter::finalize_doc(std::string_view varname, std::string_view desc, std::string_view mesg,
                                 bool equals_default, const DocParamOptions &opts) {
  if (mesg_has_been_documented(varname, mesg))
    return;

  // If this param differs from its default, the module is no longer all-default
  if (!equals_default)
    current_module_all_default_ = false;

  // Suppress from .short if the module-level all_default flag is still set
  bool suppress_short = equals_default || current_module_all_default_;
  write_message_and_desc(mesg, desc, suppress_short, opts.layout_param, opts.debugging_param);
}

namespace {

/// @brief Convert a value to its documentation string representation.
std::string to_doc_string(bool v) { return v ? "True" : "False"; }
std::string to_doc_string(int v) { return std::to_string(v); }
std::string to_doc_string(double v) { return DocFileWriter::real_string(v); }
std::string to_doc_string(const std::string &v) { return "\"" + v + "\""; }

} // namespace

// ---------------------------------------------------------------------------
// doc_param — scalar template
// ---------------------------------------------------------------------------

template <typename T>
void DocFileWriter::doc_param(std::string_view varname, std::string_view desc, std::string_view units, const T &val,
                              std::optional<T> default_val, const DocParamOptions &opts) {
  if (!prepare_doc(opts))
    return;

  std::string mesg;
  if constexpr (std::is_same_v<T, bool>) {
    mesg = val ? define_string(varname, "True", units) : undef_string(varname, units);
  } else {
    mesg = define_string(varname, to_doc_string(val), units);
  }

  bool equals_default = opts.like_default;
  if (default_val.has_value()) {
    if (val == default_val.value())
      equals_default = true;
    mesg += " default = " + to_doc_string(default_val.value());
  }

  finalize_doc(varname, desc, mesg, equals_default, opts);
}

// ---------------------------------------------------------------------------
// doc_param — vector template
// ---------------------------------------------------------------------------

template <typename T>
void DocFileWriter::doc_param(std::string_view varname, std::string_view desc, std::string_view units,
                              const std::vector<T> &vals, std::optional<std::vector<T>> default_val,
                              const DocParamOptions &opts) {
  if (!prepare_doc(opts))
    return;

  std::string valstring;
  for (size_t i = 0; i < vals.size(); ++i) {
    if (i > 0)
      valstring += ", ";
    valstring += to_doc_string(vals[i]);
  }
  std::string mesg = define_string(varname, valstring, units);

  bool equals_default = opts.like_default;
  if (default_val.has_value()) {
    const auto &dv = default_val.value();
    bool vals_match = (vals.size() == dv.size());
    if (vals_match) {
      for (size_t i = 0; i < vals.size(); ++i) {
        if (vals[i] != dv[i]) {
          vals_match = false;
          break;
        }
      }
    }
    equals_default = equals_default || vals_match;
    std::string defstr;
    for (size_t i = 0; i < dv.size(); ++i) {
      if (i > 0)
        defstr += ", ";
      defstr += to_doc_string(dv[i]);
    }
    mesg += " default = " + defstr;
  }

  finalize_doc(varname, desc, mesg, equals_default, opts);
}

// Explicit instantiations — scalar
template void DocFileWriter::doc_param<bool>(std::string_view, std::string_view, std::string_view, const bool &,
                                             std::optional<bool>, const DocParamOptions &);
template void DocFileWriter::doc_param<int>(std::string_view, std::string_view, std::string_view, const int &,
                                            std::optional<int>, const DocParamOptions &);
template void DocFileWriter::doc_param<double>(std::string_view, std::string_view, std::string_view, const double &,
                                               std::optional<double>, const DocParamOptions &);
template void DocFileWriter::doc_param<std::string>(std::string_view, std::string_view, std::string_view,
                                                    const std::string &, std::optional<std::string>,
                                                    const DocParamOptions &);

// Explicit instantiations — vector
template void DocFileWriter::doc_param<bool>(std::string_view, std::string_view, std::string_view,
                                             const std::vector<bool> &, std::optional<std::vector<bool>>,
                                             const DocParamOptions &);
template void DocFileWriter::doc_param<int>(std::string_view, std::string_view, std::string_view,
                                            const std::vector<int> &, std::optional<std::vector<int>>,
                                            const DocParamOptions &);
template void DocFileWriter::doc_param<double>(std::string_view, std::string_view, std::string_view,
                                               const std::vector<double> &, std::optional<std::vector<double>>,
                                               const DocParamOptions &);
template void DocFileWriter::doc_param<std::string>(std::string_view, std::string_view, std::string_view,
                                                    const std::vector<std::string> &,
                                                    std::optional<std::vector<std::string>>, const DocParamOptions &);

// ---------------------------------------------------------------------------
// Module / block documentation
// ---------------------------------------------------------------------------

void DocFileWriter::doc_module(std::string_view modname, std::string_view desc, bool layout_mod, bool debugging_mod,
                               bool all_default) {
  open_files();
  if (!files_are_open_)
    return;

  // If this module is already open, skip the redundant close/reopen cycle.
  if (current_module_ == modname) {
    logger::warning("doc_module(\"", modname, "\") called but module is already open — skipping.");
    return;
  }

  // Close any previously open module before starting a new one
  close_module();

  // Blank line for delineation. Pass all_default so that when every parameter
  // in this module equals its default, the separator is also omitted from .short.
  write_message_and_desc("", "", all_default, layout_mod, debugging_mod);

  std::string mesg = "! === module ";
  mesg += modname;
  mesg += " ===";
  write_message_and_desc(mesg, desc, all_default, layout_mod, debugging_mod);

  current_module_ = modname;
  current_module_layout_ = layout_mod;
  current_module_debugging_ = debugging_mod;
  current_module_all_default_ = all_default;
}

void DocFileWriter::close_module() {
  if (!files_are_open_ || current_module_.empty())
    return;

  current_module_.clear();
  current_module_layout_ = false;
  current_module_debugging_ = false;
  current_module_all_default_ = false;
}

void DocFileWriter::doc_openBlock(std::string_view blockName, std::string_view desc) {
  open_files();
  if (!files_are_open_)
    return;

  std::string mesg;
  mesg += blockName;
  mesg += '%';
  write_message_and_desc(mesg, desc, current_module_all_default_,
                         current_module_layout_, current_module_debugging_);

  block_prefix_ += blockName;
  block_prefix_ += '%';
}

void DocFileWriter::doc_closeBlock(std::string_view blockName) {
  open_files();
  if (!files_are_open_)
    return;

  std::string mesg = "%";
  mesg += blockName;
  write_message_and_desc(mesg, "", current_module_all_default_,
                         current_module_layout_, current_module_debugging_);

  // Remove the last occurrence of "blockName%" from block_prefix_
  std::string target;
  target += blockName;
  target += '%';
  auto pos = block_prefix_.rfind(target);
  if (pos != std::string::npos) {
    block_prefix_.erase(pos, target.size());
  }
}

} // namespace MOM
