local measurement = dofile(assert(arg[1]) .. '/bench/lonejson_measure.lua')
local clock, calls = 0, 0
local function run_once() clock = clock + 1000000; calls = calls + 1; return true end
local function wall() return clock end
local result = measurement.measure(run_once, 1, wall, wall, false)
assert(result.batch_iterations == 12 and #result.samples == 1)
assert(result.warmup_elapsed_ns == 1000000 and calls == 24)
assert(result.samples[1].elapsed_ns == 12000000 and result.samples[1].cpu_ns == 12000000)
do
  local cpu, delayed_wall, operations = 0, 0, 0
  local function delayed_once()
    cpu, delayed_wall, operations = cpu + 1000000, delayed_wall + 1000000, operations + 1
    if operations == 2 then delayed_wall = delayed_wall + 1000000000 end
    return true
  end
  local function wall_time() return delayed_wall end
  local function cpu_time() return cpu end
  local measured = measurement.measure(delayed_once, 1, wall_time, cpu_time, false)
  assert(measured.batch_iterations == 12 and operations == 24)
  assert(measured.samples[1].elapsed_ns == 12000000)
  cpu, delayed_wall, operations = 0, 0, 0
  measured = measurement.measure(delayed_once, 5, wall_time, cpu_time, true)
  assert(measured.samples[1].elapsed_ns == 1005000000)
  assert(measured.samples[1].cpu_ns == 5000000)
end
clock, calls = 0, 0
result = measurement.measure(run_once, 5, wall, wall, true)
assert(calls == 6 and result.batch_iterations == 5)
assert(not pcall(measurement.measure, run_once, 0, wall, wall, false))
assert(not pcall(measurement.measure, function() return true end, 1, function() return 0 end, wall, false))
assert(not pcall(measurement.measure, function() return false end, 1, wall, wall, false))
assert(not pcall(measurement.measure, run_once, 1, wall, function() return 0 end, false))
do
  local ticks = 0
  local function operation() ticks = ticks + 1; return true end
  local function precise_wall() return ticks * 100000 end
  local function coarse_cpu() return ticks // 10 * 1000000 end
  local measured = measurement.measure(operation, 1, precise_wall, coarse_cpu, false)
  assert(measured.batch_iterations == 120 and ticks == 232)
  assert(measured.samples[1].elapsed_ns == 12000000)
  assert(measured.samples[1].cpu_ns == 12000000)
  ticks = 0
  assert(not pcall(measurement.measure, operation, 1, precise_wall, function() return 0 end, false))
  assert(ticks <= 1200, 'a stuck coarse clock must fail with bounded work')
  assert(not pcall(measurement.measure, operation, 1, precise_wall, function() return -ticks end, false))
end
for _, count in ipairs({27, 50}) do
  local positions = {}
  for step = 0, count - 1 do positions[step] = {0, 0} end
  for round = 1, count * 4 do
    local seen = {}
    for step = 0, count - 1 do
      local index = measurement.case_index(count, round, step)
      assert(index >= 1 and index <= count and not seen[index])
      seen[index] = true
      if index == 1 then positions[step][round % 2 + 1] = positions[step][round % 2 + 1] + 1 end
    end
  end
  for step = 0, count - 1 do assert(positions[step][1] == 2 and positions[step][2] == 2) end
end
do
  collectgarbage('collect')
  assert(collectgarbage('isrunning'))
  local finalized = 0
  local mt = {__gc = function() finalized = finalized + 1 end}
  measurement.measure(function()
    local value = setmetatable({}, mt)
    clock = clock + 1000000
    return value ~= nil
  end, 50000, wall, wall, true)
  assert(finalized > 0 and collectgarbage('isrunning'), 'timed allocations must include automatic GC')
end
print('PASS Lua pilot exclusion, fixed confirmation work, clock and correctness failures')
-- Exercise every maintained Lua workload and its existing correctness assertions.
local work = assert(arg[2])
local function verify_round(output)
  arg = {'round', output, '-'}
  local original_collect, collections = collectgarbage, 0
  collectgarbage = function(operation, ...)
    if operation == 'collect' then collections = collections + 1 end
    return original_collect(operation, ...)
  end
  dofile('bench/lonejson_lua_bench.lua')
  collectgarbage = original_collect
  local json = assert(io.open(output, 'rb'))
  local report = require('lonejson').new():decode_json(json:read('*a'))
  json:close()
  assert(collections == #report.results, 'settle fixture and preceding-case allocations once per case')
  local names, count, definitions, budget = {}, 0, 0, 0
  for _, result in ipairs(report.results) do
    assert(not names[result.name] and result.mismatch_count == 0 and #result.samples == 1)
    names[result.name] = true
  end
  for line in io.lines('bench/workloads.tsv') do
    local ns = assert(tonumber(line:match('(%d+)$')))
    definitions, budget = definitions + 1, budget + ns
    local name = line:match('^lua%s+(%S+)')
    if name then assert(names[name], 'missing Lua workload: ' .. name); count = count + 1 end
  end
  assert(count == 27 and definitions == 77)
  assert(budget * measurement.sample_count * 2 <= 160000000000, "case budgets leave insufficient five-minute headroom")
  return report
end
verify_round(work .. '/lua-round.json')
-- Exercise optional-case accounting even on hosts without lua-cjson installed.
-- This stand-in tests collection/report ownership, not decoder performance.
local saved_cjson = package.loaded.cjson
local decoder = require('lonejson').new()
package.loaded.cjson = {decode = function(json) return decoder:decode_json(json) end}
local optional = verify_round(work .. '/lua-optional-round.json')
package.loaded.cjson = saved_cjson
local optional_count = 0
for _, result in ipairs(optional.results) do
  if result.name:sub(-6) == '/cjson' then optional_count = optional_count + 1 end
end
assert(optional_count == 11, 'exercise every optional decoder workload')
print('PASS all 27 Lua workloads and 11 optional decoder cases')
