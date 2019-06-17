#pragma once

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

