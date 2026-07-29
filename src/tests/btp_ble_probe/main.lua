-- Minimal BTP-KP20D BFM BLE probe.
--
-- Deliberately independent from hidpad.so: this performs a complete HOGP
-- discovery/read/subscribe sequence in Lua. JoyU's normal connection path
-- subscribes 7313 and mirrors the official app's 09 command sync session.
-- The HTTP command endpoint can probe a stalled link without power-cycling:
-- read_report checks whether ATT is still responsive, while watchdog_cycle
-- mirrors JoyU's pause/resume WDT lifecycle.
--
-- The known KP20D layout is:
--   Report ID 3 value handle 26, CCCD handle 27
--   Report ID 4 value handle 30, CCCD handle 31
-- This isolation test first performs Android-style complete primary-service
-- discovery, then completes the private 0x7311/0x7312/0x7313/0x7320
-- initialization before enabling the HID input reports.

local CFG = {
  address = "40:e4:02:16:a4:65",
  addr_type = 0,
  report_handle = 26,
  cccd_handles = { 27 },
  preferred_mtu = 23,
  mode = "kp20d-official-09-sync+session-before-hid+mtu23",
  route = "/btp-probe",
  screen_width = 320,
  screen_height = 240,
}

local S = {
  version = "1.1.17",
  mode = CFG.mode,
  phase = "starting",
  connected = false,
  connecting = false,
  encrypted = false,
  authenticated = false,
  bonded = false,
  encryption_status = 0,
  connected_ms = 0,
  conn_handle = -1,
  address = CFG.address,
  scan_count = 0,
  connection_count = 0,
  disconnect_count = 0,
  disconnect_reason = 0,
  notify_count = 0,
  other_notify_count = 0,
  report3_notify_count = 0,
  report4_notify_count = 0,
  negotiated_mtu = 0,
  mtu_status = 0,
  service_count = 0,
  services = {},
  gatt_service_start = 0,
  gatt_service_end = 0,
  service_changed_handle = 0,
  service_changed_cccd_handle = 0,
  service_changed_subscribe_status = 0,
  service_changed_recovery_count = 0,
  service_changed_recovery_status = 0,
  service_changed_recovery_ms = 0,
  service_changed_recovery_connection_count = 0,
  service_changed_recovered = false,
  client_supported_features_handle = 0,
  database_hash_handle = 0,
  recovery_service_count = 0,
  ready_ms = 0,
  control_recovery_count = 0,
  control_recovery_started_ms = 0,
  control_recovery_connection_count = 0,
  control_recovery_notify_baseline = 0,
  control_recovery_suspend_accepted = false,
  control_recovery_exit_accepted = false,
  control_recovery_result = "",
  control_recovered = false,
  gap_service_start = 0,
  gap_service_end = 0,
  ppcp_handle = 0,
  ppcp_hex = "",
  ppcp_min_interval = 0,
  ppcp_max_interval = 0,
  ppcp_latency = 0,
  ppcp_timeout = 0,
  ppcp_read_status = 0,
  hid_info_handle = 0,
  hid_info_hex = "",
  hid_info_read_status = 0,
  first_notify_ms = 0,
  last_notify_ms = 0,
  last_notify_gap_ms = 0,
  max_notify_gap_ms = 0,
  last_report_hex = "",
  last_report_len = 0,
  buttons_text = "NONE",
  last_active_buttons_text = "NONE",
  last_active_ms = 0,
  active_report_count = 0,
  dpad = "CENTER",
  lx = 128,
  ly = 128,
  rx = 128,
  ry = 128,
  lt = 0,
  rt = 0,
  cccd_index = 0,
  cccd_action = "",
  cccd_disable_status = 0,
  cccd_write_status = 0,
  subscription_cycle_count = 0,
  stall_rearm_count = 0,
  stall_rearm_disable_status = 0,
  stall_rearm_enable_status = 0,
  stall_rearm_ms = 0,
  hid_service_start = 0,
  hid_service_end = 0,
  report_map_handle = 0,
  report_map_hex = "",
  report_map_len = 0,
  external_report_reference_handle = 0,
  external_report_reference_hex = "",
  external_report_reference_status = 0,
  protocol_mode_handle = 0,
  protocol_mode_write_accepted = false,
  report_count = 0,
  input_report_count = 0,
  control_point_handle = 0,
  control_point_write_count = 0,
  control_point_write_status = 0,
  control_point_write_error_count = 0,
  control_point_last_write_ms = 0,
  vendor_service_start = 0,
  vendor_service_end = 0,
  vendor_input_handle = 0,
  vendor_write_handle = 0,
  vendor_reply_handle = 0,
  vendor_aux_handle = 0,
  vendor_subscribed_count = 0,
  vendor_notify_count = 0,
  vendor_write_count = 0,
  vendor_heartbeat_count = 0,
  vendor_heartbeat_reply_count = 0,
  vendor_watchdog_reply_count = 0,
  vendor_last_notify_ms = 0,
  vendor_stage = 0,
  vendor_handshake_sent = false,
  vendor_seen_battery_reply = false,
  vendor_seen_info_reply = false,
  vendor_session_complete = false,
  vendor_session_start_count = 0,
  vendor_session_retry_count = 0,
  official_sync_started = false,
  official_sync_cycle_count = 0,
  official_sync_write_count = 0,
  official_sync_last_ms = 0,
  official_sync_read_pending = false,
  official_sync_read_count = 0,
  official_sync_read_status = 0,
  official_sync_read_hex = "",
  vendor_first_disable_count = 0,
  vendor_first_disable_status = 0,
  hid_subscription_started = false,
  vendor_last_hex = "",
  vendor_read_hex = "",
  diagnostic_action = "",
  diagnostic_count = 0,
  diagnostic_started_ms = 0,
  diagnostic_notify_baseline = 0,
  diagnostic_result = "",
  diagnostic_read_hex = "",
  diagnostic_read_status = 0,
  diagnostic_read_pending = false,
  event_count = 0,
  last_error = "",
  events = {},
  ui_available = false,
  ui_error = "",
  ui_draw_count = 0,
}

local action_timer
local watchdog_timer
local vendor_timer
local official_sync_timer
local official_sync_read_timer

local V = {
  characteristics = {},
  channels = {},
  descriptor_index = 0,
  subscribe_index = 0,
}

local H = {
  characteristics = {},
  report_map = nil,
  reports = {},
  descriptor_index = 0,
  reference_index = 0,
  disable_index = 0,
  subscribe_index = 0,
}

local G = {
  characteristics = {},
  service_changed = nil,
}

local UI = {
  canvas = nil,
  last_render_key = nil,
  colors = {
    background = 0x0B1020,
    panel = 0x151C2F,
    border = 0x27324A,
    text = 0xF4F7FB,
    muted = 0x94A3B8,
    blue = 0x4DA3FF,
    green = 0x39D98A,
    amber = 0xF5B942,
    red = 0xFF5C6C,
  },
}

local function now_ms()
  if tmr and tmr.now then
    local ok, value = pcall(tmr.now)
    if ok and type(value) == "number" then
      -- tmr.now() is the signed view of a wrapping uint32 microsecond
      -- counter. Normalize it before converting to milliseconds.
      if value < 0 then value = value + 4294967296 end
      return math.floor(value / 1000)
    end
  end
  return 0
end

local function elapsed_ms(at, before)
  if before == 0 then return 0 end
  local elapsed = at - before
  if elapsed < 0 then elapsed = elapsed + 4294967 end
  return elapsed
end

local function hex(data)
  if type(data) ~= "string" then return "" end
  return (data:gsub(".", function(ch) return string.format("%02x", string.byte(ch)) end))
end

local function is_uuid16(value, short)
  local text = string.lower(tostring(value or ""))
  short = string.lower(tostring(short or ""))
  if text:sub(1, 2) == "0x" then text = text:sub(3) end
  return text == short or text:find("0000" .. short .. "-", 1, true) ~= nil
end

local function has_bit(value, bit)
  return math.floor((tonumber(value) or 0) / (2 ^ bit)) % 2 == 1
end

