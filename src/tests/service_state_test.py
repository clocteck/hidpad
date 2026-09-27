"""Focused service tests with a Lua 5.4 runtime (pip install lupa)."""
import json
from pathlib import Path
from lupa import LuaRuntime

runtime = LuaRuntime(unpack_returned_tuples=True)
files = {}
def lua_value(value):
    return runtime.table_from({k: lua_value(v) for k, v in value.items()}) if isinstance(value, dict) else value
def python_value(value):
    return {k: python_value(v) for k, v in value.items()} if hasattr(value, "items") else value
def write(path, raw):
    files[path] = raw
    return True
runtime.globals().json = runtime.table_from({
    "decode": lambda raw: lua_value(json.loads(raw)),
    "encode": lambda value: json.dumps(python_value(value)),
})
runtime.globals().file = runtime.table_from({"getcontents": files.get, "putcontents": write})
source = (Path(__file__).parents[2] / "package/main.lua").read_text(encoding="utf-8")
assert not isinstance(runtime.eval("load")(source), tuple)
api = runtime.execute(source[:source.rindex("\nload_config()\n")] + """
return {state=state_snapshot, input=input_snapshot, command=handle_command,
 language=read_language, poll=poll_driver, setup=function(driver) S.driver=driver end,
 error=function(value) S.last_error=value end}
""")
driver = runtime.execute("""
return {set_preferred=function() return true end,
 set_auto_connect=function(value) policy=value; return true end,
 connect=function() return true,17 end, forget=function() return true,18 end,
 poll=function(target)
   for key,value in pairs(next_state) do target[key]=value end
   return target,true
 end}
""")
api.setup(driver)
assert api.state().config.auto_connect is True
assert api.command("set_config", lua_value({"auto_connect": False}))[0]
assert runtime.globals().policy == 0
assert json.loads(files["/sd/apps/hidpad/config.json"])["auto_connect"] is False

files["/sd/apps/settings.json"] = '{"language":"en-US"}'
assert api.language() == "en"
files["/sd/settings.json"] = '{"locale":"zh_Hant"}'
assert api.language() == "zh-TW"
files["/sd/settings.json"] = "invalid"
assert api.language() == "en"

# Connect accepted is not yet a successful connection and must not save a target.
accepted, token = api.command("connect_device", lua_value({"address": "01:02:03:04:05:06"}))
assert accepted and token == 17
assert api.state().config.preferred_address == ""
api.error("old connection error")
runtime.globals().next_state = lua_value({"connected": True, "ready": True, "started": True,
    "phase": "ready", "last_error": "", "address": "01:02:03:04:05:06", "name": "Xbox",
    "profile": "xbox", "command_id": 17, "command_kind": "connect", "command_status": "succeeded"})
api.poll()
assert api.state().last_error is None  # Same-table poll must clear the previous error.
assert api.state().config.preferred_address == "01:02:03:04:05:06"
assert api.input().last_report_hex is None

# An accepted or failed forget must leave the saved device intact.
assert api.command("forget", "")[0]
assert api.state().config.preferred_address != ""
runtime.globals().next_state = lua_value({"command_id": 18, "command_kind": "forget",
    "command_status": "failed", "command_error": "forget failed", "last_error": "forget failed"})
api.poll()
assert api.state().config.preferred_address != ""
assert api.state().last_error == "forget failed"
runtime.globals().next_state = lua_value({"connected": False, "ready": False, "phase": "select_device",
    "command_id": 19, "command_kind": "forget", "command_status": "succeeded", "last_error": ""})
api.poll()
assert api.state().config.preferred_address == ""

# SD write failure does not silently commit the policy.
runtime.globals().file.putcontents = lambda path, raw: False
assert api.command("set_config", lua_value({"auto_connect": True}))[0] is False
assert api.state().config.auto_connect is False and runtime.globals().policy == 0
print("Lua service: command completion, stale error, language and persistence checks passed")
