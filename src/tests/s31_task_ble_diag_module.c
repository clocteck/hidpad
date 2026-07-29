#include "module_abi.h"

#include <stddef.h>
#include <stdint.h>

#define DIAG_EXPORT __attribute__((visibility("default")))
#define DIAG_WORKER_STACK_BYTES (4u * 1024u)
#define DIAG_WORKER_PRIORITY 2u
#define DIAG_WORKER_CORE 0

typedef struct diag_instance_t {
    void (*serial_println)(const char *text);
    void (*task_delay)(uint32_t ms);
    int32_t (*task_create_ex)(const char *name, void (*entry)(void *), void *arg,
                              uint32_t stack_bytes, uint32_t priority, int32_t core,
                              uint32_t heap_caps, void **out_task);
    int32_t (*sync_create_counting)(uint32_t max_count, uint32_t initial_count,
                                    module_sync_handle_t *out_handle);
    int32_t (*sync_take)(module_sync_handle_t handle, uint32_t timeout_ms);
    int32_t (*sync_give)(module_sync_handle_t handle);
    void (*sync_destroy)(module_sync_handle_t handle);
    int32_t (*ble_open)(uint32_t owner_token, const module_ble_config_t *cfg,
                        module_ble_session_t *out_session);
    int32_t (*ble_close)(module_ble_session_t session);
    int32_t (*ble_event_poll)(module_ble_session_t session, module_ble_event_t *out_event);

    int (*lua_gettop)(lua_State *L);
    void *(*lua_touserdata)(lua_State *L, int idx);
    void (*lua_pushboolean)(lua_State *L, int value);
    void (*lua_pushinteger)(lua_State *L, int64_t value);
    void (*lua_pushstring)(lua_State *L, const char *text);
    void (*lua_pushlightuserdata)(lua_State *L, void *ptr);
    void (*lua_pushcclosure)(lua_State *L, module_lua_cfunction_t fn, int nup);
    void (*lua_createtable)(lua_State *L, int narr, int nrec);
    void (*lua_setfield)(lua_State *L, int idx, const char *key);
    int (*lua_upvalue_index)(int n);

    uint32_t owner_token;
    void *worker_task;
    module_sync_handle_t worker_stopped;
    module_ble_session_t session;
    volatile uint32_t stage;
    volatile uint32_t counter;
    volatile int32_t last_rc;
    volatile uint32_t last_irq;
    volatile uint8_t worker_running;
    volatile uint8_t worker_stop;
    volatile uint8_t worker_mode;
} diag_instance_t;

static diag_instance_t s_diag;

static const module_manifest_t s_manifest = {
    MODULE_MANIFEST_MAGIC,
    MODULE_SDK_VERSION,
    sizeof(module_manifest_t),
    "s31diag",
    "1.0.0",
    "S31 dynamic-module task and BLE ABI staged probe",
    0,
    MODULE_BOOTSTRAP_ABI_VERSION,
};

static void zero_bytes(void *ptr, size_t size)
{
    volatile uint8_t *bytes = (volatile uint8_t *)ptr;
    size_t i;
    for (i = 0; i < size; ++i) {
        bytes[i] = 0;
    }
}

void *memset(void *ptr, int value, size_t size)
{
    volatile uint8_t *bytes = (volatile uint8_t *)ptr;
    while (size > 0) {
        *bytes++ = (uint8_t)value;
        --size;
    }
    return ptr;
}

static int32_t resolve_required(module_host_resolve_v2_fn resolve, void *resolve_ctx,
                                uint32_t proc_id, void **out_proc)
{
    int32_t rc;
    if (!resolve || !out_proc) {
        return MODULE_ERR_INVALID_ARG;
    }
    *out_proc = NULL;
    rc = resolve(resolve_ctx, proc_id, out_proc);
    if (rc != MODULE_OK) {
        return rc;
    }
    return *out_proc ? MODULE_OK : MODULE_ERR_UNSUPPORTED;
}

static int32_t resolve_optional(module_host_resolve_v2_fn resolve, void *resolve_ctx,
                                uint32_t proc_id, void **out_proc)
{
    int32_t rc;
    if (!resolve || !out_proc) {
        return MODULE_ERR_INVALID_ARG;
    }
    *out_proc = NULL;
    rc = resolve(resolve_ctx, proc_id, out_proc);
    if (rc == MODULE_ERR_NOT_FOUND || rc == MODULE_ERR_UNSUPPORTED) {
        *out_proc = NULL;
        return MODULE_OK;
    }
    return rc;
}

