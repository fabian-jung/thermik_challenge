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

struct sample_t {
	units::time::second_t time;
	units::gps_position position;
	units::length::meter_t altitude;
	units::length::meter_t fix_accuracy;
	units::velocity::kilometers_per_hour_t true_air_speed;
	units::velocity::kilometers_per_hour_t ground_speed;
	units::velocity::meters_per_second_t total_energy_vario;
	units::angle::degree_t true_heading;
	units::angle::degree_t true_track;
	units::temperature::celsius_t oat;
	units::acceleration::standard_gravity_t gload;
	units::angle::degree_t gps_track;
	units::angular_velocity::degrees_per_second_t angularspeed;
	units::angular_velocity::degrees_per_second_t floating_average_angularspeed;
};

template <class iterator_t>
struct thermal_t {
	iterator_t begin;
	iterator_t end;
	units::length::meter_t gain;
	units::length::meter_t gain_te;
	units::velocity::meters_per_second_t average;
	double points;

	thermal_t(
		iterator_t begin,
		iterator_t end
	) :
		begin(begin),
		end(end),
		gain(end->altitude - begin->altitude),
		gain_te(
			gain +
			((
				units::math::pow<2>(end->true_air_speed) -
				units::math::pow<2>(begin->true_air_speed)
			) /
				units::acceleration::standard_gravity_t(2))
		),
		average(gain_te/ (end->time - begin->time)),
		points(std::max(gain_te.to<double>(),0.0) * average.to<double>())
	{
	}
};

template <size_t N, class forward_it, class T>
void floating_average(const forward_it begin, const forward_it end, T forward_it::value_type::*attribute, T forward_it::value_type::*result) {
	auto rolling_sum = T{0};
	auto window_end = begin;
	for(size_t i = 0; i < N; ++i) {
		rolling_sum = rolling_sum + (*window_end).*attribute;
		++window_end;
	}
	auto window_middle = begin;
	for(size_t i = 0; i < N/2; ++i) {
		++window_middle;
	}
	auto window_begin = begin;
	while(window_end != end) {
		(*window_middle).*result = rolling_sum/N;
		rolling_sum = rolling_sum - (*window_begin).*attribute;
		rolling_sum = rolling_sum + (*window_end).*attribute;
		++window_middle;
		++window_begin;
		++window_end;
	}
}

template <class iterator_t>
void optimize(thermal_t<iterator_t>& thermal) {
	thermal_t<iterator_t> max = thermal;

	for(auto it1 = thermal.begin; it1 != thermal.end; ++it1) {
		for(auto it2 = it1+1; it2 != thermal.end; ++it2) {
			thermal_t<iterator_t> t(it1, it2);
			if(t.points > max.points) max = t;
		}
	}

	thermal = max;
}

template<class iterator_t>
bool is_local(thermal_t<iterator_t> thermal, const airport_t& ap) {
	return std::any_of(
		thermal.begin,
		thermal.end,
		[&](const auto& sample){
			return units::distance(sample.position, ap.position) > units::length::kilometer_t(10);
		}
	);
}

template <class iterator_t>
bool is_remote(thermal_t<iterator_t> thermal, const airport_t& ap) {
	return (thermal.begin->altitude*40) > (units::distance(thermal.begin->position, ap.position));
}

template <class thermal_it>
auto find_max_hour(const thermal_it begin, const thermal_it end) {

	struct {
		double points;
		thermal_it begin;
		thermal_it end;
	} max;

	for(auto it1 = begin; it1 != end; ++it1) {
		for(auto it2 = it1+1; it2 != end; ++it2) {
			if((it2->end->time - it1->begin->time) <= units::time::hour_t(1)) {
				double total_points = 0;
				for(auto it3 = it1; it3 != it2; ++it3) {
					total_points += it3->points;
				}
				total_points += it2->points;
				if(total_points > max.points) {
					max.points = total_points;
					max.begin = it1;
					max.end = it2;
				}
			}
		}
	}

	return max;
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

int main(int argc, char **argv) {

	if(argc <= 1) {
		std::cerr << "Keine Datein angegeben." << std::endl;
		return 1;
	}

	std::vector<sample_t> samples;
	std::ifstream file(argv[1]);
	parser::parse(file, std::back_inserter(samples));

	// First pass
	for(size_t i = 0; i < samples.size()-1;++i) {
		samples[i].gps_track = units::forward_azimuth(samples[i].position, samples[i+1].position);
		if(i>0) {
			auto normalize =[](units::angle::degree_t a)->units::angle::degree_t {
				a += units::angle::degree_t(360);
				a = units::math::fmod(a, units::angle::degree_t(360));
				if(a > units::angle::degree_t(180)) a -= units::angle::degree_t(360);
				return a;
			};
			samples[i].angularspeed = normalize(samples[i-1].gps_track - samples[i].gps_track) / (samples[i].time - samples[i-1].time);
		}
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
	for(auto thermal_it = thermals.begin(); thermal_it+1 != thermals.end(); ) {
		auto next_it = thermal_it +1;
		if(next_it->begin->time - thermal_it->end->time <= units::time::second_t(12)) {
			thermal_t merged(thermal_it->begin, next_it->end);
			*thermal_it = merged;
			thermals.erase(next_it);
		} else {
			++thermal_it;
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

	std::cout << "Took of on " << start_airport.name << std::endl;

	for(auto& t : thermals) {
		std::cout << t << std::endl;
	}

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

	std::cout << "Überland Wertung:" << std::endl;
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

	return 0;
}
