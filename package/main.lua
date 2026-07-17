local APP = {
  VERSION = "1.0.0",
  APP_DIR = "/sd/apps/hidpad",
  MODULE_PATH = "/sd/apps/hidpad/modules/hidpad.so",
  CONFIG_PATH = "/sd/apps/hidpad/config.json",
  SOURCE = "ble-main",
  IPC_ENDPOINT = "ble-controller",
  FIXED_ROUTE_BASE = "/hidpad",
  ROUTE_BASE = "/hidpad",
  POLL_READY_MS = 10,
  POLL_SCAN_MS = 20,
  POLL_IDLE_MS = 50,
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

local function merge_config(doc)
  if type(doc) ~= "table" then return end
  if type(doc.enabled) == "boolean" then S.config.enabled = doc.enabled end
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
  if not raw then return false, err end
  if not file or not file.putcontents then return false, "file.putcontents missing" end
  local ok, result = pcall(function() return file.putcontents(APP.CONFIG_PATH, raw) end)
  if not ok or result == false then return false, tostring(result or "write failed") end
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
  if not S.raw.connected then return false, "请先连接手柄" end
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
  return true, result
end

local function update_preferred(address, addr_type, profile, name)
  S.config.preferred_address = address or ""
  S.config.preferred_addr_type = math.floor(clamp(addr_type, 0, 3))
  S.config.preferred_profile = profile or ""
  S.config.preferred_name = name or ""
end

local function set_driver_preferred()
  if not S.driver or type(S.driver.set_preferred) ~= "function" then return true end
  local ok, err = pcall(S.driver.set_preferred,
    S.config.preferred_address or "",
    S.config.preferred_addr_type or 0,
    S.config.preferred_profile or "",
    S.config.preferred_name or "")
  if not ok then log("set preferred failed", tostring(err)) end
  return ok
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
    S.raw.last_report_handle = raw.last_report_handle or S.raw.last_report_handle or 0
    S.raw.last_report_len = raw.last_report_len or S.raw.last_report_len or 0
    S.raw.last_report_hex = raw.last_report_hex or S.raw.last_report_hex or ""
    for index = 0, 1 do
      for _, suffix in ipairs({ "handle", "id", "len", "notify_count", "hex" }) do
        local key = "report" .. index .. "_" .. suffix
        S.raw[key] = raw[key] ~= nil and raw[key] or S.raw[key]
      end
    end
  end
  if status_update then remember_ready_device(S.raw) end
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
    enabled = S.enabled,
    source = APP.SOURCE,
    route_base = APP.ROUTE_BASE,
    fixed_route_base = APP.FIXED_ROUTE_BASE,
    driver_error = S.driver_error,
    last_error = S.last_error or S.raw.last_error,
    phase = S.raw.phase or "stopped",
    profile = S.raw.profile or "unknown",
    connected = S.raw.connected == true,
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
      last_report_handle = S.raw.last_report_handle or 0,
      last_report_len = S.raw.last_report_len or 0,
      last_report_hex = S.raw.last_report_hex or "",
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
    connecting = S.raw.connecting == true,
    phase = S.raw.phase or "stopped",
    profile = S.raw.profile or "unknown",
    name = S.raw.name or "",
    address = S.raw.address or "",
    manual_scan = S.raw.manual_scan == true,
    scan_count = S.raw.scan_count or 0,
    last_error = S.last_error or S.raw.last_error,
    buttons = S.output.buttons or 0,
    raw_buttons = S.raw.buttons or 0,
    events = events,
    lx = S.raw.lx or 0, ly = S.raw.ly or 0,
    rx = S.raw.rx or 0, ry = S.raw.ry or 0,
    lt = S.raw.lt or 0, rt = S.raw.rt or 0,
    notify_count = S.raw.notify_count or 0,
    last_report_handle = S.raw.last_report_handle or 0,
    last_report_len = S.raw.last_report_len or 0,
    last_report_hex = S.raw.last_report_hex or "",
    report0_handle = S.raw.report0_handle or 0,
    report0_id = S.raw.report0_id or 0,
    report0_len = S.raw.report0_len or 0,
    report0_notify_count = S.raw.report0_notify_count or 0,
    report0_hex = S.raw.report0_hex or "",
    report1_handle = S.raw.report1_handle or 0,
    report1_id = S.raw.report1_id or 0,
    report1_len = S.raw.report1_len or 0,
    report1_notify_count = S.raw.report1_notify_count or 0,
    report1_hex = S.raw.report1_hex or "",
  }
