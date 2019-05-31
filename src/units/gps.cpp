#include "units/gps.hpp"

#include <cmath>

namespace units {
gps_position::gps_position()
{

}

double init(const std::string& str) {
	char dir = *str.rbegin();
	double sign = (dir == 'N' || dir == 'E') ? 1 : -1;
	std::uint8_t lat_size = (dir == 'E' || dir == 'W') ? 3 : 2;
	double degrees = std::stod(str.substr(0,lat_size));
	double minutes = std::stod(str.substr(lat_size, 5)) / 1000;
	return sign * (degrees + minutes / 60);
}

gps_position::gps_position(double latitude, double longitude) :
	latitude(latitude),
	longitude(longitude)
{
}

gps_position::gps_position(const std::string& latitude, const std::string& longitude) :
	latitude(init(latitude)),
	longitude(init(longitude))
{

}

gps_position::gps_position(double lat_deg, double lat_min, char n, double lon_deg, double lon_min, char e) :
    latitude(lat_deg+lat_min/60),
    longitude(lon_deg+lon_min/60)
{
    if(n=='S') latitude *= -1;
    if(e=='W') longitude *= -1;
}

std::ostream& operator<<(std::ostream& lhs, const gps_position& rhs) {
	lhs
		<< std::abs(rhs.latitude) << (rhs.latitude >= 0 ? 'N' : 'S')
		<< std::abs(rhs.longitude) << (rhs.longitude >= 0 ? 'E' : 'W');
	return lhs;
}

constexpr double rad(double deg) {
	return deg * M_PI / 180;
}

constexpr double deg(double rad) {
	return rad * 180 / M_PI;
}

units::length::meter_t distance(const gps_position& pos1, const gps_position& pos2) {
	auto R = units::length::kilometer_t(6371.0);
	auto phi1 = rad(pos1.latitude);
	auto phi2 = rad(pos2.latitude);
	auto dphi = rad(pos2.latitude-pos1.latitude);
	auto dlambda = rad(pos2.longitude-pos1.longitude);

	auto a = std::sin(dphi/2) * std::sin(dphi/2) +
			std::cos(phi1) * std::cos(phi2) *
			std::sin(dlambda/2) * std::sin(dlambda/2);

	auto c = 2 * std::atan2(std::sqrt(a), std::sqrt(1-a));

	return c * R;
}

units::angle::degree_t forward_azimuth(const gps_position& pos1, const gps_position& pos2) {

	const auto phi1 = rad(pos1.latitude);
	const auto lam1 = rad(pos1.longitude);
	const auto phi2 = rad(pos2.latitude);
	const auto lam2 = rad(pos2.longitude);

	auto y = std::sin(lam2-lam1) * std::cos(phi2);
	auto x = std::cos(phi1)*std::sin(phi2) -
			std::sin(phi1)*std::cos(phi2)*std::cos(lam2-lam1);
	auto brng = deg(std::atan2(y, x));

	return units::angle::degree_t(std::fmod(brng+360, 360));
}


}
