#include <iostream>

#include <parser.hpp>
#include <vector>
#include <iterator>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <functional>
#include <numeric>

#include "airports.hpp"
#include <algorithm/floating_average.hpp>
#include "thermal.hpp"

struct date_time {

	units::time::second_t time;

	date_time(units::time::second_t t) :
		time(t)
	{}

};

std::ostream& operator<<(std::ostream& lhs, date_time rhs) {
	auto hour = units::time::hour_t(rhs.time).to<int>();
	rhs.time -= units::time::hour_t(hour);
	auto minute = units::time::minute_t(rhs.time).to<int>();
	rhs.time -= units::time::minute_t(minute);
	auto second = rhs.time.to<int>();

	lhs << hour << ":" << minute << ":" << second;
	return lhs;
}

template <class iterator_t>
std::ostream& operator<<(std::ostream& lhs, thermal_t<iterator_t> rhs) {
	lhs
		<< "Von " << date_time(rhs.begin->time)
		<< " bis " << date_time(rhs.end->time)
		<< " mit " << rhs.gain_te
		<< " und " << rhs.average
		<< " eribt " << rhs.points << " Punkte";
	return lhs;
}

using parser::attributes;
struct sample_t : public parser::sample_t<
	attributes::time,
	attributes::position,
	attributes::altitude
> {
	units::angle::degree_t gps_track;
	units::velocity::kilometers_per_hour_t ground_speed;
	units::angular_velocity::degrees_per_second_t angularspeed;
	units::angular_velocity::degrees_per_second_t floating_average_angularspeed;
};

int main(int argc, char **argv) {

	if(argc <= 1) {
		std::cerr << "No IGC file specified. Use with ./thermik_challenge <PATH_TO_IGC>" << std::endl;
		return 1;
	}

	std::vector<sample_t> samples;
	std::ifstream file(argv[1]);
	if(!file.is_open()) {
		std::cerr << "Could not open: " << argv[0] << "/" << argv[1] << std::endl;
		return 1;
	} else {
		std::cout << "Reading from " << argv[0] << "/" << argv[1] << std::endl;
	}
	auto header = parser::parse(file, std::back_inserter(samples));

	// First pass
	for(size_t i = 1; i < samples.size()-1;++i) {
		samples[i].gps_track = units::forward_azimuth(samples[i].position, samples[i+1].position);
		auto normalize =[](units::angle::degree_t a)->units::angle::degree_t {
			a += units::angle::degree_t(360);
			a = units::math::fmod(a, units::angle::degree_t(360));
			if(a > units::angle::degree_t(180)) a -= units::angle::degree_t(360);
			return a;
		};
		samples[i].angularspeed = normalize(samples[i-1].gps_track - samples[i].gps_track) / (samples[i].time - samples[i-1].time);
		samples[i].ground_speed =
			(
				units::distance(samples[i-1].position, samples[i].position) +
				units::distance(samples[i].position, samples[i+1].position)
			) / (
				samples[i+1].time -
				samples[i-1].time
			);
	}

	// Second pass
	floating_average<17>(
		samples.begin(),
		samples.end(),
		&sample_t::angularspeed,
		&sample_t::floating_average_angularspeed
	);

	// Third pass
	std::vector<thermal_t<decltype(samples)::iterator>> thermals;
	for(auto it = samples.begin(); it!=samples.end(); ++it) {
		if(
			units::math::abs(
				it->floating_average_angularspeed
			) >= units::angular_velocity::degrees_per_second_t(6)
		) {
			auto begin = it;
			for(
				;
				it != samples.end() &&
				units::math::abs(it->floating_average_angularspeed) >= units::angular_velocity::degrees_per_second_t(6);
				++it
			) {}
			thermal_t<decltype(it)> thermal(begin, it);

			if(thermal.points > 0) {
				thermals.push_back(std::move(thermal));
			}
		}
	}

	//Fourth pass
	if(thermals.size() > 0) {
		for(auto thermal_it = thermals.begin(); thermal_it+1 != thermals.end(); ) {
			auto next_it = thermal_it+1;
			if(next_it->begin->time - thermal_it->end->time <= units::time::second_t(12)) {
				thermal_t merged(thermal_it->begin, next_it->end);
				*thermal_it = merged;
				thermals.erase(next_it);
			} else {
				++thermal_it;
			}
		}
	}
	// Fith pass
	for(auto& t : thermals) {
		optimize(t);
	}

	auto start_airport = std::accumulate(
		airports.begin(),
		airports.end(),
		airports.front(),
		[&](airport_t lhs, airport_t rhs) {
			if(
				units::distance(
					samples.front().position,
					lhs.position
				) <
				units::distance(
					samples.front().position,
					rhs.position
				)
			) {
				return lhs;
			} else {
				return rhs;
			}
		}
	);

	std::cout << "Flight from " << header.date << " with " << header.glider_type << " " << header.glider_id << std::endl;
	std::cout << "Tookoff on " << start_airport.name << std::endl;

// 	for(auto& t : thermals) {
// 		std::cout << t << std::endl;
// 	}

	std::vector<thermal_t<decltype(samples)::iterator>> local_thermals;
	std::copy_if(
		thermals.begin(),
		thermals.end(),
		std::back_inserter(local_thermals),
		[&](auto& t) -> bool {
			return is_local(t, start_airport);
		}
	);



	std::vector<thermal_t<decltype(samples)::iterator>> remote_thermals;
	std::copy_if(
		thermals.begin(),
		thermals.end(),
		std::back_inserter(remote_thermals),
		[&](auto& t) -> bool {
			return is_remote(t, start_airport);
		}
	);

	std::cout << "Lokale Wertung:" << std::endl;
	if(local_thermals.size() > 0) {
		auto strongest_local = std::accumulate(
			local_thermals.begin(),
			local_thermals.end(),
			local_thermals.front(),
			[](const auto& t1, const auto& t2){
				if(t1.points > t2.points) {
					return t1;
				} else {
					return t2;
				}
			}
		);


		std::cout << "Stärkster Bart:" << strongest_local << std::endl;

		auto local_max_hour = find_max_hour(local_thermals.begin(), local_thermals.end());
		std::cout
			<< "60min akkumuliert: " << local_max_hour.points
			<< " Punkte, Wertung von " << date_time(local_max_hour.begin->begin->time)
			<< " bis " << date_time(local_max_hour.end->end->time) << std::endl;

	}
	std::cout << "Überland Wertung:" << std::endl;
	if(remote_thermals.size() > 0) {
		auto strongest_remote = std::accumulate(
			remote_thermals.begin(),
			remote_thermals.end(),
			remote_thermals.front(),
			[](const auto& t1, const auto& t2){
				if(t1.points > t2.points) {
					return t1;
				} else {
					return t2;
				}
			}
		);

		std::cout << "Stärkster Bart:" << strongest_remote << std::endl;

		auto remote_max_hour = find_max_hour(remote_thermals.begin(), remote_thermals.end());
		std::cout
			<< "60min akkumuliert: " << remote_max_hour.points
			<< " Punkte, Wertung von " << date_time(remote_max_hour.begin->begin->time)
			<< " bis " << date_time(remote_max_hour.end->end->time) << std::endl;
	}

	return 0;
}
