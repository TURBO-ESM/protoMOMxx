/**
 * @file MOM_document.cpp
 * @brief Implementation of the MOM parameter documentation system.
 *
 * Reimplements the Fortran MOM_document module in C++.
 */

#include "MOM_document.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DocFileWriter::DocFileWriter(const std::string& doc_file_base,
                             bool complete, bool minimal,
                             bool layout, bool debugging)
    : doc_file_base_(doc_file_base),
      complete_(complete), minimal_(minimal),
      layout_(layout), debugging_(debugging)
{}

DocFileWriter::~DocFileWriter() {
  // Close the current module if one is open
  if (!current_module_.empty()) {
    open_files();  // Ensure files are open before writing
    if (files_are_open_) {
      write_message_and_desc("", "");  // Blank line
      std::string closing = "%" + current_module_;
      write_message_and_desc(closing, "");
    }
  }
  
  // Close any open files
  if (file_all_.is_open())       file_all_.close();
  if (file_short_.is_open())     file_short_.close();
  if (file_layout_.is_open())    file_layout_.close();
  if (file_debugging_.is_open()) file_debugging_.close();
}

// ---------------------------------------------------------------------------
// File management
// ---------------------------------------------------------------------------

void DocFileWriter::open_files() {
  if (files_are_open_ || doc_file_base_.empty()) return;

  if (complete_ && !file_all_.is_open()) {
    file_all_.open(doc_file_base_ + ".all");
    if (!file_all_)
      throw std::runtime_error("Failed to open doc file: " + doc_file_base_ + ".all");
    file_all_ << "! This file was written by the model and records all non-layout "
              << "or debugging parameters used at run-time.\n";
  }

  if (minimal_ && !file_short_.is_open()) {
    file_short_.open(doc_file_base_ + ".short");
    if (!file_short_)
      throw std::runtime_error("Failed to open doc file: " + doc_file_base_ + ".short");
    file_short_ << "! This file was written by the model and records the non-default "
                << "parameters used at run-time.\n";
  }

  if (layout_ && !file_layout_.is_open()) {
    file_layout_.open(doc_file_base_ + ".layout");
    if (!file_layout_)
      throw std::runtime_error("Failed to open doc file: " + doc_file_base_ + ".layout");
    file_layout_ << "! This file was written by the model and records the layout "
                 << "parameters used at run-time.\n";
  }

  if (debugging_ && !file_debugging_.is_open()) {
    file_debugging_.open(doc_file_base_ + ".debugging");
    if (!file_debugging_)
      throw std::runtime_error("Failed to open doc file: " + doc_file_base_ + ".debugging");
    file_debugging_ << "! This file was written by the model and records the debugging "
                    << "parameters used at run-time.\n";
  }

  files_are_open_ = true;
}

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------

std::string DocFileWriter::int_string(std::int64_t val) {
  return std::to_string(val);
}

std::string DocFileWriter::real_string(double val) {
  if (val == 0.0) return "0.0";

  std::ostringstream oss;
  double absval = std::abs(val);

  if (absval >= 1.0e-3 && absval < 1.0e4) {
    // Fixed notation — try increasing precision until round-trip matches
    for (int prec = 11; prec <= 16; ++prec) {
      oss.str(""); oss.clear();
      oss << std::fixed << std::setprecision(prec) << val;
      std::string s = oss.str();
      // Verify round-trip
      double check = std::stod(s);
      if (check == val) {
        // Trim trailing zeros but keep at least one digit after '.'
        auto dot = s.find('.');
        if (dot != std::string::npos) {
          auto last_nonzero = s.find_last_not_of('0');
          if (last_nonzero != std::string::npos && last_nonzero > dot) {
            s.erase(last_nonzero + 1);
          } else {
            // Keep at least "x.0"
            s.erase(dot + 2);
          }
        }
        return s;
      }
    }
    // Fallback
    oss.str(""); oss.clear();
    oss << std::fixed << std::setprecision(15) << val;
    return oss.str();
  }

  // Scientific notation
  for (int prec = 14; prec <= 16; ++prec) {
    oss.str(""); oss.clear();
    oss << std::scientific << std::setprecision(prec) << val;
    std::string s = oss.str();
    double check = std::stod(s);
    if (check == val) {
      // Trim trailing zeros before 'e'
      auto e_pos = s.find('e');
      if (e_pos == std::string::npos) e_pos = s.find('E');
      if (e_pos != std::string::npos) {
        auto dot = s.find('.');
        if (dot != std::string::npos && dot < e_pos) {
          auto last_nonzero = s.find_last_not_of('0', e_pos - 1);
          if (last_nonzero != std::string::npos && last_nonzero > dot) {
            s.erase(last_nonzero + 1, e_pos - last_nonzero - 1);
          }
        }
      }
      return s;
    }
  }
  oss.str(""); oss.clear();
  oss << std::scientific << std::setprecision(15) << val;
  return oss.str();
}

