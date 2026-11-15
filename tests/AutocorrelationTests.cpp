#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <acf/Autocorrelation.h>
#include "support/Reference.h"
#include <type_traits>
#include <utility>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

TEST_CASE("Parameters preserve fractional windows and delays")
{
	CHECK(acf::parameters(1).oldest == 2499);
	CHECK(acf::parameters(500).oldest == 4);
	CHECK(acf::parameters(3).tail == doctest::Approx(2.0 / 3));
	CHECK(acf::parameters(3).fraction == doctest::Approx(2.0 / 3));
	CHECK(acf::parameters(499).window == 4);
	CHECK_THROWS_AS(acf::parameters(0), std::invalid_argument);
	CHECK_THROWS_AS(acf::parameters(501), std::invalid_argument);
}

TEST_CASE("Both methods match independent reference through repeated ring wraps")
{
	std::mt19937 generator(124);
	std::uniform_real_distribution<double> distribution(-1, 1);
	std::vector<double> samples(8300);
	for (auto & x : samples)
		x = distribution(generator);
	for (auto method : { acf::Method::Direct, acf::Method::Recursive })
	{
		acf::Autocorrelation processor(method);
		acf::Row saved {};
		for (std::size_t i = 0; i < samples.size(); ++i)
		{
			auto row = processor.push(samples[i]);
			if (i == 6300)
				std::copy(row.begin(), row.end(), saved.begin());
			if (i % 257 == 0 || i == 4 || i == 2498 || i == 2499 || i == 4095
				|| i == 4096 || i == 8191 || i == 8192 || i == 8299)
			{
				for (std::size_t f = 1; f <= acf::FrequencyCount; ++f)
				{
					CAPTURE(i);
					CAPTURE(f);
					double const expected = acf::test::reference(samples, i, f);
					if (std::isnan(expected))
						CHECK(std::isnan(row[f - 1]));
					else
						CHECK(std::abs(row[f - 1] - expected) < 2e-14);
				}
			}
		}
		CHECK(processor.availableRows() == acf::RowCount);
		auto oldest = processor.row(1999);
		CHECK(std::equal(saved.begin(), saved.end(), oldest.begin()));
		CHECK_THROWS_AS(processor.row(2000), std::out_of_range);
	}
}

TEST_CASE("Constants have the analytical integral C squared over f")
{
	for (auto method : { acf::Method::Direct, acf::Method::Recursive })
	{
		for (double x : { 0.0, 1.0, -1.0, 0.3 })
		{
			acf::Autocorrelation processor(method);
			for (std::size_t i = 0; i < 2500; ++i)
				processor.push(x);
			auto row = processor.row();
			for (std::size_t f = 1; f <= acf::FrequencyCount; ++f)
				CHECK(std::abs(row[f - 1] - x * x / f) < 2e-14);
		}
	}
}

TEST_CASE("Sinusoid and step agree with direct integration")
{
	std::vector<double> samples(4200);
	for (std::size_t i = 0; i < samples.size(); ++i)
		samples[i] = i < 2700 ? 0.7 * std::sin(2 * std::acos(-1.0) * 73 * i / 2000) : 0.3;
	acf::Autocorrelation processor(acf::Method::Recursive, 257);
	for (std::size_t i = 0; i < samples.size(); ++i)
	{
		auto row = processor.push(samples[i]);
		if (i == 2499 || i == 2699 || i == 2700 || i == 3000 || i == 4199)
			for (std::size_t f = 1; f <= acf::FrequencyCount; ++f)
				CHECK(std::abs(row[f - 1] - acf::test::reference(samples, i, f)) < 2e-14);
	}
}

TEST_CASE("Impulse has zero correlation at all positive delays")
{
	acf::Autocorrelation processor(acf::Method::Recursive);
	for (std::size_t i = 0; i < 2700; ++i)
	{
		auto row = processor.push(i == 1200 ? 1.0 : 0.0);
		for (std::size_t f = 1; f <= acf::FrequencyCount; ++f)
			if (i >= acf::parameters(f).oldest)
				CHECK(row[f - 1] == 0);
	}
}

