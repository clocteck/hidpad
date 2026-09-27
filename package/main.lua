local APP = {
  VERSION = "1.1.0",
  APP_DIR = "/sd/apps/hidpad",
  MODULE_PATH = "/sd/apps/hidpad/modules/hidpad.so",
  CONFIG_PATH = "/sd/apps/hidpad/config.json",
  SOURCE = "ble-main",
  IPC_ENDPOINT = "ble-controller",
  FIXED_ROUTE_BASE = "/hidpad",
  ROUTE_BASE = "/hidpad",
  POLL_READY_MS = 10,
  POLL_SCAN_MS = 20,
  POLL_IDLE_MS = 1000,
  EVENT_MODE = false,
  DEBUG_BUTTONS = false,
  routes = {},
  timers = {},
}

if app and app.current then
  local current = app.current()
  local entry = current and current.entry
  local dir = type(entry) == "string" and entry:gsub("\\", "/"):match("^(.*)/[^/]+$") or nil
  if dir and dir ~= "" then
    APP.APP_DIR = dir
    APP.MODULE_PATH = dir .. "/modules/hidpad.so"
    APP.CONFIG_PATH = dir .. "/config.json"
  end
end
if app and app.route_base then
  APP.ROUTE_BASE = app.route_base() or APP.ROUTE_BASE
end

local BUTTONS = {
  { key = "UP", bit = 1, source_bit = 1 },
  { key = "DOWN", bit = 2, source_bit = 2 },
  { key = "LEFT", bit = 4, source_bit = 4 },
  { key = "RIGHT", bit = 8, source_bit = 8 },
  { key = "A", bit = 16, source_bit = 16 },
  { key = "B", bit = 32, source_bit = 32 },
  { key = "X", bit = 64, source_bit = 64 },
  { key = "Y", bit = 128, source_bit = 128 },
  { key = "L", bit = 256, source_bit = 256 },
  { key = "R", bit = 512, source_bit = 512 },
  { key = "LS", bit = 1024, source_bit = 1024 },
  { key = "RS", bit = 2048, source_bit = 2048 },
  { key = "SELECT", bit = 4096, source_bit = 4096 },
  { key = "START", bit = 8192, source_bit = 8192 },
  { key = "SHARE", bit = 16384, source_bit = 16384 },
  { key = "HOME", bit = 32768, source_bit = 32768 },
}

local BUTTON_BY_KEY = {}
for _, item in ipairs(BUTTONS) do BUTTON_BY_KEY[item.key] = item end

local function default_axis()
  return { min = -32767, center = 0, max = 32767, invert = false }
end

local function default_config()
  local mapping = {}
  for _, item in ipairs(BUTTONS) do mapping[item.key] = item.key end
  return {
    version = 2,
    enabled = true,
    auto_connect = true,
    preferred_address = "",
    preferred_addr_type = 0,
    preferred_profile = "",
    preferred_name = "",
    deadzone = 3200,
    mapping = mapping,
    axes = { lx = default_axis(), ly = default_axis(), rx = default_axis(), ry = default_axis() },
    triggers = { lt = { min = 0, max = 65535 }, rt = { min = 0, max = 65535 } },
  }
end

local S = {
  driver = nil,
  driver_error = nil,
  raw = { connected = false, buttons = 0, lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0 },
  output = { connected = false, buttons = 0, lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0 },
  config = default_config(),
  enabled = true,
  calibration = nil,
  publish_seq = 0,
  last_driver_seq = -1,
  debug_connected = nil,
  debug_raw_buttons = -1,
  debug_output_buttons = -1,
  web_button_events = 0,
  last_output = nil,
  last_error = nil,
  config_error = nil,
  last_completed_command = 0,
}

local function rebuild_mapping_bits()
  for _, destination in ipairs(BUTTONS) do
    local source = BUTTON_BY_KEY[S.config.mapping[destination.key] or "NONE"]
    destination.source_bit = source and source.bit or 0
  end
end

rebuild_mapping_bits()

local function log(...)
  print("[hidpad]", ...)
end

local function clamp(value, low, high)
  value = tonumber(value) or 0
  if value < low then return low end
  if value > high then return high end
  return value
end

local function copy_table(value)
  if type(value) ~= "table" then return value end
  local out = {}
  for key, item in pairs(value) do out[key] = copy_table(item) end
  return out
end

local function json_encode(value)
  if not json or not json.encode then return nil, "json.encode missing" end
  local ok, result = pcall(function() return json.encode(value) end)
  if not ok then return nil, tostring(result) end
  return result, nil
end

local function json_decode(raw)
  if not json or not json.decode then return nil, "json.decode missing" end
  local ok, value, err = pcall(function() return json.decode(raw) end)
  if not ok then return nil, tostring(value) end
  if value == nil then return nil, tostring(err or "invalid json") end
  return value, nil
