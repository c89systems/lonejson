#!/usr/bin/env lua

local lj = require("lonejson")

local BASELINES = {
  c = "perflogs/hosts/%s/baseline.json",
  lua = "perflogs/hosts/%s/lua/baseline.json",
}

local DEFAULT_HOST_IDS = {
  "f259bcc951a8f53802cc755f08e5e218",
}

local function shell_quote(value)
  return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local function run_git(repo, args)
  local parts = { "git", "-C", shell_quote(repo), "-c", "log.showSignature=false" }
  for _, item in ipairs(args) do
    parts[#parts + 1] = shell_quote(item)
  end
  local pipe = assert(io.popen(table.concat(parts, " ") .. " 2>/dev/null", "r"))
  local output = pipe:read("*a")
  local ok, why, code = pipe:close()
  if not ok then
    error(string.format("git command failed: %s (%s %s)", table.concat(args, " "), tostring(why), tostring(code)))
  end
  return output
end

local function load_json_at(repo, commit, path)
  local ok, output = pcall(run_git, repo, { "show", commit .. ":" .. path })
  if not ok then
    return nil
  end
  return lj.decode_json(output)
end

local function serialize_key(run)
  local parts = {
    tostring(run.schema_version),
    tostring(run.method),
    tostring(run.timestamp_utc),
    tostring(run.host),
  }
  for _, result in ipairs(run.results or {}) do
    parts[#parts + 1] = tostring(result.name)
    parts[#parts + 1] = tostring(result.mib_per_sec)
  end
  return table.concat(parts, "|")
end

local function format_baseline_path(kind, host_id)
  return string.format(BASELINES[kind], host_id)
end

local function load_snapshots(repo, kind, limit, host_id)
  local path = format_baseline_path(kind, host_id)
  local log = run_git(repo, {
    "log",
    "--reverse",
    "--format=%H%x00%h%x00%ad%x00%s",
    "--date=short",
    "--",
    path,
  })
  local rows = {}
  for line in log:gmatch("[^\n]+") do
    rows[#rows + 1] = line
  end
  if limit and limit > 0 and #rows > limit then
    local trimmed = {}
    for i = #rows - limit + 1, #rows do
      trimmed[#trimmed + 1] = rows[i]
    end
    rows = trimmed
  end

  local snapshots = {}
  local last_key = nil
  for _, line in ipairs(rows) do
    local commit, short_commit, commit_date, subject =
      line:match("^([^\0]*)%z([^\0]*)%z([^\0]*)%z(.*)$")
    if commit then
      local run = load_json_at(repo, commit, path)
      if run then
        local key = serialize_key(run)
        if key ~= last_key then
          last_key = key
          local results = {}
          for _, result in ipairs(run.results or {}) do
            if type(result.name) == "string" and result.name ~= "" and type(result.mib_per_sec) == "number" then
              results[result.name] = result.mib_per_sec
            end
          end
          snapshots[#snapshots + 1] = {
            label = "B" .. tostring(#snapshots),
            short_commit = short_commit,
            commit_date = commit_date,
            subject = subject,
            schema = run.schema_version or "?",
            method = run.method or "unspecified",
            run_time = run.timestamp_utc or "?",
            host = run.host or "?",
            host_id = host_id,
            results = results,
          }
        end
      end
    end
  end
  return snapshots
end

local function pct_delta(prior, current)
  if prior == 0 then
    return 0
  end
  return ((current - prior) / prior) * 100
end

local function classify(values, small, material, snapshots)
  local severity = "ok"
  local peak = nil
  local previous = nil
  for i, value in ipairs(values) do
    if i > 1 and snapshots[i].method ~= snapshots[i - 1].method then
      peak, previous = nil, nil
    end
    if value ~= false then
      if previous then
        local delta = pct_delta(previous, value)
        if delta <= -material then
          return "regression"
        elseif delta <= -small then
          severity = "small"
        end
      end
      if peak then
        local delta = pct_delta(peak, value)
        if delta <= -material then
          return "regression"
        elseif delta <= -small then
          severity = "small"
        end
      end
      if not peak or value > peak then
        peak = value
      end
      previous = value
    end
  end
  return severity
end

local function worst_regression(values, snapshots)
  local worst = nil
  local peak_value = nil
  local peak_label = nil
  local previous = nil
  local previous_label = nil
  for i, value in ipairs(values) do
    if i > 1 and snapshots[i].method ~= snapshots[i - 1].method then
      peak_value, peak_label, previous, previous_label = nil, nil, nil, nil
    end
    if value ~= false then
      local candidates = {}
      if previous then
        candidates[#candidates + 1] = { pct_delta(previous, value), previous_label, snapshots[i].label }
      end
      if peak_value then
        candidates[#candidates + 1] = { pct_delta(peak_value, value), peak_label, snapshots[i].label }
      end
      for _, candidate in ipairs(candidates) do
        if not worst or candidate[1] < worst[1] then
          worst = candidate
        end
      end
      if not peak_value or value > peak_value then
        peak_value = value
        peak_label = snapshots[i].label
      end
      previous = value
      previous_label = snapshots[i].label
    end
  end
  return worst
end

local function trim(text, width)
  text = tostring(text)
  if #text <= width then
    return text
  end
  if width <= 1 then
    return text:sub(1, width)
  end
  return text:sub(1, width - 1) .. "~"
end

local function format_value(value)
  if value == false then
    return "."
  elseif value >= 10000 then
    return string.format("%.0f", value)
  elseif value >= 1000 then
    return string.format("%.0f", value)
  elseif value >= 100 then
    return string.format("%.1f", value)
  elseif value >= 10 then
    return string.format("%.2f", value)
  end
  return string.format("%.3f", value)
end

local function metric_sort_key(name)
  local group, rest = name:match("^([^/]+)/(.*)$")
  return (group or name) .. "\0" .. (rest or "")
end

local function sorted_metric_names(snapshots)
  local seen = {}
  local names = {}
  for _, snapshot in ipairs(snapshots) do
    for name in pairs(snapshot.results) do
      if not seen[name] then
        seen[name] = true
        names[#names + 1] = name
      end
    end
  end
  table.sort(names, function(a, b)
    return metric_sort_key(a) < metric_sort_key(b)
  end)
  return names
end

local function count_results(results)
  local count = 0
  for _ in pairs(results) do
    count = count + 1
  end
  return count
end

local function print_kind(kind, snapshots, small, material)
  local title = kind == "c" and "C" or "Lua"
  print(title .. " baseline history")
  print(string.rep("-", 80))
  if #snapshots == 0 then
    print("no baselines found")
    print("")
    return 0
  end

  print("id  commit   date        schema run-time             host-id  results method")
  for _, snapshot in ipairs(snapshots) do
    print(string.format(
      "%-3s %-7s %-10s %-6s %-20s %-8s %7d %s",
      snapshot.label,
      snapshot.short_commit,
      snapshot.commit_date,
      tostring(snapshot.schema),
      snapshot.run_time,
      trim(snapshot.host_id or snapshot.host, 8),
      count_results(snapshot.results), trim(snapshot.method, 12)
    ))
  end
  print("")

  local rows = {}
  local summary = { regression = {}, small = {} }
  for _, name in ipairs(sorted_metric_names(snapshots)) do
    local values = {}
    for _, snapshot in ipairs(snapshots) do
      values[#values + 1] = snapshot.results[name] or false
    end
    local severity = classify(values, small, material, snapshots)
    if severity ~= "ok" then
      local worst = worst_regression(values, snapshots)
      if worst then
        summary[severity][#summary[severity] + 1] = {
          delta = worst[1],
          name = name,
          prior = worst[2],
          current = worst[3],
        }
      end
    end
    rows[#rows + 1] = { severity = severity, name = name, values = values }
  end

  if #summary.regression > 0 then
    print(string.format("REGRESSION: %d metric(s) dropped >= %.0f%%", #summary.regression, material))
  end
  if #summary.small > 0 then
    print(string.format("SMALL-REGRESSION: %d metric(s) dropped >= %.0f%%", #summary.small, small))
  end
  if #summary.regression == 0 and #summary.small == 0 then
    print("no regressions detected across frozen baselines")
  end
  print("")

  local notable = {}
  for _, item in ipairs(summary.regression) do
    notable[#notable + 1] = item
  end
  for _, item in ipairs(summary.small) do
    notable[#notable + 1] = item
  end
  table.sort(notable, function(a, b)
    return a.delta < b.delta
  end)
  if #notable > 0 then
    print("worst drops")
    print("drop     from to  metric")
    for i = 1, math.min(12, #notable) do
      local item = notable[i]
      print(string.format("%6.1f%% %-4s %-3s %s", item.delta, item.prior, item.current, trim(item.name, 58)))
    end
    print("")
  end

  local max_cols = math.max(1, math.floor((80 - 2 - 24) / 7))
  for start = 1, #snapshots, max_cols do
    local chunk_count = math.min(max_cols, #snapshots - start + 1)
    local name_width = 80 - 2 - (chunk_count * 7)
    name_width = math.min(42, math.max(24, name_width))
    io.write("MiB/s progression\n")
    io.write("  " .. string.format("%-" .. name_width .. "s", "metric"))
    for offset = 0, chunk_count - 1 do
      io.write(string.format(" %6s", snapshots[start + offset].label))
    end
    io.write("\n")
    for _, row in ipairs(rows) do
      local marker = row.severity == "regression" and "!" or row.severity == "small" and "~" or " "
      io.write(marker .. " " .. string.format("%-" .. name_width .. "s", trim(row.name, name_width)))
      for offset = 0, chunk_count - 1 do
        io.write(string.format(" %6s", format_value(row.values[start + offset])))
      end
      io.write("\n")
    end
    print("")
  end
  return #summary.regression
end

local function parse_args(argv)
  local args = {
    repo = ".",
    kind = "all",
    limit = nil,
    small = 3.0,
    material = 10.0,
    host_ids = DEFAULT_HOST_IDS,
  }
  local i = 1
  while i <= #argv do
    local item = argv[i]
    if item == "--repo" then
      i = i + 1
      args.repo = assert(argv[i], "--repo requires a value")
    elseif item == "--kind" then
      i = i + 1
      args.kind = assert(argv[i], "--kind requires a value")
    elseif item == "--limit" then
      i = i + 1
      args.limit = tonumber(assert(argv[i], "--limit requires a value"))
    elseif item == "--small" then
      i = i + 1
      args.small = tonumber(assert(argv[i], "--small requires a value"))
    elseif item == "--material" then
      i = i + 1
      args.material = tonumber(assert(argv[i], "--material requires a value"))
    elseif item == "--host-id" then
      i = i + 1
      args.host_ids = { assert(argv[i], "--host-id requires a value") }
    elseif item == "--help" or item == "-h" then
      print("usage: bench_baseline_history.lua [--repo DIR] [--kind all|c|lua]")
      print("                                  [--host-id HASH] [--limit N] [--small PCT]")
      print("                                  [--material PCT]")
      os.exit(0)
    else
      error("unknown argument: " .. item)
    end
    i = i + 1
  end
  if args.kind ~= "all" and args.kind ~= "c" and args.kind ~= "lua" then
    error("--kind must be all, c, or lua")
  end
  return args
end

local args = parse_args(arg)
local kinds = args.kind == "all" and { "c", "lua" } or { args.kind }
local total_regressions = 0
local section = 0
for _, host_id in ipairs(args.host_ids) do
  for _, kind in ipairs(kinds) do
    section = section + 1
    if section > 1 then
      print("")
    end
    total_regressions = total_regressions
      + print_kind(kind, load_snapshots(args.repo, kind, args.limit, host_id), args.small, args.material)
  end
end
if total_regressions > 0 then
  print(string.format("NOTICE: %d material regression(s); exit status remains 0.", total_regressions))
end
