#include <stdint.h>
#include <string.h>

#include <Wire.h>

#include "raat.hpp"
#include "raat-buffer.hpp"

#include "raat-oneshot-timer.hpp"
#include "raat-oneshot-task.hpp"
#include "raat-task.hpp"

#include "http-get-server.hpp"

static HTTPGetServer s_server(true);
static const raat_devices_struct * s_pDevices = NULL;

static void depower_servo_task_fn(RAATOneShotTask& this_task, void * pTaskData)
{
    (void)this_task; (void)pTaskData;
    s_pDevices->pServoPower->set(false);
}
static RAATOneShotTask s_depower_servo_task(750, depower_servo_task_fn, NULL);

static void send_standard_erm_response()
{
    s_server.set_response_code_P(PSTR("200 OK"));
    s_server.set_header_P(PSTR("Access-Control-Allow-Origin"), PSTR("*"));
    s_server.finish_headers();
}

static void get_cabinet_status(char const * const url)
{
    if (url)
    {
        send_standard_erm_response();
    }
    s_server.add_body_P(s_pDevices->pHandleSenseInput->state() ? PSTR("OPEN\r\n\r\n") : PSTR("CLOSED\r\n\r\n"));
}

static void lock_door(char const * const url)
{
    raat_logln_P(LOG_APP, PSTR("Locking door"));
    if (url)
    {
        send_standard_erm_response();
    }
    s_pDevices->pServoPower->set(true);
    s_pDevices->pLockingServo->set(0);
    s_depower_servo_task.start();
}

static void unlock_door(char const * const url)
{
    raat_logln_P(LOG_APP, PSTR("Unlocking door"));
    if (url)
    {
        send_standard_erm_response();
    }
    s_pDevices->pServoPower->set(true);
    s_pDevices->pLockingServo->set(90);
    s_depower_servo_task.start();
}

static void release_back(char const * const url)
{
    raat_logln_P(LOG_APP, PSTR("Releasing back"));
    if (url)
    {
        send_standard_erm_response();
    }
    s_pDevices->pMaglock1->set(false);
}

static void release_pigeon(char const * const url)
{
    raat_logln_P(LOG_APP, PSTR("Releasing pigeon"));
    if (url)
    {
        send_standard_erm_response();
    }
    s_pDevices->pMaglock2->set(false);
}

static const char CABINET_STATUS_URL[] PROGMEM = "/cabinet/status";
static const char CABINET_LOCK_DOOR_URL[] PROGMEM = "/cabinet/door/lock";
static const char CABINET_UNLOCK_DOOR_URL[] PROGMEM = "/cabinet/door/unlock";
static const char CABINET_RELEASE_BACK_URL[] PROGMEM = "/cabinet/back/release";
static const char PIGEON_RELEASE_URL[] PROGMEM = "/pigeon/release";

static http_get_handler s_handlers[] = 
{
    {CABINET_STATUS_URL, get_cabinet_status},
    {CABINET_LOCK_DOOR_URL, lock_door},
    {CABINET_UNLOCK_DOOR_URL, unlock_door},
    {CABINET_RELEASE_BACK_URL, release_back},
    {PIGEON_RELEASE_URL, release_pigeon},
    {"", NULL}
};

void ethernet_packet_handler(char * req)
{
    s_server.handle_req(s_handlers, req);
}

char * ethernet_response_provider()
{
    return s_server.get_response();
}

static void unlock_back_delay_fn(RAATOneShotTask& this_task, void * pTaskData)
{
    (void)this_task; (void)pTaskData;
    release_back(NULL);
}
static RAATOneShotTask s_unlock_back_delay_task(3000, unlock_back_delay_fn, NULL);

void raat_custom_setup(const raat_devices_struct& devices, const raat_params_struct& params)
{
    (void)params;

    s_pDevices = &devices;
}

void raat_custom_loop(const raat_devices_struct& devices, const raat_params_struct& params)
{
    (void)params;
    static bool s_locked_once = false;

    if (devices.pHandleSenseInput->check_low_and_clear())
    {
        lock_door(NULL);
        if (s_locked_once)
        {
            s_unlock_back_delay_task.start();
        }
        else
        {
            s_locked_once = true;
        }
    }
    else if (devices.pHandleSenseInput->check_high_and_clear())
    {
        unlock_door(NULL);   
    }

    s_depower_servo_task.run();
    s_unlock_back_delay_task.run();
}
