#ifndef _BROKER_MANAGER_H
#define _BROKER_MANAGER_H

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
#include "mqtt_conf.h"

// Defined this way because of circular dependency
typedef struct connection_t connection_t;
typedef struct subscription_t subscription_t;

// A connection as seen by the manager (information about client and list of its subscriptions)
struct connection_t {
    cli_to_man_connect_t *client_info;
    subscription_t *client_subscription_list[MAX_NUM_TOPICS];
};

// A subscription on its own. Has a pointer to the name of the subscription topic, a pointer to the client how subscribed
// and a mutex (because these resources are manipulated by several parties)
struct subscription_t {
    connection_t *subscriber_info;
    char *topic_name; // Reference to the string present on subscribable_topic_t
};

// An array of subscriptions on the same topic (working as "indexes").
typedef struct {
    char *topic_name; // String with the name of the topic. It owns the string
    subscription_t subscription_list[MAX_NUM_CLIENTS];
} subscribable_topic_t;

// Messages passed from "client's" threads to a manager thread
extern message_queue_t *manager_message_queue;
extern pthread_mutex_t *manager_message_queue_lock;
extern pthread_cond_t *message_size_cond;

void *manager_routine(void *arg);

#endif