end

local function normalize_language(value)
  local lang = tostring(value or ""):lower():gsub("_", "-")
  if lang == "en" or lang:match("^en%-") then return "en" end
  if lang == "ja" or lang:match("^ja%-") then return "ja" end
  if lang == "zh-tw" or lang == "zh-hk" or lang == "zh-mo"
      or lang:match("^zh%-hant") then return "zh-TW" end
  return "zh-CN"
end

local function read_language()
  -- Root-level settings take precedence; existing Holo apps use /sd/apps.
  -- Never write to shared settings from the controller page.
  for _, path in ipairs({ "/sd/settings.json", "/sd/apps/settings.json" }) do
    local ok, raw = pcall(function() return file.getcontents(path) end)
    if ok and type(raw) == "string" and raw ~= "" then
      local codec = json or sjson
      local decoded, doc = pcall(function() return codec.decode(raw) end)
      if decoded and type(doc) == "table" then
        local value = doc.language or doc.locale or doc.lang
        if type(value) == "string" and value ~= "" then return normalize_language(value) end
      end
    end
  end
  return "zh-CN"
end

local function merge_config(doc)
  if type(doc) ~= "table" then return end
  if type(doc.enabled) == "boolean" then S.config.enabled = doc.enabled end
  if type(doc.auto_connect) == "boolean" then S.config.auto_connect = doc.auto_connect end
  S.config.deadzone = clamp(doc.deadzone or S.config.deadzone, 0, 16000)
  if type(doc.preferred_address) == "string" and #doc.preferred_address <= 17 then
    S.config.preferred_address = doc.preferred_address
  end
  if doc.preferred_addr_type ~= nil then
    S.config.preferred_addr_type = math.floor(clamp(doc.preferred_addr_type, 0, 3))
  end
  if type(doc.preferred_profile) == "string" and #doc.preferred_profile <= 16 then
    S.config.preferred_profile = doc.preferred_profile
  end
  if type(doc.preferred_name) == "string" and #doc.preferred_name <= 39 then
    S.config.preferred_name = doc.preferred_name
  end
  if type(doc.mapping) == "table" then
    for _, item in ipairs(BUTTONS) do
      local source = doc.mapping[item.key]
      if source == "NONE" or BUTTON_BY_KEY[source] then S.config.mapping[item.key] = source end
    end
  end
  if type(doc.axes) == "table" then
    for _, name in ipairs({ "lx", "ly", "rx", "ry" }) do
      local src = doc.axes[name]
      local dst = S.config.axes[name]
      if type(src) == "table" then
        dst.min = clamp(src.min or dst.min, -32768, 32767)
        dst.center = clamp(src.center or dst.center, -32768, 32767)
        dst.max = clamp(src.max or dst.max, -32768, 32767)
        dst.invert = src.invert == true
      end
    end
  end
  if type(doc.triggers) == "table" then
    for _, name in ipairs({ "lt", "rt" }) do
      local src = doc.triggers[name]
      local dst = S.config.triggers[name]
      if type(src) == "table" then
        dst.min = clamp(src.min or dst.min, 0, 65535)
        dst.max = clamp(src.max or dst.max, 0, 65535)
      end
    end
  end
  rebuild_mapping_bits()
end

local function load_config()
  if not file or not file.getcontents then return end
  local ok, raw = pcall(function() return file.getcontents(APP.CONFIG_PATH) end)
  if not ok or type(raw) ~= "string" or raw == "" then return end
  local doc, err = json_decode(raw)
  if not doc then
    S.last_error = "配置读取失败: " .. tostring(err)
    return
  end
  merge_config(doc)
end

local function save_config()
  local raw, err = json_encode(S.config)
  if not raw then S.config_error = err; return false, err end
  if not file or not file.putcontents then
    S.config_error = "file.putcontents missing"; return false, S.config_error
  end
  local ok, result = pcall(function() return file.putcontents(APP.CONFIG_PATH, raw) end)
  if not ok or result == false then
    S.config_error = "配置保存失败: " .. tostring(result or "write failed")
    return false, S.config_error
  end
  S.config_error = nil
  return true, nil
end

local function has_bit(mask, bit)
  mask = math.floor(tonumber(mask) or 0)
  return (mask & bit) ~= 0
end

local function map_buttons(raw_mask)
  raw_mask = math.floor(tonumber(raw_mask) or 0)
  local out = 0
  for _, destination in ipairs(BUTTONS) do
    local source_bit = destination.source_bit
    if source_bit ~= 0 and (raw_mask & source_bit) ~= 0 then out = out | destination.bit end
  end
  return out
end

