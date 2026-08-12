#pragma once
/// @file MOM_unsupported_params.h
/// @brief Handling of MOM6 parameters that protoMOMxx does not support.
///
/// The parameters handled here are read only to warn users that protoMOMxx
/// does not support them. (Distinct from legacy MOM6's MOM_obsolete_params, which
/// flags parameters removed from legacy MOM6 itself; that scan is not ported yet.)

#include <string>

#include "MOM_file_parser.h"
#include "MOM_logger.h"

namespace MOM {

/// @brief Read a MOM6 parameter that protoMOMxx does not support and warn if
/// it is set to a value protoMOMxx's behavior is incompatible with.
///
/// The parameter is read unlogged with @p supported_value as the default, so
/// absence stays silent and the unsupported name stays out of the doc files;
/// an explicit setting that protoMOMxx cannot support logs a warning naming
/// the parameter.
///
/// @tparam T The parameter's value type.
/// @param params Runtime parameters.
/// @param name The unsupported parameter's name.
/// @param supported_value The value protoMOMxx's fixed behavior is compatible
///        with (not necessarily the parameter's legacy MOM6 default); any other
///        value triggers the warning.
/// @param detail The reason the parameter is ignored, appended to the warning.
template <typename T>
void unsupported_param(RuntimeParams &params, const std::string &name,
                       const T &supported_value, const std::string &detail) {
  T value = supported_value;
  params.get(name, value,
             {.default_value = supported_value,
              .desc = "Legacy MOM6 parameter; not supported by protoMOMxx.",
              .do_not_log = true});
  if (value != supported_value) {
    logger::warning(name, " is ignored; ", detail);
  }
}

} // namespace MOM