#define RESOLVE_REQUIRED(id, slot)                                            \
    do {                                                                       \
        void *proc = NULL;                                                     \
        rc = resolve_required(resolve, resolve_ctx, (id), &proc);             \
        if (rc != MODULE_OK) return rc;                                        \
        (slot) = (__typeof__(slot))proc;                                       \
    } while (0)

#define RESOLVE_OPTIONAL(id, slot)                                            \
    do {                                                                       \
        void *proc = NULL;                                                     \
        rc = resolve_optional(resolve, resolve_ctx, (id), &proc);             \
        if (rc != MODULE_OK) return rc;                                        \
        (slot) = (__typeof__(slot))proc;                                       \
    } while (0)

static diag_instance_t *lua_instance(lua_State *L)
{
    int idx = s_diag.lua_upvalue_index(1);
    return (diag_instance_t *)s_diag.lua_touserdata(L, idx);
}

static void set_integer(lua_State *L, const char *name, int64_t value)
{
    s_diag.lua_pushinteger(L, value);
    s_diag.lua_setfield(L, -2, name);
}

static void set_boolean(lua_State *L, const char *name, int value)
{
    s_diag.lua_pushboolean(L, value);
    s_diag.lua_setfield(L, -2, name);
}

static void worker_main(void *arg)
{
    diag_instance_t *diag = (diag_instance_t *)arg;
    module_ble_config_t cfg;
    module_ble_event_t event;
    int32_t rc;

    if (!diag) {
        return;
    }

    diag->stage = 10;
    if (diag->serial_println) {
        diag->serial_println("[s31diag] worker entered");
    }

    if (diag->worker_mode == 2) {
        zero_bytes(&cfg, sizeof(cfg));
        cfg.size = sizeof(cfg);
        cfg.mtu = 32;
        cfg.rxbuf = 1024;
        cfg.bond = 0;
        cfg.mitm = 0;
        cfg.le_secure = 0;
        cfg.io_capability = MODULE_BLE_IO_NO_INPUT_OUTPUT;
        cfg.own_addr_type = MODULE_BLE_OWN_ADDR_PUBLIC;
        cfg.gap_name[0] = 'S';
        cfg.gap_name[1] = '3';
        cfg.gap_name[2] = '1';
        cfg.gap_name[3] = '-';
        cfg.gap_name[4] = 'D';
        cfg.gap_name[5] = 'i';
        cfg.gap_name[6] = 'a';
        cfg.gap_name[7] = 'g';
        cfg.gap_name[8] = '\0';

        diag->stage = 20;
        if (diag->serial_println) {
            diag->serial_println("[s31diag] before BLE open");
        }
        rc = diag->ble_open(diag->owner_token, &cfg, &diag->session);
        diag->last_rc = rc;
        diag->stage = 30;
        if (diag->serial_println) {
            diag->serial_println("[s31diag] after BLE open");
        }
    } else if (diag->worker_mode == 3) {
        zero_bytes(&event, sizeof(event));
        event.size = sizeof(event);
        diag->stage = 40;
        rc = diag->ble_event_poll(diag->session, &event);
        diag->last_rc = rc;
        diag->last_irq = event.irq;
        diag->stage = 41;
    }

    while (!diag->worker_stop) {
        diag->counter++;
        diag->task_delay(10);
    }

    diag->stage = 90;
    diag->worker_running = 0;
    if (diag->worker_stopped) {
        (void)diag->sync_give(diag->worker_stopped);
    }
    if (diag->serial_println) {
        diag->serial_println("[s31diag] worker returning");
    }
}

static int start_worker(diag_instance_t *diag, uint8_t mode)
{
    int32_t rc;
    if (!diag || diag->worker_running) {
        return MODULE_ERR_BUSY;
    }
    if (diag->worker_stopped) {
        diag->sync_destroy(diag->worker_stopped);
        diag->worker_stopped = NULL;
    }
    rc = diag->sync_create_counting(1, 0, &diag->worker_stopped);
    if (rc != MODULE_OK) {
        return rc;
    }

    diag->worker_mode = mode;
    diag->worker_stop = 0;
    diag->worker_running = 1;
    diag->counter = 0;
    diag->last_rc = MODULE_OK;
    diag->last_irq = 0;
    diag->stage = 1;
    rc = diag->task_create_ex("s31diag_worker", worker_main, diag,
                              DIAG_WORKER_STACK_BYTES, DIAG_WORKER_PRIORITY,
                              DIAG_WORKER_CORE, MODULE_HEAP_PSRAM | MODULE_HEAP_8BIT,
                              &diag->worker_task);
    if (rc != MODULE_OK) {
        diag->last_rc = rc;
        diag->stage = 99;
        diag->worker_running = 0;
    }
    return rc;
}

