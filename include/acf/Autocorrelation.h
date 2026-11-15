#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace acf {
inline constexpr std::size_t SampleRate = 2000;
inline constexpr std::size_t FrequencyCount = 500;
inline constexpr std::size_t HistorySize = 4096;
inline constexpr std::size_t RowCount = 2000;
using Row = std::array<double, FrequencyCount>;

enum class Method
{
	Direct,
	Recursive
};

struct Parameters
{
	std::size_t window;
	std::size_t delay;
	double tail;
	double fraction;
	std::size_t oldest;
};

Parameters parameters(std::size_t frequency);
// Binary64 reference over normalized samples; unavailable history returns NaN.
double reference(std::span<double const> samples, std::size_t index, std::size_t frequency);
// Scales the complete signed integer range; does not promise bit-exact results.
double normalize(std::int64_t sample) noexcept;

class Autocorrelation
{
public:
	explicit Autocorrelation(Method method = Method::Direct, std::size_t rebaseEvery = 2048);
	// No allocation after construction. Rejects nonfinite values and |value| > 1.
	// Returned view is overwritten after RowCount pushes or reset(). Not thread-safe.
	std::span<double const, FrequencyCount> push(double value);
	std::span<double const, FrequencyCount> row(std::size_t age = 0) const;
	void reset() noexcept;
	std::size_t availableRows() const noexcept { return m_availableRows; }

private:
	double sample(std::size_t age) const noexcept;
	double product(std::size_t age, Parameters const & p) const noexcept;
	Method m_method;
	std::size_t m_rebaseEvery;
	std::size_t m_sinceRebase = 0;
	std::size_t m_head = HistorySize - 1;
	std::size_t m_available = 0;
	std::size_t m_latestRow = RowCount - 1;
	std::size_t m_availableRows = 0;
	std::array<double, HistorySize> m_history {};
	std::array<Parameters, FrequencyCount> m_parameters {};
	Row m_sums {};
	std::vector<Row> m_matrix;
};
}
