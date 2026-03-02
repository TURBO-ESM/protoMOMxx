#include "MOM.h"
#include "MOM_get_input.h"
#include <iostream>

MOM::MOM(
    int ensemble_num
) {

  // Read paths and filenames from namelist and store in "dirs".
  auto dirs = MOM_get_input::get_directories();
  
  // Instantiate RuntimeParams by reading the parameter files specified in the namelist.
  params = std::make_unique<RuntimeParams>(MOM_get_input::get_parameter_filenames());

  bool verbosity = false;
  params->get("VERBOSITY", verbosity, {
        .default_value = 2,
        .desc = "Integer controlling level of messaging\n"
                "\t0 = Only FATAL messages\n"
                "\t2 = Only FATAL, WARNING, NOTE [default]\n"
                "\t9 = All",
        .units = "",
        .fail_if_missing = false
  });
  std::cout << "MOM initialized with verbosity level: " << verbosity_ << std::endl;

  bool split = false;
  params->get("SPLIT", split, {
        .default_value = true,
        .desc =  "Use the split time stepping if true."
  });

  bool split_rk4 = false;
  params->get("SPLIT_RK4", split_rk4, {
        .default_value = false,
        .desc =  "If true, use a version of the split explicit time stepping scheme that "
                 "exchanges velocities with step_MOM that have the average barotropic phase over "
                 "a baroclinic timestep rather than the instantaneous barotropic phase.",
        .do_not_log = !split
  });

    

}