local function decode_report(data)
  if type(data) ~= "string" or #data < 10 then return end

  local hat = (string.byte(data, 2) or 15) % 16
  local hats = {
    [0] = { "UP" },
    [1] = { "UP", "RIGHT" },
    [2] = { "RIGHT" },
    [3] = { "DOWN", "RIGHT" },
    [4] = { "DOWN" },
    [5] = { "DOWN", "LEFT" },
    [6] = { "LEFT" },
    [7] = { "UP", "LEFT" },
  }
  local names = {}
  local directions = hats[hat]
  if directions then
    S.dpad = table.concat(directions, "+")
    for _, name in ipairs(directions) do names[#names + 1] = name end
  else
    S.dpad = "CENTER"
  end

  local mask = (string.byte(data, 3) or 0) +
    (string.byte(data, 4) or 0) * 256
  local mappings = {
    { 0, "A" },
    { 1, "B" },
    { 3, "X" },
    { 4, "Y" },
    { 6, "LB" },
    { 7, "RB" },
    { 10, "VIEW" },
    { 11, "MENU" },
    { 12, "HOME" },
    { 13, "SHARE" },
  }
  for _, item in ipairs(mappings) do
    if has_bit(mask, item[1]) then names[#names + 1] = item[2] end
  end

  S.lx = string.byte(data, 5) or 128
  S.ly = string.byte(data, 6) or 128
  S.rx = string.byte(data, 7) or 128
  S.ry = string.byte(data, 8) or 128
  S.lt = string.byte(data, 9) or 0
  S.rt = string.byte(data, 10) or 0

  if (has_bit(mask, 8) or S.lt > 24) then names[#names + 1] = "LT" end
  if (has_bit(mask, 9) or S.rt > 24) then names[#names + 1] = "RT" end
  S.buttons_text = #names > 0 and table.concat(names, "  ") or "NONE"
  if S.buttons_text ~= "NONE" then
    S.last_active_buttons_text = S.buttons_text
    S.last_active_ms = now_ms()
    S.active_report_count = S.active_report_count + 1
  end
end

local function screen_status()
  local since_notify = elapsed_ms(now_ms(), S.last_notify_ms)
  if S.last_error ~= "" then return "ERROR", UI.colors.red end
  if S.connected and S.notify_count > 0 and since_notify > 2000 then
    return "STALLED", UI.colors.red
  end
  if S.connected and S.encrypted and S.phase == "ready" then
    if S.notify_count == 0 then return "WAIT INPUT", UI.colors.amber end
    return "CONNECTED", UI.colors.green
  end
  if S.phase == "failed" then return "FAILED", UI.colors.red end
  return string.upper(S.phase or "STARTING"), UI.colors.amber
end

local function draw_ui()
  if not UI.canvas then return end
  local status, status_color = screen_status()
  local displayed_buttons = S.buttons_text
  if displayed_buttons == "NONE" and S.last_active_ms > 0 and
     elapsed_ms(now_ms(), S.last_active_ms) < 700 then
    displayed_buttons = S.last_active_buttons_text
  end
  local raw = S.last_report_hex ~= "" and string.upper(S.last_report_hex) or "--"
  local render_key = table.concat({
    status,
    tostring(status_color),
    displayed_buttons,
    tostring(S.lx), tostring(S.ly), tostring(S.rx), tostring(S.ry),
    tostring(S.lt), tostring(S.rt),
    tostring(S.notify_count), tostring(S.last_notify_gap_ms),
    raw,
  }, "\0")
  if render_key == UI.last_render_key then return end

  local begin_frame = rawget(_G, "lv_canvas_frame_begin") or
    rawget(_G, "lv_canvas_begin")
  local end_frame = rawget(_G, "lv_canvas_frame_end") or
    rawget(_G, "lv_canvas_end")
  local fill = rawget(_G, "lv_canvas_fill_bg") or
    rawget(_G, "lv_canvas_fill")
  local rect = rawget(_G, "lv_canvas_draw_rect")
  local text = rawget(_G, "lv_canvas_draw_text")
  if type(begin_frame) ~= "function" or type(end_frame) ~= "function" or
     type(fill) ~= "function" or type(rect) ~= "function" or
     type(text) ~= "function" then
    S.ui_error = "canvas drawing API missing"
    return
  end

  local align_left = rawget(_G, "LV_TEXT_ALIGN_LEFT") or 0
  local align_center = rawget(_G, "LV_TEXT_ALIGN_CENTER") or 1
  local began, begin_error = pcall(begin_frame, UI.canvas)
  if not began then
    S.ui_error = "frame begin: " .. tostring(begin_error)
    return
  end
  local ok, draw_error = pcall(function()
    if service_ui and service_ui.clear then service_ui.clear(UI.canvas) end
    fill(UI.canvas, UI.colors.background, 255)

    rect(UI.canvas, 0, 0, 320, 44, {
      bg_color = UI.colors.panel, bg_opa = 255, border_width = 0,
    })
    text(UI.canvas, 14, 13, 196, "BTP HID PROBE", {
      color = UI.colors.text, opa = 255, align = align_left, font_size = 16,
    })
    rect(UI.canvas, 218, 8, 90, 28, {
      bg_color = status_color, bg_opa = 255, border_width = 0, radius = 7,
    })
    text(UI.canvas, 220, 14, 86, status, {
      color = UI.colors.background, opa = 255, align = align_center, font_size = 13,
    })

    text(UI.canvas, 14, 54, 292, "ID3  40:E4:02:16:A4:65", {
      color = UI.colors.muted, opa = 255, align = align_left, font_size = 13,
    })

    rect(UI.canvas, 12, 78, 296, 82, {
      bg_color = UI.colors.panel, bg_opa = 255,
      border_color = UI.colors.border, border_opa = 255,
      border_width = 1, radius = 9,
    })
    text(UI.canvas, 24, 90, 272, "BUTTONS", {
      color = UI.colors.blue, opa = 255, align = align_left, font_size = 13,
    })
    text(UI.canvas, 24, 119, 272, displayed_buttons, {
      color = UI.colors.text, opa = 255, align = align_left, font_size = 20,
    })

    text(UI.canvas, 16, 171, 288,
      string.format("L %3d,%3d    R %3d,%3d", S.lx, S.ly, S.rx, S.ry), {
        color = UI.colors.text, opa = 255, align = align_left, font_size = 14,
      })
    text(UI.canvas, 16, 193, 288,
      string.format("LT %3d  RT %3d    PKT %d", S.lt, S.rt, S.notify_count), {
        color = UI.colors.muted, opa = 255, align = align_left, font_size = 13,
      })
    text(UI.canvas, 16, 217, 288,
      "RAW " .. raw .. "  GAP " .. tostring(S.last_notify_gap_ms) .. "ms", {
        color = UI.colors.muted, opa = 255, align = align_left, font_size = 12,
      })
  end)
  pcall(end_frame, UI.canvas)
  if not ok then
    S.ui_error = "draw: " .. tostring(draw_error)
    return
  end

  if service_ui and service_ui.show then pcall(service_ui.show, UI.canvas) end
  UI.last_render_key = render_key
  S.ui_draw_count = S.ui_draw_count + 1
  S.ui_error = ""
end

local function init_ui()
  if not service_ui or type(service_ui.acquire) ~= "function" then
    S.ui_error = "service_ui unavailable"
    return false
  end
  local id, err = service_ui.acquire(
    0, 0, CFG.screen_width, CFG.screen_height
  )
  if not id then
    S.ui_error = "acquire: " .. tostring(err or "failed")
    print("[btp-probe]", S.ui_error)
    return false
  end
  UI.canvas = id
  S.ui_available = true
  draw_ui()
  return true
end

local function add_event(kind, detail)
  local events = S.events
  events[#events + 1] = {
    ms = now_ms(),
    kind = tostring(kind or ""),
    detail = tostring(detail or ""),
  }
  if #events > 24 then table.remove(events, 1) end
  print("[btp-probe]", kind, detail or "")
end

local function set_error(message)
  S.last_error = tostring(message or "")
  add_event("error", S.last_error)
end

local function schedule(delay_ms, callback)
  if not tmr or not tmr.create then
    set_error("tmr.create missing")
    return false
  end
  if action_timer then pcall(function() action_timer:stop() end) end
  action_timer = tmr.create()
  action_timer:alarm(delay_ms, tmr.ALARM_SINGLE, function()
    callback()
  end)
  return true
end

local function schedule_vendor(delay_ms, callback)
  if not tmr or not tmr.create then
    set_error("tmr.create missing")
    return false
  end
  if vendor_timer then pcall(function() vendor_timer:stop() end) end
  vendor_timer = tmr.create()
  vendor_timer:alarm(delay_ms, tmr.ALARM_SINGLE, function()
    callback()
  end)
  return true
end

local function start_scan()
  if S.connected or S.connecting then return end
  S.phase = "scanning"
  local ok, err = ble.gap_scan(5000, 48000, 30000, true)
  if not ok then
    set_error("scan: " .. tostring(err))
    S.phase = "wait_scan"
    schedule(1000, start_scan)
  end
end

local function connect(addr_type)
  if S.connected or S.connecting then return end
  S.connecting = true
  S.phase = "connecting"
  local ok, err = ble.gap_connect(addr_type or CFG.addr_type, CFG.address, 15000)
  if not ok then
    S.connecting = false
    S.phase = "wait_scan"
    set_error("connect: " .. tostring(err))
    schedule(1000, start_scan)
  end
end

local function write_cccd(handle, enabled)
  S.cccd_action = enabled and "enable" or "disable"
  S.phase = enabled and "subscribe" or "reset_cccd"
  local value = enabled and 1 or 0
  local ok, err = ble.gattc_write(
    S.conn_handle,
    handle,
    string.char(value, 0),
    ble.WRITE_WITH_RESPONSE
  )
  if not ok then
    set_error("CCCD " .. S.cccd_action .. " " .. handle .. ": " .. tostring(err))
  end
end

local start_vendor_discovery
local start_gap_discovery
local start_hid_discovery
local start_all_service_discovery
local start_gatt_discovery
local discover_next_hid_descriptors
local read_next_hid_reference
local set_protocol_then_subscribe
local disable_next_hid_before_vendor
local begin_hid_subscription
local subscribe_next
local discover_next_vendor_descriptors
local subscribe_next_vendor
local start_vendor_session
local start_control_recovery
local retry_vendor_session
local finish_vendor_session
local start_official_sync

local function complete_ready()
  S.phase = "ready"
  S.ready_ms = now_ms()
  S.last_error = ""
  add_event("ready",
    CFG.mode .. " cycle=" .. tostring(S.subscription_cycle_count))
end

start_control_recovery = function(reason)
  if not S.connected or S.control_point_handle == 0 or
     S.control_recovery_count > 0 then
    return false
  end
  S.control_recovery_count = S.control_recovery_count + 1
  S.control_recovery_started_ms = now_ms()
  S.control_recovery_connection_count = S.connection_count
  S.control_recovery_notify_baseline = S.notify_count
  S.control_recovery_suspend_accepted = false
  S.control_recovery_exit_accepted = false
  S.control_recovery_result = "suspend"
  S.control_recovered = false
  S.phase = "control_recovery"
  add_event("control_recovery",
    "suspend reason=" .. tostring(reason) ..
    " conn_count=" .. tostring(S.connection_count))
  local ok, err = ble.gattc_write(
    S.conn_handle,
    S.control_point_handle,
    string.char(0),
    ble.WRITE_NO_RESPONSE
  )
  if not ok then
    S.control_recovery_result = "suspend_failed: " .. tostring(err)
    set_error("Control Point suspend: " .. tostring(err))
    return false
  end
  S.control_recovery_suspend_accepted = true
  schedule(150, function()
    if not S.connected or
       S.connection_count ~= S.control_recovery_connection_count then
      S.control_recovery_result = "connection_changed"
      return
    end
    local exit_ok, exit_err = ble.gattc_write(
      S.conn_handle,
      S.control_point_handle,
      string.char(1),
      ble.WRITE_NO_RESPONSE
    )
    if not exit_ok then
      S.control_recovery_result = "exit_failed: " .. tostring(exit_err)
      set_error("Control Point exit suspend: " .. tostring(exit_err))
      return
    end
    S.control_recovery_exit_accepted = true
    S.control_recovery_result = "waiting_for_notify"
    add_event("control_recovery", "exit_suspend accepted")
    schedule(250, function()
      if S.connected and
         S.connection_count == S.control_recovery_connection_count and
         S.phase == "control_recovery" then
        S.phase = "ready"
      end
    end)
  end)
  return true
end

local function write_vendor(data, label)
  if not S.connected or S.vendor_write_handle == 0 then
    set_error("vendor write unavailable")
    return false
  end
  local ok, err = ble.gattc_write(
    S.conn_handle,
    S.vendor_write_handle,
    data,
    ble.WRITE_NO_RESPONSE
  )
  if not ok then
    set_error("vendor " .. tostring(label) .. ": " .. tostring(err))
    return false
  end
  S.vendor_write_count = S.vendor_write_count + 1
  add_event("vendor_write", tostring(label) .. "=" .. hex(data))
  return true
end

local function write_official_sync(command)
  if not S.connected or S.vendor_write_handle == 0 then return false end
  local payload = string.char(0x09, command, 0x00, 0x00)
  local ok, err = ble.gattc_write(
    S.conn_handle,
    S.vendor_write_handle,
    payload,
    ble.WRITE_NO_RESPONSE
  )
  if not ok then
    add_event("official_sync_error",
      hex(payload) .. " " .. tostring(err))
    return false
  end
  S.official_sync_write_count = S.official_sync_write_count + 1
  S.official_sync_last_ms = now_ms()
  return true
end

start_official_sync = function()
  if S.official_sync_started or not S.connected or
     S.vendor_write_handle == 0 then
    return
  end
  S.official_sync_started = true
  S.official_sync_cycle_count = 0
  S.official_sync_write_count = 0
  S.official_sync_read_pending = false
  S.official_sync_read_count = 0
  S.official_sync_read_status = 0
  S.official_sync_read_hex = ""

  -- JoyU sends 09 01 00 00 once at startup. Its 500 ms runnable then
  -- sends only 09 21 00 00 and reads 0x7320. It repeats 09 01 only
  -- after a controller-originated 09 02 sync request.
  write_official_sync(0x01)
  add_event("official_sync", "start 09010000")

  if not tmr or not tmr.create then return end
  official_sync_timer = tmr.create()
  official_sync_read_timer = tmr.create()
  official_sync_timer:alarm(500, tmr.ALARM_AUTO, function()
    if not S.connected or not S.official_sync_started then return end
    S.official_sync_cycle_count = S.official_sync_cycle_count + 1
    write_official_sync(0x21)
    if S.official_sync_cycle_count % 10 == 0 then
      add_event("official_sync",
        "cycle=" .. tostring(S.official_sync_cycle_count) ..
        " writes=" .. tostring(S.official_sync_write_count))
    end
    official_sync_read_timer:alarm(20, tmr.ALARM_SINGLE, function()
      if not S.connected or not S.official_sync_started or
         S.official_sync_read_pending or S.vendor_aux_handle == 0 then
        return
      end
      S.official_sync_read_pending = true
      S.official_sync_read_hex = ""
      local ok, err = ble.gattc_read(
        S.conn_handle, S.vendor_aux_handle
      )
      if not ok then
        S.official_sync_read_pending = false
        S.official_sync_read_status = -1
        add_event("official_sync_read_error", tostring(err))
      end
    end)
  end)
end

retry_vendor_session = function()
  if not S.connected then return end
  if S.vendor_seen_battery_reply and S.vendor_seen_info_reply then
    finish_vendor_session()
    return
  end
  S.vendor_session_retry_count = S.vendor_session_retry_count + 1
  if not S.vendor_seen_info_reply then
    write_vendor(string.char(0x55), "official_info_retry")
  end
  schedule_vendor(50, function()
    if not S.connected then return end
    if not S.vendor_seen_battery_reply then
      write_vendor(string.char(0x15), "official_battery_retry")
    end
    schedule_vendor(1000, retry_vendor_session)
  end)
end

finish_vendor_session = function()
  if S.vendor_session_complete then return end
  if not S.vendor_seen_battery_reply or not S.vendor_seen_info_reply then
    return
  end
  S.vendor_session_complete = true
  add_event("vendor_session", "complete before HID")
  schedule(100, begin_hid_subscription)
end

start_vendor_session = function()
  -- JoyU 6.8.1 treats KP20D as its PC-handle protocol family:
  -- q.b() sends 21 00 00, 15, 55 immediately. q$1 retries only the missing
  -- 15/55 queries every second because u.h("KP20D") is false. WDT and
  -- 11 00 20 are not part of this model's base connection sequence.
  S.vendor_stage = 1
  S.vendor_seen_battery_reply = false
  S.vendor_seen_info_reply = false
  S.vendor_session_complete = false
  S.vendor_session_start_count = S.vendor_session_start_count + 1
  start_official_sync()
  write_vendor(string.char(0x21, 0x00, 0x00), "official_session")
  schedule_vendor(50, function()
    if not S.connected then return end
    write_vendor(string.char(0x15), "official_battery")
    schedule_vendor(50, function()
      if not S.connected then return end
      write_vendor(string.char(0x55), "official_info")
      schedule_vendor(500, retry_vendor_session)
    end)
  end)
end

subscribe_next_vendor = function()
  V.subscribe_index = V.subscribe_index + 1
  local channel = V.channels[V.subscribe_index]
  while channel and (tonumber(channel.cccd_handle) or 0) == 0 do
    V.subscribe_index = V.subscribe_index + 1
    channel = V.channels[V.subscribe_index]
  end
  if not channel then
    if S.vendor_input_handle ~= 0 then
      S.phase = "vendor_read"
      local ok, err = ble.gattc_read(S.conn_handle, S.vendor_input_handle)
      if ok then return end
      add_event("vendor_read_skip", tostring(err or "start failed"))
    end
    start_vendor_session()
    return
  end

  S.phase = "vendor_subscribe"
  local ok, err = ble.gattc_write(
    S.conn_handle,
    channel.cccd_handle,
    string.char(1, 0),
    ble.WRITE_WITH_RESPONSE
  )
  if not ok then
    set_error("vendor CCCD " .. tostring(channel.cccd_handle) ..
      ": " .. tostring(err))
  end
end

discover_next_vendor_descriptors = function()
  V.descriptor_index = V.descriptor_index + 1
  local channel = V.channels[V.descriptor_index]
  while channel and channel.descriptor_end < channel.value_handle + 1 do
    V.descriptor_index = V.descriptor_index + 1
    channel = V.channels[V.descriptor_index]
  end
  if not channel then
    V.subscribe_index = 0
    subscribe_next_vendor()
    return
  end

  S.phase = "vendor_descriptors"
  local ok, err = ble.gattc_discover_descriptors(
    S.conn_handle,
    channel.value_handle,
    channel.descriptor_end
  )
  if not ok then
    set_error("vendor descriptors: " .. tostring(err))
  end
end

start_vendor_discovery = function()
  if S.vendor_service_start == 0 or S.vendor_service_end == 0 then
    set_error("vendor service not found in complete discovery")
    return false
  end
  S.phase = "vendor_characteristics"
  local ok, err = ble.gattc_discover_characteristics(
    S.conn_handle, S.vendor_service_start, S.vendor_service_end
  )
  if not ok then
    set_error("vendor characteristics: " .. tostring(err))
    return false
  end
  return true
end

local function begin_hid_map_read()
  if S.report_map_handle == 0 then
    set_error("HID Report Map missing")
    return
  end
  S.phase = "hid_map_read"
  S.report_map_hex = ""
  S.report_map_len = 0
  local ok, err = ble.gattc_read(S.conn_handle, S.report_map_handle)
  if not ok then set_error("HID Report Map read: " .. tostring(err)) end
end

local function begin_hid_info_read()
  if S.hid_info_handle == 0 then
    begin_hid_map_read()
    return
  end
  S.phase = "hid_info_read"
  S.hid_info_hex = ""
  local ok, err = ble.gattc_read(S.conn_handle, S.hid_info_handle)
  if not ok then set_error("HID Information read: " .. tostring(err)) end
end

start_gap_discovery = function()
  S.phase = "gap_service"
  local ok, err = ble.gattc_discover_services(S.conn_handle, "1800")
  if not ok then
    set_error("GAP service: " .. tostring(err))
    return false
  end
  return true
end

local function begin_hid_reads()
  -- Android's HID Host does not write the HID Control Point as part of
  -- connection setup. Keep it untouched until a same-connection recovery is
  -- actually needed.
  if S.control_point_handle > 0 then
    add_event("control_point",
      "handle=" .. tostring(S.control_point_handle) .. " passive")
  end
  begin_hid_info_read()
end

set_protocol_then_subscribe = function()
  if not S.connected then return end
  if S.protocol_mode_handle > 0 then
    local ok, err = ble.gattc_write(
      S.conn_handle,
      S.protocol_mode_handle,
      string.char(1),
      ble.WRITE_NO_RESPONSE
    )
    if not ok then
      set_error("Protocol Mode: " .. tostring(err))
      return
    end
    S.protocol_mode_write_accepted = true
    add_event("protocol_mode",
      "handle=" .. tostring(S.protocol_mode_handle) .. " report last")
  end
  schedule(80, function()
    if not S.connected then return end
    H.disable_index = 0
    disable_next_hid_before_vendor()
  end)
end

disable_next_hid_before_vendor = function()
  H.disable_index = H.disable_index + 1
  local report = H.reports[H.disable_index]
  while report and
        (report.report_type ~= 1 or (tonumber(report.cccd_handle) or 0) == 0) do
    H.disable_index = H.disable_index + 1
    report = H.reports[H.disable_index]
  end
  if not report then
    add_event("vendor_first", "all HID CCCDs disabled")
    start_vendor_discovery()
    return
  end
  S.phase = "vendor_first_hid_disable"
  S.cccd_action = "vendor_first_disable"
  local ok, err = ble.gattc_write(
    S.conn_handle,
    report.cccd_handle,
    string.char(0, 0),
    ble.WRITE_WITH_RESPONSE
  )
  if not ok then
    set_error("vendor-first HID disable " ..
      tostring(report.cccd_handle) .. ": " .. tostring(err))
  end
end

begin_hid_subscription = function()
  if not S.connected or S.hid_subscription_started then return end
  S.hid_subscription_started = true
  H.subscribe_index = 0
  S.cccd_index = 0
  add_event("vendor_first", "enable HID after session replies")
  subscribe_next()
end

read_next_hid_reference = function()
  H.reference_index = H.reference_index + 1
  local report = H.reports[H.reference_index]
  while report and (tonumber(report.reference_handle) or 0) == 0 do
    H.reference_index = H.reference_index + 1
    report = H.reports[H.reference_index]
  end
  if not report then
    set_protocol_then_subscribe()
    return
  end
  S.phase = "hid_reference_read"
  local ok, err = ble.gattc_read(S.conn_handle, report.reference_handle)
  if not ok then set_error("HID Report Reference: " .. tostring(err)) end
end

discover_next_hid_descriptors = function()
  H.descriptor_index = H.descriptor_index + 1
  local report = H.reports[H.descriptor_index]
  while report and report.descriptor_end < report.value_handle + 1 do
    H.descriptor_index = H.descriptor_index + 1
    report = H.reports[H.descriptor_index]
  end
  if not report then
    begin_hid_reads()
    return
  end
  S.phase = "hid_descriptors"
  local ok, err = ble.gattc_discover_descriptors(
    S.conn_handle,
    report.value_handle,
    report.descriptor_end
  )
  if not ok then set_error("HID descriptors: " .. tostring(err)) end
end

start_hid_discovery = function()
  if S.hid_service_start == 0 or S.hid_service_end == 0 then
    set_error("HID service not found in complete discovery")
    return false
  end
  S.phase = "hid_characteristics"
  local ok, err = ble.gattc_discover_characteristics(
    S.conn_handle, S.hid_service_start, S.hid_service_end
  )
  if not ok then
    set_error("HID characteristics: " .. tostring(err))
    return false
  end
  return true
end

start_gatt_discovery = function()
  if S.gatt_service_start == 0 or S.gatt_service_end == 0 then
    add_event("gatt_service_skip", "not present")
    return start_hid_discovery()
  end
  S.phase = "gatt_characteristics"
  local ok, err = ble.gattc_discover_characteristics(
    S.conn_handle, S.gatt_service_start, S.gatt_service_end
  )
  if not ok then
    set_error("GATT characteristics: " .. tostring(err))
    return false
  end
  return true
end

start_all_service_discovery = function()
  S.phase = "all_services"
  S.service_count = 0
  S.services = {}
  local ok, err = ble.gattc_discover_services(S.conn_handle)
  if not ok then
    set_error("complete service discovery: " .. tostring(err))
    return false
  end
  return true
end

subscribe_next = function()
  H.subscribe_index = H.subscribe_index + 1
  local report = H.reports[H.subscribe_index]
  while report and
        (report.report_type ~= 1 or (tonumber(report.cccd_handle) or 0) == 0) do
    H.subscribe_index = H.subscribe_index + 1
    report = H.reports[H.subscribe_index]
  end
  if not report then
    S.cccd_action = ""
    S.subscription_cycle_count = S.subscription_cycle_count + 1
    complete_ready()
    return
  end
  write_cccd(report.cccd_handle, true)
end

local function on_ble(irq, data)
  S.event_count = S.event_count + 1

  if irq == ble.IRQ_SCAN_RESULT then
    if tostring(data.addr or "") ~= CFG.address then return end
    S.scan_count = S.scan_count + 1
    CFG.addr_type = tonumber(data.addr_type) or CFG.addr_type
    add_event("found", CFG.address .. " rssi=" .. tostring(data.rssi))
    pcall(ble.gap_scan)
    schedule(50, function() connect(CFG.addr_type) end)
    return
  end

  if irq == ble.IRQ_SCAN_DONE then
    if not S.connected and not S.connecting then
      S.phase = "wait_scan"
      schedule(500, start_scan)
    end
    return
  end

  if irq == ble.IRQ_PERIPHERAL_CONNECT then
    S.connected = true
    S.connecting = false
    S.encrypted = false
    S.authenticated = false
    S.bonded = false
    S.encryption_status = 0
    S.connected_ms = now_ms()
    S.conn_handle = tonumber(data.conn_handle) or -1
    S.connection_count = S.connection_count + 1
    S.notify_count = 0
    S.other_notify_count = 0
    S.report3_notify_count = 0
    S.report4_notify_count = 0
    S.negotiated_mtu = 0
    S.mtu_status = 0
    S.service_count = 0
    S.services = {}
    S.gatt_service_start = 0
    S.gatt_service_end = 0
    S.service_changed_handle = 0
    S.service_changed_cccd_handle = 0
    S.service_changed_subscribe_status = 0
    S.service_changed_recovery_count = 0
    S.service_changed_recovery_status = 0
    S.service_changed_recovery_ms = 0
    S.service_changed_recovery_connection_count = 0
    S.service_changed_recovered = false
    S.client_supported_features_handle = 0
    S.database_hash_handle = 0
    S.recovery_service_count = 0
    S.ready_ms = 0
    S.control_recovery_count = 0
    S.control_recovery_started_ms = 0
    S.control_recovery_connection_count = 0
    S.control_recovery_notify_baseline = 0
    S.control_recovery_suspend_accepted = false
    S.control_recovery_exit_accepted = false
    S.control_recovery_result = ""
    S.control_recovered = false
    S.gap_service_start = 0
    S.gap_service_end = 0
    S.ppcp_handle = 0
    S.ppcp_hex = ""
    S.ppcp_min_interval = 0
    S.ppcp_max_interval = 0
    S.ppcp_latency = 0
    S.ppcp_timeout = 0
    S.ppcp_read_status = 0
    S.hid_info_handle = 0
    S.hid_info_hex = ""
    S.hid_info_read_status = 0
    S.first_notify_ms = 0
    S.last_notify_ms = 0
    S.last_notify_gap_ms = 0
    S.max_notify_gap_ms = 0
    S.last_report_hex = ""
    S.last_report_len = 0
    S.buttons_text = "NONE"
    S.last_active_buttons_text = "NONE"
    S.last_active_ms = 0
    S.active_report_count = 0
    S.dpad = "CENTER"
    S.diagnostic_read_pending = false
    S.diagnostic_result = ""
    S.lx, S.ly, S.rx, S.ry = 128, 128, 128, 128
    S.lt, S.rt = 0, 0
    S.cccd_index = 0
    S.cccd_action = ""
    S.stall_rearm_count = 0
    S.stall_rearm_disable_status = 0
    S.stall_rearm_enable_status = 0
    S.stall_rearm_ms = 0
    S.hid_service_start = 0
    S.hid_service_end = 0
    S.report_map_handle = 0
    S.report_map_hex = ""
    S.report_map_len = 0
    S.external_report_reference_handle = 0
    S.external_report_reference_hex = ""
    S.external_report_reference_status = 0
    S.protocol_mode_handle = 0
    S.protocol_mode_write_accepted = false
    S.report_count = 0
    S.input_report_count = 0
    S.control_point_handle = 0
    S.control_point_write_count = 0
    S.control_point_write_status = 0
    S.control_point_write_error_count = 0
    S.control_point_last_write_ms = 0
    S.vendor_service_start = 0
    S.vendor_service_end = 0
    S.vendor_input_handle = 0
    S.vendor_write_handle = 0
    S.vendor_reply_handle = 0
    S.vendor_aux_handle = 0
    S.vendor_subscribed_count = 0
    S.vendor_notify_count = 0
    S.vendor_write_count = 0
    S.vendor_heartbeat_count = 0
    S.vendor_heartbeat_reply_count = 0
    S.vendor_watchdog_reply_count = 0
    S.vendor_last_notify_ms = 0
    S.vendor_stage = 0
    S.vendor_handshake_sent = false
    S.vendor_seen_battery_reply = false
    S.vendor_seen_info_reply = false
    S.vendor_session_complete = false
    S.vendor_session_start_count = 0
    S.vendor_session_retry_count = 0
    S.official_sync_started = false
    S.official_sync_cycle_count = 0
    S.official_sync_write_count = 0
    S.official_sync_last_ms = 0
    S.official_sync_read_pending = false
    S.official_sync_read_count = 0
    S.official_sync_read_status = 0
    S.official_sync_read_hex = ""
    S.vendor_first_disable_count = 0
    S.vendor_first_disable_status = 0
    S.hid_subscription_started = false
    S.vendor_last_hex = ""
    S.vendor_read_hex = ""
    V.characteristics = {}
    V.channels = {}
    V.descriptor_index = 0
    V.subscribe_index = 0
    G.characteristics = {}
    G.service_changed = nil
    H.characteristics = {}
    H.report_map = nil
    H.reports = {}
    H.descriptor_index = 0
    H.reference_index = 0
    H.disable_index = 0
    H.subscribe_index = 0
    S.phase = "pairing"
    add_event("connected", "handle=" .. tostring(S.conn_handle))
    local ok, err = ble.gap_pair(S.conn_handle, true)
    if not ok then set_error("pair: " .. tostring(err)) end
    return
  end

  if irq == ble.IRQ_ENCRYPTION_UPDATE then
    S.encrypted = data.encrypted == true
    S.authenticated = data.authenticated == true
    S.bonded = data.bonded == true
    S.encryption_status = tonumber(data.status) or -1
    add_event("encryption",
      "status=" .. tostring(data.status) ..
      " encrypted=" .. tostring(S.encrypted) ..
      " bonded=" .. tostring(data.bonded))
    if tonumber(data.status) == 0 and S.encrypted then
      start_all_service_discovery()
    else
      set_error("encryption failed: " .. tostring(data.status))
    end
    return
  end

  if irq == ble.IRQ_GATTC_SERVICE_RESULT and
     S.phase == "diagnostic_rediscover_services" then
    S.recovery_service_count = S.recovery_service_count + 1
    return
  end

  if irq == ble.IRQ_GATTC_SERVICE_DONE and
     S.phase == "diagnostic_rediscover_services" then
    local status = tonumber(data.status) or -1
    S.phase = "ready"
    if status == 0 then
      S.diagnostic_result = "waiting_for_notify"
    else
      S.diagnostic_result = "failed: " .. tostring(status)
    end
    add_event("diagnostic_rediscover_done",
      "status=" .. tostring(status) ..
      " services=" .. tostring(S.recovery_service_count))
    return
  end

  if irq == ble.IRQ_GATTC_SERVICE_RESULT and S.phase == "all_services" then
    local service = {
      uuid = tostring(data.uuid or ""),
      start_handle = tonumber(data.start_handle) or 0,
      end_handle = tonumber(data.end_handle) or 0,
    }
    S.services[#S.services + 1] = service
    S.service_count = #S.services
    if is_uuid16(service.uuid, "1800") then
      S.gap_service_start = service.start_handle
      S.gap_service_end = service.end_handle
    elseif is_uuid16(service.uuid, "1801") then
      S.gatt_service_start = service.start_handle
      S.gatt_service_end = service.end_handle
    elseif is_uuid16(service.uuid, "1812") then
      S.hid_service_start = service.start_handle
      S.hid_service_end = service.end_handle
    elseif is_uuid16(service.uuid, "7310") then
      S.vendor_service_start = service.start_handle
      S.vendor_service_end = service.end_handle
    end
    add_event("service",
      service.uuid .. "=" .. tostring(service.start_handle) ..
      "-" .. tostring(service.end_handle))
    return
  end

  if irq == ble.IRQ_GATTC_SERVICE_DONE and S.phase == "all_services" then
    if tonumber(data.status) ~= 0 then
      set_error("complete service discovery done: " .. tostring(data.status))
      return
    end
    add_event("all_services_done", tostring(S.service_count))
    if S.hid_service_start == 0 then
      set_error("HID service missing after complete discovery")
      return
    end
    start_gatt_discovery()
    return
  end

  if irq == ble.IRQ_GATTC_CHARACTERISTIC_RESULT and
     S.phase == "gatt_characteristics" then
    local characteristic = {
      def_handle = tonumber(data.def_handle) or 0,
      value_handle = tonumber(data.value_handle) or 0,
      descriptor_end = S.gatt_service_end,
      properties = tonumber(data.properties) or 0,
      uuid = tostring(data.uuid or ""),
    }
    local previous = G.characteristics[#G.characteristics]
    if previous and characteristic.def_handle > 0 then
      previous.descriptor_end = characteristic.def_handle - 1
    end
    G.characteristics[#G.characteristics + 1] = characteristic
    if is_uuid16(characteristic.uuid, "2a05") then
      G.service_changed = characteristic
      S.service_changed_handle = characteristic.value_handle
    elseif is_uuid16(characteristic.uuid, "2b29") then
      S.client_supported_features_handle = characteristic.value_handle
    elseif is_uuid16(characteristic.uuid, "2b2a") then
      S.database_hash_handle = characteristic.value_handle
    end
    add_event("gatt_char",
      characteristic.uuid .. "=" .. tostring(characteristic.value_handle) ..
      " p=" .. tostring(characteristic.properties))
    return
  end

  if irq == ble.IRQ_GATTC_CHARACTERISTIC_DONE and
     S.phase == "gatt_characteristics" then
    if tonumber(data.status) ~= 0 then
      set_error("GATT characteristic done: " .. tostring(data.status))
      return
    end
    local characteristic = G.service_changed
    if not characteristic or
       characteristic.descriptor_end < characteristic.value_handle + 1 then
      add_event("service_changed_skip", "characteristic/descriptor missing")
      start_hid_discovery()
      return
    end
    S.phase = "gatt_service_changed_descriptors"
    local ok, err = ble.gattc_discover_descriptors(
      S.conn_handle,
      characteristic.value_handle,
      characteristic.descriptor_end
    )
    if not ok then
      set_error("Service Changed descriptors: " .. tostring(err))
    end
    return
  end

  if irq == ble.IRQ_GATTC_DESCRIPTOR_RESULT and
     S.phase == "gatt_service_changed_descriptors" then
    if is_uuid16(data.uuid, "2902") then
      S.service_changed_cccd_handle = tonumber(
        data.descriptor_handle or data.value_handle
      ) or 0
      add_event("service_changed_cccd",
        tostring(S.service_changed_cccd_handle))
    end
    return
  end

  if irq == ble.IRQ_GATTC_DESCRIPTOR_DONE and
     S.phase == "gatt_service_changed_descriptors" then
    if tonumber(data.status) ~= 0 or S.service_changed_cccd_handle == 0 then
      set_error("Service Changed descriptor done: " .. tostring(data.status))
      return
    end
    S.phase = "gatt_service_changed_subscribe"
    local ok, err = ble.gattc_write(
      S.conn_handle,
      S.service_changed_cccd_handle,
      string.char(2, 0),
      ble.WRITE_WITH_RESPONSE
    )
    if not ok then
      set_error("Service Changed subscribe: " .. tostring(err))
    end
    return
  end

  if irq == ble.IRQ_GATTC_SERVICE_RESULT and S.phase == "gap_service" then
    S.gap_service_start = tonumber(data.start_handle) or 0
    S.gap_service_end = tonumber(data.end_handle) or 0
    add_event("gap_service",
      tostring(S.gap_service_start) .. "-" .. tostring(S.gap_service_end))
    return
  end

  if irq == ble.IRQ_GATTC_SERVICE_DONE and S.phase == "gap_service" then
    if tonumber(data.status) ~= 0 or S.gap_service_start == 0 then
      add_event("gap_service_skip", "status=" .. tostring(data.status))
      start_hid_discovery()
      return
    end
    S.phase = "gap_characteristics"
    local ok, err = ble.gattc_discover_characteristics(
      S.conn_handle, S.gap_service_start, S.gap_service_end
    )
    if not ok then set_error("GAP characteristics: " .. tostring(err)) end
    return
  end

  if irq == ble.IRQ_GATTC_CHARACTERISTIC_RESULT and
     S.phase == "gap_characteristics" then
    if is_uuid16(data.uuid, "2a04") then
      S.ppcp_handle = tonumber(data.value_handle) or 0
      add_event("ppcp_handle", tostring(S.ppcp_handle))
    end
    return
  end

  if irq == ble.IRQ_GATTC_CHARACTERISTIC_DONE and
     S.phase == "gap_characteristics" then
    if tonumber(data.status) ~= 0 then
      set_error("GAP characteristic done: " .. tostring(data.status))
      return
    end
    if S.ppcp_handle == 0 then
      add_event("ppcp_skip", "not present")
      start_hid_discovery()
      return
    end
    S.phase = "ppcp_read"
    local ok, err = ble.gattc_read(S.conn_handle, S.ppcp_handle)
    if not ok then set_error("PPCP read: " .. tostring(err)) end
    return
  end

  if irq == ble.IRQ_MTU_EXCHANGED then
    S.negotiated_mtu = tonumber(data.mtu) or 0
    S.mtu_status = tonumber(data.status) or 0
    add_event("mtu",
      tostring(S.negotiated_mtu) .. " status=" .. tostring(S.mtu_status))
    return
  end

  if irq == ble.IRQ_GATTC_SERVICE_RESULT and S.phase == "hid_service" then
    S.hid_service_start = tonumber(data.start_handle) or 0
    S.hid_service_end = tonumber(data.end_handle) or 0
    add_event("hid_service",
      tostring(S.hid_service_start) .. "-" .. tostring(S.hid_service_end))
    return
  end

  if irq == ble.IRQ_GATTC_SERVICE_DONE and S.phase == "hid_service" then
    if tonumber(data.status) ~= 0 or S.hid_service_start == 0 then
      set_error("HID service not found: " .. tostring(data.status))
      return
    end
    S.phase = "hid_characteristics"
    local ok, err = ble.gattc_discover_characteristics(
      S.conn_handle, S.hid_service_start, S.hid_service_end
    )
    if not ok then set_error("HID characteristics: " .. tostring(err)) end
    return
  end

  if irq == ble.IRQ_GATTC_CHARACTERISTIC_RESULT and
     S.phase == "hid_characteristics" then
    local characteristic = {
      def_handle = tonumber(data.def_handle) or 0,
      value_handle = tonumber(data.value_handle) or 0,
      descriptor_end = S.hid_service_end,
      properties = tonumber(data.properties) or 0,
      uuid = tostring(data.uuid or ""),
      cccd_handle = 0,
      reference_handle = 0,
      report_id = 0,
      report_type = 0,
    }
    local previous = H.characteristics[#H.characteristics]
    if previous and characteristic.def_handle > 0 then
      previous.descriptor_end = characteristic.def_handle - 1
    end
    H.characteristics[#H.characteristics + 1] = characteristic
    if is_uuid16(characteristic.uuid, "2a4b") then
      S.report_map_handle = characteristic.value_handle
      H.report_map = characteristic
    elseif is_uuid16(characteristic.uuid, "2a4a") then
      S.hid_info_handle = characteristic.value_handle
    elseif is_uuid16(characteristic.uuid, "2a4c") then
      S.control_point_handle = characteristic.value_handle
    elseif is_uuid16(characteristic.uuid, "2a4e") then
      S.protocol_mode_handle = characteristic.value_handle
    elseif is_uuid16(characteristic.uuid, "2a4d") then
      H.reports[#H.reports + 1] = characteristic
      S.report_count = #H.reports
    end
    add_event("hid_char",
      characteristic.uuid .. "=" .. tostring(characteristic.value_handle) ..
      " p=" .. tostring(characteristic.properties))
    return
  end

  if irq == ble.IRQ_GATTC_CHARACTERISTIC_DONE and
     S.phase == "hid_characteristics" then
    if tonumber(data.status) ~= 0 or S.report_map_handle == 0 or
       #H.reports == 0 then
      set_error("HID characteristics incomplete: " .. tostring(data.status))
      return
    end
    if H.report_map and
       H.report_map.descriptor_end >= H.report_map.value_handle + 1 then
      S.phase = "hid_map_descriptors"
      local ok, err = ble.gattc_discover_descriptors(
        S.conn_handle,
        H.report_map.value_handle,
        H.report_map.descriptor_end
      )
      if not ok then
        set_error("HID Report Map descriptors: " .. tostring(err))
      end
    else
      H.descriptor_index = 0
      schedule(50, discover_next_hid_descriptors)
    end
    return
  end

  if irq == ble.IRQ_GATTC_DESCRIPTOR_RESULT and
     S.phase == "hid_map_descriptors" then
    if is_uuid16(data.uuid, "2907") then
      S.external_report_reference_handle =
        tonumber(data.descriptor_handle or data.value_handle) or 0
    end
    add_event("hid_map_desc",
      tostring(data.uuid) .. "=" ..
      tostring(data.descriptor_handle or data.value_handle))
    return
  end

  if irq == ble.IRQ_GATTC_DESCRIPTOR_DONE and
     S.phase == "hid_map_descriptors" then
    if tonumber(data.status) ~= 0 then
      set_error("HID Report Map descriptor done: " .. tostring(data.status))
      return
    end
    H.descriptor_index = 0
    schedule(40, discover_next_hid_descriptors)
    return
  end

  if irq == ble.IRQ_GATTC_DESCRIPTOR_RESULT and
     S.phase == "hid_descriptors" then
    local report = H.reports[H.descriptor_index]
    if report and is_uuid16(data.uuid, "2902") then
      report.cccd_handle = tonumber(data.descriptor_handle or
        data.value_handle) or 0
    elseif report and is_uuid16(data.uuid, "2908") then
      report.reference_handle = tonumber(data.descriptor_handle or
        data.value_handle) or 0
    end
    if report then
      add_event("hid_desc",
        tostring(report.value_handle) .. " " .. tostring(data.uuid) ..
        "=" .. tostring(data.descriptor_handle or data.value_handle))
    end
    return
  end

  if irq == ble.IRQ_GATTC_DESCRIPTOR_DONE and
     S.phase == "hid_descriptors" then
    if tonumber(data.status) ~= 0 then
      set_error("HID descriptor done: " .. tostring(data.status))
      return
    end
    schedule(40, discover_next_hid_descriptors)
    return
  end

  if irq == ble.IRQ_GATTC_SERVICE_RESULT and S.phase == "vendor_service" then
    S.vendor_service_start = tonumber(data.start_handle) or 0
    S.vendor_service_end = tonumber(data.end_handle) or 0
    add_event("vendor_service",
      tostring(S.vendor_service_start) .. "-" .. tostring(S.vendor_service_end))
    return
  end

  if irq == ble.IRQ_GATTC_SERVICE_DONE and S.phase == "vendor_service" then
    if tonumber(data.status) ~= 0 or S.vendor_service_start == 0 then
      set_error("vendor service not found: " .. tostring(data.status))
      return
    end
    S.phase = "vendor_characteristics"
    local ok, err = ble.gattc_discover_characteristics(
      S.conn_handle, S.vendor_service_start, S.vendor_service_end
    )
    if not ok then set_error("vendor characteristics: " .. tostring(err)) end
    return
  end

  if irq == ble.IRQ_GATTC_CHARACTERISTIC_RESULT and
     S.phase == "vendor_characteristics" then
    local characteristic = {
      def_handle = tonumber(data.def_handle) or 0,
      value_handle = tonumber(data.value_handle) or 0,
      descriptor_end = S.vendor_service_end,
      uuid = tostring(data.uuid or ""),
    }
    local previous = V.characteristics[#V.characteristics]
    if previous and characteristic.def_handle > 0 then
      previous.descriptor_end = characteristic.def_handle - 1
    end
    V.characteristics[#V.characteristics + 1] = characteristic
    if is_uuid16(characteristic.uuid, "7311") then
      S.vendor_input_handle = characteristic.value_handle
      V.channels[#V.channels + 1] = characteristic
    elseif is_uuid16(characteristic.uuid, "7312") then
      S.vendor_write_handle = characteristic.value_handle
    elseif is_uuid16(characteristic.uuid, "7313") then
      S.vendor_reply_handle = characteristic.value_handle
      V.channels[#V.channels + 1] = characteristic
    elseif is_uuid16(characteristic.uuid, "7320") then
      S.vendor_aux_handle = characteristic.value_handle
    end
    add_event("vendor_char",
      characteristic.uuid .. "=" .. tostring(characteristic.value_handle))
    return
  end

  if irq == ble.IRQ_GATTC_CHARACTERISTIC_DONE and
     S.phase == "vendor_characteristics" then
    if tonumber(data.status) ~= 0 or S.vendor_write_handle == 0 then
      set_error("vendor characteristics incomplete: " .. tostring(data.status))
      return
    end
    V.descriptor_index = 0
    schedule_vendor(50, discover_next_vendor_descriptors)
    return
  end

  if irq == ble.IRQ_GATTC_DESCRIPTOR_RESULT and
     S.phase == "vendor_descriptors" then
    local channel = V.channels[V.descriptor_index]
    if channel and is_uuid16(data.uuid, "2902") then
      channel.cccd_handle = tonumber(data.descriptor_handle or
        data.value_handle) or 0
      add_event("vendor_cccd",
        channel.uuid .. "=" .. tostring(channel.cccd_handle))
    end
    return
  end

  if irq == ble.IRQ_GATTC_DESCRIPTOR_DONE and
     S.phase == "vendor_descriptors" then
    if tonumber(data.status) ~= 0 then
      set_error("vendor descriptor done: " .. tostring(data.status))
      return
    end
    schedule_vendor(50, discover_next_vendor_descriptors)
    return
  end

  if irq == ble.IRQ_GATTC_WRITE_DONE then
    local status = tonumber(data.status) or -1
    if S.phase == "diagnostic_client_features" then
      S.phase = "ready"
      if status == 0 then
        S.diagnostic_result = "waiting_for_notify"
      else
        S.diagnostic_result = "failed: " .. tostring(status)
      end
      add_event("diagnostic_client_features_done",
        "status=" .. tostring(status))
      return
    elseif S.phase == "gatt_service_changed_subscribe" then
      S.service_changed_subscribe_status = status
      add_event("service_changed_subscribe", "status=" .. tostring(status))
      if status ~= 0 then
        set_error("Service Changed subscribe failed: " .. tostring(status))
        return
      end
      start_hid_discovery()
      return
    elseif S.phase == "service_changed_recovery" then
      S.service_changed_recovery_status = status
      S.service_changed_recovery_ms = now_ms()
      add_event("service_changed_recovery",
        "status=" .. tostring(status) ..
        " conn_count=" .. tostring(S.connection_count))
      if status ~= 0 then
        set_error("Service Changed recovery failed: " .. tostring(status))
        return
      end
      S.phase = "ready"
      return
    elseif S.phase == "stall_rearm_disable" then
      S.stall_rearm_disable_status = status
      add_event("stall_rearm", "disable status=" .. tostring(status))
      if status ~= 0 then
        set_error("stall rearm disable failed: " .. tostring(status))
        return
      end
      S.phase = "stall_rearm_enable"
      local ok, err = ble.gattc_write(
        S.conn_handle,
        CFG.cccd_handles[1],
        string.char(1, 0),
        ble.WRITE_WITH_RESPONSE
      )
      if not ok then set_error("stall rearm enable: " .. tostring(err)) end
      return
    elseif S.phase == "stall_rearm_enable" then
      S.stall_rearm_enable_status = status
      S.stall_rearm_count = S.stall_rearm_count + 1
      S.stall_rearm_ms = now_ms()
      add_event("stall_rearm", "enable status=" .. tostring(status))
      if status ~= 0 then
        set_error("stall rearm enable failed: " .. tostring(status))
        return
      end
      S.phase = "ready"
      return
    elseif S.phase == "vendor_first_hid_disable" then
      S.vendor_first_disable_status = status
      add_event("vendor_first_disable",
        "handle=" .. tostring(data.value_handle) ..
        " status=" .. tostring(status))
      if status ~= 0 then
        set_error("vendor-first HID disable failed: " .. tostring(status))
        return
      end
      S.vendor_first_disable_count = S.vendor_first_disable_count + 1
      disable_next_hid_before_vendor()
      return
    end
    if S.phase == "vendor_subscribe" then
      local channel = V.channels[V.subscribe_index]
      add_event("vendor_subscribe",
        tostring(channel and channel.cccd_handle or data.value_handle) ..
        " status=" .. tostring(status))
      if status ~= 0 then
        set_error("vendor CCCD failed: " .. tostring(status))
        return
      end
      S.vendor_subscribed_count = S.vendor_subscribed_count + 1
      subscribe_next_vendor()
      return
    end
    if S.cccd_action == "disable" then
      S.cccd_disable_status = status
    else
      S.cccd_write_status = status
    end
    add_event("write",
      "handle=" .. tostring(data.value_handle) ..
      " action=" .. tostring(S.cccd_action) ..
      " status=" .. tostring(status))
    if S.phase == "reset_cccd" and S.cccd_action == "disable" and status == 0 then
      write_cccd(CFG.cccd_handles[S.cccd_index], true)
    elseif S.phase == "subscribe" and S.cccd_action == "enable" and status == 0 then
      subscribe_next()
    elseif status ~= 0 then
      set_error("CCCD " .. tostring(S.cccd_action) ..
        " failed: " .. tostring(status))
    end
    return
  end

  if irq == ble.IRQ_GATTC_READ_RESULT and S.phase == "vendor_read" then
    S.vendor_read_hex = hex(data.data or "")
    add_event("vendor_read", S.vendor_read_hex)
    return
  end

  if irq == ble.IRQ_GATTC_READ_RESULT and
     S.official_sync_read_pending then
    S.official_sync_read_hex =
      S.official_sync_read_hex .. hex(data.data or "")
    return
  end

  if irq == ble.IRQ_GATTC_READ_RESULT and S.phase == "ppcp_read" then
    local payload = data.data or ""
    S.ppcp_hex = hex(payload)
    if #payload >= 8 then
      local function le16(offset)
        return (string.byte(payload, offset) or 0) +
          (string.byte(payload, offset + 1) or 0) * 256
      end
      S.ppcp_min_interval = le16(1)
      S.ppcp_max_interval = le16(3)
      S.ppcp_latency = le16(5)
      S.ppcp_timeout = le16(7)
    end
    add_event("ppcp", S.ppcp_hex)
    return
  end

  if irq == ble.IRQ_GATTC_READ_RESULT and S.phase == "hid_info_read" then
    S.hid_info_hex = S.hid_info_hex .. hex(data.data or "")
    add_event("hid_info", S.hid_info_hex)
    return
  end

  if irq == ble.IRQ_GATTC_READ_DONE and S.phase == "hid_info_read" then
    S.hid_info_read_status = tonumber(data.status) or -1
    add_event("hid_info_done",
      "status=" .. tostring(S.hid_info_read_status))
    if S.hid_info_read_status ~= 0 then
      set_error("HID Information done: " ..
        tostring(S.hid_info_read_status))
      return
    end
    begin_hid_map_read()
    return
  end

  if irq == ble.IRQ_GATTC_READ_DONE and S.phase == "ppcp_read" then
    S.ppcp_read_status = tonumber(data.status) or -1
    add_event("ppcp_done", "status=" .. tostring(S.ppcp_read_status))
    if S.ppcp_read_status ~= 0 then
      set_error("PPCP read done: " .. tostring(S.ppcp_read_status))
      return
    end
    start_hid_discovery()
    return
  end

  if irq == ble.IRQ_GATTC_READ_RESULT and S.diagnostic_read_pending then
    S.diagnostic_read_hex = S.diagnostic_read_hex .. hex(data.data or "")
    add_event("diagnostic_read",
      "data=" .. hex(data.data or ""))
    return
  end

  if irq == ble.IRQ_GATTC_READ_RESULT and S.phase == "hid_map_read" then
    local payload = data.data or ""
    S.report_map_len = S.report_map_len + #payload
    S.report_map_hex = S.report_map_hex .. hex(payload)
    return
  end

  if irq == ble.IRQ_GATTC_READ_DONE and S.phase == "hid_map_read" then
    if tonumber(data.status) ~= 0 or S.report_map_len == 0 then
      set_error("HID Report Map done: " .. tostring(data.status))
      return
    end
    add_event("report_map", tostring(S.report_map_len) .. " bytes")
    if S.external_report_reference_handle > 0 then
      S.phase = "hid_external_reference_read"
      S.external_report_reference_hex = ""
      local ok, err = ble.gattc_read(
        S.conn_handle, S.external_report_reference_handle
      )
      if not ok then
        set_error("HID External Report Reference: " .. tostring(err))
      end
    else
      H.reference_index = 0
      read_next_hid_reference()
    end
    return
  end

  if irq == ble.IRQ_GATTC_READ_RESULT and
     S.phase == "hid_external_reference_read" then
    S.external_report_reference_hex =
      S.external_report_reference_hex .. hex(data.data or "")
    add_event("external_report_ref", S.external_report_reference_hex)
    return
  end

  if irq == ble.IRQ_GATTC_READ_DONE and
     S.phase == "hid_external_reference_read" then
    S.external_report_reference_status = tonumber(data.status) or -1
    add_event("external_report_ref_done",
      "status=" .. tostring(S.external_report_reference_status))
    -- 0x2907 is optional. AOSP only logs a read failure and continues
    -- configuring the HID service, so this probe must do the same.
    H.reference_index = 0
    read_next_hid_reference()
    return
  end

  if irq == ble.IRQ_GATTC_READ_RESULT and
     S.phase == "hid_reference_read" then
    local report = H.reports[H.reference_index]
    local payload = data.data or ""
    if report and #payload >= 2 then
      report.report_id = string.byte(payload, 1) or 0
      report.report_type = string.byte(payload, 2) or 0
      if report.report_type == 1 then
        S.input_report_count = S.input_report_count + 1
      end
      add_event("report_ref",
        tostring(report.value_handle) .. " id=" ..
        tostring(report.report_id) .. " type=" ..
        tostring(report.report_type))
    end
    return
  end

  if irq == ble.IRQ_GATTC_READ_DONE and
     S.phase == "hid_reference_read" then
    if tonumber(data.status) ~= 0 then
      set_error("HID Report Reference done: " .. tostring(data.status))
      return
    end
    read_next_hid_reference()
    return
  end

  if irq == ble.IRQ_GATTC_READ_DONE and S.phase == "vendor_read" then
    add_event("vendor_read_done", "status=" .. tostring(data.status))
    start_vendor_session()
    return
  end

  if irq == ble.IRQ_GATTC_READ_DONE and
     S.official_sync_read_pending then
    S.official_sync_read_pending = false
    S.official_sync_read_status = tonumber(data.status) or -1
    S.official_sync_read_count = S.official_sync_read_count + 1
    if S.official_sync_read_count <= 3 or
       S.official_sync_read_count % 20 == 0 then
      add_event("official_sync_read",
        "status=" .. tostring(S.official_sync_read_status) ..
        " data=" .. S.official_sync_read_hex)
    end
    return
  end

  if irq == ble.IRQ_GATTC_READ_DONE and S.diagnostic_read_pending then
    S.diagnostic_read_pending = false
    S.diagnostic_read_status = tonumber(data.status) or -1
    if S.diagnostic_read_status == 0 then
      S.diagnostic_result = "read_ok"
    else
      S.diagnostic_result =
        "read_failed: " .. tostring(S.diagnostic_read_status)
    end
    add_event("diagnostic_read_done",
      "status=" .. tostring(S.diagnostic_read_status))
    return
  end

  if irq == ble.IRQ_GATTC_NOTIFY then
    local at = now_ms()
    local handle = tonumber(data.value_handle) or 0
    if handle == CFG.report_handle then
      S.report3_notify_count = S.report3_notify_count + 1
      if S.last_notify_ms > 0 then
        local gap = elapsed_ms(at, S.last_notify_ms)
        S.last_notify_gap_ms = gap
        if gap > S.max_notify_gap_ms then S.max_notify_gap_ms = gap end
      end
      S.notify_count = S.notify_count + 1
      if S.first_notify_ms == 0 then
        S.first_notify_ms = at
        add_event("first_notify", "len=" .. tostring(#(data.data or "")))
      end
      S.last_notify_ms = at
      if S.diagnostic_action == "watchdog_cycle" and
         S.notify_count > S.diagnostic_notify_baseline then
        S.diagnostic_result = "notify_resumed"
      end
      if (S.diagnostic_action == "protocol_cycle" or
          S.diagnostic_action == "client_features" or
          S.diagnostic_action == "rediscover_services" or
          S.diagnostic_action == "vendor_commit" or
          S.diagnostic_action == "cccd_command_cycle") and
         S.notify_count > S.diagnostic_notify_baseline and
         S.diagnostic_result == "waiting_for_notify" then
        S.diagnostic_result = "notify_resumed_same_connection"
        add_event("diagnostic_recovered",
          S.diagnostic_action .. " conn_count=" ..
          tostring(S.connection_count))
      end
      if S.service_changed_recovery_count > 0 and
         not S.service_changed_recovered and
         S.connection_count == S.service_changed_recovery_connection_count then
        S.service_changed_recovered = true
        add_event("recovered",
          "service_changed same_connection notify=" ..
          tostring(S.notify_count))
      end
      if S.control_recovery_count > 0 and
         not S.control_recovered and
         S.notify_count > S.control_recovery_notify_baseline and
         S.connection_count == S.control_recovery_connection_count then
        S.control_recovered = true
        S.control_recovery_result = "notify_resumed_same_connection"
        add_event("recovered",
          "control_cycle same_connection notify=" ..
          tostring(S.notify_count))
      end
      S.last_report_len = #(data.data or "")
      S.last_report_hex = hex(data.data or "")
      decode_report(data.data or "")
      if S.notify_count % 500 == 0 then
        add_event("notify", tostring(S.notify_count))
      end
    elseif handle == 30 then
      S.report4_notify_count = S.report4_notify_count + 1
      S.other_notify_count = S.other_notify_count + 1
    elseif handle == S.vendor_input_handle or
           handle == S.vendor_reply_handle then
      local payload = data.data or ""
      S.vendor_notify_count = S.vendor_notify_count + 1
      S.vendor_last_notify_ms = at
      S.vendor_last_hex = hex(payload)
      add_event("vendor_notify",
        tostring(handle) .. "=" .. S.vendor_last_hex)
      if #payload >= 4 and
         string.byte(payload, 1) == 0x11 and
         string.byte(payload, 2) == 0x57 and
         string.byte(payload, 3) == 0x44 and
         string.byte(payload, 4) == 0x54 then
        S.vendor_watchdog_reply_count =
          S.vendor_watchdog_reply_count + 1
      elseif #payload > 0 and string.byte(payload, 1) == 0x15 then
        S.vendor_seen_battery_reply = true
        add_event("vendor_session", "battery_reply")
      elseif #payload > 0 and string.byte(payload, 1) == 0x55 then
        S.vendor_seen_info_reply = true
        add_event("vendor_session", "info_reply")
      end
      finish_vendor_session()
    else
      S.other_notify_count = S.other_notify_count + 1
    end
    return
  end

  if irq == ble.IRQ_PERIPHERAL_DISCONNECT then
    S.connected = false
    S.connecting = false
    S.encrypted = false
    S.conn_handle = -1
    S.diagnostic_read_pending = false
    if S.diagnostic_result == "waiting_for_notify" then
      S.diagnostic_result = "disconnected"
    end
    if vendor_timer then pcall(function() vendor_timer:stop() end) end
    S.official_sync_started = false
    if official_sync_timer then
      pcall(function() official_sync_timer:stop() end)
    end
    if official_sync_read_timer then
      pcall(function() official_sync_read_timer:stop() end)
    end
    S.disconnect_count = S.disconnect_count + 1
    S.disconnect_reason = tonumber(data.reason or data.status) or 0
    S.phase = "wait_scan"
    add_event("disconnected", "reason=" .. tostring(S.disconnect_reason))
    schedule(1000, start_scan)
  end
end

local function snapshot()
  local at = now_ms()
  local since_notify = 0
  local reports = {}
  if S.last_notify_ms > 0 then since_notify = elapsed_ms(at, S.last_notify_ms) end
  for index, report in ipairs(H.reports) do
    reports[index] = {
      value_handle = report.value_handle,
      properties = report.properties,
      cccd_handle = report.cccd_handle,
      reference_handle = report.reference_handle,
      report_id = report.report_id,
      report_type = report.report_type,
    }
  end
  return {
    ok = true,
    version = S.version,
    mode = S.mode,
    clock_ms = at,
    phase = S.phase,
    connected = S.connected,
    connecting = S.connecting,
    encrypted = S.encrypted,
    authenticated = S.authenticated,
    bonded = S.bonded,
    encryption_status = S.encryption_status,
    connected_ms = S.connected_ms,
    conn_handle = S.conn_handle,
    address = S.address,
    scan_count = S.scan_count,
    connection_count = S.connection_count,
    disconnect_count = S.disconnect_count,
    disconnect_reason = S.disconnect_reason,
    notify_count = S.notify_count,
    other_notify_count = S.other_notify_count,
    report3_notify_count = S.report3_notify_count,
    report4_notify_count = S.report4_notify_count,
    preferred_mtu = CFG.preferred_mtu,
    negotiated_mtu = S.negotiated_mtu,
    mtu_status = S.mtu_status,
    service_count = S.service_count,
    services = S.services,
    gatt_service_start = S.gatt_service_start,
    gatt_service_end = S.gatt_service_end,
    service_changed_handle = S.service_changed_handle,
    service_changed_cccd_handle = S.service_changed_cccd_handle,
    service_changed_subscribe_status = S.service_changed_subscribe_status,
    service_changed_recovery_count = S.service_changed_recovery_count,
    service_changed_recovery_status = S.service_changed_recovery_status,
    service_changed_recovery_ms = S.service_changed_recovery_ms,
    service_changed_recovery_connection_count =
      S.service_changed_recovery_connection_count,
    service_changed_recovered = S.service_changed_recovered,
    client_supported_features_handle = S.client_supported_features_handle,
    database_hash_handle = S.database_hash_handle,
    recovery_service_count = S.recovery_service_count,
    ready_ms = S.ready_ms,
    control_recovery_count = S.control_recovery_count,
    control_recovery_started_ms = S.control_recovery_started_ms,
    control_recovery_connection_count = S.control_recovery_connection_count,
    control_recovery_notify_baseline = S.control_recovery_notify_baseline,
    control_recovery_suspend_accepted = S.control_recovery_suspend_accepted,
    control_recovery_exit_accepted = S.control_recovery_exit_accepted,
    control_recovery_result = S.control_recovery_result,
    control_recovered = S.control_recovered,
    gap_service_start = S.gap_service_start,
    gap_service_end = S.gap_service_end,
    ppcp_handle = S.ppcp_handle,
    ppcp_hex = S.ppcp_hex,
    ppcp_min_interval = S.ppcp_min_interval,
    ppcp_max_interval = S.ppcp_max_interval,
    ppcp_latency = S.ppcp_latency,
    ppcp_timeout = S.ppcp_timeout,
    ppcp_read_status = S.ppcp_read_status,
    hid_info_handle = S.hid_info_handle,
    hid_info_hex = S.hid_info_hex,
    hid_info_read_status = S.hid_info_read_status,
    first_notify_ms = S.first_notify_ms,
    last_notify_ms = S.last_notify_ms,
    since_last_notify_ms = since_notify,
    max_notify_gap_ms = S.max_notify_gap_ms,
    stalled = S.connected and S.notify_count > 0 and since_notify > 2000,
    last_report_len = S.last_report_len,
    last_report_hex = S.last_report_hex,
    buttons_text = S.buttons_text,
    last_active_buttons_text = S.last_active_buttons_text,
    last_active_ms = S.last_active_ms,
    active_report_count = S.active_report_count,
    dpad = S.dpad,
    lx = S.lx,
    ly = S.ly,
    rx = S.rx,
    ry = S.ry,
    lt = S.lt,
    rt = S.rt,
    cccd_handles = CFG.cccd_handles,
    cccd_action = S.cccd_action,
    cccd_disable_status = S.cccd_disable_status,
    cccd_write_status = S.cccd_write_status,
    subscription_cycle_count = S.subscription_cycle_count,
    stall_rearm_count = S.stall_rearm_count,
    stall_rearm_disable_status = S.stall_rearm_disable_status,
    stall_rearm_enable_status = S.stall_rearm_enable_status,
    stall_rearm_ms = S.stall_rearm_ms,
    hid_service_start = S.hid_service_start,
    hid_service_end = S.hid_service_end,
    report_map_handle = S.report_map_handle,
    report_map_len = S.report_map_len,
    report_map_hex = S.report_map_hex,
    external_report_reference_handle =
      S.external_report_reference_handle,
    external_report_reference_hex = S.external_report_reference_hex,
    external_report_reference_status =
      S.external_report_reference_status,
    protocol_mode_handle = S.protocol_mode_handle,
    protocol_mode_write_accepted = S.protocol_mode_write_accepted,
    report_count = S.report_count,
    input_report_count = S.input_report_count,
    reports = reports,
    control_point_handle = S.control_point_handle,
    control_point_write_count = S.control_point_write_count,
    control_point_write_status = S.control_point_write_status,
    control_point_write_error_count = S.control_point_write_error_count,
    control_point_last_write_ms = S.control_point_last_write_ms,
    vendor_service_start = S.vendor_service_start,
    vendor_service_end = S.vendor_service_end,
    vendor_input_handle = S.vendor_input_handle,
    vendor_write_handle = S.vendor_write_handle,
    vendor_reply_handle = S.vendor_reply_handle,
    vendor_aux_handle = S.vendor_aux_handle,
    vendor_subscribed_count = S.vendor_subscribed_count,
    vendor_notify_count = S.vendor_notify_count,
    vendor_write_count = S.vendor_write_count,
    vendor_heartbeat_count = S.vendor_heartbeat_count,
    vendor_heartbeat_reply_count = S.vendor_heartbeat_reply_count,
    vendor_watchdog_reply_count = S.vendor_watchdog_reply_count,
    vendor_last_notify_ms = S.vendor_last_notify_ms,
    vendor_stage = S.vendor_stage,
    vendor_handshake_sent = S.vendor_handshake_sent,
    vendor_seen_battery_reply = S.vendor_seen_battery_reply,
    vendor_seen_info_reply = S.vendor_seen_info_reply,
    vendor_session_complete = S.vendor_session_complete,
    vendor_session_start_count = S.vendor_session_start_count,
    vendor_session_retry_count = S.vendor_session_retry_count,
    official_sync_started = S.official_sync_started,
    official_sync_cycle_count = S.official_sync_cycle_count,
    official_sync_write_count = S.official_sync_write_count,
    official_sync_last_ms = S.official_sync_last_ms,
    official_sync_read_pending = S.official_sync_read_pending,
    official_sync_read_count = S.official_sync_read_count,
    official_sync_read_status = S.official_sync_read_status,
    official_sync_read_hex = S.official_sync_read_hex,
    vendor_first_disable_count = S.vendor_first_disable_count,
    vendor_first_disable_status = S.vendor_first_disable_status,
    hid_subscription_started = S.hid_subscription_started,
    vendor_last_hex = S.vendor_last_hex,
    vendor_read_hex = S.vendor_read_hex,
    diagnostic_action = S.diagnostic_action,
    diagnostic_count = S.diagnostic_count,
    diagnostic_started_ms = S.diagnostic_started_ms,
    diagnostic_notify_baseline = S.diagnostic_notify_baseline,
    diagnostic_result = S.diagnostic_result,
    diagnostic_read_hex = S.diagnostic_read_hex,
    diagnostic_read_status = S.diagnostic_read_status,
    diagnostic_read_pending = S.diagnostic_read_pending,
    event_count = S.event_count,
    last_error = S.last_error,
    ui_available = S.ui_available,
    ui_error = S.ui_error,
    ui_draw_count = S.ui_draw_count,
    events = S.events,
  }
end

local function response(value)
  local raw = json and json.encode and json.encode(value) or '{"ok":false}'
  return {
    status = "200 OK",
    type = "application/json; charset=utf-8",
    headers = { ["cache-control"] = "no-store" },
    body = raw,
  }
end

local function error_response(status, message)
  local raw = json and json.encode and json.encode({
    ok = false,
    error = tostring(message or "request failed"),
  }) or '{"ok":false,"error":"request failed"}'
  return {
    status = status or "400 Bad Request",
    type = "application/json; charset=utf-8",
    headers = { ["cache-control"] = "no-store" },
    body = raw,
  }
end

local function read_body(req, limit)
  if not req or not req.getbody then return "", nil end
  local parts, total = {}, 0
  while true do
    local chunk = req.getbody()
    if not chunk then break end
    total = total + #chunk
    if total > limit then return nil, "request body too large" end
    parts[#parts + 1] = chunk
  end
  return table.concat(parts), nil
end

local function start_diagnostic(action)
  if not S.connected or S.conn_handle < 0 then
    return false, "not connected"
  end
  if S.diagnostic_read_pending then
    return false, "diagnostic read pending"
  end

  S.diagnostic_action = action
  S.diagnostic_count = S.diagnostic_count + 1
  S.diagnostic_started_ms = now_ms()
  S.diagnostic_notify_baseline = S.notify_count
  S.diagnostic_result = "started"
  S.diagnostic_read_hex = ""
  S.diagnostic_read_status = 0

  if action == "read_report" then
    S.diagnostic_read_pending = true
    local ok, err = ble.gattc_read(S.conn_handle, CFG.report_handle)
    if not ok then
      S.diagnostic_read_pending = false
      S.diagnostic_result = "start_failed: " .. tostring(err)
      return false, S.diagnostic_result
    end
    add_event("diagnostic", "read_report handle=" .. tostring(CFG.report_handle))
    return true
  end

  if action == "watchdog_cycle" then
    if not write_vendor(
        string.char(0x10, 0x57, 0x44, 0x54, 0x01, 0x00),
        "diagnostic_watchdog_off") then
      S.diagnostic_result = "watchdog_off_failed"
      return false, S.diagnostic_result
    end
    schedule_vendor(300, function()
      if not S.connected then
        S.diagnostic_result = "disconnected"
        return
      end
      if write_vendor(
          string.char(0x10, 0x57, 0x44, 0x54, 0x01, 0x01),
          "diagnostic_watchdog_on") then
        S.diagnostic_result = "waiting_for_notify"
      else
        S.diagnostic_result = "watchdog_on_failed"
      end
    end)
    add_event("diagnostic", "watchdog_cycle")
    return true
  end

  if action == "protocol_cycle" then
    if S.protocol_mode_handle == 0 then
      S.diagnostic_result = "protocol_mode_missing"
      return false, S.diagnostic_result
    end
    S.phase = "diagnostic_protocol_cycle"
    local ok, err = ble.gattc_write(
      S.conn_handle,
      S.protocol_mode_handle,
      string.char(0),
      ble.WRITE_NO_RESPONSE
    )
    if not ok then
      S.phase = "ready"
      S.diagnostic_result = "boot_failed: " .. tostring(err)
      return false, S.diagnostic_result
    end
    add_event("diagnostic", "protocol boot accepted")
    schedule(150, function()
      if not S.connected then
        S.diagnostic_result = "disconnected"
        return
      end
      local report_ok, report_err = ble.gattc_write(
        S.conn_handle,
        S.protocol_mode_handle,
        string.char(1),
        ble.WRITE_NO_RESPONSE
      )
      S.phase = "ready"
      if not report_ok then
        S.diagnostic_result = "report_failed: " .. tostring(report_err)
        return
      end
      S.diagnostic_result = "waiting_for_notify"
      add_event("diagnostic", "protocol report accepted")
    end)
    return true
  end

  if action == "client_features" then
    if S.client_supported_features_handle == 0 then
      S.diagnostic_result = "client_features_missing"
      return false, S.diagnostic_result
    end
    S.phase = "diagnostic_client_features"
    local ok, err = ble.gattc_write(
      S.conn_handle,
      S.client_supported_features_handle,
      string.char(1),
      ble.WRITE_WITH_RESPONSE
    )
    if not ok then
      S.phase = "ready"
      S.diagnostic_result = "start_failed: " .. tostring(err)
      return false, S.diagnostic_result
    end
    add_event("diagnostic",
      "client_features handle=" ..
      tostring(S.client_supported_features_handle))
    return true
  end

  if action == "rediscover_services" then
    S.phase = "diagnostic_rediscover_services"
    S.recovery_service_count = 0
    local ok, err = ble.gattc_discover_services(S.conn_handle)
    if not ok then
      S.phase = "ready"
      S.diagnostic_result = "start_failed: " .. tostring(err)
      return false, S.diagnostic_result
    end
    add_event("diagnostic", "rediscover services")
    return true
  end

  if action == "read_database_hash" then
    if S.database_hash_handle == 0 then
      S.diagnostic_result = "database_hash_missing"
      return false, S.diagnostic_result
    end
    S.diagnostic_read_pending = true
    local ok, err = ble.gattc_read(
      S.conn_handle, S.database_hash_handle
    )
    if not ok then
      S.diagnostic_read_pending = false
      S.diagnostic_result = "start_failed: " .. tostring(err)
      return false, S.diagnostic_result
    end
    add_event("diagnostic",
      "read_database_hash handle=" .. tostring(S.database_hash_handle))
    return true
  end

  if action == "read_external_reference" then
    if S.report_map_handle == 0 then
      S.diagnostic_result = "report_map_missing"
      return false, S.diagnostic_result
    end
    -- KP20D places the External Report Reference immediately after the
    -- Report Map value (34 -> 35). Reading it is the remaining AOSP HOGP
    -- discovery step that the normal probe intentionally leaves isolated.
    local handle = S.report_map_handle + 1
    S.diagnostic_read_pending = true
    local ok, err = ble.gattc_read(S.conn_handle, handle)
    if not ok then
      S.diagnostic_read_pending = false
      S.diagnostic_result = "start_failed: " .. tostring(err)
      return false, S.diagnostic_result
    end
    add_event("diagnostic",
      "read_external_reference handle=" .. tostring(handle))
    return true
  end

  if action == "vendor_commit" then
    if not write_vendor(
        string.char(0x31, 0x00, 0x00),
        "official_commit") then
      S.diagnostic_result = "write_failed"
      return false, S.diagnostic_result
    end
    S.diagnostic_result = "waiting_for_notify"
    add_event("diagnostic", "vendor_commit")
    return true
  end

  if action == "cccd_command_cycle" then
    local handles = {}
    for _, report in ipairs(H.reports) do
      if report.report_type == 1 and
         (tonumber(report.cccd_handle) or 0) > 0 then
        handles[#handles + 1] = report.cccd_handle
      end
    end
    if #handles == 0 then
      S.diagnostic_result = "input_cccd_missing"
      return false, S.diagnostic_result
    end
    S.phase = "diagnostic_cccd_command_cycle"
    for _, handle in ipairs(handles) do
      local ok, err = ble.gattc_write(
        S.conn_handle,
        handle,
        string.char(0, 0),
        ble.WRITE_NO_RESPONSE
      )
      if not ok then
        S.phase = "ready"
        S.diagnostic_result = "disable_failed: " .. tostring(err)
        return false, S.diagnostic_result
      end
    end
    add_event("diagnostic", "CCCD command disable")
    schedule(150, function()
      if not S.connected then
        S.diagnostic_result = "disconnected"
        return
      end
      for _, handle in ipairs(handles) do
        local ok, err = ble.gattc_write(
          S.conn_handle,
          handle,
          string.char(1, 0),
          ble.WRITE_NO_RESPONSE
        )
        if not ok then
          S.phase = "ready"
          S.diagnostic_result = "enable_failed: " .. tostring(err)
          return
        end
      end
      S.phase = "ready"
      S.diagnostic_result = "waiting_for_notify"
      add_event("diagnostic", "CCCD command enable")
    end)
    return true
  end

  S.diagnostic_result = "unknown_action"
  return false, "unknown action"
end

local function route_command(req)
  local raw, read_err = read_body(req, 1024)
  if not raw then return error_response("400 Bad Request", read_err) end
  if not json or not json.decode then
    return error_response("500 Internal Server Error", "json.decode missing")
  end
  local ok, doc = pcall(function() return json.decode(raw) end)
  if not ok or type(doc) ~= "table" or type(doc.action) ~= "string" then
    return error_response("400 Bad Request", "action missing")
  end
  local started, err = start_diagnostic(doc.action)
  if not started then return error_response("409 Conflict", err) end
  return response(snapshot())
end

init_ui()

if not ble or not ble.irq or not ble.gap_scan then
  S.phase = "failed"
  set_error("Lua BLE API missing; run as a background service")
else
  if httpd and httpd.start then
    pcall(function()
      httpd.start({ webroot = "/sd", auto_index = httpd.INDEX_NONE, max_handlers = 64 })
    end)
  end
  if httpd and httpd.dynamic then
    local get, post = httpd.GET or "GET", httpd.POST or "POST"
    pcall(function()
      httpd.dynamic(get, CFG.route .. "/api/state", function()
        return response(snapshot())
      end)
    end)
    pcall(function()
      httpd.dynamic(post, CFG.route .. "/api/command", route_command)
    end)
  end

  local ok, err = ble.config({
    -- JoyU's GATT client never calls BluetoothGatt.requestMtu(), so keep the
    -- Android default for this single-variable controller comparison.
    mtu = CFG.preferred_mtu,
    rxbuf = 2048,
    bond = true,
    mitm = false,
    le_secure = true,
    io = ble.IO_CAPABILITY_NO_INPUT_OUTPUT,
    addr_mode = ble.OWN_ADDR_PUBLIC,
    gap_name = "Cubic-BTP-Probe",
  })
  if not ok then
    S.phase = "failed"
    set_error("ble.config: " .. tostring(err))
  elseif not ble.active(true) then
    S.phase = "failed"
    set_error("ble.active failed")
  else
    ble.irq(on_ble)
    add_event("started", CFG.mode)
    start_scan()
  end

  if tmr and tmr.create then
    watchdog_timer = tmr.create()
    watchdog_timer:alarm(150, tmr.ALARM_AUTO, function()
      -- UI rendering is coalesced outside the high-rate notification callback.
      draw_ui()
    end)
  end
end
