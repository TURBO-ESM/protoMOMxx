#pragma once
/// @file MOM_obsolete_params.h
/// @brief Handling of retired MOM6 parameters.

#include <string>

#include "MOM_file_parser.h"
#include "MOM_logger.h"

namespace MOM {

/// @brief Read a retired MOM6 parameter and warn if it is explicitly set.
///
/// The parameter is read unlogged with @p not_set as the default, so absence
/// stays silent and the retired name stays out of the doc files; an explicit
/// setting that protoMOMxx cannot adopt logs a warning naming the parameter.
///
/// @tparam T The parameter's value type.
/// @param params Runtime parameters.
/// @param name The retired parameter's name.
/// @param not_set The value representing "not set" (the retired parameter's
///        MOM6 default); any other value triggers the warning.
/// @param detail The reason the parameter is ignored, appended to the warning.
template <typename T>
void retired_param(RuntimeParams &params, const std::string &name,
                   const T &not_set, const std::string &detail) {
  T value = not_set;
  params.get(name, value,
             {.default_value = not_set,
              .desc = "Retired parameter; not honored by protoMOMxx.",
              .do_not_log = true});
  if (value != not_set) {
    logger::warning(name, " is ignored; ", detail);
  }
}

} // namespace MOM
