#pragma once

#include "airports.hpp"

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
			gain /*+
			((
				units::math::pow<2>(end->ground_speed) -
				units::math::pow<2>(begin->ground_speed)
			) /
				units::acceleration::standard_gravity_t(2))*/
		),
		average(gain_te/ (end->time - begin->time)),
		points(std::max(gain_te.to<double>(),0.0) * average.to<double>())
	{
	}
};

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

template <class thermal_it>
auto find_max_hour(const thermal_it begin, const thermal_it end) {

	struct {
		double points;
		thermal_it begin;
		thermal_it end;
	} max;

	max.begin = begin;
	max.end = begin;

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
