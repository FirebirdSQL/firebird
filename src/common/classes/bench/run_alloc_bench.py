#!/usr/bin/env python3
"""Run small-allocation workloads and save a report for later comparison.

Build first with `make alloc_bench TARGET=Release`, then run this script.
Use --list to inspect workloads and --case NAME to select individual workloads.
Allocation counts are totals per iteration, divided among worker threads.
Use --threads 1,4,8 --pool-per-thread --output-prefix wp6
to save a report for each thread count.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import platform
import shlex
import subprocess
import sys
import time
from typing import NamedTuple


ROOT = Path(__file__).resolve().parents[4]


class Case(NamedTuple):
    name: str
    strategy: str
    allocations: int
    iterations: int
    purpose: str
    probability: float = 0.0
    min_free: int = 0
    max_free: int = 0


CASES = (
    Case("sequence-long", "sequence", 4_000_000, 2,
         "Large live set, full cleanup, and retained memory"),
    Case("sequence-reuse", "sequence", 1_000_000, 5,
         "Repeated allocation and extent reuse"),
    Case("mixed-frequent", "mixed", 2_000_000, 3,
         "Frequent small frees; approximately balanced load", 0.5, 1, 3),
    Case("mixed-rare", "mixed", 2_000_000, 3,
         "Rare large bursts; approximately balanced load", 0.01, 80, 120),
    Case("mixed-drain", "mixed", 2_000_000, 3,
         "Repeatedly empty the live-object list", 0.1, 20, 40),
    Case("mixed-growing", "mixed", 1_000_000, 3,
         "Growing live set despite intermittent frees", 0.1, 1, 5),
)


def integer_list(value):
    try:
        return list(dict.fromkeys(int(item) for item in value.split(",")))
    except ValueError:
        raise argparse.ArgumentTypeError("expected comma-separated integers") from None


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--threads", type=integer_list, default=[1],
                        help="comma-separated positive thread counts (default: 1)")
    parser.add_argument("--pool-per-thread", action="store_true",
                        help="use separate child pools with one shared parent")
    parser.add_argument("--case", action="append", choices=[case.name for case in CASES],
                        help="select a workload (repeatable; default: all)")
    parser.add_argument("--list", action="store_true", help="list workloads without running")
    parser.add_argument("--binary", type=Path,
                        default=ROOT / "gen/Release/firebird/tests/alloc_bench")
    parser.add_argument("--output", type=Path, help="exact report path for a single configuration")
    parser.add_argument("--output-folder", type=Path,
                        help="report directory (default: repository bench_results)")
    parser.add_argument("--output-prefix", help="report filename prefix (default: timestamp-based)")
    args = parser.parse_args(argv)
    if any(threads <= 0 for threads in args.threads):
        parser.error("--threads values must be positive")
    if args.output:
        if len(args.threads) != 1:
            parser.error("--output requires a single thread count")
        if args.output_folder is not None or args.output_prefix is not None:
            parser.error("--output cannot be combined with --output-folder or --output-prefix")
    if args.output_prefix is not None:
        if not args.output_prefix or "/" in args.output_prefix or "\\" in args.output_prefix:
            parser.error("--output-prefix must be a nonempty filename prefix without path separators")
    return args


def build_command(case, args):
    allocations = max(1, case.allocations // args.threads)
    command = [str(args.binary), "--strategy", case.strategy,
               "--allocs", str(allocations), "--iterations", str(case.iterations),
               "--threads", str(args.threads), "--seed", "0", "--warmup", "0",
               "--min-size", "8", "--max-size", "1024",
               "--mean", "0.5", "--deviation", "0.75"]
    if args.pool_per_thread:
        command.append("--pool-per-thread")
    if case.strategy == "mixed":
        command.extend(["--dealloc-chance", str(case.probability),
                        "--min-dealloc", str(case.min_free),
                        "--max-dealloc", str(case.max_free)])
    return command


def write_metadata(report, args, started):
    digest = hashlib.sha256()
    with args.binary.open("rb") as binary:
        for chunk in iter(lambda: binary.read(1024 * 1024), b""):
            digest.update(chunk)
    report.write("ALLOCATOR BENCHMARK SUITE\n")
    report.write(f"started_utc={started.isoformat()}\n")
    report.write(f"platform={platform.platform()} cpu_count={os.cpu_count()}\n")
    report.write(f"binary={args.binary}\nsha256={digest.hexdigest()}\n")
    report.write(f"threads={args.threads} pool_per_thread={args.pool_per_thread}\n")
    report.write("Sizes: 8-1024 bytes; seed=0; mean=0.5; deviation=0.75; warmup=0\n")
    report.write("Mixed frees are LIFO and capped by live objects.\n")
    report.write("Case wall time includes setup, statistics, and cleanup.\n")
    report.flush()


def run_case(case, args, report):
    command = build_command(case, args)
    actual_total = max(1, case.allocations // args.threads) * args.threads
    report.write(f"\n=========== CASE {case.name} ===========\n{case.purpose}\n")
    report.write(f"requested_allocations={case.allocations} actual_allocations={actual_total} "
                 "(per iteration)\n")
    report.write(f"command: {shlex.join(command)}\n--- stdout and stderr ---\n")
    report.flush()
    started = time.monotonic()
    # Write directly to the report so partial output survives failures/interruption.
    process = subprocess.Popen(command, stdout=report, stderr=subprocess.STDOUT)
    try:
        code = process.wait()
    except KeyboardInterrupt:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        report.write(f"\ninterrupted=true exit_status={process.returncode} "
                     f"elapsed_seconds={time.monotonic() - started:.3f}\n")
        report.flush()
        raise
    elapsed = time.monotonic() - started
    report.write(f"\nexit_status={code} elapsed_seconds={elapsed:.3f}\n")
    report.flush()
    return code, elapsed


def main(argv=None):
    args = parse_args(argv)
    cases = [case for case in CASES if not args.case or case.name in args.case]
    if args.list:
        print("All cases: sizes=8-1024, seed=0, mean=0.5, deviation=0.75, warmup=0")
        for case in cases:
            burst = (f", probability={case.probability}, burst={case.min_free}-{case.max_free}"
                     if case.strategy == "mixed" else "")
            print(f"{case.name}: {case.strategy}, allocations={case.allocations}, "
                  f"iterations={case.iterations}{burst}\n  {case.purpose}")
        return 0

    args.binary = args.binary.expanduser().resolve()
    if not args.binary.is_file() or not os.access(args.binary, os.X_OK):
        print(f"Executable not found or not executable: {args.binary}", file=sys.stderr)
        return 1

    folder = (args.output_folder or ROOT / "bench_results").expanduser()
    prefix = args.output_prefix or (
        "alloc-suite-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ"))
    runs = []
    for threads in args.threads:
        run = argparse.Namespace(**vars(args))
        run.threads = threads
        run.output = (args.output.expanduser() if args.output else
                      folder / f"{prefix}-t{threads}.txt")
        runs.append(run)

    # Check every destination before starting any benchmark.
    for run in runs:
        if os.path.lexists(run.output):
            print(f"Report already exists: {run.output}", file=sys.stderr)
            return 1
    for run in runs:
        code = run_suite(run, cases)
        if code:
            return code
    return 0


def run_suite(args, cases):
    started = datetime.now(timezone.utc)
    output = args.output
    suite_start = time.monotonic()
    completed = 0
    try:
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("x") as report:
            print(f"Report: {output.resolve()}", flush=True)
            try:
                write_metadata(report, args, started)
                for index, case in enumerate(cases, 1):
                    print(f"[{index}/{len(cases)}] {case.name}", flush=True)
                    code, elapsed = run_case(case, args, report)
                    print(f"  exit={code}, {elapsed:.2f}s", flush=True)
                    if code:
                        report.write(f"FAILED: {case.name}; completed_cases={completed}\n")
                        return 1
                    completed += 1
            except KeyboardInterrupt:
                report.write(f"\nINTERRUPTED; completed_cases={completed}\n")
                return 130
            except OSError as error:
                report.write(f"\nERROR: {error}; completed_cases={completed}\n")
                raise
            report.write(f"\nSUCCESS completed_cases={completed} "
                         f"elapsed_seconds={time.monotonic() - suite_start:.3f}\n")
    except OSError as error:
        print(f"alloc_bench suite: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
