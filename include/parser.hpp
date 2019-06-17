#pragma once

#include <istream>
#include <map>
#include <vector>
#include <algorithm>
#include <units.h>
#include <units/gps.hpp>

namespace parser {

	enum class attributes {
		time,
		position,
		altitude,
		fix_accuracy,
		true_air_speed,
		ground_speed,
		total_energy_vario,
		true_heading,
		true_track,
		oat,
		gload
	};

namespace detail {
	template <attributes value, attributes... list>
	struct contains;

	template <attributes value, attributes... list>
	constexpr bool contains_v = contains<value, list...>::value;

	template <attributes _value>
	struct contains<_value> {
		constexpr static bool value = false;
	};

	template <attributes _value, attributes begin, attributes... list>
	struct contains<_value, begin, list...> {
		constexpr static bool value = (_value == begin) || contains_v<_value, list...>;
	};

}

	template <attributes attr>
	struct sample_base_t;

	template<>
	struct sample_base_t<attributes::time> {
		units::time::second_t time;
	};

	template<>
	struct sample_base_t<attributes::position> {
		units::gps_position position;
	};

	template<>
	struct sample_base_t<attributes::altitude> {
		units::length::meter_t altitude;
	};
	template<>
	struct sample_base_t<attributes::fix_accuracy> {
		units::length::meter_t fix_accuracy;
	};
	template<>
	struct sample_base_t<attributes::true_air_speed> {
		units::velocity::kilometers_per_hour_t true_air_speed;
	};
	template<>
	struct sample_base_t<attributes::ground_speed> {
		units::velocity::kilometers_per_hour_t ground_speed;
	};
	template<>
	struct sample_base_t<attributes::total_energy_vario> {
		units::velocity::meters_per_second_t total_energy_vario;
	};
	template<>
	struct sample_base_t<attributes::true_heading> {
		units::angle::degree_t true_heading;
	};
	template<>
	struct sample_base_t<attributes::true_track> {
		units::angle::degree_t true_track;
	};
	template<>
	struct sample_base_t<attributes::oat> {
		units::temperature::celsius_t oat;
	};
	template<>
	struct sample_base_t<attributes::gload> {
		units::acceleration::standard_gravity_t gload;
	};

	template <attributes... attr>
	struct sample_t : public sample_base_t<attr>... {
		template <attributes a>
		static constexpr bool is_active() {
			return detail::contains_v<a, attr...>;
		}
	};

	enum class take_off_t {
		whinch,
		aerotow,
		selflaunch
	};

	struct header_t {
// 		units::time::seconds_t logger_interval;
		std::string glider_type; //HFGTYGLIDERTYPE
		std::string glider_id; //HFGIDGLIDERID
		std::string pic; //HFPLTPILOTINCHARGE
		struct date_t { //HFDTEDATE 260319
			unsigned int year;
			unsigned int month;
			unsigned int day;
		} date;
	};

	std::ostream& operator<<(std::ostream& lhs, const header_t::date_t& rhs) {
		lhs << rhs.day << "." << rhs.month << "." << rhs.year;
		return lhs;
	}

	template <attributes... attributes, class inserter_t>
	header_t parse(std::istream& input, inserter_t inserter) {
		header_t header;
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
			if(line[0] == 'H') {
				auto split = std::find(line.begin(), line.end(), ':');
				if(split != line.end()) {
					auto key = std::string(line.begin(), split);
					auto value = std::string(split+1, line.end());
					if(key == "HFGTYGLIDERTYPE") {
						header.glider_type = value;
					}
					if(key == "HFGIDGLIDERID") {
						header.glider_id = value;
					}
					if(key == "HFPLTPILOTINCHARGE") {
						header.pic = value;
					}
					if(key == "HFDTEDATE") {
						header.date.day = std::stoi(value.substr(0,2));
						header.date.month = std::stoi(value.substr(2,2));
						header.date.year = std::stoi(value.substr(4,2));
					}
				} else {
					if(line.substr(0,5) == "HFDTE") {
						header.date.day = std::stoi(line.substr(5,2));
						header.date.month = std::stoi(line.substr(7,2));
						header.date.year = std::stoi(line.substr(9,2));
					}
				}
			}

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
			if constexpr(value_type::template is_active<attributes::time>()) {
				s.time =
					units::time::hour_t(std::stoi(sample_map["hour"])) +
					units::time::minute_t(std::stoi(sample_map["minute"])) +
					units::time::second_t(std::stoi(sample_map["second"]));
			}

			if constexpr(value_type::template is_active<attributes::position>()) {
				s.position = units::gps_position(sample_map["latitude"], sample_map["longitude"]);
			}

			if constexpr(value_type::template is_active<attributes::altitude>()) {
				s.altitude = units::length::meter_t(std::stod(sample_map["altitude"]));
			}

			if constexpr(value_type::template is_active<attributes::fix_accuracy>()) {
				s.fix_accuracy = units::length::meter_t(std::stod(sample_map["FXA"]));
			};

			if constexpr(value_type::template is_active<attributes::true_air_speed>()) {
				s.true_air_speed = units::velocity::kilometers_per_hour_t(std::stod(sample_map["TAS"]) / 100);
			};

			if constexpr(value_type::template is_active<attributes::ground_speed>()) {
				s.ground_speed = units::velocity::kilometers_per_hour_t(std::stod(sample_map["GSP"]) / 100);
			};

			if constexpr(value_type::template is_active<attributes::total_energy_vario>()) {
				s.total_energy_v = units::velocity::meters_per_second_t(std::stod(sample_map["VAT"]) / 100);
			};

			if constexpr(value_type::template is_active<attributes::true_heading>()) {
				s.true_heading = units::angle::degree_t(std::stod(sample_map["HDT"]));
			};

			if constexpr(value_type::template is_active<attributes::true_track>()) {
				s.true_track = units::angle::degree_t(std::stod(sample_map["TRT"]));
			};

			if constexpr(value_type::template is_active<attributes::oat>()) {
				s.oat = units::temperature::celsius_t(std::stod(sample_map["OAT"]) / 10);
			};

			if constexpr(value_type::template is_active<attributes::gload>()) {
				s.gload = units::acceleration::standard_gravity_t(std::stod(sample_map["ACZ"]) / 100);
			};

			inserter = std::move(s);

		}

		return header;
	}

}
