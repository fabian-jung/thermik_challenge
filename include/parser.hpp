#pragma once

#include <istream>
#include <map>
#include <vector>
#include <units.h>
#include <units/gps.hpp>

namespace parser {

	template <class inserter_t>
	void parse(std::istream& input, inserter_t inserter) {
		std::string line;
		std::map<std::string, std::pair<std::uint8_t, std::uint8_t>> position;

		position["hour"] = std::make_pair(1, 2);
		position["minute"] = std::make_pair(3, 2);
		position["second"] = std::make_pair(5, 2);
		position["latitude"] = std::make_pair(7, 8);
		position["longitude"] = std::make_pair(15, 9);
		position["altitude"] = std::make_pair(25, 5);

		using value_type = typename inserter_t::container_type::value_type;

		std::vector<std::map<std::string, std::string>> values;
		while( std::getline(input, line) ) {

			if(line[0] == 'I') {
				for(unsigned int i=3; i + 7 < line.size(); i += 7) {
					auto param = line.substr(i, 7);
					std::uint8_t begin = std::stoi( param.substr(0, 2) );
					std::uint8_t end = std::stoi( param.substr(2, 2) );

					auto code = param.substr(4, 3);

					position[code] = std::make_pair(begin - 1, end - begin + 1);
				}
			}

			if(line[0] == 'B') {
				std::map<std::string, std::string> sample_map;
				for(const auto& p : position) {
					sample_map[p.first] = line.substr(p.second.first, p.second.second);
				}
				values.emplace_back(std::move(sample_map));
			}
		}


		for(auto& sample_map : values) {
			value_type s;
			s.time =
				units::time::hour_t(std::stoi(sample_map["hour"])) +
				units::time::minute_t(std::stoi(sample_map["minute"]))+
				units::time::second_t(std::stoi(sample_map["second"]));

			s.position = units::gps_position(sample_map["latitude"], sample_map["longitude"]);
			s.altitude = units::length::meter_t(std::stod(sample_map["altitude"]));
			s.fix_accuracy = units::length::meter_t(std::stod(sample_map["FXA"]));
			s.true_air_speed = units::velocity::kilometers_per_hour_t(std::stod(sample_map["TAS"]) / 100);
			s.ground_speed = units::velocity::kilometers_per_hour_t(std::stod(sample_map["GSP"]) / 100);
			s.total_energy_vario = units::velocity::meters_per_second_t(std::stod(sample_map["VAT"]) / 100);
			s.true_heading = units::angle::degree_t(std::stod(sample_map["HDT"]));
			s.true_track = units::angle::degree_t(std::stod(sample_map["TRT"]));
			s.oat = units::temperature::celsius_t(std::stod(sample_map["OAT"]) / 10);
			s.gload = units::acceleration::standard_gravity_t(std::stod(sample_map["ACZ"]) / 100);

			inserter = std::move(s);
		}

	}

}
