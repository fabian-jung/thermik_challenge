#pragma once

#include <ostream>
#include <units.h>

namespace units {

class gps_position {

	// Latitude and longitude in °, negative values indicating S/W;
	double latitude, longitude;
	friend std::ostream& operator<<(std::ostream& lhs, const gps_position& rhs);
	friend units::length::meter_t distance(const gps_position& pos1, const gps_position& pos2);
	friend units::angle::degree_t forward_azimuth(const gps_position& pos1, const gps_position& pos2);

public:

	gps_position();
	gps_position(double latitude, double longitude);
	gps_position(const std::string& latitude, const std::string& longitude);
	gps_position(double lat_deg, double lat_min, char n, double lon_deg, double lon_min, char e);

};

std::ostream& operator<<(std::ostream& lhs, const gps_position& rhs);

units::length::meter_t distance(const gps_position& pos1, const gps_position& pos2);
units::angle::degree_t forward_azimuth(const gps_position& pos1, const gps_position& pos2);
}
