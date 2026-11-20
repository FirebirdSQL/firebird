#ifndef CLASSES_BENCH_ALLOC_BENCH_H
#define CLASSES_BENCH_ALLOC_BENCH_H

#include <string>
#include <cstddef>
#include <variant>

namespace Firebird {
	struct SequenceStrategy {};
	struct MixedStrategy
	{
		double dealloc_chance = 0.0;

		size_t min_dealloc_count = 0;
		size_t max_dealloc_count = 0;
	};

	struct BenchParameters
	{
		std::variant<SequenceStrategy, MixedStrategy> bench_strategy;

		size_t seed = 0;

		size_t min_alloc_size = 0;
		size_t max_alloc_size = 0;
		// For normal distribution.
		double mean = 0.5; // 0.5 - the bell in middle, 0.75 - right, 0.25 - left.
		double deviation = 0.5; // How bell is flatten, 0.5 - base, 0.25 - narrow, 0.75 - wide.

		size_t allocs_count = 0;

		size_t iterations = 0;
		size_t warmup_allocs = 0;

		size_t threads = 1;
		bool pool_per_thread = false;
	};

	std::string alloc_benchmark(const BenchParameters& params);
}

#endif // CLASSES_BENCH_ALLOC_BENCH_H
