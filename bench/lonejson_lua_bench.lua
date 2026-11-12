local lonejson = require("lonejson")
local lj = lonejson.new()
local lj_reuse = lonejson.new({ clear_destination_by_default = false })
local lj_pretty = lonejson.new({ write_pretty = true })
local core = lonejson.core
local HAVE_CJSON, cjson = pcall(require, "cjson")

local measurement = dofile('bench/lonejson_measure.lua')

local BENCH_SCHEMA_VERSION = 30

local PARSE_JSON = table.concat({
  '{"name":"Alice","nickname":"Wonderland","age":34,',
  '"score":99.5,"active":true,',
  '"address":{"city":"Stockholm","zip":12345},',
  '"lucky_numbers":[7,11,42],',
  '"tags":["admin","ops"],',
  '"items":[{"id":1,"label":"alpha"},{"id":2,',
  '"label":"bravo-longer-than-fit"}]}',
})

local VENDOR_LONG_STRINGS_JSON =
    '{"x":[{"id": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"}], "id": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"}'
local VENDOR_EXTREME_NUMBERS_JSON = '{ "min": -1.0e+28, "max": 1.0e+28 }'
local VENDOR_STRING_UNICODE_JSON =
    '{"title":"\\u041f\\u043e\\u043b\\u0442\\u043e\\u0440\\u0430 \\u0417\\u0435\\u043c\\u043b\\u0435\\u043a\\u043e\\u043f\\u0430" }'

local function benchmark_host_id()
  local env = os.getenv("LONEJSON_BENCH_HOST_ID")
  local p
  local out
  if env ~= nil and env ~= "" then
    return env
  end
  p = io.popen("./scripts/bench_host_id.sh 2>/dev/null", "r")
  if p == nil then
    return "unknown"
  end
  out = p:read("*a") or ""
  p:close()
  out = out:match("^(%S+)") or ""
  if out == "" then
    return "unknown"
  end
  return out
end

local function read_file(path)
  local f = assert(io.open(path, "rb"))
  local data = assert(f:read("*a"))
  f:close()
  return data
end

local JAPANESE_UTF8_JSON = read_file("tests/fixtures/languages/japanese_hojoki.json")
local HEBREW_UTF8_JSON = read_file("tests/fixtures/languages/hebrew_fox_and_stork.json")
local ARABIC_UTF8_JSON = read_file("tests/fixtures/languages/arabic_kalila_intro.json")
local JAPANESE_UTF8_WIDE_JSON = read_file("tests/fixtures/languages/japanese_hojoki_wide.json")
local HEBREW_UTF8_WIDE_JSON = read_file("tests/fixtures/languages/hebrew_fox_and_stork_wide.json")
local ARABIC_UTF8_WIDE_JSON = read_file("tests/fixtures/languages/arabic_kalila_intro_wide.json")

local JAPANESE_UTF8_PREFIX = "行く川のながれは絶えずして、しかも本の水にあらず。"
local HEBREW_UTF8_PREFIX = "השועל והחסידה הוא משל."
local ARABIC_UTF8_PREFIX = "قدمها بهنود بن سحوان ويعرف بعلي بن الشاه الفارسي."

local function utc_timestamp_from_ns(ns)
  local sec = math.floor(ns / 1000000000)
  return os.date("!%Y-%m-%dT%H:%M:%SZ", sec)
end

local function wall_clock_epoch_ns()
  return os.time() * 1000000000
end

local Address = lj.object {
  fields = {
    lj.field("city", lj.string { required = true, fixed_capacity = 24, overflow = "fail" }),
    lj.field("zip", lj.i64 { required = true }),
  },
}

local Item = lj.object {
  fields = {
    lj.field("id", lj.i64 { required = true }),
    lj.field("label", lj.string { fixed_capacity = 16, overflow = "truncate" }),
  },
}

local function build_fixed_record_schema(runtime, name)
  return runtime.schema(name, {
    lj.field("name", lj.string { required = true, fixed_capacity = 24, overflow = "fail" }),
    lj.field("nickname", lj.string { fixed_capacity = 16, overflow = "truncate" }),
    lj.field("age", lj.i64()),
    lj.field("score", lj.f64()),
    lj.field("active", lj.bool()),
    lj.field("address", lj.object { required = true, fields = Address.fields }),
    lj.field("lucky_numbers", lj.i64_array { fixed_capacity = 8, overflow = "fail" }),
    lj.field("items", lj.object_array { fields = Item.fields, fixed_capacity = 8, overflow = "fail" }),
  })
end

local FixedRecord = build_fixed_record_schema(lj, "BenchFixed")
local FixedRecordReuse = build_fixed_record_schema(lj_reuse, "BenchFixedReuse")
local FixedRecordPretty = build_fixed_record_schema(lj_pretty, "BenchFixedPretty")

local DynamicRecord = lj.schema("BenchDynamic", {
  lj.field("name", lj.string { required = true }),
  lj.field("nickname", lj.string { fixed_capacity = 16, overflow = "truncate" }),
  lj.field("age", lj.i64()),
  lj.field("score", lj.f64()),
  lj.field("active", lj.bool()),
  lj.field("address", lj.object { required = true, fields = Address.fields }),
  lj.field("lucky_numbers", lj.i64_array { overflow = "fail" }),
  lj.field("tags", lj.string_array { overflow = "fail" }),
  lj.field("items", lj.object_array { fields = Item.fields, overflow = "fail" }),
})

