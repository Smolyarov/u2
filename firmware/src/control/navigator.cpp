#include "main.h"

#include "navigator.hpp"
#include "param_registry.hpp"

namespace control {

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */

/*
 ******************************************************************************
 * PROTOTYPES
 ******************************************************************************
 */

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */

/*
 ******************************************************************************
 ******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************
 ******************************************************************************
 */

/*
 ******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************
 */

/**
 *
 */
Navigator::Navigator(void) {
  return;
}

/**
 *
 */
NavOut<double> Navigator::update(const NavIn<double> &in) {

  osalDbgCheck(ready);

  auto crosstrack = sphere.crosstrack(in.lat, in.lon);
  auto crs_dist = sphere.course_distance(in.lat, in.lon);

  return NavOut<double>(crosstrack.xtd, crosstrack.atd, crs_dist.dist, crs_dist.crs);
}

/**
 *
 */
void Navigator::loadLine(const NavLine<double> &line) {
  ready = true;
  sphere.updatePoints(line.latA, line.lonA, line.latB, line.lonB);
}

/**
 *
 */
void Navigator::stop(void) {
  ready = false;
}

} /* namespace */
