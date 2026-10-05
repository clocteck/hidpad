/* Deterministic state-machine checks. Includes the real driver, mocks only BLE/time.
 * Runs freestanding in a CPU emulator; no hardware or OS scheduler needed. */
#include "../main/hidpad_module.c"

static hidpad_instance_t test_inst;
static hidpad_cold_state_t test_cold;
static hidpad_host_api_t test_host;
static uint32_t clock_ms;
static int scans, disconnects, connects, scan_error, open_error, forget_error;
static uint32_t test_clock(void) { return clock_ms; }
static int32_t test_scan(module_ble_session_t s, const module_ble_scan_config_t *cfg)
{ (void)s; (void)cfg; scans++; return scan_error; }
static int32_t test_stop_scan(module_ble_session_t s) { (void)s; return MODULE_OK; }
static int32_t test_disconnect(module_ble_session_t s, uint16_t h)
{ (void)s; (void)h; disconnects++; return MODULE_OK; }
static int32_t test_connect(module_ble_session_t s, uint8_t t, const char *a, uint32_t timeout)
{ (void)s; (void)t; (void)a; (void)timeout; connects++; return MODULE_OK; }
static int32_t test_pair(module_ble_session_t s, uint16_t h, int32_t async)
{ (void)s; (void)h; (void)async; return MODULE_OK; }
static int32_t test_poll(module_ble_session_t s, module_ble_event_t *e)
{ (void)s; (void)e; return MODULE_ERR_NOT_FOUND; }
static int32_t test_open(uint32_t owner, const module_ble_config_t *cfg, module_ble_session_t *s)
{ (void)owner; (void)cfg; *s=1; return open_error; }
static int32_t test_close(module_ble_session_t s) { (void)s; return MODULE_OK; }
static int32_t test_forget(module_ble_session_t s, uint8_t t, const char *a)
{ (void)s; (void)t; (void)a; return forget_error; }
static void setup(void)
{
    zero_bytes(&test_inst, sizeof(test_inst)); zero_bytes(&test_cold, sizeof(test_cold));
    zero_bytes(&test_host, sizeof(test_host));
    test_inst.host=&test_host; test_inst.cold=&test_cold; test_inst.started=1;
    test_inst.auto_connect=1; test_inst.scan_ms=1000; test_inst.conn_handle=0xffff;
    test_inst.rescan_backoff_ms=HIDPAD_RESCAN_MIN_MS;
    test_host.time.millis=test_clock; test_host.ble.gap_scan=test_scan;
    test_host.ble.gap_scan_stop=test_stop_scan; test_host.ble.gap_disconnect=test_disconnect;
    test_host.ble.gap_connect=test_connect; test_host.ble.gap_pair=test_pair;
    test_host.ble.event_poll=test_poll; test_host.ble.open=test_open;
    test_host.ble.close=test_close; test_host.ble.gap_forget_device=test_forget;
    clock_ms=100; scans=disconnects=connects=scan_error=open_error=forget_error=0;
}
static void ready(void)
{
    test_inst.state.connected=1; test_inst.conn_handle=1; test_inst.phase=PHASE_READY;
    copy_text(test_inst.state.address,18,"01:02:03:04:05:06",17);
}
static void device(void)
{
    test_inst.scan_result_count=1;
    copy_text(test_cold.scan_results[0].address,18,"01:02:03:04:05:06",17);
    copy_text(test_cold.scan_results[0].name,40,"Xbox Controller",15);
}
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int run_tests(void)
{
    module_ble_event_t e;
    zero_bytes(&e,sizeof(e)); e.irq=MODULE_BLE_IRQ_SCAN_DONE;
    /* A missing saved address must not block any supported-name fallback. */
    {
        const char *names[] = {"Xbox Controller", "Q34", "Q36 for Android",
                               "BTP-KP20D BFM", "Flydigi APEX 5"};
        unsigned int i;
        for (i=0; i<sizeof(names)/sizeof(names[0]); ++i) {
            setup(); device();
            copy_text(test_cold.scan_results[0].name,40,names[i],strlen(names[i]));
            CHECK(should_auto_connect(&test_inst,&test_cold.scan_results[0]));
            copy_text(test_cold.preferred_address,18,"01:02:03:04:05:07",17);
            CHECK(should_auto_connect(&test_inst,&test_cold.scan_results[0]));
            test_inst.scan_active=1; handle_event(&test_inst,&e);
            CHECK(connects==1 && test_inst.state.connecting);
        }
    }
    setup(); device();
    copy_text(test_cold.preferred_address,18,"01:02:03:04:05:07",17);
    copy_text(test_cold.scan_results[0].name,40,"Unknown HID",11);
    CHECK(!should_auto_connect(&test_inst,&test_cold.scan_results[0]));
    test_cold.scan_results[1]=test_cold.scan_results[0];
    copy_text(test_cold.scan_results[1].address,18,test_cold.preferred_address,17);
    copy_text(test_cold.scan_results[0].name,40,"Xbox Controller",15);
    test_cold.scan_results[0].score=200; test_cold.scan_results[1].score=1;
    test_inst.scan_result_count=2;
    CHECK(select_auto_device(&test_inst)==&test_cold.scan_results[1]);
    test_inst.auto_connect=0;
    CHECK(select_auto_device(&test_inst)==NULL);
    test_inst.auto_connect=1; test_inst.manual_scan=1;
    CHECK(select_auto_device(&test_inst)==NULL);

    setup(); ready(); test_inst.phase=PHASE_PAIRING;
    CHECK(prepare_command(&test_inst,WORKER_COMMAND_SCAN,0)==MODULE_OK);
    execute_worker_command(&test_inst,WORKER_COMMAND_SCAN);
    CHECK(scans==0 && test_cold.command_status==3 && test_inst.phase==PHASE_PAIRING);
    test_inst.scan_active=1; handle_event(&test_inst,&e);
    CHECK(test_inst.phase==PHASE_PAIRING); /* Late scan done cannot make input ready. */

    setup(); ready(); prepare_command(&test_inst,WORKER_COMMAND_SCAN,0);
    execute_worker_command(&test_inst,WORKER_COMMAND_SCAN);
    CHECK(scans==1 && test_inst.phase==PHASE_READY && test_cold.command_status==1);
    handle_event(&test_inst,&e); CHECK(test_cold.command_status==2 && !test_inst.scan_active);

    setup(); ready(); prepare_command(&test_inst,WORKER_COMMAND_DISCONNECT,0);
    execute_worker_command(&test_inst,WORKER_COMMAND_DISCONNECT);
    CHECK(disconnects==1 && test_cold.command_status==1);
    e.irq=MODULE_BLE_IRQ_PERIPHERAL_DISCONNECT; handle_event(&test_inst,&e);
    CHECK(test_cold.command_status==2 && test_inst.manual_scan);
    driver_poll(&test_inst); device(); e.irq=MODULE_BLE_IRQ_SCAN_DONE; handle_event(&test_inst,&e);
    CHECK(connects==0 && test_inst.phase==PHASE_SELECT_DEVICE);

    setup(); test_inst.auto_connect=0; CHECK(driver_start(&test_inst)==MODULE_OK);
    prepare_command(&test_inst,WORKER_COMMAND_SCAN,0); execute_worker_command(&test_inst,WORKER_COMMAND_SCAN);
    device(); test_inst.auto_connect=1; test_inst.auto_connect_dirty=1; driver_poll(&test_inst);
    CHECK(!test_inst.manual_scan);
    handle_event(&test_inst,&e); CHECK(connects==1);

    setup(); test_inst.auto_connect=0; ready(); test_inst.profile=DEVICE_PROFILE_Q36;
    test_inst.subscribed_count=1; test_inst.next_notification_reconnect_ms=clock_ms;
    poll_notification_reconnect(&test_inst);
#if HIDPAD_ENABLE_NOTIFY_RECONNECT
    CHECK(disconnects==1 && test_inst.notification_reconnect_pending);
    e.irq=MODULE_BLE_IRQ_PERIPHERAL_DISCONNECT; handle_event(&test_inst,&e);
    CHECK(test_inst.phase==PHASE_WAIT_RESCAN);
    clock_ms+=1200; driver_poll(&test_inst); device(); e.irq=MODULE_BLE_IRQ_SCAN_DONE; handle_event(&test_inst,&e);
    CHECK(connects==1); e.irq=MODULE_BLE_IRQ_PERIPHERAL_DISCONNECT; handle_event(&test_inst,&e);
    CHECK(test_inst.phase==PHASE_SELECT_DEVICE && !test_inst.notification_reconnect_pending);
#else
    CHECK(disconnects==0 && !test_inst.notification_reconnect_pending);
    CHECK(test_inst.state.connected && test_inst.phase==PHASE_READY);
#endif

    setup(); device(); prepare_command(&test_inst,WORKER_COMMAND_CONNECT,"01:02:03:04:05:06");
    test_inst.scan_result_count=0; /* Worker execution must not depend on the mutable list. */
    execute_worker_command(&test_inst,WORKER_COMMAND_CONNECT);
    CHECK(connects==1 && test_cold.command_status==1);
    ready(); test_inst.last_error="previous error"; complete_ready(&test_inst);
    CHECK(test_cold.command_status==2 && !test_inst.last_error);

    setup(); ready(); forget_error=MODULE_ERR_UNSUPPORTED;
    prepare_command(&test_inst,WORKER_COMMAND_FORGET,0); execute_worker_command(&test_inst,WORKER_COMMAND_FORGET);
    CHECK(test_cold.command_status==1);
    e.irq=MODULE_BLE_IRQ_PERIPHERAL_DISCONNECT; handle_event(&test_inst,&e);
    CHECK(test_cold.command_status==3);

    setup(); test_inst.started=0; open_error=MODULE_ERR_BUSY;
    prepare_command(&test_inst,WORKER_COMMAND_START,0); execute_worker_command(&test_inst,WORKER_COMMAND_START);
    CHECK(test_cold.command_status==3 && !test_inst.started);
    open_error=0; prepare_command(&test_inst,WORKER_COMMAND_START,0); execute_worker_command(&test_inst,WORKER_COMMAND_START);
    CHECK(test_cold.command_status==2 && test_inst.started);

    setup(); CHECK(worker_wait_ms(&test_inst)==1000);
    schedule_rescan(&test_inst,8000); CHECK(worker_wait_ms(&test_inst)==1000);
    clock_ms=test_inst.next_scan_ms-15; CHECK(worker_wait_ms(&test_inst)==15);
    ready(); CHECK(worker_wait_ms(&test_inst)==10);
    return 0;
}
unsigned int hot_size(void) { return sizeof(hidpad_instance_t); }
unsigned int cold_size(void) { return sizeof(hidpad_cold_state_t); }
