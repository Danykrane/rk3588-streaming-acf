#include <acf/Autocorrelation.h>

#include <charconv>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

template<class T> T parse(std::string const & text)
{
	T value {};
	auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc() || end != text.data() + text.size())
		throw std::invalid_argument("invalid input token: " + text);
	return value;
}

int main(int argc, char ** argv)
{
	try
	{
		bool integerInput = false;
		auto method = acf::Method::Direct;
		for (int i = 1; i < argc; ++i)
		{
			std::string_view arg(argv[i]);
			if (arg == "--int64")
				integerInput = true;
			else if (arg == "--recursive")
				method = acf::Method::Recursive;
			else if (arg == "--help")
			{
				std::cout << "Usage: acf_cli [--int64] [--recursive]\n"
							 "Read whitespace-separated samples from stdin; write 500-column CSV.\n"
							 "Default input: normalized finite doubles in [-1,1].\n"
							 "--int64: signed integers, normalized by 2^63. Output stays normalized.\n"
							 "Rows are chronological; unavailable values are nan.\n";
				return 0;
			}
			else
				throw std::invalid_argument("unknown option: " + std::string(arg));
		}
		acf::Autocorrelation processor(method);
		std::cout << std::setprecision(17);
		std::string token;
		while (std::cin >> token)
		{
			auto const value = integerInput ? acf::normalize(parse<std::int64_t>(token)) : parse<double>(token);
			auto const row = processor.push(value);
			for (std::size_t c = 0; c < row.size(); ++c)
				std::cout << (c == 0 ? "" : ",") << row[c];
			std::cout << '\n';
			if (!std::cout)
				throw std::runtime_error("output write failed");
		}
		if (!std::cin.eof())
			throw std::runtime_error("input read failed");
		std::cout.flush();
		if (!std::cout)
			throw std::runtime_error("output flush failed");
		return 0;
	}
	catch (std::exception const & error)
	{
		std::cerr << "acf: " << error.what() << '\n';
		return 1;
	}
}
