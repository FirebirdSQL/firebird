#include "../common/classes/bench/AllocBench.h"

#include "firebird.h"
#include "../common/classes/alloc.h"
#include "../common/utils_proto.h"

#include <random>
#include <algorithm>
#include <vector>
#include <chrono>
#include <stdexcept>

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <exception>
#include <limits>

namespace
{
	// Reusable phase barrier. Cancellation releases workers if any worker fails.
	class Barrier
	{
	public:
		explicit Barrier(size_t count) : participants(count), remaining(count) {}

		void arrive_and_wait()
		{
			std::unique_lock<std::mutex> lock(mutex);

			if (cancelled)
				throw std::runtime_error("Benchmark cancelled");

			const size_t current = generation;
			if (--remaining == 0)
			{
				remaining = participants;
				++generation;
				condition.notify_all();
			}
			else
			{
				condition.wait(lock, [&] {
					return cancelled || generation != current;
				});
			}

			if (cancelled)
				throw std::runtime_error("Benchmark cancelled");
		}

		void cancel()
		{
			std::lock_guard<std::mutex> lock(mutex);
			cancelled = true;
			condition.notify_all();
		}

	private:
		std::mutex mutex;
		std::condition_variable condition;
		size_t participants = 0;
		size_t remaining = 0;
		size_t generation = 0;
		bool cancelled = false;
	};
}

namespace
{
	template <typename Real = double>
	class TruncatedNormalDistribution
	{
	public:
		// mean and deviation are normalized to [0, 1]
		TruncatedNormalDistribution(Real min, Real max, double mean = 0.5, double deviation = 0.5)
			: m_min(min), m_max(max),
			m_dist(
				static_cast<double>(min) +
					(static_cast<double>(max) -
					static_cast<double>(min)) * mean,
				std::max(deviation, 0.01) *
					(static_cast<double>(max) -
					static_cast<double>(min)) / 3.0
			)
		{
			if (min >= max ||
				!(mean >= 0.0 && mean <= 1.0) ||
				!(deviation >= 0.0 && deviation <= 1.0))
			{
				throw std::invalid_argument("Invalid parameters");
			}
		}

		template <typename Generator>
		Real operator()(Generator& gen)
		{
			double value;

			do{
				value = m_dist(gen);
			} while (value < m_min || value > m_max);

			return static_cast<Real>(value);
		}

	private:
		Real m_min;
		Real m_max;
		std::normal_distribution<double> m_dist;
	};

	auto BEGIN()
	{
		return std::chrono::steady_clock::now();
	}

	double END(std::chrono::steady_clock::time_point begin)
	{
		auto end = BEGIN();

		return std::chrono::duration<double, std::nano>(end - begin).count();
	}

	template <typename T>
	double percentile(const std::vector<T>& values, double p)
	{
		assert(values.size() > 0 && p >= 0.0 && p <= 100.0);

		const double index = (values.size() - 1) * p / 100.0;
		const size_t lo = static_cast<size_t>(index);
		const size_t hi = static_cast<size_t>(std::ceil(index));

		if (lo == hi)
			return values[lo];

		return values[lo] + (static_cast<double>(values[hi]) - values[lo]) * (index - lo);
	}

	struct SampleStats
	{
		size_t count = 0;
		double sum = 0.0;

		double min = std::numeric_limits<double>::max();
		double max = 0.0;

		double avg = 0.0;

		double p50 = 0.0;
		double p90 = 0.0;
		double p99 = 0.0;
	};

	template <typename T>
	SampleStats calculate_sample_stats(std::vector<T>& data)
	{
		if (data.empty())
			return {};

		SampleStats stats;
		stats.count = data.size();

		std::sort(data.begin(), data.end());

		for (auto v : data)
		{
			stats.sum += v;

			stats.min = MIN(stats.min, v);
			stats.max = MAX(stats.max, v);
		}

		stats.avg = stats.sum / data.size();

		stats.p50 = percentile(data, 50.0);
		stats.p90 = percentile(data, 90.0);
		stats.p99 = percentile(data, 99.0);

		return stats;
	}

	struct AllocatorStats
	{
		size_t mapped_mem = 0;
		size_t used_mem = 0;
	};

	AllocatorStats get_allocator_stats(Firebird::MemoryPool& pool)
	{
		auto& stats = pool.getStatsGroup();

		return {.mapped_mem = stats.getCurrentMapping(), .used_mem = stats.getCurrentUsage()};
	}

	struct DeallocationBurst
	{
		SampleStats burst_size;
	};

	struct IterationStats
	{
		struct AllocatorsStats
		{
			AllocatorStats child;
			AllocatorStats parent;
		} allocators_before, allocators_before_cleanup, allocators_after;