local VendorLong = lj.schema("BenchVendorLong", {
  lj.field("x", lj.object_array {
    overflow = "fail",
    fields = {
      lj.field("id", lj.string { required = true, fixed_capacity = 48, overflow = "fail" }),
    },
  }),
  lj.field("id", lj.string { required = true, fixed_capacity = 48, overflow = "fail" }),
})

local VendorNumbers = lj.schema("BenchVendorNumbers", {
  lj.field("min", lj.f64 { required = true }),
  lj.field("max", lj.f64 { required = true }),
})

local VendorUnicode = lj.schema("BenchVendorUnicode", {
  lj.field("title", lj.string { required = true, fixed_capacity = 96, overflow = "fail" }),
})

local VendorLanguage = lj.schema("BenchVendorLanguage", {
  lj.field("title", lj.string { required = true, fixed_capacity = 2048, overflow = "fail" }),
})

local VendorLanguageWide = lj.schema("BenchVendorLanguageWide", {
  lj.field("lang", lj.string { required = true, fixed_capacity = 8, overflow = "fail" }),
  lj.field("script", lj.string { required = true, fixed_capacity = 24, overflow = "fail" }),
  lj.field("source", lj.string { required = true, fixed_capacity = 32, overflow = "fail" }),
  lj.field("title", lj.string { required = true, fixed_capacity = 64, overflow = "fail" }),
  lj.field("summary", lj.string { required = true, fixed_capacity = 256, overflow = "fail" }),
  lj.field("body", lj.string { required = true, fixed_capacity = 2048, overflow = "fail" }),
  lj.field("metrics", lj.object {
    required = true,
    fields = {
      lj.field("chars", lj.i64 { required = true }),
      lj.field("segments", lj.i64 { required = true }),
    },
  }),
})

local PhaseStats = lj.object {
  fields = {
    lj.field("ops", lj.i64()),
    lj.field("errors", lj.i64()),
  },
}

local WorkloadPhasesFields = {
  lj.field("acquire", lj.object { fields = PhaseStats.fields }),
  lj.field("attach", lj.object { fields = PhaseStats.fields }),
  lj.field("delete", lj.object { fields = PhaseStats.fields }),
  lj.field("get", lj.object { fields = PhaseStats.fields }),
  lj.field("release", lj.object { fields = PhaseStats.fields }),
  lj.field("total", lj.object { fields = PhaseStats.fields }),
  lj.field("update", lj.object { fields = PhaseStats.fields }),
  lj.field("query", lj.object { fields = PhaseStats.fields }),
  lj.field("acquire-a", lj.object { fields = PhaseStats.fields }),
  lj.field("acquire-b", lj.object { fields = PhaseStats.fields }),
  lj.field("decide", lj.object { fields = PhaseStats.fields }),
  lj.field("update-a", lj.object { fields = PhaseStats.fields }),
  lj.field("update-b", lj.object { fields = PhaseStats.fields }),
}

local WorkloadCase = lj.object {
  fields = {
    lj.field("workload", lj.string { required = true, fixed_capacity = 24, overflow = "fail" }),
    lj.field("size", lj.i64()),
    lj.field("ops", lj.i64 { required = true }),
    lj.field("payload_bytes", lj.i64()),
    lj.field("attachment_bytes", lj.i64()),
    lj.field("phases", lj.object { required = true, fields = WorkloadPhasesFields }),
  },
}

local function build_workload_run_schema(runtime, name)
  return runtime.schema(name, {
    lj.field("run_id", lj.string { required = true, fixed_capacity = 64, overflow = "fail" }),
    lj.field("timestamp", lj.string { required = true, fixed_capacity = 32, overflow = "fail" }),
    lj.field("history_branch", lj.string { fixed_capacity = 32, overflow = "fail" }),
    lj.field("preset", lj.string { required = true, fixed_capacity = 16, overflow = "fail" }),
    lj.field("backend", lj.string { required = true, fixed_capacity = 16, overflow = "fail" }),
    lj.field("store", lj.string { fixed_capacity = 32, overflow = "fail" }),
    lj.field("disk_root", lj.string { fixed_capacity = 128, overflow = "fail" }),
    lj.field("git", lj.object {
      required = true,
      fields = {
        lj.field("short_sha", lj.string { required = true, fixed_capacity = 16, overflow = "fail" }),
        lj.field("dirty", lj.bool { required = true }),
      },
    }),
    lj.field("golden", lj.bool()),
    lj.field("config", lj.object {
      required = true,
      fields = {
        lj.field("ha_mode", lj.string { required = true, fixed_capacity = 16, overflow = "fail" }),
        lj.field("concurrency", lj.i64 { required = true }),
        lj.field("runs", lj.i64 { required = true }),
        lj.field("warmup", lj.i64 { required = true }),
        lj.field("query_return", lj.string { fixed_capacity = 16, overflow = "fail" }),
        lj.field("qrf_disabled", lj.bool()),
      },
    }),
    lj.field("cases", lj.object_array { fields = WorkloadCase.fields, fixed_capacity = 16, overflow = "fail" }),
  })
