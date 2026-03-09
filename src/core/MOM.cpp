#include "MOM.h"
#include "MOM_logger.h"

MOM::MOM(int ensemble_num)
  : ensemble_num_(ensemble_num) {

  // todo: read VERBOSITY param instead of hardcoding it below
  MOM_logger::set_verbosity(MOM_logger::LogLevel::DEBUG);
  MOM_logger::CallTree scope("MOM::MOM");

  MOM_logger::info("Initializing protoMOMxx...");
  if (ensemble_num_ >= 0) {
    MOM_logger::info("Ensemble number: ", ensemble_num_);
  }

  // todo: Additional initialization code will be added here in the future.

}
