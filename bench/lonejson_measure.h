#ifndef LONEJSON_BENCH_MEASURE_H
#define LONEJSON_BENCH_MEASURE_H

#include <limits.h>
#include <math.h>
#include <string.h>

#define BENCH_MEASUREMENT_METHOD "whole-suite-mean"
#define BENCH_SAMPLE_COUNT 399u
#define BENCH_PROCESS_COUNT 399u
#define BENCH_PROCESS_SAMPLE_COUNT 1u
#define BENCH_WARMUP_COUNT 1u
#define BENCH_SETTLE_SECONDS 20u
#define BENCH_TARGET_SAMPLE_NS 12000000u
#define BENCH_CONFIDENCE_PCT 95.0
#define BENCH_MAX_RELATIVE_ERROR_PCT 5.0
#define BENCH_MAX_DRIFT_PCT 5.0

typedef struct bench_sample {
  lonejson_uint64 elapsed_ns;
  lonejson_uint64 cpu_ns;
  lonejson_uint64 iterations;
  lonejson_uint64 process_index;
  lonejson_uint64 mismatch_count;
} bench_sample;

typedef struct bench_measurement {
  unsigned long batch_iterations;
  lonejson_uint64 warmup_elapsed_ns;
  lonejson_uint64 mismatch_count;
  bench_sample samples[BENCH_SAMPLE_COUNT];
  size_t sample_count;
  double mean_cost_ns;
  double cost_low_ns;
  double cost_high_ns;
  double relative_error_pct;
  double drift_ci_low_pct;
  double drift_ci_high_pct;
  double cv_pct;
  double drift_pct;
} bench_measurement;

/* Two-sided 95% Student-t bounds for 399 workers (398 degrees of freedom)
 * and their 133-worker thirds (132 degrees). Round critical values upward.
 * Batch means approximate normality; all observed costs, including pauses,
 * contribute. Repeated operations are not independent observations. */
static void bench_measure_cost_interval(const bench_sample *samples,
                                        size_t count, double *mean, double *low,
                                        double *high, double *variance) {
  size_t i;
  double margin;
  *mean = *variance = 0.0;
  for (i = 0u; i < count; ++i)
    *mean += (double)samples[i].elapsed_ns / (double)samples[i].iterations;
  *mean /= (double)count;
  for (i = 0u; i < count; ++i) {
    double difference =
        (double)samples[i].elapsed_ns / (double)samples[i].iterations - *mean;
    *variance += difference * difference;
  }
  if (count > 1u)
    *variance /= (double)(count - 1u);
  margin = count == 1u ? 0.0
                       : (count == BENCH_SAMPLE_COUNT ? 1.967 : 1.979) *
                             sqrt(*variance / (double)count);
  *low = *mean - margin;
  *high = *mean + margin;
}

/* Source order alternates each round. Reverse case order on a different
 * two-round cycle so the two factors are counterbalanced independently. */
static size_t bench_measure_case_index(size_t count, size_t round,
                                       size_t step) {
  return (round * 17u + ((round / 2u) % 2u ? count - 1u - step : step)) % count;
}

typedef lonejson_uint64 (*bench_measure_clock_fn)(void *, int);
typedef lonejson_uint64 (*bench_measure_batch_fn)(void *, unsigned long);

