# Allocator benchmark

From the root of a configured Firebird checkout, use `make alloc_bench` to build benchmark or `make run_alloc_bench` to build it and run. Pass benchmark options through `BENCH_ARGS`.
`alloc_bench` has `--help`, so don't afraid to use it.

```sh
make alloc_bench
or
make run_alloc_bench BENCH_ARGS="--strategy mixed --allocs 10000 --iterations 1"
```


With the Release build ready, run all six workloads at one and eight threads, using one child pool per thread and a shared parent.

```sh
python3 src/common/classes/bench/run_alloc_bench.py --threads 1,8 --pool-per-thread --binary ~/firebird/gen/Release/firebird/tests/alloc_bench --output-folder ~/firebird/bench_results/ --output-prefix with-patch
```

Use `--list` to see the workloads or `--case mixed-rare` to select one.
