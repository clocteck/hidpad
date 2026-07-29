local APP_DIR = "/sd/apps/s31diag"
local RESULT_PATH = APP_DIR .. "/result.json"
local MODULE_PATH = APP_DIR .. "/modules/s31diag.so"

local result = {
  marker = "s31diag-service-ble-open-poll-v2",
  require_ok = false,
  samples = {},
}

local function save()
  if file and file.putcontents and json and json.encode then
    pcall(function() file.putcontents(RESULT_PATH, json.encode(result)) end)
  end
end

local ok, mod = pcall(require, MODULE_PATH)
result.require_ok = ok
result.loaded_type = type(mod)
if not ok or type(mod) ~= "table" then
  result.error = tostring(mod)
  print("[s31diag-service] require failed", result.error)
  save()
  return
end

result.initial = mod.status()
print("[s31diag-service] loaded owner_token=" .. tostring(result.initial.owner_token))

local start_ok, start_rc = mod.start_ble_open()
result.start_ok = start_ok
result.start_rc = start_rc
print("[s31diag-service] start_ble_open ok=" .. tostring(start_ok) ..
      " rc=" .. tostring(start_rc))
save()

local sample_count = 0
local phase = "open"
local poll = tmr.create()
poll:alarm(250, tmr.ALARM_AUTO, function()
  sample_count = sample_count + 1
  local status = mod.status()
  result.samples[#result.samples + 1] = status
  if #result.samples > 40 then
    table.remove(result.samples, 1)
  end

  if sample_count == 1 or sample_count % 4 == 0 or status.stage ~= 20 then
    print("[s31diag-service] sample=" .. tostring(sample_count) ..
          " stage=" .. tostring(status.stage) ..
          " rc=" .. tostring(status.last_rc) ..
          " session=" .. tostring(status.session) ..
          " counter=" .. tostring(status.counter))
    save()
  end

  if phase == "open" and status.stage == 30 then
    local stop_ok, stop_rc = mod.stop_worker()
    result.open_stop_ok = stop_ok
    result.open_stop_rc = stop_rc
    result.after_open = mod.status()
    local poll_ok, poll_rc = mod.start_poll_once()
    result.poll_start_ok = poll_ok
    result.poll_start_rc = poll_rc
    phase = "event_poll"
    print("[s31diag-service] BLE open completed; event_poll started ok=" ..
          tostring(poll_ok) .. " rc=" .. tostring(poll_rc))
    save()
  elseif phase == "event_poll" and status.stage == 41 then
    phase = "done"
    local stop_ok, stop_rc = mod.stop_worker()
    local close_ok, close_rc = mod.close_ble()
    result.poll_stop_ok = stop_ok
    result.poll_stop_rc = stop_rc
    result.close_ok = close_ok
    result.close_rc = close_rc
    result.final = mod.status()
    print("[s31diag-service] event_poll completed; worker stopped and session closed")
    save()
    poll:unregister()
  end
end)