end

local WorkloadRun = build_workload_run_schema(lj, "BenchWorkloadRun")
local WorkloadRunReuse = build_workload_run_schema(lj_reuse, "BenchWorkloadRunReuse")

local WORKLOAD_JSONL = read_file("tests/fixtures/workloadbench.jsonl")
local BENCH_RECORD_COMPACT_JSON = read_file("tests/golden/bench_record_compact.json")
local BENCH_RECORD_PRETTY_JSON = read_file("tests/golden/bench_record_pretty.json")
local WORKLOAD_LINES = {}
for line in WORKLOAD_JSONL:gmatch("[^\r\n]+") do
  WORKLOAD_LINES[#WORKLOAD_LINES + 1] = line
end
local EXPECTED_LABEL = "bravo-longer-th"

local function build_serialize_source()
  return {
    name = "Alice",
    nickname = "Wonderland",
    age = 34,
    score = 99.5,
    active = true,
    address = { city = "Stockholm", zip = 12345 },
    lucky_numbers = { 7, 11, 42 },
    items = {
      { id = 1, label = "alpha" },
      { id = 2, label = "bravo" },
    },
  }
end
local EXPECTED_JSON = BENCH_RECORD_COMPACT_JSON
local EXPECTED_PRETTY_JSON = BENCH_RECORD_PRETTY_JSON
local STREAM_JSON = table.concat({
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
  EXPECTED_JSON,
}, "")

local FIXED_PATHS = {
  name = FixedRecord:compile_get("name"),
  nickname = FixedRecord:compile_get("nickname"),
  age = FixedRecord:compile_get("age"),
  score = FixedRecord:compile_get("score"),
  active = FixedRecord:compile_get("active"),
  address_city = FixedRecord:compile_get("address.city"),
  address_zip = FixedRecord:compile_get("address.zip"),
  lucky_numbers_count = FixedRecord:compile_count("lucky_numbers"),
  lucky_1 = FixedRecord:compile_get("lucky_numbers[1]"),
  lucky_2 = FixedRecord:compile_get("lucky_numbers[2]"),
  lucky_3 = FixedRecord:compile_get("lucky_numbers[3]"),
  items_count = FixedRecord:compile_count("items"),
  item1_id = FixedRecord:compile_get("items[1].id"),
  item1_label = FixedRecord:compile_get("items[1].label"),
  item2_id = FixedRecord:compile_get("items[2].id"),
  item2_label = FixedRecord:compile_get("items[2].label"),
}

local FIXED_PATHS_REUSE = {
  name = FixedRecordReuse:compile_get("name"),
  nickname = FixedRecordReuse:compile_get("nickname"),
  age = FixedRecordReuse:compile_get("age"),
  score = FixedRecordReuse:compile_get("score"),
  active = FixedRecordReuse:compile_get("active"),
  address_city = FixedRecordReuse:compile_get("address.city"),
  address_zip = FixedRecordReuse:compile_get("address.zip"),
  lucky_numbers_count = FixedRecordReuse:compile_count("lucky_numbers"),
  lucky_1 = FixedRecordReuse:compile_get("lucky_numbers[1]"),
  lucky_2 = FixedRecordReuse:compile_get("lucky_numbers[2]"),
  lucky_3 = FixedRecordReuse:compile_get("lucky_numbers[3]"),
  items_count = FixedRecordReuse:compile_count("items"),
  item1_id = FixedRecordReuse:compile_get("items[1].id"),
  item1_label = FixedRecordReuse:compile_get("items[1].label"),
  item2_id = FixedRecordReuse:compile_get("items[2].id"),
  item2_label = FixedRecordReuse:compile_get("items[2].label"),
}

local WORKLOAD_PATHS = {
  run_id = WorkloadRun:compile_get("run_id"),
  git_short_sha = WorkloadRun:compile_get("git.short_sha"),
  git_dirty = WorkloadRun:compile_get("git.dirty"),
  config_concurrency = WorkloadRun:compile_get("config.concurrency"),
  cases_count = WorkloadRun:compile_count("cases"),
  case1_workload = WorkloadRun:compile_get("cases[1].workload"),
  case5_workload = WorkloadRun:compile_get("cases[5].workload"),
  case9_workload = WorkloadRun:compile_get("cases[9].workload"),
  case1_acquire_ops = WorkloadRun:compile_get("cases[1].phases.acquire.ops"),
}

local WORKLOAD_PATHS_REUSE = {
  run_id = WorkloadRunReuse:compile_get("run_id"),
  git_short_sha = WorkloadRunReuse:compile_get("git.short_sha"),
  git_dirty = WorkloadRunReuse:compile_get("git.dirty"),
  config_concurrency = WorkloadRunReuse:compile_get("config.concurrency"),
  cases_count = WorkloadRunReuse:compile_count("cases"),
  case1_workload = WorkloadRunReuse:compile_get("cases[1].workload"),
  case5_workload = WorkloadRunReuse:compile_get("cases[5].workload"),
  case9_workload = WorkloadRunReuse:compile_get("cases[9].workload"),
  case1_acquire_ops = WorkloadRunReuse:compile_get("cases[1].phases.acquire.ops"),
}

local function assert_fixed_record_with_paths(paths, rec, item2_label)
  if paths.name(rec) ~= "Alice" or paths.nickname(rec) ~= "Wonderland" or paths.age(rec) ~= 34 or paths.score(rec) ~= 99.5 or paths.active(rec) ~= true then
    return false
  end
  if paths.address_city(rec) ~= "Stockholm" or paths.address_zip(rec) ~= 12345 then
    return false
  end
  if paths.lucky_numbers_count(rec) ~= 3 or paths.lucky_1(rec) ~= 7 or paths.lucky_2(rec) ~= 11 or paths.lucky_3(rec) ~= 42 then
    return false
  end
  if paths.items_count(rec) ~= 2 or paths.item1_id(rec) ~= 1 or paths.item1_label(rec) ~= "alpha" or paths.item2_id(rec) ~= 2 or paths.item2_label(rec) ~= item2_label then
    return false
  end
  return true
end

local function assert_fixed_record(rec)
  return assert_fixed_record_with_paths(FIXED_PATHS, rec, EXPECTED_LABEL)
end

local function assert_fixed_record_reuse(rec)
  return assert_fixed_record_with_paths(FIXED_PATHS_REUSE, rec, EXPECTED_LABEL)
end

local function assert_stream_fixed_record(rec)
  return assert_fixed_record_with_paths(FIXED_PATHS, rec, "bravo")
end

local function assert_stream_fixed_record_reuse(rec)
  return assert_fixed_record_with_paths(FIXED_PATHS_REUSE, rec, "bravo")
end

local function assert_dynamic_record(obj)
  if obj.name ~= "Alice" or obj.nickname ~= "Wonderland" or obj.age ~= 34 or obj.score ~= 99.5 or obj.active ~= true then
    return false
  end
  if obj.address.city ~= "Stockholm" or obj.address.zip ~= 12345 then
    return false
  end
  if #obj.lucky_numbers ~= 3 or obj.lucky_numbers[1] ~= 7 or obj.lucky_numbers[2] ~= 11 or obj.lucky_numbers[3] ~= 42 then
    return false
  end
  if #obj.tags ~= 2 or obj.tags[1] ~= "admin" or obj.tags[2] ~= "ops" then
    return false
  end
  if #obj.items ~= 2 or obj.items[1].id ~= 1 or obj.items[1].label ~= "alpha" or obj.items[2].id ~= 2 or obj.items[2].label ~= EXPECTED_LABEL then
    return false
  end
  return true
end

local function assert_plain_table_record(obj)
  if obj.name ~= "Alice" or obj.nickname ~= "Wonderland" or obj.age ~= 34 or obj.score ~= 99.5 or obj.active ~= true then
    return false
  end
  if obj.address == nil or obj.address.city ~= "Stockholm" or obj.address.zip ~= 12345 then
    return false
  end
  if type(obj.lucky_numbers) ~= "table" or #obj.lucky_numbers ~= 3 or obj.lucky_numbers[1] ~= 7 or obj.lucky_numbers[2] ~= 11 or obj.lucky_numbers[3] ~= 42 then
    return false
  end
  if type(obj.items) ~= "table" or #obj.items ~= 2 or obj.items[1].id ~= 1 or obj.items[1].label ~= "alpha" or obj.items[2].id ~= 2 or obj.items[2].label ~= "bravo-longer-than-fit" then
    return false
  end
  return true
end

local function assert_vendor_long_table(obj)
  return obj ~= nil and obj.id == "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      and type(obj.x) == "table" and #obj.x == 1
      and type(obj.x[1]) == "table"
      and obj.x[1].id == "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
end

local function assert_vendor_numbers_table(obj)
  return obj ~= nil and obj.min ~= nil and obj.max ~= nil
      and obj.min < -9.9e27 and obj.min > -1.1e28
      and obj.max > 9.9e27 and obj.max < 1.1e28
end

local function assert_vendor_unicode_table(obj)
  return obj ~= nil and obj.title ~= nil and #obj.title > 0
end

local function assert_vendor_language_table(obj, prefix)
  return obj ~= nil and obj.title ~= nil and #obj.title > #prefix and obj.title:sub(1, #prefix) == prefix
end

local function assert_vendor_language_wide_table(obj, lang, script, source, summary_prefix, body_prefix, min_chars)
  return obj ~= nil
      and obj.lang == lang
      and obj.script == script
      and obj.source == source
      and obj.title ~= nil and #obj.title > 0
      and obj.summary ~= nil and obj.summary:sub(1, #summary_prefix) == summary_prefix
      and obj.body ~= nil and obj.body:sub(1, #body_prefix) == body_prefix
      and obj.metrics ~= nil
      and obj.metrics.segments == 4
      and obj.metrics.chars ~= nil
      and obj.metrics.chars >= min_chars
end

local function assert_workload_table(obj, index)
  local ids = {
    "20260322T130525Z-b62770c-dirty-fast-disk",
    "20260322T141810Z-b62770c-dirty-fast-disk",
    "20260323T003016Z-08a87c0-clean-fast-disk",
    "20260325T004115Z-55f841b-dirty-fast-disk",
    "20260325T155329Z-e66dc79-dirty-fast-disk",
  }
  return obj ~= nil
      and obj.run_id == ids[index]
      and obj.git ~= nil
      and obj.git.short_sha ~= nil
      and obj.git.dirty ~= nil
      and obj.config ~= nil
      and obj.config.concurrency == 8
      and type(obj.cases) == "table"
      and #obj.cases == 9
      and obj.cases[1] ~= nil
      and obj.cases[1].workload == "attachments"
      and obj.cases[5] ~= nil
      and obj.cases[5].workload == "public-read"
      and obj.cases[9] ~= nil
      and obj.cases[9].workload == "xa-rollback"
      and obj.cases[1].phases ~= nil
      and obj.cases[1].phases.acquire ~= nil
      and obj.cases[1].phases.acquire.ops == 4000
end

local function cjson_decode(json)
  return cjson.decode(json)
end

local function bench_cases()
  local cases = {}

  do
    local rec = VendorLong:new_record()
    cases[#cases + 1] = {
      name = "decode/table_dynamic/lua",
      group = "decode",
      sibling_name = "parse/buffer_dynamic/lonejson",
      bytes_per_call = #PARSE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_dynamic_record(DynamicRecord:decode(PARSE_JSON))
      end,
    }
  end

  do
    local rec = VendorLong:new_record()
    cases[#cases + 1] = {
      name = "decode/doc_long_strings/lua",
      group = "decode",
      bytes_per_call = #VENDOR_LONG_STRINGS_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_long_table(VendorLong:decode(VENDOR_LONG_STRINGS_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_extreme_numbers/lua",
      group = "decode",
      bytes_per_call = #VENDOR_EXTREME_NUMBERS_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_numbers_table(VendorNumbers:decode(VENDOR_EXTREME_NUMBERS_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_string_unicode/lua",
      group = "decode",
      bytes_per_call = #VENDOR_STRING_UNICODE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_unicode_table(VendorUnicode:decode(VENDOR_STRING_UNICODE_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/workloadbench/lua",
      group = "decode",
      bytes_per_call = #WORKLOAD_JSONL,
      docs_per_call = #WORKLOAD_LINES,
      run_once = function()
        local i
        for i = 1, #WORKLOAD_LINES do
          if not assert_workload_table(WorkloadRun:decode(WORKLOAD_LINES[i]), i) then
            return false
          end
        end
        return true
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_japanese/lua",
      group = "decode",
      bytes_per_call = #JAPANESE_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(VendorLanguage:decode(JAPANESE_UTF8_JSON), JAPANESE_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_hebrew/lua",
      group = "decode",
      bytes_per_call = #HEBREW_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(VendorLanguage:decode(HEBREW_UTF8_JSON), HEBREW_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_arabic/lua",
      group = "decode",
      bytes_per_call = #ARABIC_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(VendorLanguage:decode(ARABIC_UTF8_JSON), ARABIC_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_japanese_wide/lua",
      group = "decode",
      bytes_per_call = #JAPANESE_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(VendorLanguageWide:decode(JAPANESE_UTF8_WIDE_JSON),
          "ja", "japanese", "hojoki", JAPANESE_UTF8_PREFIX, JAPANESE_UTF8_PREFIX, 180)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_hebrew_wide/lua",
      group = "decode",
      bytes_per_call = #HEBREW_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(VendorLanguageWide:decode(HEBREW_UTF8_WIDE_JSON),
          "he", "hebrew", "aesop_wikisource", "השועל מזמין את החסידה לביתו", HEBREW_UTF8_PREFIX, 220)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_arabic_wide/lua",
      group = "decode",
      bytes_per_call = #ARABIC_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(VendorLanguageWide:decode(ARABIC_UTF8_WIDE_JSON),
          "ar", "arabic", "kalila_dimna", "مقدمة تشرح سبب وضع كتاب كليلة", ARABIC_UTF8_PREFIX, 220)
      end,
    }
  end

  if HAVE_CJSON then
    cases[#cases + 1] = {
      name = "decode/table_dynamic/cjson",
      group = "decode",
      bytes_per_call = #PARSE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_plain_table_record(cjson_decode(PARSE_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_long_strings/cjson",
      group = "decode",
      bytes_per_call = #VENDOR_LONG_STRINGS_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_long_table(cjson_decode(VENDOR_LONG_STRINGS_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_extreme_numbers/cjson",
      group = "decode",
      bytes_per_call = #VENDOR_EXTREME_NUMBERS_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_numbers_table(cjson_decode(VENDOR_EXTREME_NUMBERS_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_string_unicode/cjson",
      group = "decode",
      bytes_per_call = #VENDOR_STRING_UNICODE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_unicode_table(cjson_decode(VENDOR_STRING_UNICODE_JSON))
      end,
    }
    cases[#cases + 1] = {
      name = "decode/workloadbench/cjson",
      group = "decode",
      bytes_per_call = #WORKLOAD_JSONL,
      docs_per_call = #WORKLOAD_LINES,
      run_once = function()
        local i
        for i = 1, #WORKLOAD_LINES do
          if not assert_workload_table(cjson_decode(WORKLOAD_LINES[i]), i) then
            return false
          end
        end
        return true
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_japanese/cjson",
      group = "decode",
      bytes_per_call = #JAPANESE_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(cjson_decode(JAPANESE_UTF8_JSON), JAPANESE_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_hebrew/cjson",
      group = "decode",
      bytes_per_call = #HEBREW_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(cjson_decode(HEBREW_UTF8_JSON), HEBREW_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_arabic/cjson",
      group = "decode",
      bytes_per_call = #ARABIC_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_table(cjson_decode(ARABIC_UTF8_JSON), ARABIC_UTF8_PREFIX)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_japanese_wide/cjson",
      group = "decode",
      bytes_per_call = #JAPANESE_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(cjson_decode(JAPANESE_UTF8_WIDE_JSON),
          "ja", "japanese", "hojoki", JAPANESE_UTF8_PREFIX, JAPANESE_UTF8_PREFIX, 180)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_hebrew_wide/cjson",
      group = "decode",
      bytes_per_call = #HEBREW_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(cjson_decode(HEBREW_UTF8_WIDE_JSON),
          "he", "hebrew", "aesop_wikisource", "השועל מזמין את החסידה לביתו", HEBREW_UTF8_PREFIX, 220)
      end,
    }
    cases[#cases + 1] = {
      name = "decode/doc_arabic_wide/cjson",
      group = "decode",
      bytes_per_call = #ARABIC_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        return assert_vendor_language_wide_table(cjson_decode(ARABIC_UTF8_WIDE_JSON),
          "ar", "arabic", "kalila_dimna", "مقدمة تشرح سبب وضع كتاب كليلة", ARABIC_UTF8_PREFIX, 220)
      end,
    }
  end

  do
    local rec = FixedRecord:new_record()
    cases[#cases + 1] = {
      name = "decode/record_fixed/lua",
      group = "decode",
      sibling_name = "parse/buffer_fixed/lonejson",
      bytes_per_call = #PARSE_JSON,
      docs_per_call = 1,
      run_once = function()
        FixedRecord:decode_into(rec, PARSE_JSON)
        return assert_fixed_record(rec)
      end,
    }
  end

  do
    local rec = FixedRecordReuse:new_record()
    cases[#cases + 1] = {
      name = "decode/record_fixed_prepared/lua",
      group = "decode",
      sibling_name = "parse/buffer_fixed_prepared/lonejson",
      bytes_per_call = #PARSE_JSON,
      docs_per_call = 1,
      run_once = function()
        rec:clear()
        FixedRecordReuse:decode_into(rec, PARSE_JSON)
        return assert_fixed_record_reuse(rec)
      end,
    }
  end

  do
    local rec = VendorLong:new_record()
    cases[#cases + 1] = {
      name = "stream/record_fixed/lua",
      group = "stream",
      sibling_name = "stream/object_fixed/lonejson",
      bytes_per_call = #STREAM_JSON,
      docs_per_call = 8,
      run_once = function()
        local stream = FixedRecord:stream_string(STREAM_JSON)
        local rec = FixedRecord:new_record()
        local seen = 0
        while true do
          local obj, err, status = stream:next(rec)
          if status == "object" then
            if not assert_stream_fixed_record(obj) then
              stream:close()
              return false
            end
            seen = seen + 1
          elseif status == "eof" then
            break
          else
            stream:close()
            return false
          end
        end
        stream:close()
        return seen == 8
      end,
    }
  end

  do
    cases[#cases + 1] = {
      name = "stream/record_fixed_prepared/lua",
      group = "stream",
      sibling_name = "stream/object_fixed_prepared/lonejson",
      bytes_per_call = #STREAM_JSON,
      docs_per_call = 8,
      run_once = function()
        local stream = FixedRecordReuse:stream_string(STREAM_JSON)
        local rec = FixedRecordReuse:new_record()
        local seen = 0
        while true do
          rec:clear()
          local obj, err, status = stream:next(rec)
          if status == "object" then
            if not assert_stream_fixed_record_reuse(obj) then
              stream:close()
              return false
            end
            seen = seen + 1
          elseif status == "eof" then
            break
          else
            stream:close()
            return false
          end
        end
        stream:close()
        return seen == 8
      end,
    }
  end

  do
    local rec = VendorLong:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_long_strings/lua",
      group = "stream",
      sibling_name = "stream/doc_long_strings/lonejson",
      bytes_per_call = #VENDOR_LONG_STRINGS_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLong:stream_string(VENDOR_LONG_STRINGS_JSON)
        rec:clear()
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and obj.id == "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      end,
    }
  end

  do
    local rec = VendorNumbers:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_extreme_numbers/lua",
      group = "stream",
      sibling_name = "stream/doc_extreme_numbers/lonejson",
      bytes_per_call = #VENDOR_EXTREME_NUMBERS_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorNumbers:stream_string(VENDOR_EXTREME_NUMBERS_JSON)
        rec:clear()
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and obj.min == -1.0e+28 and obj.max == 1.0e+28
      end,
    }
  end

  do
    local rec = VendorUnicode:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_string_unicode/lua",
      group = "stream",
      sibling_name = "stream/doc_string_unicode/lonejson",
      bytes_per_call = #VENDOR_STRING_UNICODE_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorUnicode:stream_string(VENDOR_STRING_UNICODE_JSON)
        rec:clear()
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and obj.title ~= nil and #obj.title > 0
      end,
    }
  end

  do
    local rec = VendorLanguage:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_japanese/lua",
      group = "stream",
      sibling_name = "stream/doc_japanese/lonejson",
      bytes_per_call = #JAPANESE_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguage:stream_string(JAPANESE_UTF8_JSON)
        rec:clear()
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_table(obj, JAPANESE_UTF8_PREFIX)
      end,
    }
  end

  do
    local rec = VendorLanguageWide:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_japanese_wide/lua",
      group = "stream",
      sibling_name = "stream/doc_japanese_wide/lonejson",
      bytes_per_call = #JAPANESE_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguageWide:stream_string(JAPANESE_UTF8_WIDE_JSON)
        rec:clear()
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_wide_table(obj,
          "ja", "japanese", "hojoki", JAPANESE_UTF8_PREFIX, JAPANESE_UTF8_PREFIX, 180)
      end,
    }
  end

  do
    local rec = VendorLanguageWide:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_hebrew_wide/lua",
      group = "stream",
      sibling_name = "stream/doc_hebrew_wide/lonejson",
      bytes_per_call = #HEBREW_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguageWide:stream_string(HEBREW_UTF8_WIDE_JSON)
        rec:clear()
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_wide_table(obj,
          "he", "hebrew", "aesop_wikisource", "השועל מזמין את החסידה לביתו", HEBREW_UTF8_PREFIX, 220)
      end,
    }
  end

  do
    local rec = VendorLanguageWide:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_arabic_wide/lua",
      group = "stream",
      sibling_name = "stream/doc_arabic_wide/lonejson",
      bytes_per_call = #ARABIC_UTF8_WIDE_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguageWide:stream_string(ARABIC_UTF8_WIDE_JSON)
        rec:clear()
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_wide_table(obj,
          "ar", "arabic", "kalila_dimna", "مقدمة تشرح سبب وضع كتاب كليلة", ARABIC_UTF8_PREFIX, 220)
      end,
    }
  end

  do
    local rec = VendorLanguage:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_hebrew/lua",
      group = "stream",
      sibling_name = "stream/doc_hebrew/lonejson",
      bytes_per_call = #HEBREW_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguage:stream_string(HEBREW_UTF8_JSON)
        rec:clear()
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_table(obj, HEBREW_UTF8_PREFIX)
      end,
    }
  end

  do
    local rec = VendorLanguage:new_record()
    cases[#cases + 1] = {
      name = "stream/doc_arabic/lua",
      group = "stream",
      sibling_name = "stream/doc_arabic/lonejson",
      bytes_per_call = #ARABIC_UTF8_JSON,
      docs_per_call = 1,
      run_once = function()
        local stream = VendorLanguage:stream_string(ARABIC_UTF8_JSON)
        rec:clear()
        local obj, err, status = stream:next(rec)
        stream:close()
        return status == "object" and assert_vendor_language_table(obj, ARABIC_UTF8_PREFIX)
      end,
    }
  end

  do
    cases[#cases + 1] = {
      name = "stream/workloadbench/lua",
      group = "stream",
      sibling_name = "stream/workloadbench/lonejson",
      bytes_per_call = #WORKLOAD_JSONL,
      docs_per_call = 5,
      run_once = function()
        local stream = WorkloadRunReuse:stream_string(WORKLOAD_JSONL)
        local rec = WorkloadRunReuse:new_record()
        local ids = {
          "20260322T130525Z-b62770c-dirty-fast-disk",
          "20260322T141810Z-b62770c-dirty-fast-disk",
          "20260323T003016Z-08a87c0-clean-fast-disk",
          "20260325T004115Z-55f841b-dirty-fast-disk",
          "20260325T155329Z-e66dc79-dirty-fast-disk",
        }
        local count = 0
        while true do
          rec:clear()
          local obj, err, status = stream:next(rec)
          if status == "object" then
            count = count + 1
            if WORKLOAD_PATHS_REUSE.run_id(rec) ~= ids[count] or WORKLOAD_PATHS_REUSE.git_short_sha(rec) == nil or WORKLOAD_PATHS_REUSE.git_dirty(rec) == nil or WORKLOAD_PATHS_REUSE.config_concurrency(rec) ~= 8 or WORKLOAD_PATHS_REUSE.cases_count(rec) ~= 9 then
              stream:close()
              return false
            end
            if WORKLOAD_PATHS_REUSE.case1_workload(rec) ~= "attachments" or WORKLOAD_PATHS_REUSE.case5_workload(rec) ~= "public-read" or WORKLOAD_PATHS_REUSE.case9_workload(rec) ~= "xa-rollback" or WORKLOAD_PATHS_REUSE.case1_acquire_ops(rec) ~= 4000 then
              stream:close()
              return false
            end
          elseif status == "eof" then
            break
          else
            stream:close()
            return false
          end
        end
        stream:close()
        return count == 5
      end,
    }
  end

  do
    local rec = FixedRecord:new_record()
    local rec_pretty = FixedRecordPretty:new_record()
    FixedRecord:assign(rec, build_serialize_source())
    FixedRecordPretty:assign(rec_pretty, build_serialize_source())
    cases[#cases + 1] = {
      name = "encode/record_buffer/lua",
      group = "encode",
      sibling_name = "serialize/buffer/lonejson",
      bytes_per_call = #EXPECTED_JSON,
      docs_per_call = 1,
      run_once = function()
        return FixedRecord:encode(rec) == EXPECTED_JSON
      end,
    }
    cases[#cases + 1] = {
      name = "encode/record_buffer_pretty/lua",
      group = "encode",
      sibling_name = "serialize/buffer_pretty/lonejson",
      bytes_per_call = #EXPECTED_PRETTY_JSON,
      docs_per_call = 1,
      run_once = function()
        return FixedRecordPretty:encode(rec_pretty) == EXPECTED_PRETTY_JSON
      end,
    }
  end

  return cases
