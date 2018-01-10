#ifndef IRC_MATH_H
#define IRC_MATH_H

#include "constants.h"

namespace irc {

namespace tools {

namespace math {

/// Returns \param angle in the range \f$(-\pi,\pi]\f$
///
/// \param angle
/// \return
double pirange_rad(double angle);

/// Returns \param angle in the range \f$(-180,180]\f$
///
/// \param angle
/// \return
double pirange_deg(double angle);

double deg_to_rad(double angle);

double rad_to_deg(double angle);

} // namespace math

} // namespace tools

} // namespace irc

#endif //IRC_MATH_H