static int bench_measure_summarize(bench_measurement *measurement) {
  size_t i;
  double mean;
  double variance;
  double first;
  double last;
  double low;
  double high;
  if ((measurement->sample_count != BENCH_SAMPLE_COUNT &&
       measurement->sample_count != BENCH_PROCESS_SAMPLE_COUNT) ||
      measurement->batch_iterations == 0u) {
    return 0;
  }
  for (i = 0u; i < measurement->sample_count; ++i) {
    if (measurement->samples[i].elapsed_ns == 0u ||
        measurement->samples[i].cpu_ns == 0u ||
        measurement->samples[i].iterations == 0u)
      return 0;
  }
  bench_measure_cost_interval(measurement->samples, measurement->sample_count,
                              &mean, &low, &high, &variance);
  if (!(low > 0.0))
    return 0;
  measurement->mean_cost_ns = mean;
  measurement->cost_low_ns = low;
  measurement->cost_high_ns = high;
  measurement->relative_error_pct = mean / low - 1.0;
  if (1.0 - mean / high > measurement->relative_error_pct)
    measurement->relative_error_pct = 1.0 - mean / high;
  measurement->relative_error_pct *= 100.0;
  if (measurement->sample_count == 1u) {
    measurement->cv_pct = measurement->drift_pct = 0.0;
    measurement->drift_ci_low_pct = measurement->drift_ci_high_pct = 0.0;
    return 1;
  }
  measurement->cv_pct = sqrt(variance) * 100.0 / mean;
  first = 0.0;
  last = 0.0;
  for (i = 0u; i < measurement->sample_count / 3u; ++i) {
    first += (double)measurement->samples[i].elapsed_ns /
             (double)measurement->samples[i].iterations;
    last += (double)measurement->samples[measurement->sample_count - 1u - i]
                .elapsed_ns /
            (double)measurement->samples[measurement->sample_count - 1u - i]
                .iterations;
  }
  measurement->drift_pct = fabs(last - first) * 100.0 / first;
  {
    double first_low;
    double first_high;
    double last_low;
    double last_high;
    double ignored_mean;
    double ignored_variance;
    size_t count = measurement->sample_count / 3u;
    bench_measure_cost_interval(measurement->samples, count, &ignored_mean,
                                &first_low, &first_high, &ignored_variance);
    bench_measure_cost_interval(
        measurement->samples + measurement->sample_count - count, count,
        &ignored_mean, &last_low, &last_high, &ignored_variance);
    if (!(first_low > 0.0) || !(last_low > 0.0))
      return 0;
    measurement->drift_ci_low_pct = (last_low / first_high - 1.0) * 100.0;
    measurement->drift_ci_high_pct = (last_high / first_low - 1.0) * 100.0;
  }
  return 1;
}

static lonejson_uint64
bench_measure_elapsed_ns(const bench_measurement *measurement) {
  double elapsed =
      measurement->mean_cost_ns * (double)measurement->batch_iterations;
  if (!(elapsed >= 1.0) || elapsed >= (double)(lonejson_uint64)-1)
    return 0u;
  return (lonejson_uint64)(elapsed + 0.5);
}

/* Warm up before sizing the pilot. CPU time prevents a descheduled pilot from
 * shrinking every confirmation batch; recorded samples still use wall time. */
static int bench_measure(bench_measurement *measurement,
                         bench_measure_clock_fn clock_fn, void *clock_context,
                         bench_measure_batch_fn batch_fn, void *batch_context,
                         unsigned long initial_iterations, int calibrated,
                         lonejson_uint64 target_sample_ns) {
  unsigned attempt;
  lonejson_uint64 before, after, cpu_before, cpu_after, elapsed, mismatches;
  unsigned long count = initial_iterations;
  memset(measurement, 0, sizeof(*measurement));
  if (count == 0u)
    return 0;
  before = clock_fn(clock_context, 0);
  mismatches = batch_fn(batch_context, 1u);
  after = clock_fn(clock_context, 0);
  measurement->mismatch_count += mismatches;
  if (mismatches != 0u || after <= before)
    return 0;
  measurement->warmup_elapsed_ns = after - before;
  if (!calibrated) {
    for (attempt = 0u; attempt < 16u; ++attempt) {
      before = clock_fn(clock_context, 1);
      mismatches = batch_fn(batch_context, count);
      after = clock_fn(clock_context, 1);
      measurement->mismatch_count += mismatches;
      if (mismatches != 0u || after <= before)
        return 0;
      elapsed = after - before;
      if (elapsed >= 3000000u) {
        double planned =
            ceil((double)count * (double)target_sample_ns / (double)elapsed);
        if (!(planned >= 1.0) || planned >= (double)ULONG_MAX)
          return 0;
        count = (unsigned long)planned;
        break;
      }
      if (count > ULONG_MAX / 10u)
        return 0;
      count *= 10u;
    }
    if (attempt == 16u)
      return 0;
  }
  measurement->batch_iterations = count;
  cpu_before = clock_fn(clock_context, 1);
  before = clock_fn(clock_context, 0);
  mismatches = batch_fn(batch_context, count);
  after = clock_fn(clock_context, 0);
  cpu_after = clock_fn(clock_context, 1);
  measurement->mismatch_count += mismatches;
  if (mismatches != 0u || after <= before || cpu_after <= cpu_before)
    return 0;
  measurement->samples[0].elapsed_ns = after - before;
  measurement->samples[0].cpu_ns = cpu_after - cpu_before;
  measurement->samples[0].iterations = count;
  measurement->samples[0].mismatch_count = mismatches;
  measurement->sample_count = 1u;
  return bench_measure_summarize(measurement);
}

#endif