-- 仅在连接或按键变化时输出，避免摇杆报告刷满串口。
local function debug_buttons(raw, output)
  if not APP.DEBUG_BUTTONS then return end
  local connected = raw.connected == true
  if S.debug_connected ~= connected then
    S.debug_connected = connected
    log("pad", connected and "connected" or "disconnected",
      tostring(raw.profile or "unknown"), tostring(raw.name or ""), tostring(raw.address or ""))
  end
  local raw_mask = tonumber(raw.buttons) or 0
  local output_mask = tonumber(output.buttons) or 0
  if S.debug_raw_buttons == raw_mask and S.debug_output_buttons == output_mask then return end
  S.debug_raw_buttons = raw_mask
  S.debug_output_buttons = output_mask
  local raw_names, output_names = {}, {}
  for _, item in ipairs(BUTTONS) do
    if has_bit(raw_mask, item.bit) then raw_names[#raw_names + 1] = item.key end
    if has_bit(output_mask, item.bit) then output_names[#output_names + 1] = item.key end
  end
  log("buttons",
    string.format("raw=0x%08X", raw_mask), table.concat(raw_names, "+"),
    string.format("mapped=0x%08X", output_mask), table.concat(output_names, "+"))
end

local function apply_deadzone(value)
  local dz = clamp(S.config.deadzone, 0, 16000)
  local sign = value < 0 and -1 or 1
  local magnitude = math.abs(value)
  if magnitude <= dz then return 0 end
  return math.floor(sign * (magnitude - dz) * 32767 / math.max(1, 32767 - dz))
end

local function calibrate_axis(raw, cfg)
  raw = clamp(raw, -32768, 32767)
  local center = clamp(cfg.center, -32768, 32767)
  local span = raw >= center and (cfg.max - center) or (center - cfg.min)
  if span < 256 then span = 32767 end
  local value = clamp((raw - center) * 32767 / span, -32767, 32767)
  if cfg.invert then value = -value end
  return apply_deadzone(math.floor(value))
end

local function calibrate_trigger(raw, cfg)
  local span = (cfg.max or 65535) - (cfg.min or 0)
  if span < 64 then span = 65535 end
  return math.floor(clamp((clamp(raw, 0, 65535) - (cfg.min or 0)) * 65535 / span, 0, 65535))
end

local function sample_calibration(raw)
  local cal = S.calibration
  if not cal or not raw.connected then return end
  for _, name in ipairs({ "lx", "ly", "rx", "ry" }) do
    local value = clamp(raw[name], -32768, 32767)
    local axis = cal.axes[name]
    if value < axis.min then axis.min = value end
    if value > axis.max then axis.max = value end
  end
  for _, name in ipairs({ "lt", "rt" }) do
    local value = clamp(raw[name], 0, 65535)
    local trigger = cal.triggers[name]
    if value < trigger.min then trigger.min = value end
    if value > trigger.max then trigger.max = value end
  end
end

local function begin_calibration()
  if not S.raw.ready then return false, "请先连接手柄" end
  local axes = {}
  for _, name in ipairs({ "lx", "ly", "rx", "ry" }) do
    local value = clamp(S.raw[name], -32768, 32767)
    axes[name] = { min = value, center = value, max = value, invert = S.config.axes[name].invert }
  end
  local triggers = {}
  for _, name in ipairs({ "lt", "rt" }) do
    local value = clamp(S.raw[name], 0, 65535)
    triggers[name] = { min = value, max = value }
  end
  S.calibration = { axes = axes, triggers = triggers }
  return true, nil
end

local function finish_calibration()
  if not S.calibration then return false, "当前没有校准任务" end
  for _, name in ipairs({ "lx", "ly", "rx", "ry" }) do
    local sample = S.calibration.axes[name]
    if sample.max - sample.min >= 1024 then
      S.config.axes[name] = copy_table(sample)
    end
  end
  for _, name in ipairs({ "lt", "rt" }) do
    local sample = S.calibration.triggers[name]
    if sample.max - sample.min >= 256 then S.config.triggers[name] = copy_table(sample) end
  end
  S.calibration = nil
  return save_config()
end

local function update_output(raw)
  local output = S.output
  output.connected = raw.connected == true
  output.buttons = map_buttons(raw.buttons)
  output.lx = calibrate_axis(raw.lx, S.config.axes.lx)
  output.ly = calibrate_axis(raw.ly, S.config.axes.ly)
  output.rx = calibrate_axis(raw.rx, S.config.axes.rx)
  output.ry = calibrate_axis(raw.ry, S.config.axes.ry)
  output.lt = calibrate_trigger(raw.lt, S.config.triggers.lt)
  output.rt = calibrate_trigger(raw.rt, S.config.triggers.rt)
  output.name = tostring(raw.name or "")
  output.device_id = tostring(raw.address or "")
end

local function output_changed(output)
  local last = S.last_output
  if not last then return true end
  return last.connected ~= output.connected
    or last.buttons ~= output.buttons
    or last.lx ~= output.lx or last.ly ~= output.ly
    or last.rx ~= output.rx or last.ry ~= output.ry
    or last.lt ~= output.lt or last.rt ~= output.rt
    or last.name ~= output.name or last.device_id ~= output.device_id
end

local function remember_output(output)
  local last = S.last_output or {}
  last.connected = output.connected
  last.buttons = output.buttons
  last.lx, last.ly = output.lx, output.ly
  last.rx, last.ry = output.rx, output.ry
  last.lt, last.rt = output.lt, output.rt
  last.name, last.device_id = output.name, output.device_id
  S.last_output = last
end

local function publish(raw)
  update_output(raw)
  -- 仅用一个整数锁存 Web 两次轮询之间出现的短按，不创建事件表或额外定时器。
  S.web_button_events = S.web_button_events | (tonumber(S.output.buttons) or 0)
  debug_buttons(raw, S.output)
  if not output_changed(S.output) then return false end
  remember_output(S.output)
  S.publish_seq = S.publish_seq + 1
  if controller and controller.publish then
    local ok, err = pcall(controller.publish, APP.SOURCE, S.output)
    if not ok then S.last_error = "controller.publish: " .. tostring(err) end
  end
  return true
end

local function driver_call(name, ...)
  local fn = S.driver and S.driver[name]
  if type(fn) ~= "function" then return false, S.driver_error or (name .. " unavailable") end
  local ok, result, err = pcall(fn, ...)
  if not ok then return false, tostring(result) end
  if result == nil or result == false then return false, tostring(err or "driver failed") end
  return true, err -- Optional command id is the driver's second result.
end

local function set_auto_connect(enabled)
  if not S.driver then return false, S.driver_error or "hidpad.so 未加载" end
  if type(S.driver.set_auto_connect) ~= "function" then
    if enabled then return true end -- Existing modules retain their original default.
    return false, "auto_connect requires updated hidpad.so"
  end
  return driver_call("set_auto_connect", enabled and 1 or 0)
end

local function update_preferred(address, addr_type, profile, name)
  S.config.preferred_address = address or ""
  S.config.preferred_addr_type = math.floor(clamp(addr_type, 0, 3))
  S.config.preferred_profile = profile or ""
  S.config.preferred_name = name or ""
end

local function set_driver_preferred()
  if not S.driver or type(S.driver.set_preferred) ~= "function" then return true end
  return driver_call("set_preferred", S.config.preferred_address or "",
    S.config.preferred_addr_type or 0, S.config.preferred_profile or "", S.config.preferred_name or "")
end

local function remember_ready_device(raw)
  local address = raw.phase == "ready" and tostring(raw.address or "") or ""
  if address == "" then return end
  local addr_type = math.floor(clamp(raw.addr_type, 0, 3))
  local profile = tostring(raw.profile or "hid")
  local name = tostring(raw.name or "")
  if S.config.preferred_address == address
      and S.config.preferred_addr_type == addr_type
      and S.config.preferred_profile == profile
      and S.config.preferred_name == name then return end
  update_preferred(address, addr_type, profile, name)
  set_driver_preferred()
  local ok, err = save_config()
  if not ok then S.last_error = "保存首选手柄失败: " .. tostring(err) end
end

local function current_error()
  if S.config_error then return S.config_error end
  if S.raw.last_error and S.raw.last_error ~= "" then return S.raw.last_error end
  return S.last_error
end

local function consume_driver_status(raw)
  local id = tonumber(raw.command_id) or 0
  local status = raw.command_status
  if id ~= 0 and id ~= S.last_completed_command and (status == "succeeded" or status == "failed") then
    S.last_completed_command = id
    if status == "failed" then
      S.last_error = raw.command_error
    else
      S.last_error = nil
      if raw.command_kind == "forget" then
        update_preferred()
        set_driver_preferred()
        save_config()
      end
    end
  end
  -- poll(S.raw) mutates the existing table: do not rely on table identity to
  -- clear a previous connection error after the next successful connection.
  if (raw.ready == true or raw.phase == "ready") and (not raw.last_error or raw.last_error == "") then
    S.last_error = nil
    S.driver_error = nil
  elseif status == "pending" and (not raw.last_error or raw.last_error == "") then
    S.last_error = nil
  end
  if not (raw.command_kind == "forget" and status == "pending") then remember_ready_device(raw) end
end

local set_polling

local function poll_driver()
  if not S.driver then return end
  local ok, raw, status_update = pcall(S.driver.poll, S.raw)
  if not ok then S.last_error = tostring(raw); return end
  if type(raw) ~= "table" then return end
  status_update = status_update == true or raw ~= S.raw and raw.connected ~= nil
  if raw ~= S.raw and status_update then
    S.raw = raw
    if raw.connected == true and (raw.last_error == nil or raw.last_error == "") then
      S.last_error = nil
    end
  elseif raw ~= S.raw then
    S.raw.seq = raw.seq or S.raw.seq
    S.raw.timestamp_ms = raw.timestamp_ms or S.raw.timestamp_ms
    S.raw.buttons = raw.buttons or 0
    S.raw.raw_buttons = raw.raw_buttons or 0
    S.raw.lx, S.raw.ly = raw.lx or 0, raw.ly or 0
    S.raw.rx, S.raw.ry = raw.rx or 0, raw.ry or 0
    S.raw.lt, S.raw.rt = raw.lt or 0, raw.rt or 0
    S.raw.report_id = raw.report_id or 0
    S.raw.notify_count = raw.notify_count or S.raw.notify_count or 0

  end
  if status_update then consume_driver_status(S.raw) end
  sample_calibration(S.raw)
  S.last_driver_seq = S.raw.seq or S.last_driver_seq
  publish(S.raw)
  if status_update and set_polling then set_polling(S.enabled) end
end

local function state_snapshot()
  local pressed = {}
  for _, item in ipairs(BUTTONS) do
    if has_bit(S.output.buttons, item.bit) then pressed[#pressed + 1] = item.key end
  end
  return {
    ok = S.driver ~= nil,
    version = APP.VERSION,
    language = read_language(),
    auto_connect_supported = S.driver ~= nil and type(S.driver.set_auto_connect) == "function",
    enabled = S.enabled,
    source = APP.SOURCE,
    route_base = APP.ROUTE_BASE,
    fixed_route_base = APP.FIXED_ROUTE_BASE,
    driver_error = S.driver_error,
    last_error = current_error(),
    phase = S.raw.phase or "stopped",
    profile = S.raw.profile or "unknown",
    connected = S.raw.connected == true,
    ready = S.raw.ready == true,
    started = S.raw.started == true,
    scanning = S.raw.scanning == true,
    command_id = S.raw.command_id or 0,
    command_kind = S.raw.command_kind or "none",
    command_status = S.raw.command_status or "none",
    command_error = S.raw.command_error or "",
    connecting = S.raw.connecting == true,
    encrypted = S.raw.encrypted == true,
    disconnect_reason = S.raw.disconnect_reason or 0,
    manual_scan = S.raw.manual_scan == true,
    scan_count = S.raw.scan_count or 0,
    keepalive_supported = S.raw.keepalive_supported == true,
    keepalive_count = S.raw.keepalive_count or 0,
    name = S.raw.name or "",
    address = S.raw.address or "",
    raw = {
      buttons = S.raw.buttons or 0,
      lx = S.raw.lx or 0, ly = S.raw.ly or 0,
      rx = S.raw.rx or 0, ry = S.raw.ry or 0,
      lt = S.raw.lt or 0, rt = S.raw.rt or 0,
      notify_count = S.raw.notify_count or 0,
    },
    output = S.output,
    pressed = pressed,
    config = S.config,
    calibrating = S.calibration ~= nil,
    calibration = S.calibration,
  }
end

-- Web 高频读取只返回输入数据，不重复编码映射、校准等完整配置。
local function input_snapshot()
  local events = S.web_button_events
  S.web_button_events = 0
  return {
    seq = S.last_driver_seq,
    enabled = S.enabled,
    connected = S.raw.connected == true,
    ready = S.raw.ready == true,
    started = S.raw.started == true,
    scanning = S.raw.scanning == true,
    command_id = S.raw.command_id or 0,
    command_kind = S.raw.command_kind or "none",
    command_status = S.raw.command_status or "none",
    command_error = S.raw.command_error or "",
    connecting = S.raw.connecting == true,
    phase = S.raw.phase or "stopped",
    profile = S.raw.profile or "unknown",
    name = S.raw.name or "",
    address = S.raw.address or "",
    manual_scan = S.raw.manual_scan == true,
    scan_count = S.raw.scan_count or 0,
    last_error = current_error(),
    buttons = S.output.buttons or 0,
    raw_buttons = S.raw.buttons or 0,
    events = events,
    lx = S.raw.lx or 0, ly = S.raw.ly or 0,
    rx = S.raw.rx or 0, ry = S.raw.ry or 0,
    lt = S.raw.lt or 0, rt = S.raw.rt or 0,
    notify_count = S.raw.notify_count or 0,
  }
end

-- IPC 状态保持为小型定长字段；按键/事件均使用整数位图。
local function service_status()
  return {
    version = APP.VERSION,
    enabled = S.enabled,
    phase = S.enabled and (S.raw.phase or "stopped") or "disabled",
    connected = S.raw.connected == true,
    ready = S.raw.ready == true,
    started = S.raw.started == true,
    scanning = S.raw.scanning == true,
    command_id = S.raw.command_id or 0,
    command_kind = S.raw.command_kind or "none",
    command_status = S.raw.command_status or "none",
    command_error = S.raw.command_error or "",
    connecting = S.raw.connecting == true,
    profile = S.raw.profile or "unknown",
    name = S.raw.name or "",
    address = S.raw.address or "",
    scan_count = S.raw.scan_count or 0,
    keepalive_supported = S.raw.keepalive_supported == true,
    keepalive_count = S.raw.keepalive_count or 0,
    buttons = tonumber(S.output.buttons) or 0,
    raw_buttons = tonumber(S.raw.buttons) or 0,
    seq = S.last_driver_seq,
    error = current_error(),
  }
end

local function scan_devices()
  local devices = {}
  if not S.driver or type(S.driver.scan_count) ~= "function" or type(S.driver.scan_device) ~= "function" then
    return devices, "当前 hidpad.so 不支持设备选择"
  end
  local ok, count = pcall(S.driver.scan_count)
  if not ok then return devices, tostring(count) end
  count = math.min(tonumber(count) or 0, 16)
  for index = 1, count do
    local item_ok, item = pcall(S.driver.scan_device, index)
    if item_ok and type(item) == "table" then devices[#devices + 1] = item end
  end
  return devices, nil
end

local function devices_snapshot()
  local devices, err = scan_devices()
  if err then return { ok = false, devices = devices, error = err } end
  return { ok = true, scanning = S.raw.scanning == true, devices = devices }
end

local function response(status, content_type, body)
  return {
    status = status or "200 OK",
    type = content_type or "application/json; charset=utf-8",
    headers = { ["cache-control"] = "no-store" },
    body = body or "",
  }
end

local function json_response(status, value)
  local raw, err = json_encode(value)
  if not raw then return response("500 Internal Server Error", nil, '{"ok":false,"error":"' .. tostring(err) .. '"}') end
  return response(status, nil, raw)
end

local function read_body(req, limit)
  if not req or not req.getbody then return "", nil end
  local parts, total = {}, 0
  while true do
    local chunk = req.getbody()
    if not chunk then break end
    total = total + #chunk
    if total > limit then return nil, "请求内容过大" end
    parts[#parts + 1] = chunk
  end
  return table.concat(parts), nil
end

local set_driver_enabled

local function handle_command(topic, payload)
  local doc = {}
  if type(payload) == "string" and payload ~= "" then
    local decoded, err = json_decode(payload)
    if not decoded then return false, err end
    doc = decoded
  elseif type(payload) == "table" then
    doc = payload
  end
  if type(doc) ~= "table" then return false, "invalid payload" end

  if topic == "status" then return true, nil end
  if topic == "enable" then return set_driver_enabled(true) end
  if topic == "disable" then return set_driver_enabled(false) end
  if topic == "rescan" then
    if not S.enabled then return false, "蓝牙手柄已禁用" end
    return driver_call("rescan")
  end
  if topic == "scan_devices" then
    if not S.enabled then return false, "蓝牙手柄已禁用" end
    return driver_call("scan")
  end
  if topic == "connect_device" then
    local address = type(doc.address) == "string" and doc.address or ""
    if address == "" then return false, "请选择要连接的手柄" end
    -- Persist the successful device from its ready event, not queue acceptance.
    return driver_call("connect", address)
  end
  if topic == "disconnect" then return driver_call("disconnect") end
  if topic == "pair" then return driver_call("pair") end
  if topic == "forget" then return driver_call("forget") end
  if topic == "calibration_start" then
    local ok, err = begin_calibration()
    log("calibration", ok and "started" or "start failed", tostring(err or ""))
    return ok, err
  end
  if topic == "calibration_save" then
    local ok, err = finish_calibration()
    log("calibration", ok and "saved" or "save failed", tostring(err or ""))
    return ok, err
  end
  if topic == "calibration_cancel" then
    S.calibration = nil
    log("calibration", "cancelled")
    return true, nil
  end
  if topic == "restore_defaults" then
    local previous = copy_table(S.config)
    local applied, apply_err = set_auto_connect(true)
    if not applied then return false, apply_err end
    S.config = default_config()
    S.config.enabled = S.enabled
    local saved, save_err = save_config()
    if not saved then
      S.config = previous
      set_auto_connect(previous.auto_connect)
      return false, save_err
    end
    rebuild_mapping_bits()
    S.calibration = nil
    if S.driver and type(S.driver.set_preferred) == "function" then
      set_driver_preferred()
    end
    publish(S.raw)
    return true, nil
  end
  if topic == "set_mapping" then
    merge_config({ mapping = doc.mapping or doc })
    publish(S.raw)
    return save_config()
  end
  if topic == "set_config" then
    if doc.auto_connect ~= nil and type(doc.auto_connect) ~= "boolean" then
      return false, "auto_connect must be a boolean"
    end
    local previous = copy_table(S.config)
    if doc.auto_connect ~= nil then
      local applied, apply_err = set_auto_connect(doc.auto_connect)
      if not applied then return false, apply_err end
    end
    merge_config(doc)
    S.config.enabled = S.enabled
    local saved, save_err = save_config()
    if not saved then
      S.config = previous
      rebuild_mapping_bits()
      if doc.auto_connect ~= nil then set_auto_connect(previous.auto_connect) end
      return false, save_err
    end
    publish(S.raw)
    return true, nil
  end
  return false, "未知命令: " .. tostring(topic)
end

-- Read the standalone page only on navigation, not on every input poll.
local function page_response()
  local ok, page = pcall(function() return file.getcontents(APP.APP_DIR .. "/main.html") end)
  if not ok or type(page) ~= "string" or page == "" then
    return response("503 Service Unavailable", "text/plain; charset=utf-8", "HID Pad: main.html missing")
  end
  return response("200 OK", "text/html; charset=utf-8", page)
end

local function route_command(req)
  local raw, read_err = read_body(req, 4096)
  if not raw then return json_response("400 Bad Request", { ok = false, error = read_err }) end
  local doc, decode_err = json_decode(raw)
  if type(doc) ~= "table" or type(doc.topic) ~= "string" then
    return json_response("400 Bad Request", { ok = false, error = decode_err or "topic missing" })
  end
  local payload = doc.payload
  local payload_raw = type(payload) == "table" and json_encode(payload) or payload
  local ok, err = handle_command(doc.topic, payload_raw or "")
  if not ok then return json_response("400 Bad Request", { ok = false, error = err, state = state_snapshot() }) end
  return json_response("200 OK", { ok = true, command_id = type(err) == "number" and err or nil, state = state_snapshot() })
end

local function register_route(method, path, handler)
  if not httpd or not httpd.dynamic then
    S.last_error = "httpd.dynamic missing"
    log("route failed", method, path, S.last_error)
    return false
  end
  local ok, err = pcall(function() return httpd.dynamic(method, path, handler) end)
  if not ok or err then
    S.last_error = "route " .. tostring(method) .. " " .. path .. ": " .. tostring(err)
    log("route failed", method, path, tostring(err))
    return false
  end
  APP.routes[#APP.routes + 1] = { method = method, path = path }
  log("route ready", method, path)
  return true
end

local function diagnostics_snapshot()
  if not S.driver or type(S.driver.diagnostics) ~= "function" then return { ok = false, error = "diagnostics unavailable" } end
  local ok, value = pcall(S.driver.diagnostics)
  if not ok or type(value) ~= "table" then return { ok = false, error = tostring(value) } end
  return value
end

local function register_route_set(base)
  if type(base) ~= "string" or base == "" then return end
  local get, post = httpd.GET or "GET", httpd.POST or "POST"
  register_route(get, base, page_response)
  register_route(get, base .. "/", page_response)
  register_route(get, base .. "/api/state", function() return json_response("200 OK", state_snapshot()) end)
  register_route(get, base .. "/api/input", function() return json_response("200 OK", input_snapshot()) end)
  register_route(get, base .. "/api/devices", function() return json_response("200 OK", devices_snapshot()) end)
  register_route(get, base .. "/api/diagnostics", function() return json_response("200 OK", diagnostics_snapshot()) end)
  register_route(post, base .. "/api/command", route_command)
end

local function start_web()
  if not httpd then return end
  if httpd.start then
    local ok, err = pcall(function()
      return httpd.start({ webroot = APP.APP_DIR, auto_index = httpd.INDEX_NONE, max_handlers = 64 })
    end)
    if not ok or err then log("httpd start", tostring(err)) end
  end
  register_route_set(APP.ROUTE_BASE)
  -- Service 的实例路由会带 instance_id；固定别名便于用户直接访问和脚本调用。
  if APP.ROUTE_BASE ~= APP.FIXED_ROUTE_BASE then
    register_route_set(APP.FIXED_ROUTE_BASE)
  end
end

local function start_ipc()
  if not ipc or not ipc.listen then return end
  local ok, err = ipc.listen(APP.IPC_ENDPOINT, function(topic, payload)
    local handled, command_err = handle_command(topic, payload)
    if not handled then S.last_error = "IPC: " .. tostring(command_err) end
    local request = nil
    if type(payload) == "string" and payload ~= "" then
      request = json_decode(payload)
    end
    local reply = type(request) == "table" and request.reply or nil
    if type(reply) == "string" and reply ~= "" and ipc.send then
      local status = service_status()
      status.command = topic
      status.command_ok = handled == true
      status.command_error = command_err
      local raw = json_encode(status)
      if raw then
        local sent, send_err = ipc.send(reply, "status", raw)
        if not sent then log("IPC reply failed", tostring(send_err)) end
      end
    end
  end)
  if ok == nil or ok == false then S.last_error = "IPC listen: " .. tostring(err) end
end

local function sync_driver_state()
  if not S.driver or type(S.driver.state) ~= "function" then return end
  local ok, raw = pcall(S.driver.state)
  if ok and type(raw) == "table" then
    S.raw = raw
    S.last_driver_seq = raw.seq or S.last_driver_seq
    consume_driver_status(raw)
    publish(raw)
  end
end

local function start_driver()
  if not S.driver then
    local ok, module_or_err = pcall(require, APP.MODULE_PATH)
    if not ok or type(module_or_err) ~= "table" then
      S.driver_error = "固件版本不支持或 hidpad.so 加载失败: " .. tostring(module_or_err)
      S.last_error = S.driver_error
      log(S.driver_error)
      publish(S.raw)
      return false, S.driver_error
    end
    S.driver = module_or_err
  end
  APP.EVENT_MODE = false
  if type(S.driver.on_event) == "function" then
    local ok, enabled = pcall(S.driver.on_event, poll_driver)
    APP.EVENT_MODE = ok and enabled == true
  end
  set_driver_preferred()
  local policy_ok, policy_err = set_auto_connect(S.config.auto_connect)
  if not policy_ok then
    S.driver_error = policy_err
    S.last_error = policy_err
    return false, policy_err
  end
  if not S.enabled then
    sync_driver_state()
    return true, nil
  end
  local started, err = driver_call("start", 8000)
  if not started then
    S.driver_error = "驱动启动失败: " .. tostring(err)
    S.last_error = S.driver_error
    return false, S.driver_error
  end
  S.driver_error = nil
  sync_driver_state()
  return true, nil
end

set_polling = function(enabled)
  if not tmr or not tmr.create then return false end
  local timer = APP.timers.poll
  if not timer then
    timer = tmr.create()
    APP.timers.poll = timer
  end
  if APP.EVENT_MODE then
    pcall(function() timer:stop() end)
    APP.poll_ms = nil
    return true
  end
  if enabled then
    local interval = S.raw.connected and APP.POLL_READY_MS
      or S.raw.scanning and APP.POLL_SCAN_MS
      or S.raw.connecting and 50 or APP.POLL_IDLE_MS
    if APP.poll_ms ~= interval then
      APP.poll_ms = interval
      timer:alarm(interval, tmr.ALARM_AUTO, poll_driver)
    end
  else
    pcall(function() timer:stop() end)
    APP.poll_ms = nil
  end
  return true
end

set_driver_enabled = function(enable)
  enable = enable == true
  if not S.driver then return false, S.driver_error or "hidpad.so 未加载" end
  local command_id
  if enable then
    local applied, apply_err = set_auto_connect(S.config.auto_connect)
    if not applied then return false, apply_err end
    S.enabled = true
    local started, err = driver_call("start", 8000)
    command_id = started and err or nil
    if not started then
      S.enabled = false
      return false, "蓝牙手柄启动失败: " .. tostring(err)
    end
  else
    set_polling(false)
    local stopped, err = driver_call("stop")
    if not stopped then return false, "蓝牙手柄关闭失败: " .. tostring(err) end
    S.enabled = false
  end
  S.config.enabled = S.enabled
  S.driver_error = nil
  S.last_error = nil
  sync_driver_state()
  set_polling(S.enabled)
  local saved, save_err = save_config()
  if not saved then
    S.last_error = "状态已切换，但配置保存失败: " .. tostring(save_err)
    return false, S.last_error
  end
  log("driver", S.enabled and "enabled" or "disabled")
  return true, command_id
end

load_config()
S.enabled = S.config.enabled ~= false
start_web()
if not controller or not controller.publish or not ipc or not ipc.listen then
  S.driver_error = "固件版本不支持：需要 BLE ABI、controller.publish 和 ipc.listen"
  S.last_error = S.driver_error
  log(S.driver_error)
else
  start_ipc()
  local started = start_driver()
  set_polling(S.enabled and started == true)
  log("service started", APP.VERSION, APP.ROUTE_BASE)
end
