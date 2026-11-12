#ifdef NDEBUG
#undef NDEBUG
#endif
#include "lonejson.h"
#include "lonejson_measure.h"
#include <assert.h>
#include <stdio.h>

typedef struct test_clock {
  lonejson_uint64 now;
  lonejson_uint64 wall_delay;
  unsigned batches;
  unsigned fail_batch;
  unsigned pause_batch;
  int stopped;
  int stopped_cpu;
  unsigned long counts[32];
} test_clock;

static lonejson_uint64 fake_clock(void *context, int cpu) {
  test_clock *clock;
  clock = (test_clock *)context;
  if (cpu && clock->stopped_cpu)
    return 0u;
  return clock->stopped ? 0u : clock->now + (cpu ? 0u : clock->wall_delay);
}

static lonejson_uint64 fake_batch(void *context, unsigned long count) {
  test_clock *clock;
  clock = (test_clock *)context;
  assert(clock->batches < 32u);
  clock->counts[clock->batches++] = count;
  clock->now += (lonejson_uint64)count * (lonejson_uint64)1000000u;
  if (clock->batches == clock->pause_batch)
    clock->wall_delay += (lonejson_uint64)1000000000u;
  return clock->batches == clock->fail_batch ? 1u : 0u;
}

int main(void) {
  test_clock clock;
  bench_measurement result;
  size_t i;
  memset(&clock, 0, sizeof(clock));
  assert(bench_measure(&result, fake_clock, &clock, fake_batch, &clock, 1u, 0,
                       BENCH_TARGET_SAMPLE_NS));
  assert(result.batch_iterations == 12u);
  assert(result.sample_count == 1u && clock.batches == 4u);
  assert(result.warmup_elapsed_ns == (lonejson_uint64)1000000u);
  assert(result.cv_pct == 0.0 && result.drift_pct == 0.0);
  assert(clock.counts[0] == 1u && clock.counts[1] == 1u &&
         clock.counts[2] == 10u && clock.counts[3] == 12u);
  for (i = 0u; i < 1u; ++i) {
    assert(result.samples[i].elapsed_ns == (lonejson_uint64)12000000u);
    assert(result.samples[i].cpu_ns == (lonejson_uint64)12000000u);
  }
  memset(&clock, 0, sizeof(clock));
  clock.pause_batch = 2u;
  assert(bench_measure(&result, fake_clock, &clock, fake_batch, &clock, 1u, 0,
                       BENCH_TARGET_SAMPLE_NS));
  assert(result.batch_iterations == 12u && clock.batches == 4u);
  assert(result.samples[0].elapsed_ns == (lonejson_uint64)12000000u);
  memset(&clock, 0, sizeof(clock));
  clock.pause_batch = 2u;
  assert(bench_measure(&result, fake_clock, &clock, fake_batch, &clock, 5u, 1,
                       BENCH_TARGET_SAMPLE_NS));
  assert(result.samples[0].elapsed_ns == (lonejson_uint64)1005000000u);
  assert(result.samples[0].cpu_ns == (lonejson_uint64)5000000u);
  memset(&clock, 0, sizeof(clock));
  assert(bench_measure(&result, fake_clock, &clock, fake_batch, &clock, 5u, 1,
                       BENCH_TARGET_SAMPLE_NS));
  assert(clock.batches == 2u && clock.counts[0] == 1u && clock.counts[1] == 5u);
  result.samples[0].elapsed_ns = 0u;
  assert(!bench_measure_summarize(&result));

  memset(&clock, 0, sizeof(clock));
  clock.fail_batch = 1u;
  assert(!bench_measure(&result, fake_clock, &clock, fake_batch, &clock, 1u, 0,
                        BENCH_TARGET_SAMPLE_NS));
  assert(result.mismatch_count == 1u && result.sample_count == 0u);
  memset(&clock, 0, sizeof(clock));
  clock.fail_batch = 3u;
  assert(!bench_measure(&result, fake_clock, &clock, fake_batch, &clock, 1u, 0,
                        BENCH_TARGET_SAMPLE_NS));
  assert(result.mismatch_count == 1u && result.sample_count == 0u);
  memset(&clock, 0, sizeof(clock));
  clock.fail_batch = 4u;
  assert(!bench_measure(&result, fake_clock, &clock, fake_batch, &clock, 1u, 0,
                        BENCH_TARGET_SAMPLE_NS));
  assert(result.mismatch_count == 1u);
  memset(&clock, 0, sizeof(clock));
  clock.stopped = 1;
  assert(!bench_measure(&result, fake_clock, &clock, fake_batch, &clock, 1u, 0,
                        BENCH_TARGET_SAMPLE_NS));
  assert(!bench_measure(&result, fake_clock, &clock, fake_batch, &clock, 0u, 0,
                        BENCH_TARGET_SAMPLE_NS));
  memset(&clock, 0, sizeof(clock));
  clock.stopped_cpu = 1;
  assert(!bench_measure(&result, fake_clock, &clock, fake_batch, &clock, 1u, 0,
                        BENCH_TARGET_SAMPLE_NS));
  memset(&result, 0, sizeof(result));
  result.batch_iterations = 1u;
  result.sample_count = BENCH_SAMPLE_COUNT;
  for (i = 0u; i < BENCH_SAMPLE_COUNT; ++i) {
    result.samples[i].iterations = (lonejson_uint64)(i + 1u) * 100u;
    result.samples[i].elapsed_ns = result.samples[i].iterations * 10u;
    result.samples[i].cpu_ns = result.samples[i].elapsed_ns;
  }
  assert(bench_measure_summarize(&result));
  assert(result.mean_cost_ns == 10.0 && result.cv_pct == 0.0 &&
         result.drift_pct == 0.0);
  assert(bench_measure_elapsed_ns(&result) == 10u);
  assert(result.relative_error_pct == 0.0);
  for (i = 0u; i < BENCH_SAMPLE_COUNT; ++i) {
    long centered = (long)((i * 37u) % BENCH_SAMPLE_COUNT) - 199;
    result.samples[i].elapsed_ns =
        result.samples[i].iterations * (lonejson_uint64)(1000 + centered * 2);
    result.samples[i].cpu_ns = result.samples[i].elapsed_ns;
  }
  assert(bench_measure_summarize(&result));
  assert(result.cv_pct > 5.0 && result.relative_error_pct < 5.0);
  assert(result.mean_cost_ns == 1000.0);
  assert(fabs(result.relative_error_pct -
              (1000.0 / (1000.0 - 1.967 * sqrt(53200.0 / 399.0)) - 1.0) *
                  100.0) < 0.0000001);
  assert(result.drift_ci_low_pct <= 5.0 && result.drift_ci_high_pct >= -5.0);
  for (i = 0u; i < BENCH_SAMPLE_COUNT; ++i) {
    long centered = (long)((i * 37u) % BENCH_SAMPLE_COUNT) - 199;
    result.samples[i].elapsed_ns =
        result.samples[i].iterations * (lonejson_uint64)(2000 + centered * 10);
  }
  assert(bench_measure_summarize(&result));
  assert(result.relative_error_pct > BENCH_MAX_RELATIVE_ERROR_PCT);
  for (i = 0u; i < BENCH_SAMPLE_COUNT; ++i)
    result.samples[i].elapsed_ns =
        result.samples[i].iterations * (lonejson_uint64)(i / 133u + 1u) * 1000u;
  assert(bench_measure_summarize(&result));
  assert(result.mean_cost_ns == 2000.0);
  assert(fabs(result.drift_ci_low_pct - 200.0) < 0.0000001);
  assert(fabs(result.drift_ci_high_pct - 200.0) < 0.0000001);
  for (i = 0u; i < BENCH_SAMPLE_COUNT; ++i)
    result.samples[i].elapsed_ns = result.samples[i].iterations * 1000u;
  result.samples[0].elapsed_ns = result.samples[0].iterations * 9000u;
  assert(bench_measure_summarize(&result));
  assert(fabs(result.mean_cost_ns - 407000.0 / 399.0) < 0.000001);
  {
    size_t counts[50][2] = {{0}};
    size_t round, step;
    for (round = 1u; round <= 200u; ++round) {
      unsigned seen[50] = {0};
      for (step = 0u; step < 50u; ++step) {
        size_t index = bench_measure_case_index(50u, round, step);
        assert(index < 50u && seen[index]++ == 0u);
        if (index == 0u)
          ++counts[step][round % 2u];
      }
    }
    for (step = 0u; step < 50u; ++step)
      assert(counts[step][0] == 2u && counts[step][1] == 2u);
  }
  puts("PASS fixed work, excluded calibration/warmup, mean, counterbalance, "
       "instability, "
       "clock and correctness failures");
  return 0;
}