end

-- Workers execute the entire suite. Timing summaries, report validation and
-- regression gates are shared with C through lonejson_bench merge/compare/gate.
assert(arg[1] == 'round' and arg[2] and arg[3],
  'usage: lonejson_lua_bench.lua round <output> <pilot-or-dash> [case]')
local plan = {}
if arg[3] ~= '-' then
  for _, result in ipairs(lj.decode_json(read_file(arg[3])).results) do
    plan[result.name] = result.batch_iterations
  end
end
local budgets = {}
for line in io.lines(os.getenv('LONEJSON_BENCH_PLAN') or 'bench/workloads.tsv') do
  local name, ns = line:match('^%S+%s+(%S+)%s+(%d+)$')
  assert(name and not budgets[name], 'invalid case budget')
  budgets[name] = tonumber(ns)
end
local results = {}
collectgarbage('restart')
local cases = bench_cases()
local round = tonumber(os.getenv('LONEJSON_BENCH_ROUND')) or 0
for step = 0, #cases - 1 do
  local index = measurement.case_index(#cases, round, step)
  local case = cases[index]
  if not arg[4] or case.name == arg[4] then
    local target = budgets[case.name]
    if not target and case.name:sub(-6) == '/cjson' then target = measurement.target_sample_ns end
    assert(target, 'missing case budget: ' .. case.name)
    local count = plan[case.name]
    assert(arg[3] == '-' or (count and count > 0), 'missing work plan: ' .. case.name)
    -- Settle allocations from fixture construction and the preceding case.
    -- Normal automatic GC remains enabled throughout the timed operations.
    collectgarbage('collect')
    local measured = measurement.measure(case.run_once, count or 1,
      core.monotonic_ns, function() return math.floor(os.clock() * 1000000000) end,
      count ~= nil, target)
    local sample = measured.samples[1]
    local rate = case.bytes_per_call * sample.iterations * 1000000000 / (sample.elapsed_ns * 1048576)
    results[#results + 1] = {
      name = case.name, group = case.group, samples = measured.samples,
      target_sample_ns = target,
      batch_iterations = sample.iterations, elapsed_ns = sample.elapsed_ns,
      total_bytes = case.bytes_per_call * sample.iterations,
      total_documents = case.docs_per_call * sample.iterations,
      warmup_elapsed_ns = measured.warmup_elapsed_ns, mismatch_count = 0,
      mib_per_sec = rate, mib_per_sec_low = rate, mib_per_sec_high = rate,
      docs_per_sec = case.docs_per_call * sample.iterations * 1000000000 / sample.elapsed_ns,
      ns_per_byte = sample.elapsed_ns / (case.bytes_per_call * sample.iterations),
      cv_pct = 0, drift_pct = 0, relative_error_pct = 0,
      drift_ci_low_pct = 0, drift_ci_high_pct = 0,
    }
  end
end
assert(#results > 0, 'empty benchmark round')
local toolchain = lj.decode_json(read_file(assert(os.getenv('LONEJSON_BENCH_TOOLCHAIN_FILE'))))
local now_ns = wall_clock_epoch_ns()
local run = {
  schema_version = BENCH_SCHEMA_VERSION, method = measurement.method,
  affinity = os.getenv('LONEJSON_BENCH_AFFINITY') or 'unbound',
  source_revision = assert(os.getenv('LONEJSON_BENCH_SOURCE_REVISION')),
  source_src_tree = os.getenv('LONEJSON_BENCH_SRC_TREE') or 'unversioned',
  source_include_tree = os.getenv('LONEJSON_BENCH_INCLUDE_TREE') or 'unversioned',
  source_lua_tree = os.getenv('LONEJSON_BENCH_LUA_TREE') or 'unversioned',
  implementation_count = tonumber(os.getenv('LONEJSON_BENCH_IMPLEMENTATION_COUNT')) or 1,
  compiler = toolchain.compiler, toolchain = toolchain.toolchain, lua_version = _VERSION,
  sample_count = measurement.sample_count, process_count = measurement.sample_count,
  samples_per_process = 1, warmup_count = measurement.warmup_count, target_sample_ns = measurement.target_sample_ns,
  confidence_pct = measurement.confidence_pct, max_relative_error_pct = measurement.max_relative_error_pct,
  max_drift_pct = measurement.max_drift_pct,
  timestamp_epoch_ns = now_ns, timestamp_utc = utc_timestamp_from_ns(now_ns),
  host = benchmark_host_id(), iterations = 1, results = results,
}
local output = assert(io.open(arg[2], 'wb'))
assert(output:write(lj.encode_json(run)))
assert(output:close())
