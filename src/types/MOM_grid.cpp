#include <string_view>

#include "MOM_grid.h"

#include "MOM_logger.h"

namespace MOM {

namespace {

// Check that a grid field is created.
void check_created(const amrex::MultiFab &field, const std::string_view name) {
  if (field.empty()) {
    logger::fatal("Grid: the ", name, " field is not created.");
  }
}

// Check that a positive-definite extent scalar is set.
void check_positive(const amrex::Real value, const std::string_view name) {
  if (!(value > 0.0)) {
    logger::fatal("Grid: ", name, " must be positive (got ", value, ").");
  }
}

} // namespace

Grid::Grid(GridFields &&fields)
  : fields_(std::move(fields)) {

  check_positive(fields_.len_lat, "len_lat");
  check_positive(fields_.len_lon, "len_lon");
  check_positive(fields_.rad_earth, "rad_earth");

  check_created(fields_.geoLatT, "geoLatT");
  check_created(fields_.geoLonT, "geoLonT");
  check_created(fields_.dxT, "dxT");
  check_created(fields_.dyT, "dyT");
  check_created(fields_.areaT, "areaT");

  check_created(fields_.geoLatCu, "geoLatCu");
  check_created(fields_.geoLonCu, "geoLonCu");
  check_created(fields_.dxCu, "dxCu");
  check_created(fields_.dyCu, "dyCu");

  check_created(fields_.geoLatCv, "geoLatCv");
  check_created(fields_.geoLonCv, "geoLonCv");
  check_created(fields_.dxCv, "dxCv");
  check_created(fields_.dyCv, "dyCv");

  check_created(fields_.geoLatBu, "geoLatBu");
  check_created(fields_.geoLonBu, "geoLonBu");
  check_created(fields_.dxBu, "dxBu");
  check_created(fields_.dyBu, "dyBu");
  check_created(fields_.areaBu, "areaBu");

  check_created(fields_.CoriolisBu, "CoriolisBu");
}

} // namespace MOM