std::string DocFileWriter::define_string(const std::string& varname,
                                         const std::string& valstring,
                                         const std::string& units) const {
  std::string result = varname + " = " + valstring;
  int num_spaces = std::max(1, comment_column_ - static_cast<int>(result.size()));
  result += std::string(num_spaces, ' ') + "!";
  if (!units.empty())
    result += "   [" + units + "]";
  return result;
}

std::string DocFileWriter::undef_string(const std::string& varname,
                                        const std::string& units) const {
  std::string result = varname + " = False";
  int num_spaces = std::max(1, comment_column_ - static_cast<int>(result.size()));
  result += std::string(num_spaces, ' ') + "!";
  if (!units.empty())
    result += "   [" + units + "]";
  return result;
}

// ---------------------------------------------------------------------------
// Message writing
// ---------------------------------------------------------------------------

void DocFileWriter::write_message_and_desc(const std::string& mesg,
                                           const std::string& desc,
                                           bool value_was_default,
                                           bool layout_param,
                                           bool debugging_param) {
  // Determine which files to write to
  bool write_all   = complete_ && file_all_.is_open()       && !layout_param && !debugging_param;
  bool write_short = minimal_  && file_short_.is_open()     && !layout_param && !debugging_param;
  bool write_lay   = layout_param && file_layout_.is_open();
  bool write_dbg   = debugging_param && file_debugging_.is_open();

  // If value equals default, suppress writing to the .short file
  if (value_was_default) write_short = false;

  // Write the main message line
  if (write_all)   file_all_   << mesg << "\n";
  if (write_short) file_short_ << mesg << "\n";
  if (write_lay)   file_layout_ << mesg << "\n";
  if (write_dbg)   file_debugging_ << mesg << "\n";

  if (desc.empty()) return;

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
        ++i; // skip the 't'
      } else {
        processed_desc += desc[i];
      }
    } else {
      processed_desc += desc[i];
    }
  }

  // Word-wrap the description and write as comment lines
  std::string indent(comment_column_, ' ');
  int text_width = max_line_len_ - comment_column_ - 2; // "! " prefix = 2 chars

  std::size_t start = 0;
  while (start < processed_desc.size()) {
    // Skip leading spaces on continuation lines (but preserve leading spaces for indented options)
    // Find the next non-newline character
    if (start > 0 && processed_desc[start - 1] == '\n') {
      // After a newline, preserve leading spaces (for options list indentation)
      // but skip trailing spaces from the previous line
    }
    
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
      if (end >= processed_desc.size()) end = processed_desc.size();
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

    if (write_all)   file_all_   << comment_line << "\n";
    if (write_short) file_short_ << comment_line << "\n";
    if (write_lay)   file_layout_ << comment_line << "\n";
    if (write_dbg)   file_debugging_ << comment_line << "\n";
  }
}

// ---------------------------------------------------------------------------
// Duplicate detection
// ---------------------------------------------------------------------------

bool DocFileWriter::mesg_has_been_documented(const std::string& varname,
                                             const std::string& mesg) {
  std::string full_name = block_prefix_ + varname;

  auto it = documented_.find(full_name);
  if (it != documented_.end()) {
    if (it->second != mesg) {
      std::cerr << "WARNING: Inconsistent documentation for parameter "
                << full_name << "\n"
                << "  Previous: " << it->second << "\n"
                << "  New:      " << mesg << "\n";
    }
    return true;
  }

  documented_[full_name] = mesg;
  return false;
}

// ---------------------------------------------------------------------------
// doc_param overloads
// ---------------------------------------------------------------------------

void DocFileWriter::doc_param(const std::string& varname, const std::string& desc,
                              const std::string& units, bool val,
                              std::optional<bool> default_val,
                              const DocParamOptions& opts) {
  open_files();
  if (!files_are_open_) return;
  handle_module_transition(opts.module);

  std::string mesg;
  if (val) {
    mesg = define_string(varname, "True", units);
  } else {
    mesg = undef_string(varname, units);
  }

  bool equals_default = opts.like_default;
  if (default_val.has_value()) {
    if (val == default_val.value()) equals_default = true;
    mesg += " default = ";
    mesg += default_val.value() ? "True" : "False";
  }

  if (mesg_has_been_documented(varname, mesg)) return;
  write_message_and_desc(mesg, desc, equals_default,
                         opts.layout_param, opts.debugging_param);
}