TEST_CASE("Invalid samples do not mutate state and reset restores warmup")
{
	acf::Autocorrelation processor(acf::Method::Recursive, 1);
	CHECK_THROWS_AS(processor.row(), std::out_of_range);
	CHECK_THROWS_AS(acf::Autocorrelation(acf::Method::Direct, 0), std::invalid_argument);
	CHECK_THROWS_AS(acf::Autocorrelation(static_cast<acf::Method>(42)), std::invalid_argument);
	for (int i = 0; i < 2500; ++i)
		processor.push(1);
	acf::Row previous {};
	std::copy(processor.row().begin(), processor.row().end(), previous.begin());
	for (double x : { 1.01, -1.01, std::numeric_limits<double>::infinity(),
			 -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() })
	{
		CHECK_THROWS_AS(processor.push(x), std::invalid_argument);
		CHECK(std::equal(previous.begin(), previous.end(), processor.row().begin()));
	}
	processor.reset();
	CHECK(processor.availableRows() == 0);
	CHECK_THROWS_AS(processor.row(), std::out_of_range);
	auto row = processor.push(0);
	for (auto x : row)
		CHECK(std::isnan(x));
	for (int i = 1; i < 2500; ++i)
		processor.push(0);
	for (auto x : processor.row())
		CHECK(x == 0);
}

TEST_CASE("Signed 64 bit normalization is bounded and documents precision loss")
{
	CHECK(acf::normalize(std::numeric_limits<std::int64_t>::min()) == -1);
	CHECK(acf::normalize(std::numeric_limits<std::int64_t>::max()) == 1);
	CHECK(acf::normalize(0) == 0);
	CHECK(acf::normalize(1) == std::ldexp(1.0, -63));
	auto const max = std::numeric_limits<std::int64_t>::max();
	CHECK(acf::normalize(max) == acf::normalize(max - 1));
}

TEST_CASE("Reference validates indices and frequencies")
{
	std::vector<double> samples(10, 1);
	CHECK_THROWS_AS(acf::test::reference(samples, 10, 1), std::out_of_range);
	CHECK_THROWS_AS(acf::test::reference(samples, 0, 0), std::invalid_argument);
	CHECK(std::isnan(acf::test::reference(samples, 3, 500)));
	CHECK(acf::test::reference(samples, 4, 500) == doctest::Approx(0.002));
}

// Compile-time regression checks for the ownership contract.
static_assert(!std::is_copy_constructible_v<acf::Autocorrelation>);
static_assert(!std::is_copy_assignable_v<acf::Autocorrelation>);
static_assert(!std::is_move_constructible_v<acf::Autocorrelation>);
static_assert(!std::is_move_assignable_v<acf::Autocorrelation>);
template<class T>
concept TemporaryPush = requires(T && p) { std::move(p).push(0); };
template<class T>
concept TemporaryRow = requires(T && p) { std::move(p).row(); };
static_assert(!TemporaryPush<acf::Autocorrelation>);
static_assert(!TemporaryRow<acf::Autocorrelation>);

TEST_CASE("Borrowed row remains stable until its documented overwrite")
{
	acf::Autocorrelation processor(acf::Method::Recursive);
	for (int i = 0; i < 5; ++i)
		processor.push(1);
	auto const borrowed = processor.row();
	CHECK(borrowed[499] == doctest::Approx(0.002));
	for (std::size_t i = 0; i < acf::RowCount - 1; ++i)
		processor.push(0);
	CHECK(borrowed.data() == processor.row(acf::RowCount - 1).data());
	CHECK(borrowed[499] == doctest::Approx(0.002));
	processor.push(0);
	CHECK(borrowed.data() == processor.row().data());
	CHECK(borrowed[499] == 0);
	processor.reset();
	CHECK(std::isnan(borrowed[499]));
}
