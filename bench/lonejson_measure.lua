local M = {}
local file = assert(io.open('bench/lonejson_measure.h', 'rb'))
local policy = assert(file:read('*a'))
file:close()
M.method = assert(policy:match('#define BENCH_MEASUREMENT_METHOD "([^"]+)"'))
for _, name in ipairs({'sample_count', 'target_sample_ns', 'warmup_count',
    'confidence_pct', 'max_relative_error_pct', 'max_drift_pct'}) do
  M[name] = assert(tonumber(policy:match('#define BENCH_' .. name:upper() .. ' ([%d.]+)')))
end

function M.case_index(count, round, step)
  return (round * 17 + (round // 2 % 2 == 1 and count - 1 - step or step)) % count + 1
end

function M.measure(run_once, initial_iterations, wall_clock, cpu_clock, calibrated, target_sample_ns)
  assert(initial_iterations > 0 and initial_iterations == math.floor(initial_iterations), 'invalid work count')
  local function batch(count)
    for _ = 1, count do assert(run_once(), 'benchmark correctness failure') end
  end
  target_sample_ns = target_sample_ns or M.target_sample_ns
  local count = initial_iterations
  local before = wall_clock()
  batch(1)
  local warmup = wall_clock() - before
  assert(warmup > 0, 'non-advancing benchmark clock')
  if not calibrated then
    local converged = false
    for _ = 1, 16 do
      local before = cpu_clock()
      batch(count)
      local elapsed = cpu_clock() - before
      -- os.clock may have microsecond resolution. A single fast operation can
      -- fall within one tick; increase only the excluded calibration batch.
      -- A thousand operations without a tick still rejects a stuck clock.
      assert(elapsed >= 0 and (elapsed > 0 or count < 1000), 'non-advancing benchmark clock')
      if elapsed >= 3000000 then
        local planned = math.ceil(count * target_sample_ns / elapsed)
        assert(planned >= 1 and planned < math.maxinteger, 'work count overflow')
        count, converged = planned, true
        break
      end
      assert(count <= math.maxinteger // 10, 'work count overflow')
      count = count * 10
    end
    assert(converged, 'calibration did not converge')
  end
  local cpu_before = cpu_clock()
  before = wall_clock()
  batch(count)
  local after, cpu_after = wall_clock(), cpu_clock()
  assert(after > before and cpu_after > cpu_before, 'invalid benchmark clock')
  return {batch_iterations = count, warmup_elapsed_ns = warmup,
    samples = {{iterations = count, elapsed_ns = after - before,
      cpu_ns = cpu_after - cpu_before, process_index = 0, mismatch_count = 0}}}
end
return M