static int l_status(lua_State *L)
{
    diag_instance_t *diag = lua_instance(L);
    if (!diag) {
        s_diag.lua_pushstring(L, "instance missing");
        return 1;
    }
    s_diag.lua_createtable(L, 0, 12);
    set_integer(L, "stage", diag->stage);
    set_integer(L, "counter", diag->counter);
    set_integer(L, "last_rc", diag->last_rc);
    set_integer(L, "last_irq", diag->last_irq);
    set_integer(L, "session", diag->session);
    set_integer(L, "requested_priority", DIAG_WORKER_PRIORITY);
    set_integer(L, "requested_core", DIAG_WORKER_CORE);
    set_integer(L, "worker_mode", diag->worker_mode);
    set_integer(L, "owner_token", diag->owner_token);
    set_boolean(L, "ble_available", diag->ble_open && diag->ble_close &&
                                           diag->ble_event_poll);
    set_boolean(L, "worker_running", diag->worker_running);
    set_boolean(L, "worker_stop", diag->worker_stop);
    return 1;
}

static int l_start_task(lua_State *L)
{
    diag_instance_t *diag = lua_instance(L);
    int32_t rc = start_worker(diag, 1);
    s_diag.lua_pushboolean(L, rc == MODULE_OK);
    s_diag.lua_pushinteger(L, rc);
    return 2;
}

static int l_start_ble_open(lua_State *L)
{
    diag_instance_t *diag = lua_instance(L);
    int32_t rc = (!diag || !diag->owner_token || !diag->ble_open)
                     ? MODULE_ERR_UNSUPPORTED
                     : start_worker(diag, 2);
    s_diag.lua_pushboolean(L, rc == MODULE_OK);
    s_diag.lua_pushinteger(L, rc);
    return 2;
}

static int l_start_poll_once(lua_State *L)
{
    diag_instance_t *diag = lua_instance(L);
    int32_t rc;
    if (!diag || !diag->ble_event_poll || diag->session == 0) {
        rc = MODULE_ERR_BAD_STATE;
    } else {
        rc = start_worker(diag, 3);
    }
    s_diag.lua_pushboolean(L, rc == MODULE_OK);
    s_diag.lua_pushinteger(L, rc);
    return 2;
}

static int l_stop_worker(lua_State *L)
{
    diag_instance_t *diag = lua_instance(L);
    int32_t rc = MODULE_OK;
    if (!diag) {
        rc = MODULE_ERR_INVALID_ARG;
    } else if (diag->worker_running) {
        diag->worker_stop = 1;
        rc = diag->sync_take(diag->worker_stopped, 3000);
    }
    s_diag.lua_pushboolean(L, rc == MODULE_OK);
    s_diag.lua_pushinteger(L, rc);
    return 2;
}

static int l_close_ble(lua_State *L)
{
    diag_instance_t *diag = lua_instance(L);
    int32_t rc = MODULE_OK;
    if (!diag || !diag->ble_close || diag->session == 0) {
        rc = MODULE_ERR_BAD_STATE;
    } else {
        rc = diag->ble_close(diag->session);
        if (rc == MODULE_OK) {
            diag->session = 0;
        }
    }
    s_diag.lua_pushboolean(L, rc == MODULE_OK);
    s_diag.lua_pushinteger(L, rc);
    return 2;
}

static void set_function(lua_State *L, const char *name, module_lua_cfunction_t fn,
                         diag_instance_t *diag)
{
    s_diag.lua_pushlightuserdata(L, diag);
    s_diag.lua_pushcclosure(L, fn, 1);
    s_diag.lua_setfield(L, -2, name);
}

DIAG_EXPORT const module_manifest_t *module_query_v1(void)
{
    return &s_manifest;
}