void DocFileWriter::doc_param(const std::string& varname, const std::string& desc,
                              const std::string& units, std::int64_t val,
                              std::optional<std::int64_t> default_val,
                              const DocParamOptions& opts) {
  open_files();
  if (!files_are_open_) return;
  handle_module_transition(opts.module);
  if (!files_are_open_) return;

  std::string mesg = define_string(varname, int_string(val), units);

  bool equals_default = opts.like_default;
  if (default_val.has_value()) {
    if (val == default_val.value()) equals_default = true;
    mesg += " default = " + int_string(default_val.value());
  }

  if (mesg_has_been_documented(varname, mesg)) return;
  write_message_and_desc(mesg, desc, equals_default,
                         opts.layout_param, opts.debugging_param);
}

void DocFileWriter::doc_param(const std::string& varname, const std::string& desc,
                              const std::string& units, double val,
                              std::optional<double> default_val,
                              const DocParamOptions& opts) {
  open_files();
  if (!files_are_open_) return;
  handle_module_transition(opts.module);
  if (!files_are_open_) return;

  std::string mesg = define_string(varname, real_string(val), units);

  bool equals_default = opts.like_default;
  if (default_val.has_value()) {
    if (val == default_val.value()) equals_default = true;
    mesg += " default = " + real_string(default_val.value());
  }

  if (mesg_has_been_documented(varname, mesg)) return;
  write_message_and_desc(mesg, desc, equals_default,
                         opts.layout_param, opts.debugging_param);
}

void DocFileWriter::doc_param(const std::string& varname, const std::string& desc,
                              const std::string& units, const std::string& val,
                              std::optional<std::string> default_val,
                              const DocParamOptions& opts) {
  open_files();
  if (!files_are_open_) return;
  handle_module_transition(opts.module);
  if (!files_are_open_) return;

  std::string mesg = define_string(varname, "\"" + val + "\"", units);

  bool equals_default = opts.like_default;
  if (default_val.has_value()) {
    if (val == default_val.value()) equals_default = true;
    mesg += " default = \"" + default_val.value() + "\"";
  }

  if (mesg_has_been_documented(varname, mesg)) return;
  write_message_and_desc(mesg, desc, equals_default,
                         opts.layout_param, opts.debugging_param);
}

void DocFileWriter::doc_param(const std::string& varname, const std::string& desc,
                              const std::string& units, const std::vector<bool>& vals,
                              std::optional<std::vector<bool>> default_val,
                              const DocParamOptions& opts) {
  open_files();
  if (!files_are_open_) return;
  handle_module_transition(opts.module);
  if (!files_are_open_) return;

  std::string valstring;
  for (size_t i = 0; i < vals.size(); ++i) {
    if (i > 0) valstring += ", ";
    valstring += vals[i] ? "True" : "False";
  }
  std::string mesg = define_string(varname, valstring, units);

  bool equals_default = opts.like_default;
  if (default_val.has_value()) {
    const auto& dv = default_val.value();
    equals_default = (vals.size() == dv.size());
    if (equals_default) {
      for (size_t i = 0; i < vals.size(); ++i) {
        if (vals[i] != dv[i]) { equals_default = false; break; }
      }
    }
    std::string defstr;
    for (size_t i = 0; i < dv.size(); ++i) {
      if (i > 0) defstr += ", ";
      defstr += dv[i] ? "True" : "False";
    }
    mesg += " default = " + defstr;
  }

  if (mesg_has_been_documented(varname, mesg)) return;
  write_message_and_desc(mesg, desc, equals_default,
                         opts.layout_param, opts.debugging_param);
}

void DocFileWriter::doc_param(const std::string& varname, const std::string& desc,
                              const std::string& units, const std::vector<std::int64_t>& vals,
                              std::optional<std::vector<std::int64_t>> default_val,
                              const DocParamOptions& opts) {
  open_files();
  if (!files_are_open_) return;
  handle_module_transition(opts.module);
  if (!files_are_open_) return;

  std::string valstring;
  for (size_t i = 0; i < vals.size(); ++i) {
    if (i > 0) valstring += ", ";
    valstring += int_string(vals[i]);
  }
  std::string mesg = define_string(varname, valstring, units);

  bool equals_default = opts.like_default;
  if (default_val.has_value()) {
    const auto& dv = default_val.value();
    equals_default = (vals.size() == dv.size());
    if (equals_default) {
      for (size_t i = 0; i < vals.size(); ++i) {
        if (vals[i] != dv[i]) { equals_default = false; break; }
      }
    }
    std::string defstr;
    for (size_t i = 0; i < dv.size(); ++i) {
      if (i > 0) defstr += ", ";
      defstr += int_string(dv[i]);
    }
    mesg += " default = " + defstr;
  }

  if (mesg_has_been_documented(varname, mesg)) return;
  write_message_and_desc(mesg, desc, equals_default,
                         opts.layout_param, opts.debugging_param);
}

