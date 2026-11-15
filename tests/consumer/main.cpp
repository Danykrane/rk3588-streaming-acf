#include <acf/Autocorrelation.h>
#include <cmath>

int main()
{
	acf::Autocorrelation processor;
	for (int i = 0; i < 5; ++i)
		processor.push(1);
	return std::abs(processor.row()[499] - 0.002) < 1e-14 ? 0 : 1;
}