DIAG_EXPORT int32_t module_create_v2(module_host_resolve_v2_fn resolve,
                                     void *resolve_ctx,
                                     const module_open_info_t *info,
                                     void **out_instance)
{
    int32_t rc;
    if (!info || info->size < sizeof(*info) || !out_instance) {
        return MODULE_ERR_INVALID_ARG;
    }
    zero_bytes(&s_diag, sizeof(s_diag));

    RESOLVE_OPTIONAL(MODULE_PROC_SERIAL_PRINTLN_V1, s_diag.serial_println);
    RESOLVE_REQUIRED(MODULE_PROC_TASK_DELAY_V1, s_diag.task_delay);
    RESOLVE_REQUIRED(MODULE_PROC_TASK_CREATE_EX_V1, s_diag.task_create_ex);
    RESOLVE_REQUIRED(MODULE_PROC_SYNC_CREATE_COUNTING_V1, s_diag.sync_create_counting);
    RESOLVE_REQUIRED(MODULE_PROC_SYNC_TAKE_V1, s_diag.sync_take);
    RESOLVE_REQUIRED(MODULE_PROC_SYNC_GIVE_V1, s_diag.sync_give);
    RESOLVE_REQUIRED(MODULE_PROC_SYNC_DESTROY_V1, s_diag.sync_destroy);
    RESOLVE_OPTIONAL(MODULE_PROC_BLE_OPEN_V1, s_diag.ble_open);
    RESOLVE_OPTIONAL(MODULE_PROC_BLE_CLOSE_V1, s_diag.ble_close);
    RESOLVE_OPTIONAL(MODULE_PROC_BLE_EVENT_POLL_V1, s_diag.ble_event_poll);
    RESOLVE_REQUIRED(MODULE_PROC_LUA_GETTOP_V1, s_diag.lua_gettop);
    RESOLVE_REQUIRED(MODULE_PROC_LUA_TOUSERDATA_V1, s_diag.lua_touserdata);
    RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHBOOLEAN_V1, s_diag.lua_pushboolean);
    RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHINTEGER_V1, s_diag.lua_pushinteger);
    RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHSTRING_V1, s_diag.lua_pushstring);
    RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHLIGHTUSERDATA_V1, s_diag.lua_pushlightuserdata);
    RESOLVE_REQUIRED(MODULE_PROC_LUA_PUSHCCLOSURE_V1, s_diag.lua_pushcclosure);
    RESOLVE_REQUIRED(MODULE_PROC_LUA_CREATETABLE_V1, s_diag.lua_createtable);
    RESOLVE_REQUIRED(MODULE_PROC_LUA_SETFIELD_V1, s_diag.lua_setfield);
    RESOLVE_REQUIRED(MODULE_PROC_LUA_UPVALUE_INDEX_V1, s_diag.lua_upvalue_index);

    s_diag.owner_token = info->owner_token;
    *out_instance = &s_diag;
    return MODULE_OK;
}

DIAG_EXPORT int32_t module_luaopen_v1(void *instance, lua_State *L)
{
    diag_instance_t *diag = (diag_instance_t *)instance;
    if (!diag || !L) {
        return MODULE_ERR_INVALID_ARG;
    }
    s_diag.lua_createtable(L, 0, 8);
    set_function(L, "status", l_status, diag);
    set_function(L, "start_task", l_start_task, diag);
    set_function(L, "stop_worker", l_stop_worker, diag);
    set_function(L, "start_ble_open", l_start_ble_open, diag);
    set_function(L, "start_poll_once", l_start_poll_once, diag);
    set_function(L, "close_ble", l_close_ble, diag);
    s_diag.lua_pushstring(L, "1.0.0");
    s_diag.lua_setfield(L, -2, "VERSION");
    return MODULE_OK;
}

DIAG_EXPORT void module_destroy_v1(void *instance)
{
    diag_instance_t *diag = (diag_instance_t *)instance;
    if (!diag) {
        return;
    }
    if (diag->worker_running) {
        diag->worker_stop = 1;
        (void)diag->sync_take(diag->worker_stopped, 3000);
    }
    if (diag->session != 0) {
        (void)diag->ble_close(diag->session);
        diag->session = 0;
    }
    if (diag->worker_stopped) {
        diag->sync_destroy(diag->worker_stopped);
        diag->worker_stopped = NULL;
    }
}

#undef RESOLVE_REQUIRED
#undef RESOLVE_OPTIONAL
