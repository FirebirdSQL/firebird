#include "firebird.h"
#include "fb_exception.h"
#include "AllocBench.h"

#include <charconv>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
	void printHelp()
	{
		std::cout <<
			"Usage: alloc_bench [options]\n"
			"  --strategy mixed|sequence  Allocation strategy (default: mixed)\n"
			"  --seed N                   Seed value (default: 0)\n"
			"  --allocs N                 Allocations per thread per iteration (default: 100000)\n"
			"  --threads N                Worker threads (default: 1)\n"
			"  --pool-per-thread          Use a child pool per thread with a shared parent (default: off)\n"
			"  --iterations N             Measured iterations (default: 2)\n"
			"  --warmup N                 Warmup allocations (default: 0)\n"
			"  --min-size N               Minimum allocation size in bytes (default: 8)\n"
			"  --max-size N               Maximum allocation size in bytes (default: 1024)\n"
			"  --mean X                   Normalized allocation-size mean (default: 0.5)\n"
			"  --deviation X              Normalized deviation (default: 0.75)\n"
			"  --dealloc-chance X         Mixed-strategy free probability (default: 0.1)\n"
			"  --min-dealloc N            Minimum objects freed per burst (default: 1)\n"
			"  --max-dealloc N            Maximum objects freed per burst (default: 10)\n"
			"  --help                    Show this help\n"
			"\n"
			"Build: make alloc_bench [TARGET=Release|Debug]\n"
			"Run:   make run_alloc_bench BENCH_ARGS=\"--strategy mixed --allocs 100000\"\n"
			"Timings are nanoseconds; memory statistics are bytes. Large bursts can use GiB.\n";
	}

	size_t parseInt(const std::string& option, const char* value)
	{
		size_t result = 0;
		const auto end = value + std::strlen(value);
		const auto parsed = std::from_chars(value, end, result);
		if (parsed.ec != std::errc() || parsed.ptr != end)
			throw std::invalid_argument("Invalid unsigned integer for " + option + ": " + value);
		return result;
	}

	double parseDouble(const std::string& option, const char* value)
	{
		size_t consumed = 0;
		const std::string text(value);
		const double result = std::stod(text, &consumed);
		if (consumed != text.size() || !std::isfinite(result))
			throw std::invalid_argument("Invalid finite number for " + option + ": " + value);
		return result;
	}

	bool inUnitInterval(double value)
	{
		return value >= 0.0 && value <= 1.0;
	}
} // namespace

int main(int argc, char** argv)
{
	try
	{
		Firebird::BenchParameters params;
		params.min_alloc_size = 8;
		params.max_alloc_size = 1024;
		params.mean = 0.5;
		params.deviation = 0.75;
		params.allocs_count = 100000;
		params.iterations = 2;

		Firebird::MixedStrategy mixed;
		mixed.dealloc_chance = 0.1;
		mixed.min_dealloc_count = 1;
		mixed.max_dealloc_count = 10;
		std::string strategy = "mixed";

		for (int i = 1; i < argc; ++i)
		{
			const std::string option(argv[i]);
			if (option == "--help")
			{
				printHelp();
				return 0;
			}
			if (option == "--pool-per-thread")
			{
				params.pool_per_thread = true;
				continue;
			}

			if (++i == argc)
				throw std::invalid_argument("Missing value for " + option);

			const char* value = argv[i];

			if (option == "--strategy")
				strategy = value;
			else if (option == "--seed")
				params.seed = parseInt(option, value);
			else if (option == "--allocs")
				params.allocs_count = parseInt(option, value);
			else if (option == "--threads")
				params.threads = parseInt(option, value);
			else if (option == "--iterations")
				params.iterations = parseInt(option, value);
			else if (option == "--warmup")
				params.warmup_allocs = parseInt(option, value);
			else if (option == "--min-size")
				params.min_alloc_size = parseInt(option, value);
			else if (option == "--max-size")
				params.max_alloc_size = parseInt(option, value);
			else if (option == "--mean")
				params.mean = parseDouble(option, value);
			else if (option == "--deviation")
				params.deviation = parseDouble(option, value);
			else if (option == "--dealloc-chance")
				mixed.dealloc_chance = parseDouble(option, value);
			else if (option == "--min-dealloc")
				mixed.min_dealloc_count = parseInt(option, value);
			else if (option == "--max-dealloc")
				mixed.max_dealloc_count = parseInt(option, value);
			else
				throw std::invalid_argument("Unknown option: " + option);
		}

		if (!params.allocs_count || !params.iterations || !params.threads)
			throw std::invalid_argument("--allocs, --iterations and --threads must be positive");
		if (params.min_alloc_size >= params.max_alloc_size)
			throw std::invalid_argument("--min-size must be less than --max-size");
		if (!inUnitInterval(params.mean) || !inUnitInterval(params.deviation))
			throw std::invalid_argument("--mean and --deviation must be between 0 and 1");

		if (strategy == "mixed")
		{
			if (!inUnitInterval(mixed.dealloc_chance) || mixed.min_dealloc_count > mixed.max_dealloc_count)
				throw std::invalid_argument("Mixed strategy requires probability in [0, 1] and min-dealloc <= max-dealloc");
			params.bench_strategy = mixed;
		}
		else if (strategy == "sequence")
			params.bench_strategy = Firebird::SequenceStrategy{};
		else
			throw std::invalid_argument("--strategy must be mixed or sequence");

		std::cout << "strategy=" << strategy << " allocs=" << params.allocs_count
			<< " threads=" << params.threads
			<< " pool_per_thread=" << (params.pool_per_thread ? "true" : "false")
			<< " iterations=" << params.iterations << " warmup=" << params.warmup_allocs
			<< " min_size=" << params.min_alloc_size << " max_size=" << params.max_alloc_size
			<< " mean=" << params.mean << " deviation=" << params.deviation << " seed=" << params.seed << '\n';
		if (strategy == "mixed")
		{
			std::cout << "dealloc_chance=" << mixed.dealloc_chance
				<< " min_dealloc=" << mixed.min_dealloc_count << " max_dealloc=" << mixed.max_dealloc_count << '\n';
		}

		std::string output = Firebird::alloc_benchmark(params);
		std::cout << output << '\n';

		return 0;
	}
	catch (const Firebird::Exception& e)
	{
		std::cerr << "alloc_bench: " << e.what() << '\n';
		return 1;
	}
	catch (const std::exception& e)
	{
		std::cerr << "alloc_bench: " << e.what() << '\n';
		return 1;
	}
}
