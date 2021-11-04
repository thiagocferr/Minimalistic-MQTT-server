#ifndef _BROKER_CLIENT_H
#define _BROKER_CLIENT_H

#include <unistd.h>

#include <stdint.h>
#include <poll.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "misc.h"
#include "mqtt_connect.h"
#include "mqtt_connack.h"
#include "mqtt_subscribe.h"
#include "mqtt_suback.h"
#include "mqtt_publish.h"
#include "mqtt_ping.h"
#include "mqtt_disconnect.h"

#include "message_queue.h"
#include "packet_queue.h"
#include "mqtt_conf.h"

#include "mqtt.h"
#include "broker_manager.h"

void *client_connection_routine(void *arg);

#endif