		SampleStats allocs_time;
		SampleStats deallocs_time;

		std::variant<std::monostate, DeallocationBurst> strategy_stats;
	};

	IterationStats::AllocatorsStats get_allocators_stats(Firebird::MemoryPool& child, Firebird::MemoryPool& parent)
	{
		return {.child = get_allocator_stats(child), .parent = get_allocator_stats(parent)};
	}

	struct BenchResult
	{
		std::vector<IterationStats> warmups;

		std::vector<std::vector<IterationStats>> iters_results;

		double child_pools_deletion_time;
		double parent_pool_deletion_time = 0.0;
	};
}

namespace
{
	template <typename... Args>
	void pprintf(std::string& out, const char* fmt, Args&&... args)
	{
		const int size = std::snprintf(nullptr, 0, fmt, args...);
		if (size <= 0)
			return;

		const size_t offset = out.size();
		out.resize(offset + size);

		std::snprintf(out.data() + offset, size + 1, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void pprintf_ln(std::string& out, const char* fmt, Args&&... args)
	{
		pprintf(out, fmt, std::forward<Args>(args)...);
		out.push_back('\n');
	}

	void print_memory_row(std::string& out, const char* phase,
		const IterationStats::AllocatorsStats& stats)
	{
		pprintf_ln(out, "%-22s %14zu %14zu %14zu %14zu", phase,
			stats.child.used_mem, stats.child.mapped_mem,
			stats.parent.used_mem, stats.parent.mapped_mem);
	}

	void print_warmup_memory(std::string& out, const IterationStats& iteration)
	{
		pprintf_ln(out, "%-22s %14s %14s %14s %14s", "Phase",
			"Child used", "Child mapped", "Parent used", "Parent mapped");
		print_memory_row(out, "BEFORE", iteration.allocators_before);
		print_memory_row(out, "BEFORE FINAL CLEANUP", iteration.allocators_before_cleanup);
		print_memory_row(out, "AFTER", iteration.allocators_after);
	}

	void print_memory_phase(std::string& out, const char* phase,
		const std::vector<IterationStats>& threads, bool pool_per_thread,
		IterationStats::AllocatorsStats IterationStats::* snapshot)
	{
		pprintf_ln(out, "%s", phase);
		pprintf_ln(out, "%-22s %14s %14s", "Pool", "Used", "Mapped");
		const size_t child_count = pool_per_thread ? threads.size() : 1;
		for (size_t child = 0; child < child_count; ++child)
		{
			const auto& stats = (threads[child].*snapshot).child;
			const std::string label = pool_per_thread
				? "Child " + std::to_string(child) : "Child (shared)";
			pprintf_ln(out, "%-22s %14zu %14zu", label.c_str(), stats.used_mem, stats.mapped_mem);
		}
		const auto& parent = (threads.front().*snapshot).parent;
		pprintf_ln(out, "%-22s %14zu %14zu", "Parent (shared)", parent.used_mem, parent.mapped_mem);
		pprintf_ln(out, "");
	}

	void print_sample_header(std::string& out, const char* count_label, const char* row_label)
	{
		pprintf_ln(out, "%-8s %12s %18s %12s %12s %12s %12s %12s %14s",
			row_label, count_label, "Sum", "Min", "Avg", "p50", "p90", "p99", "Max");
	}

	void print_sample_row(std::string& out, size_t thread, const SampleStats& stats)
	{
		constexpr const char* fmt = "%-8zu %12zu %18.2f %12.2f %12.2f %12.2f %12.2f %12.2f %14.2f";

		if (!stats.count)
		{
			pprintf_ln(out, fmt,
				thread, stats.count, NAN, NAN, NAN, NAN, NAN, NAN, NAN);
			return;
		}

		pprintf_ln(out, fmt,
			thread, stats.count, stats.sum, stats.min, stats.avg,
			stats.p50, stats.p90, stats.p99, stats.max);
	}

	void print_samples(std::string& out,
		const std::vector<IterationStats>& threads, const char* row_label)
	{
		pprintf_ln(out, "== ALLOC (ns) ==");
		print_sample_header(out, "Count", row_label);
		for (size_t thread = 0; thread < threads.size(); ++thread)
			print_sample_row(out, thread, threads[thread].allocs_time);

		pprintf_ln(out, "");
		pprintf_ln(out, "== DEALLOC (ns) ==");
		print_sample_header(out, "Count", row_label);
		for (size_t thread = 0; thread < threads.size(); ++thread)
			print_sample_row(out, thread, threads[thread].deallocs_time);

		if (std::holds_alternative<DeallocationBurst>(threads.front().strategy_stats))
		{
			pprintf_ln(out, "");
			pprintf_ln(out, "== DEALLOC BURST SIZE (bytes) ==");
			print_sample_header(out, "Bursts", row_label);
			for (size_t thread = 0; thread < threads.size(); ++thread)
			{
				print_sample_row(out, thread,
					std::get<DeallocationBurst>(threads[thread].strategy_stats).burst_size);
			}
		}
		pprintf_ln(out, "");
	}

	void print_iteration(std::string& out,
		const std::vector<IterationStats>& threads, size_t iteration, bool pool_per_thread)
	{
		pprintf_ln(out, "===== ITER #%zu =====", iteration + 1);
		pprintf_ln(out, "== MEMORY (bytes) ==");
		pprintf_ln(out, "");
		print_memory_phase(out, "BEFORE", threads, pool_per_thread, &IterationStats::allocators_before);
		print_memory_phase(out, "BEFORE FINAL CLEANUP", threads, pool_per_thread,
			&IterationStats::allocators_before_cleanup);
		print_memory_phase(out, "AFTER", threads, pool_per_thread, &IterationStats::allocators_after);
		print_samples(out, threads, "Thread");
	}

	std::string print_bench_result(const BenchResult& result, const Firebird::BenchParameters& params)
	{
		std::string out;
		out.reserve(2048);
		pprintf_ln(out, "=========== BENCH RESULT ===========");
		pprintf_ln(out, "threads=%zu pool_mode=%s parent_pool=shared", params.threads,
			params.pool_per_thread ? "per-thread" : "shared");
		pprintf_ln(out, "allocs_per_thread=%zu allocs_total=%zu (per iteration)",
			params.allocs_count, params.allocs_count * params.threads);
		pprintf_ln(out, "Parent memory includes child statistics; do not add them together.");
		pprintf_ln(out, "");
		if (!result.warmups.empty())
		{
			pprintf_ln(out, "=========== WARMUP (sequential, per child pool) ===========");
			pprintf_ln(out, "== MEMORY (bytes) ==");
			for (size_t child = 0; child < result.warmups.size(); ++child)
			{
				pprintf_ln(out, "");
				if (params.pool_per_thread)
					pprintf_ln(out, "Pool: Child %zu", child);
				else
					pprintf_ln(out, "Pool: Child 0 (shared)");
				print_warmup_memory(out, result.warmups[child]);
			}
			pprintf_ln(out, "");
			print_samples(out, result.warmups, "Pool");
		}
		pprintf_ln(out, "=========== ITERATIONS ===========");
		for (size_t i = 0; i < result.iters_results.size(); ++i)
			print_iteration(out, result.iters_results[i], i, params.pool_per_thread);
		pprintf_ln(out, "=========== POOL DELETION ===========");
		pprintf_ln(out, "Child:  %.2f ns", result.child_pools_deletion_time);
		pprintf_ln(out, "Parent: %.2f ns", result.parent_pool_deletion_time);
		return out;
	}
}

namespace
{
	void deallocate_all(MemoryPool& pool, std::vector<void*>& allocs, std::vector<double>& deallocs_time)
	{
		for (size_t i = 0; i < allocs.size(); i++)
		{
			auto begin = BEGIN();
			pool.deallocate(allocs[i]);
			deallocs_time.push_back(END(begin));
		}

		allocs.clear();
	};

	IterationStats warmup(const Firebird::BenchParameters& params,
		MemoryPool& child_pool, MemoryPool& parent_pool)
	{
		IterationStats result;

		std::mt19937 gen(params.seed + 33);
		TruncatedNormalDistribution<size_t> alloc_size_dist(params.min_alloc_size, params.max_alloc_size, params.mean, params.deviation);

		std::vector<void*> warmup_allocs;
		warmup_allocs.reserve(params.warmup_allocs);

		std::vector<double> temp_buffer_time;
		temp_buffer_time.reserve(params.warmup_allocs);

		result.allocators_before = get_allocators_stats(child_pool, parent_pool);

		for (size_t i = 0; i < params.warmup_allocs; i++)
		{
			const size_t alloc_size = alloc_size_dist(gen);

			auto begin = BEGIN();
			warmup_allocs.push_back(child_pool.allocate(alloc_size));
			temp_buffer_time.push_back(END(begin));
		}
		result.allocs_time = calculate_sample_stats(temp_buffer_time);
		temp_buffer_time.clear();

		result.allocators_before_cleanup = get_allocators_stats(child_pool, parent_pool);

		deallocate_all(child_pool, warmup_allocs, temp_buffer_time);
		result.deallocs_time = calculate_sample_stats(temp_buffer_time);
		temp_buffer_time.clear();

		result.allocators_after = get_allocators_stats(child_pool, parent_pool);

		return result;
	}

	std::vector<IterationStats> sequence_iterations(const Firebird::BenchParameters& params,
		MemoryPool& child_pool, MemoryPool& parent_pool,
		Barrier& barrier, size_t thread_id)
	{
		std::vector<IterationStats> iterations;
		iterations.reserve(params.iterations);

		std::vector<void*> allocs;
		allocs.reserve(params.allocs_count);

		std::vector<double> allocs_time;
		allocs_time.reserve(params.allocs_count);

		std::vector<double> deallocs_time;
		deallocs_time.reserve(params.allocs_count);

		std::mt19937 gen(params.seed + thread_id * 100);
		TruncatedNormalDistribution<size_t> alloc_size_dist(params.min_alloc_size, params.max_alloc_size, params.mean, params.deviation);

		barrier.arrive_and_wait();

		for (size_t iter = 0; iter < params.iterations; iter++)
		{
			IterationStats iter_stats;

			barrier.arrive_and_wait();
			iter_stats.allocators_before = get_allocators_stats(child_pool, parent_pool);
			barrier.arrive_and_wait();

			for (size_t i = 0; i < params.allocs_count; i++)
			{
				const size_t alloc_size = alloc_size_dist(gen);

				auto begin = BEGIN();
				allocs.push_back(child_pool.allocate(alloc_size));
				allocs_time.push_back(END(begin));
			}
			iter_stats.allocs_time = calculate_sample_stats(allocs_time);
			allocs_time.clear();

			barrier.arrive_and_wait();
			iter_stats.allocators_before_cleanup = get_allocators_stats(child_pool, parent_pool);
			barrier.arrive_and_wait();

			deallocate_all(child_pool, allocs, deallocs_time);
			iter_stats.deallocs_time = calculate_sample_stats(deallocs_time);
			deallocs_time.clear();

			iter_stats.strategy_stats = std::monostate{};

			barrier.arrive_and_wait();
			iter_stats.allocators_after = get_allocators_stats(child_pool, parent_pool);
			barrier.arrive_and_wait();

			iterations.push_back(iter_stats);
		}

		return iterations;
	}

	std::vector<IterationStats> mixed_iterations(const Firebird::BenchParameters& params,
		MemoryPool& child_pool, MemoryPool& parent_pool,
		Barrier& barrier, size_t thread_id)
	{
		std::vector<IterationStats> iterations;
		iterations.reserve(params.iterations);

		std::vector<void*> allocs;
		allocs.reserve(params.allocs_count);

		std::vector<size_t> allocs_size;
		allocs_size.reserve(params.allocs_count);

		std::vector<double> allocs_time;
		allocs_time.reserve(params.allocs_count);

		std::vector<double> deallocs_time;
		deallocs_time.reserve(params.allocs_count);

		std::vector<size_t> deallocs_burst_size;
		deallocs_burst_size.reserve(params.allocs_count);

		std::mt19937 dealloc_gen(params.seed + 123);
		std::uniform_real_distribution dealloc_dist;

		const auto& strategy = std::get<Firebird::MixedStrategy>(params.bench_strategy);
		std::mt19937 dealloc_count_gen(params.seed + 321);
		std::uniform_int_distribution dealloc_count_dist(strategy.min_dealloc_count, strategy.max_dealloc_count);

		std::mt19937 gen(params.seed + thread_id * 100);
		TruncatedNormalDistribution<size_t> alloc_size_dist(params.min_alloc_size, params.max_alloc_size, params.mean, params.deviation);

		for (size_t iter = 0; iter < params.iterations; iter++)
		{
			IterationStats iter_stats;

			barrier.arrive_and_wait();
			iter_stats.allocators_before = get_allocators_stats(child_pool, parent_pool);
			barrier.arrive_and_wait();

			for (size_t i = 0; i < params.allocs_count; i++)
			{
				const size_t alloc_size = alloc_size_dist(gen);

				auto begin = BEGIN();
				allocs.push_back(child_pool.allocate(alloc_size));
				allocs_time.push_back(END(begin));

				allocs_size.push_back(alloc_size);

				if (dealloc_dist(dealloc_gen) < strategy.dealloc_chance)
				{
					const size_t dealloc_count = dealloc_count_dist(dealloc_count_gen);
					size_t dealloc_burst_size = 0;
					for (size_t j = 0; j < dealloc_count; j++)
					{
						if (allocs.empty())
							break;

						begin = BEGIN();
						child_pool.deallocate(allocs.back());
						deallocs_time.push_back(END(begin));
						allocs.pop_back();

						dealloc_burst_size += allocs_size.back();
						allocs_size.pop_back();
					}

					deallocs_burst_size.push_back(dealloc_burst_size);
				}
			}
			iter_stats.allocs_time = calculate_sample_stats(allocs_time);
			allocs_time.clear();

			allocs_size.clear();

			// Analyze only deallocation that happens in mix load.
			iter_stats.deallocs_time = calculate_sample_stats(deallocs_time);

			barrier.arrive_and_wait();
			iter_stats.allocators_before_cleanup = get_allocators_stats(child_pool, parent_pool);
			barrier.arrive_and_wait();

			deallocate_all(child_pool, allocs, deallocs_time);
			deallocs_time.clear();

			iter_stats.strategy_stats = DeallocationBurst{.burst_size = calculate_sample_stats(deallocs_burst_size)};
			deallocs_burst_size.clear();

			barrier.arrive_and_wait();
			iter_stats.allocators_after = get_allocators_stats(child_pool, parent_pool);
			barrier.arrive_and_wait();

			iterations.push_back(iter_stats);
		}

		return iterations;
	}

}

namespace Firebird
{
	std::string alloc_benchmark(const BenchParameters& params)
	{
		if (!params.threads || !params.iterations || !params.allocs_count)
			throw std::invalid_argument("threads, iterations and allocations must be positive");
		if (params.allocs_count > std::numeric_limits<size_t>::max() / params.threads)
			throw std::invalid_argument("Total allocation count overflows size_t");

		BenchResult result;

		auto parent_stats = std::make_unique<MemoryStats>();
		MemoryPool* parent_pool = MemoryPool::createPool(nullptr, *parent_stats);

		std::vector<MemoryPool*> child_pools;
		std::vector<std::unique_ptr<MemoryStats>> memory_stats;

		const size_t child_pools_count = params.pool_per_thread ? params.threads : 1;
		for (size_t i = 0; i < child_pools_count; i++)
		{
			auto child_stats = std::make_unique<MemoryStats>(parent_stats.get());
			MemoryPool* child_pool = MemoryPool::createPool(parent_pool, *child_stats);

			child_pools.push_back(child_pool);
			memory_stats.push_back(std::move(child_stats));
		}

		if (params.warmup_allocs > 0)
		{
			for (auto* pool : child_pools)
				result.warmups.push_back(warmup(params, *pool, *parent_pool));
		}

		Barrier barrier(params.threads);
		std::vector<std::thread> threads;
		threads.reserve(params.threads);

		result.iters_results.resize(params.iterations);
		for (auto& iters : result.iters_results)
			iters.resize(params.threads);

		std::exception_ptr failure;
		std::mutex failure_mutex;

		for (size_t thread_id = 0; thread_id < params.threads; ++thread_id)
		{
			auto* child_pool = params.pool_per_thread
				? child_pools[thread_id]
				: child_pools[0];

			threads.emplace_back([&, thread_id, child_pool] {
				try
				{
					auto bench_result = std::visit([&](const auto& strategy)
					{
						using Strategy = std::decay_t<decltype(strategy)>;

						if constexpr (std::is_same_v<Strategy, SequenceStrategy>)
							return sequence_iterations(params, *child_pool, *parent_pool, barrier, thread_id);
						else if constexpr (std::is_same_v<Strategy, MixedStrategy>)
							return mixed_iterations(params, *child_pool, *parent_pool, barrier, thread_id);
						else
							static_assert(fb_utils::fb_always_false_v<Strategy>);
					}, params.bench_strategy);

					// Place each iteration at corresponding thread slot.
					for (size_t iter = 0; iter < params.iterations; iter++)
						result.iters_results[iter][thread_id] = std::move(bench_result[iter]);
				}
				catch (...)
				{
					std::lock_guard<std::mutex> lock(failure_mutex);
					if (!failure)
						failure = std::current_exception();
					barrier.cancel();
				}
			});
		}

		for (auto& thread : threads)
			thread.join();

		{
			auto begin = BEGIN();
			for (auto* child_pool : child_pools)
				MemoryPool::deletePool(child_pool);
			result.child_pools_deletion_time = END(begin);

			child_pools.clear();
		}
		{
			auto begin = BEGIN();
			MemoryPool::deletePool(parent_pool);
			result.parent_pool_deletion_time = END(begin);

			parent_pool = nullptr;
		}

		if (failure)
			std::rethrow_exception(failure);

		return print_bench_result(result, params);
	}
}