void DocFileWriter::doc_param(const std::string& varname, const std::string& desc,
                              const std::string& units, const std::vector<double>& vals,
                              std::optional<std::vector<double>> default_val,
                              const DocParamOptions& opts) {
  open_files();
  if (!files_are_open_) return;
  handle_module_transition(opts.module);
  if (!files_are_open_) return;

  std::string valstring;
  for (size_t i = 0; i < vals.size(); ++i) {
    if (i > 0) valstring += ", ";
    valstring += real_string(vals[i]);
  }
  std::string mesg = define_string(varname, valstring, units);

  bool equals_default = opts.like_default;
  if (default_val.has_value()) {
    const auto& dv = default_val.value();
    equals_default = (vals.size() == dv.size());
    if (equals_default) {
      for (size_t i = 0; i < vals.size(); ++i) {
        if (vals[i] != dv[i]) { equals_default = false; break; }
      }
    }
    std::string defstr;
    for (size_t i = 0; i < dv.size(); ++i) {
      if (i > 0) defstr += ", ";
      defstr += real_string(dv[i]);
    }
    mesg += " default = " + defstr;
  }

  if (mesg_has_been_documented(varname, mesg)) return;
  write_message_and_desc(mesg, desc, equals_default,
                         opts.layout_param, opts.debugging_param);
}

void DocFileWriter::doc_param(const std::string& varname, const std::string& desc,
                              const std::string& units, const std::vector<std::string>& vals,
                              std::optional<std::vector<std::string>> default_val,
                              const DocParamOptions& opts) {
  open_files();
  if (!files_are_open_) return;
  handle_module_transition(opts.module);
  if (!files_are_open_) return;

  std::string valstring;
  for (size_t i = 0; i < vals.size(); ++i) {
    if (i > 0) valstring += ", ";
    valstring += "\"" + vals[i] + "\"";
  }
  std::string mesg = define_string(varname, valstring, units);

  bool equals_default = opts.like_default;
  if (default_val.has_value()) {
    const auto& dv = default_val.value();
    equals_default = (vals.size() == dv.size());
    if (equals_default) {
      for (size_t i = 0; i < vals.size(); ++i) {
        if (vals[i] != dv[i]) { equals_default = false; break; }
      }
    }
    std::string defstr;
    for (size_t i = 0; i < dv.size(); ++i) {
      if (i > 0) defstr += ", ";
      defstr += "\"" + dv[i] + "\"";
    }
    mesg += " default = " + defstr;
  }

  if (mesg_has_been_documented(varname, mesg)) return;
  write_message_and_desc(mesg, desc, equals_default,
                         opts.layout_param, opts.debugging_param);
}

// ---------------------------------------------------------------------------
// Module / block documentation
// ---------------------------------------------------------------------------

void DocFileWriter::doc_module(const std::string& modname, const std::string& desc,
                               bool layout_mod, bool debugging_mod,
                               bool all_default) {
  open_files();
  if (!files_are_open_) return;

  // Blank line for delineation
  write_message_and_desc("", "", all_default, layout_mod, debugging_mod);

  std::string mesg = "! === module " + modname + " ===";
  write_message_and_desc(mesg, desc, all_default, layout_mod, debugging_mod);
}

void DocFileWriter::close_module() {
  if (!files_are_open_ || current_module_.empty()) return;

  // Blank line for delineation
  write_message_and_desc("", "");

  // Closing module block
  std::string mesg = "%" + current_module_;
  write_message_and_desc(mesg, "");
  
  current_module_.clear();
}

void DocFileWriter::handle_module_transition(const std::string& new_module) {
  open_files();  // Ensure files are open
  if (!files_are_open_) return;

  // If we're switching modules, close the previous one and open the new one
  if (new_module != current_module_) {
    if (!current_module_.empty()) {
      // Close the previous module
      write_message_and_desc("", "");  // Blank line
      std::string closing = "%" + current_module_;
      write_message_and_desc(closing, "");
    }
    
    if (!new_module.empty()) {
      // Open the new module
      write_message_and_desc("", "");  // Blank line
      std::string opening = new_module + "%";
      write_message_and_desc(opening, "");
    }
    
    current_module_ = new_module;
  }
}

void DocFileWriter::doc_openBlock(const std::string& blockName,
                                  const std::string& desc) {
  open_files();
  if (!files_are_open_) return;

  std::string mesg = blockName + "%";
  write_message_and_desc(mesg, desc);

  block_prefix_ += blockName + "%";
}

void DocFileWriter::doc_closeBlock(const std::string& blockName) {
  open_files();
  if (!files_are_open_) return;

  std::string mesg = "%" + blockName;
  write_message_and_desc(mesg, "");

  // Remove the last occurrence of "blockName%" from block_prefix_
  std::string target = blockName + "%";
  auto pos = block_prefix_.rfind(target);
  if (pos != std::string::npos) {
    block_prefix_.erase(pos, target.size());
  }
}