end

-- IPC 状态保持为小型定长字段；按键/事件均使用整数位图。
local function service_status()
  return {
    version = APP.VERSION,
    enabled = S.enabled,
    phase = S.enabled and (S.raw.phase or "stopped") or "disabled",
    connected = S.raw.connected == true,
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
    error = S.last_error or S.raw.last_error,
  }
end

local function scan_devices()
  local devices = {}
  if not S.driver or type(S.driver.scan_count) ~= "function" or type(S.driver.scan_device) ~= "function" then
    return devices, "当前 hidpad.so 不支持设备选择"
  end
  local ok, count = pcall(S.driver.scan_count)
  if not ok then return devices, tostring(count) end
  count = math.min(tonumber(count) or 0, 8)
  for index = 1, count do
    local item_ok, item = pcall(S.driver.scan_device, index)
    if item_ok and type(item) == "table" then devices[#devices + 1] = item end
  end
  return devices, nil
end

local function devices_snapshot()
  local devices, err = scan_devices()
  if err then return { ok = false, devices = devices, error = err } end
  return { ok = true, scanning = S.raw.phase == "scanning", devices = devices }
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

  if topic == "status" then return true, nil end
  if topic == "enable" then return set_driver_enabled(true) end
  if topic == "disable" then return set_driver_enabled(false) end
  if topic == "rescan" then
    if not S.enabled then return false, "蓝牙手柄已禁用" end
    return driver_call("rescan")
  end
  if topic == "scan_devices" then return driver_call("scan") end
  if topic == "connect_device" then
    local address = type(doc.address) == "string" and doc.address or ""
    if address == "" then return false, "请选择要连接的手柄" end
    local selected = nil
    for _, device in ipairs(scan_devices()) do
      if device.address == address then selected = device; break end
    end
    local ok, err = driver_call("connect", address)
    if not ok then return false, err end
    update_preferred(address, selected and selected.addr_type,
      selected and selected.profile, selected and selected.name)
    local saved, save_err = save_config()
    if not saved then S.last_error = "保存首选手柄失败: " .. tostring(save_err) end
    return true, nil
  end
  if topic == "disconnect" then return driver_call("disconnect") end
  if topic == "pair" then return driver_call("pair") end
  if topic == "forget" then
    local ok, err = driver_call("forget")
    if ok then
      update_preferred()
      set_driver_preferred()
      local saved, save_err = save_config()
      if saved then
        S.last_error = nil
      else
        S.last_error = "忘记成功，但配置保存失败: " .. tostring(save_err)
      end
    end
    return ok, err
  end
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
    S.config = default_config()
    rebuild_mapping_bits()
    S.calibration = nil
    if S.driver and type(S.driver.set_preferred) == "function" then
      set_driver_preferred()
    end
    publish(S.raw)
    return save_config()
  end
  if topic == "set_mapping" then
    merge_config({ mapping = doc.mapping or doc })
    publish(S.raw)
    return save_config()
  end
  if topic == "set_config" then
    merge_config(doc)
    publish(S.raw)
    return save_config()
  end
  return false, "未知命令: " .. tostring(topic)
end

local PAGE = [==[
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BLE 手柄服务</title><style>
:root{color-scheme:light;--bg:#f4f6f8;--card:#fff;--text:#17202a;--muted:#68737d;--line:#dfe5ea;--blue:#1769e0;--green:#16845b;--red:#c63d3d;--shadow:0 8px 28px rgba(18,32,48,.07)}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.5 system-ui,"PingFang SC","Microsoft YaHei",sans-serif}main{max-width:1040px;margin:auto;padding:20px}.top{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:16px}h1{font-size:24px;margin:0}.sub{color:var(--muted);margin:3px 0 0}.status{display:inline-flex;align-items:center;gap:8px;padding:8px 12px;border:1px solid var(--line);border-radius:999px;background:var(--card)}.dot{width:9px;height:9px;border-radius:50%;background:#9aa4ad}.online .dot{background:var(--green)}.grid{display:grid;grid-template-columns:1.1fr .9fr;gap:16px}.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:18px;box-shadow:var(--shadow)}.card h2{font-size:16px;margin:0 0 14px}.meta{display:grid;grid-template-columns:86px 1fr;gap:7px 12px;margin-bottom:14px}.meta span:nth-child(odd){color:var(--muted)}.axes{display:grid;grid-template-columns:1fr 1fr;gap:10px}.axis{padding:10px;border-radius:10px;background:#f7f9fb}.axis b{display:flex;justify-content:space-between;margin-bottom:7px}.track{height:7px;background:#dde4ea;border-radius:9px;overflow:hidden}.fill{height:100%;width:50%;background:var(--blue)}.pressed{min-height:44px;display:flex;flex-wrap:wrap;gap:7px}.chip{padding:6px 9px;border-radius:8px;background:#e9f2ff;color:#1258b8;font-weight:650}.empty,.mask{color:var(--muted)}.mask{display:block;margin-top:5px}.field-label{display:block;color:var(--muted);margin-bottom:6px}.device-picker{display:grid;grid-template-columns:1fr auto;gap:9px;margin-bottom:10px}.device-picker select{min-width:0;min-height:44px;border:1px solid var(--line);border-radius:9px;padding:0 10px;background:#fff}button,select,input{font:inherit}button{min-height:44px;border:0;border-radius:9px;padding:0 15px;background:var(--blue);color:white;font-weight:650;cursor:pointer;touch-action:manipulation}button:active{filter:brightness(.92)}button:disabled{cursor:not-allowed;opacity:.5}button:focus-visible,select:focus-visible,input:focus-visible{outline:3px solid rgba(23,105,224,.28);outline-offset:2px}button.secondary{background:#edf1f5;color:var(--text);border:1px solid var(--line)}button.danger{background:#fff0f0;color:var(--red);border:1px solid #f1cccc}.actions{display:flex;flex-wrap:wrap;gap:9px}.notice{min-height:22px;margin:10px 0 0;color:var(--muted)}.notice.bad{color:var(--red)}.notice.good{color:var(--green)}.cal-help{color:var(--muted);margin:-5px 0 14px}.range-row{display:grid;grid-template-columns:100px 1fr 66px;gap:10px;align-items:center;margin:10px 0}.range-row input{width:100%}.map{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.map-row{display:grid;grid-template-columns:1fr 1.25fr;align-items:center;gap:8px}.map-row select{width:100%;min-height:44px;border:1px solid var(--line);border-radius:9px;padding:0 10px;background:#fff}.wide{grid-column:1/-1}.footer{color:var(--muted);margin:16px 2px 0;font-size:12px}@media(max-width:760px){main{padding:12px}.top{align-items:flex-start;flex-direction:column}.grid{grid-template-columns:1fr}.map{grid-template-columns:1fr}.card{padding:15px}.range-row{grid-template-columns:84px 1fr 58px}.device-picker{grid-template-columns:1fr}button,select,input{font-size:16px}}
</style></head><body><main><header class="top"><div><h1>BLE 手柄服务</h1><p class="sub">兼容 Xbox、Q34、Q36 · 输入映射与校准</p></div><div id="status" class="status"><i class="dot"></i><span>读取中</span></div></header>
<section class="grid"><article class="card"><h2>实时状态</h2><div class="meta"><span>设备</span><strong id="device">--</strong><span>地址</span><code id="address">--</code><span>驱动</span><span id="profile">--</span><span>阶段</span><span id="phase">--</span></div><div class="axes"><div class="axis"><b><span>LX</span><span id="lxv">0</span></b><div class="track"><div class="fill" id="lx"></div></div></div><div class="axis"><b><span>LY</span><span id="lyv">0</span></b><div class="track"><div class="fill" id="ly"></div></div></div><div class="axis"><b><span>RX</span><span id="rxv">0</span></b><div class="track"><div class="fill" id="rx"></div></div></div><div class="axis"><b><span>RY</span><span id="ryv">0</span></b><div class="track"><div class="fill" id="ry"></div></div></div><div class="axis"><b><span>LT</span><span id="ltv">0</span></b><div class="track"><div class="fill" id="lt"></div></div></div><div class="axis"><b><span>RT</span><span id="rtv">0</span></b><div class="track"><div class="fill" id="rt"></div></div></div></div><h2 style="margin-top:16px">应用收到的按键</h2><div id="pressed" class="pressed"><span class="empty">未按下</span></div><code id="buttonMask" class="mask">raw=0x0000 mapped=0x0000</code></article>
<article class="card"><h2>连接管理</h2><div class="actions" style="margin-bottom:14px"><button class="secondary" id="enableDriver" data-cmd="enable">启用驱动</button><button class="secondary" id="disableDriver" data-cmd="disable">禁用驱动</button></div><label class="field-label" for="deviceList">扫描到的手柄</label><div class="device-picker"><select id="deviceList"><option value="">点击“扫描手柄”查找设备</option></select><button id="connectDevice" disabled>连接并配对</button></div><div class="actions"><button class="secondary" id="scanDevices">扫描手柄</button><button class="secondary" data-cmd="pair">重新配对当前</button><button class="secondary" data-cmd="disconnect">断开</button><button class="danger" data-cmd="forget">忘记当前手柄</button></div><p id="message" class="notice" aria-live="polite"></p><h2 style="margin-top:22px">摇杆校准</h2><p class="cal-help">手柄静止时开始，然后把两个摇杆转满一圈、按满扳机，最后保存。</p><div class="actions"><button id="calStart" data-cmd="calibration_start">开始采样</button><button class="secondary" data-cmd="calibration_save">保存校准</button><button class="secondary" data-cmd="calibration_cancel">取消</button></div><div class="range-row"><label for="deadzone">中心死区</label><input id="deadzone" type="range" min="0" max="16000" step="100"><output id="deadzoneValue">0</output></div><div class="actions"><button class="secondary" id="saveConfig">保存死区</button><button class="danger" data-cmd="restore_defaults">恢复默认</button></div></article>
<article class="card wide"><h2>按键映射</h2><p class="cal-help">左侧是应用收到的目标键，右侧选择手柄原始键。支持交换 A/B、X/Y 或自定义肩键。</p><div id="mapping" class="map"></div><div class="actions" style="margin-top:16px"><button id="saveMapping">保存映射</button></div></article></section><p class="footer">输入通过 controller source <code>ble-main</code> 发布；RetroGo 无需直接持有 BLE。</p></main>
<script>
const base=location.pathname.replace(/\/$/,''), names=['UP','DOWN','LEFT','RIGHT','A','B','X','Y','L','R','LS','RS','SELECT','START','SHARE','HOME'], bits=[1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768], labels={'UP':'方向 上','DOWN':'方向 下','LEFT':'方向 左','RIGHT':'方向 右','A':'A','B':'B','X':'X','Y':'Y','L':'LB / L','R':'RB / R','LS':'左摇杆按下','RS':'右摇杆按下','SELECT':'View / Select','START':'Menu / Start','SHARE':'Share','HOME':'Home'};let initialized=false,currentMask=0,currentRawMask=0,latchedMask=0,latchTimer,devicesKey='',scanMode=false,statusBusy=false,padConnected=false,driverEnabled=true;
const $=id=>document.getElementById(id);function msg(text,kind=''){const e=$('message');e.textContent=text||'';e.className='notice '+kind}function axis(id,v,trigger=false){v=Number(v)||0;$(id+'v').textContent=v;$(id).style.width=(trigger?Math.max(0,Math.min(100,v/65535*100)):Math.max(0,Math.min(100,(v+32767)/65534*100)))+'%'}
function syncDriverButtons(){ $('enableDriver').disabled=driverEnabled;$('disableDriver').disabled=!driverEnabled;$('scanDevices').disabled=!driverEnabled }
function run(button,promise){button.disabled=true;return promise.finally(()=>{button.disabled=false;syncDriverButtons()})}
function buildMap(mapping){const root=$('mapping');root.innerHTML='';for(const key of names){const row=document.createElement('label');row.className='map-row';row.innerHTML='<span>'+labels[key]+'</span>';const select=document.createElement('select');select.dataset.target=key;for(const source of ['NONE',...names]){const o=document.createElement('option');o.value=source;o.textContent=source==='NONE'?'不映射':labels[source];o.selected=(mapping[key]||key)===source;select.appendChild(o)}row.appendChild(select);root.appendChild(row)}}
function renderButtons(){const mask=currentMask|latchedMask,list=[];for(let i=0;i<bits.length;i++)if(mask&bits[i])list.push(names[i]);$('pressed').innerHTML=list.length?list.map(x=>'<span class="chip">'+labels[x]+'</span>').join(''):'<span class="empty">未按下</span>';$('buttonMask').textContent='raw=0x'+currentRawMask.toString(16).padStart(4,'0').toUpperCase()+' mapped=0x'+currentMask.toString(16).padStart(4,'0').toUpperCase()}
function paintInput(s){const connected=!!s.connected,status=$('status'),wasScanning=scanMode;driverEnabled=s.enabled!==false;padConnected=connected;scanMode=!!s.manual_scan||s.phase==='scanning'||s.phase==='select_device';statusBusy=connected||!!s.connecting||scanMode;syncDriverButtons();$('connectDevice').disabled=!driverEnabled||connected||!$('deviceList').value;status.classList.toggle('online',connected);status.querySelector('span').textContent=!driverEnabled?'已禁用':(connected?'已连接':(s.connecting?'连接中':(scanMode?'扫描中':'未连接')));$('device').textContent=s.name||'--';$('address').textContent=s.address||'--';$('profile').textContent=s.profile||'--';$('phase').textContent=s.phase||'--';for(const k of ['lx','ly','rx','ry'])axis(k,s[k]);for(const k of ['lt','rt'])axis(k,s[k],true);currentMask=Number(s.buttons)||0;currentRawMask=Number(s.raw_buttons)||0;const events=Number(s.events)||0;if(events){latchedMask|=events;clearTimeout(latchTimer);latchTimer=setTimeout(()=>{latchedMask=0;renderButtons()},700)}renderButtons();if(wasScanning&&!scanMode)devices().catch(()=>{});if(s.last_error)msg(s.last_error,'bad');else if(connected&&$('message').classList.contains('bad'))msg('')}
function paint(s){paintInput({enabled:s.enabled,connected:s.connected,connecting:s.connecting,manual_scan:s.manual_scan,phase:s.phase,profile:s.profile,name:s.name,address:s.address,buttons:s.output&&s.output.buttons,raw_buttons:s.raw&&s.raw.buttons,lx:s.raw&&s.raw.lx,ly:s.raw&&s.raw.ly,rx:s.raw&&s.raw.rx,ry:s.raw&&s.raw.ry,lt:s.raw&&s.raw.lt,rt:s.raw&&s.raw.rt});$('calStart').textContent=s.calibrating?'正在采样…':'开始采样';if(!initialized&&s.config){$('deadzone').value=s.config.deadzone||0;$('deadzoneValue').textContent=$('deadzone').value;buildMap(s.config.mapping||{});initialized=true}if(s.last_error)msg(s.last_error,'bad')}
function paintDevices(d){const list=d.devices||[],key=(d.scanning?'1':'0')+JSON.stringify(list),select=$('deviceList'),old=select.value;if(key===devicesKey)return;devicesKey=key;select.innerHTML='';$('connectDevice').disabled=!driverEnabled||!list.length||padConnected;if(!list.length){const o=document.createElement('option');o.value='';o.textContent=d.scanning?'正在扫描…':'没有发现支持的手柄';select.appendChild(o);return}for(const x of list){const o=document.createElement('option');o.value=x.address;o.textContent=(x.name||'BLE HID Gamepad')+' · '+x.address+' · '+x.rssi+' dBm';select.appendChild(o)}if([...select.options].some(x=>x.value===old))select.value=old}
async function state(){const r=await fetch(base+'/api/state',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);paint(await r.json())}async function input(){const r=await fetch(base+'/api/input',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);paintInput(await r.json())}async function devices(){const r=await fetch(base+'/api/devices',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);paintDevices(await r.json())}async function command(topic,payload={}){msg('处理中…');const r=await fetch(base+'/api/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({topic,payload})});const d=await r.json();if(!r.ok||!d.ok)throw Error(d.error||('HTTP '+r.status));msg('已完成','good');paint(d.state);return d}
async function pollInput(){try{await input()}catch(e){}setTimeout(pollInput,document.hidden?5000:(statusBusy?400:1500))}async function pollDevices(){if(scanMode){try{await devices()}catch(e){}}setTimeout(pollDevices,document.hidden?2000:(scanMode?800:1000))}
document.addEventListener('click',e=>{const cmd=e.target.dataset&&e.target.dataset.cmd;if(cmd)run(e.target,command(cmd)).catch(x=>msg(x.message,'bad'))});$('scanDevices').onclick=e=>{scanMode=true;devicesKey='';run(e.currentTarget,command('scan_devices').then(devices)).catch(x=>msg(x.message,'bad'))};$('connectDevice').onclick=e=>run(e.currentTarget,command('connect_device',{address:$('deviceList').value})).catch(x=>msg(x.message,'bad'));$('deadzone').oninput=()=>$('deadzoneValue').textContent=$('deadzone').value;$('saveConfig').onclick=e=>run(e.currentTarget,command('set_config',{deadzone:Number($('deadzone').value)})).catch(x=>msg(x.message,'bad'));$('saveMapping').onclick=e=>{const mapping={};document.querySelectorAll('select[data-target]').forEach(x=>mapping[x.dataset.target]=x.value);run(e.currentTarget,command('set_mapping',{mapping})).catch(x=>msg(x.message,'bad'))};state().catch(x=>msg(x.message,'bad')).finally(()=>{devices().catch(()=>{});pollInput();pollDevices()});
</script></body></html>
]==]

local function route_command(req)
  local raw, read_err = read_body(req, 4096)
  if not raw then return json_response("400 Bad Request", { ok = false, error = read_err }) end
  local doc, decode_err = json_decode(raw)
  if not doc or type(doc.topic) ~= "string" then
    return json_response("400 Bad Request", { ok = false, error = decode_err or "topic missing" })
  end
  local payload = doc.payload
  local payload_raw = type(payload) == "table" and json_encode(payload) or payload
  local ok, err = handle_command(doc.topic, payload_raw or "")
  if not ok then return json_response("400 Bad Request", { ok = false, error = err, state = state_snapshot() }) end
  return json_response("200 OK", { ok = true, state = state_snapshot() })
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

local function register_route_set(base)
  if type(base) ~= "string" or base == "" then return end
  local get, post = httpd.GET or "GET", httpd.POST or "POST"
  register_route(get, base, function() return response("200 OK", "text/html; charset=utf-8", PAGE) end)
  register_route(get, base .. "/", function() return response("200 OK", "text/html; charset=utf-8", PAGE) end)
  register_route(get, base .. "/api/state", function() return json_response("200 OK", state_snapshot()) end)
  register_route(get, base .. "/api/input", function() return json_response("200 OK", input_snapshot()) end)
  register_route(get, base .. "/api/devices", function() return json_response("200 OK", devices_snapshot()) end)
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
    local interval = APP.POLL_READY_MS
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
  if enable then
    S.enabled = true
    local started, err = driver_call("start", 8000)
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
  return true, nil
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
