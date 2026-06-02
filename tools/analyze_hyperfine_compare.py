#!/usr/bin/env python3
"""Analyze an interleaved hyperfine comparison with a one-sided Welch test."""

from __future__ import annotations

import argparse
import json
import math
import statistics
import sys
from pathlib import Path

import numpy as np
from scipy import stats


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Test whether a candidate hyperfine result beats a baseline by a practical margin."
    )
    parser.add_argument("--json", required=True, type=Path, help="hyperfine JSON file")
    parser.add_argument("--summary-output", type=Path, help="optional path for the printed summary")
    parser.add_argument("--baseline", required=True, help="baseline command name")
    parser.add_argument("--candidate", required=True, help="candidate command name")
    parser.add_argument(
        "--min-speedup",
        required=True,
        type=float,
        help="minimum practical speedup percentage required for a pass",
    )
    parser.add_argument("--alpha", required=True, type=float, help="significance threshold")
    return parser.parse_args()


def result_by_name(data: dict, name: str) -> dict:
    matches = [result for result in data.get("results", []) if result.get("command") == name]
    if len(matches) != 1:
        raise ValueError(f"expected exactly one hyperfine result named {name!r}, found {len(matches)}")
    return matches[0]


def times_from_result(result: dict, name: str) -> np.ndarray:
    times = result.get("times")
    if not isinstance(times, list) or len(times) < 2:
        raise ValueError(f"hyperfine result {name!r} must contain at least two timing samples")
    if any((not isinstance(t, (int, float))) or t <= 0 for t in times):
        raise ValueError(f"hyperfine result {name!r} contains non-positive or non-numeric samples")
    return np.array(times, dtype=float)


def welch_t_test_less_than_margin(
    baseline_times: np.ndarray, candidate_times: np.ndarray, min_speedup_pct: float
) -> tuple[float, float, float]:
    if not 0 <= min_speedup_pct < 100:
        raise ValueError("--min-speedup must be between 0 and 100")

    baseline_log = np.log(baseline_times)
    candidate_log = np.log(candidate_times)
    margin = math.log(1 - (min_speedup_pct / 100))

    baseline_var = float(np.var(baseline_log, ddof=1))
    candidate_var = float(np.var(candidate_log, ddof=1))
    baseline_n = len(baseline_log)
    candidate_n = len(candidate_log)
    standard_error = math.sqrt((baseline_var / baseline_n) + (candidate_var / candidate_n))
    if standard_error == 0:
        observed_diff = float(np.mean(candidate_log) - np.mean(baseline_log))
        p_value = 0.0 if observed_diff < margin else 1.0
        return observed_diff, math.inf, p_value

    observed_diff = float(np.mean(candidate_log) - np.mean(baseline_log))
    t_stat = (observed_diff - margin) / standard_error
    numerator = ((baseline_var / baseline_n) + (candidate_var / candidate_n)) ** 2
    denominator = ((baseline_var / baseline_n) ** 2 / (baseline_n - 1)) + (
        (candidate_var / candidate_n) ** 2 / (candidate_n - 1)
    )
    degrees_of_freedom = numerator / denominator
    p_value = float(stats.t.cdf(t_stat, degrees_of_freedom))
    return observed_diff, degrees_of_freedom, p_value


def percent_speedup(baseline: float, candidate: float) -> float:
    return (baseline - candidate) / baseline * 100


def summary_text(
    baseline_name: str,
    candidate_name: str,
    baseline_times: np.ndarray,
    candidate_times: np.ndarray,
    min_speedup: float,
    alpha: float,
    degrees_of_freedom: float,
    p_value: float,
    passed: bool,
) -> str:
    baseline_mean = statistics.fmean(baseline_times)
    candidate_mean = statistics.fmean(candidate_times)
    baseline_median = statistics.median(baseline_times)
    candidate_median = statistics.median(candidate_times)
    baseline_stddev = statistics.stdev(baseline_times)
    candidate_stddev = statistics.stdev(candidate_times)

    lines = [
        "metric\tbaseline\tcandidate",
        f"name\t{baseline_name}\t{candidate_name}",
        f"samples\t{len(baseline_times)}\t{len(candidate_times)}",
        f"mean_s\t{baseline_mean:.9g}\t{candidate_mean:.9g}",
        f"median_s\t{baseline_median:.9g}\t{candidate_median:.9g}",
        f"stddev_s\t{baseline_stddev:.9g}\t{candidate_stddev:.9g}",
        f"mean_speedup_pct\t{percent_speedup(baseline_mean, candidate_mean):.6g}",
        f"median_speedup_pct\t{percent_speedup(baseline_median, candidate_median):.6g}",
        f"required_speedup_pct\t{min_speedup:.6g}",
        f"welch_df\t{degrees_of_freedom:.6g}",
        f"p_value\t{p_value:.6g}",
        f"alpha\t{alpha:.6g}",
        f"result\t{'pass' if passed else 'fail'}",
    ]
    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    if args.alpha <= 0 or args.alpha >= 1:
        print("--alpha must be greater than 0 and less than 1", file=sys.stderr)
        return 2

    try:
        data = json.loads(args.json.read_text(encoding="utf-8"))
        baseline_result = result_by_name(data, args.baseline)
        candidate_result = result_by_name(data, args.candidate)
        baseline_times = times_from_result(baseline_result, args.baseline)
        candidate_times = times_from_result(candidate_result, args.candidate)
        _, degrees_of_freedom, p_value = welch_t_test_less_than_margin(
            baseline_times, candidate_times, args.min_speedup
        )
        observed_speedup = percent_speedup(statistics.fmean(baseline_times), statistics.fmean(candidate_times))
        passed = observed_speedup >= args.min_speedup and p_value < args.alpha
        summary = summary_text(
            args.baseline,
            args.candidate,
            baseline_times,
            candidate_times,
            args.min_speedup,
            args.alpha,
            degrees_of_freedom,
            p_value,
            passed,
        )
        print(summary, end="")
        if args.summary_output is not None:
            args.summary_output.write_text(summary, encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
