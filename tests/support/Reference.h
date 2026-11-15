#pragma once

#include <acf/Autocorrelation.h>
#include <limits>
#include <stdexcept>

namespace acf::test {
// Independent batch oracle; input must be finite and normalized.
inline double reference(std::span<double const> samples, std::size_t index, std::size_t frequency)
{
	auto const p = parameters(frequency);
	if (index >= samples.size())
		throw std::out_of_range("sample index is not available");
	if (index < p.oldest)
		return std::numeric_limits<double>::quiet_NaN();
	// Long-double accumulation is an independent oracle, not the production update.
	long double sum = 0;
	for (std::size_t k = 0; k < p.window + (p.tail != 0); ++k)
	{
		auto const j = index - k;
		long double delayed = samples[j - p.delay];
		if (p.fraction != 0)
			delayed += p.fraction * (static_cast<long double>(samples[j - p.delay - 1]) - delayed);
		auto const weight = k == p.window ? p.tail : 1.0;
		sum += weight * static_cast<long double>(samples[j]) * delayed;
	}
	return static_cast<double>(sum / SampleRate);
}

}
