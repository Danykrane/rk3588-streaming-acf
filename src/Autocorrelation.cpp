#include <acf/Autocorrelation.h>

#include <range/v3/view/iota.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace acf {
namespace {
constexpr double Invalid = std::numeric_limits<double>::quiet_NaN();
}

Parameters parameters(std::size_t frequency)
{
	if (frequency == 0 || frequency > FrequencyCount)
		throw std::invalid_argument("frequency must be in [1, 500]");
	auto const n = SampleRate / frequency;
	auto const q = (SampleRate / 4) / frequency;
	auto const r = SampleRate % frequency;
	auto const a = (SampleRate / 4) % frequency;
	return { n, q, static_cast<double>(r) / frequency, static_cast<double>(a) / frequency,
		n - (r == 0) + q + (a != 0) };
}

double normalize(std::int64_t sample) noexcept
{
	return std::ldexp(static_cast<double>(sample), -63);
}

double reference(std::span<double const> samples, std::size_t index, std::size_t frequency)
{
	auto const p = parameters(frequency);
	if (index >= samples.size())
		throw std::out_of_range("sample index is not available");
	if (index < p.oldest)
		return Invalid;
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

Autocorrelation::Autocorrelation(Method method, std::size_t rebaseEvery)
	: m_method(method)
	, m_rebaseEvery(rebaseEvery)
	, m_matrix(RowCount)
{
	if (rebaseEvery == 0)
		throw std::invalid_argument("rebase interval must be positive");
	if (method != Method::Direct && method != Method::Recursive)
		throw std::invalid_argument("unknown computation method");
	for (auto c : ranges::views::iota(std::size_t { 0 }, FrequencyCount))
		m_parameters[c] = parameters(c + 1);
	reset();
}

void Autocorrelation::reset() noexcept
{
	m_history.fill(0);
	m_sums.fill(0);
	for (auto & row : m_matrix)
		row.fill(Invalid);
	m_head = HistorySize - 1;
	m_latestRow = RowCount - 1;
	m_available = m_availableRows = m_sinceRebase = 0;
}

double Autocorrelation::sample(std::size_t age) const noexcept
{
	if (age >= m_available)
		return 0;
	return m_history[(m_head + HistorySize - age) % HistorySize];
}

double Autocorrelation::product(std::size_t age, Parameters const & p) const noexcept
{
	double delayed = sample(age + p.delay);
	if (p.fraction != 0)
		delayed = (1 - p.fraction) * delayed + p.fraction * sample(age + p.delay + 1);
	return sample(age) * delayed;
}

std::span<double const, FrequencyCount> Autocorrelation::push(double value)
{
	if (!std::isfinite(value) || std::abs(value) > 1)
		throw std::invalid_argument("sample must be finite and normalized to [-1, 1]");
	m_head = (m_head + 1) % HistorySize;
	m_history[m_head] = value;
	m_available = std::min(m_available + 1, HistorySize);
	m_latestRow = (m_latestRow + 1) % RowCount;
	m_availableRows = std::min(m_availableRows + 1, RowCount);
	auto & output = m_matrix[m_latestRow];
	for (auto c : ranges::views::iota(std::size_t { 0 }, FrequencyCount))
	{
		auto const & p = m_parameters[c];
		auto const old = product(p.window, p);
		if (m_method == Method::Direct || m_sinceRebase == 0)
		{
			double sum = 0;
			for (std::size_t k = 0; k < p.window; ++k)
				sum += product(k, p);
			m_sums[c] = sum;
		}
		else
			m_sums[c] += product(0, p) - old;
		output[c] = m_available > p.oldest ? (m_sums[c] + p.tail * old) / SampleRate : Invalid;
	}
	// Bounded counters permit indefinite operation without a sample-index overflow.
	m_sinceRebase = m_sinceRebase == m_rebaseEvery - 1 ? 0 : m_sinceRebase + 1;
	return output;
}

std::span<double const, FrequencyCount> Autocorrelation::row(std::size_t age) const
{
	if (age >= m_availableRows)
		throw std::out_of_range("row is not available");
	return m_matrix[(m_latestRow + RowCount - age) % RowCount];
}
}
