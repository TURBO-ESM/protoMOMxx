#include "MOM.h"
#include "MOM_logger.h"

namespace MOM {

Model::Model(const int ensemble_num)
  : ensemble_num_(ensemble_num) {

  // todo: read VERBOSITY param instead of hardcoding it below
  logger::set_verbosity(logger::LogLevel::DEBUG);
  logger::CallTree scope("MOM::Model::Model");

  logger::info("Initializing protoMOMxx...");
  if (ensemble_num_ >= 0) {
    logger::info("Ensemble number: ", ensemble_num_);
  }

  // todo: Additional initialization code will be added here in the future.

}

} // namespace MOM
