
#include "broker_manager.h"

message_queue_t *manager_message_queue;
pthread_mutex_t *manager_message_queue_lock;
pthread_cond_t *message_size_cond;

static void initializeManagerTopicsArray(subscribable_topic_t topic_list[MAX_NUM_TOPICS]) {
    for (int i = 0; i < MAX_NUM_TOPICS; i++) {
        topic_list[i].topic_name = NULL;
        for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
            topic_list[i].subscription_list[j].subscriber_info = NULL;
            topic_list[i].subscription_list[j].topic_name = NULL;
        }
    }
}

static void initializeManagerConnectionsArray(connection_t connection_list[MAX_NUM_CLIENTS]) {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        connection_list[i].client_info = NULL;
        for (int j = 0; j < MAX_NUM_TOPICS; j++) {
            connection_list[i].client_subscription_list[j] = NULL;
        }
    }
}


static int findFreeConnectionListIndex(connection_t connection_list[MAX_NUM_CLIENTS]) {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++)
        if (connection_list[i].client_info == NULL)
            return i;
    return -1;
}

static int findConnectionListIndexByID(connection_t connection_list[MAX_NUM_CLIENTS], char *id) {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        if (connection_list[i].client_info != NULL) {
            char *connection_id = connection_list[i].client_info->client_id;
            if (!strcmp(connection_id, id)) {
                return i;
            }
        }
    }
    return -1;
}

// Given a string, find a topic array index on the subscription matrix
static int findTopicListIndexByName(subscribable_topic_t topic_list[MAX_NUM_TOPICS], char *string) {
    for (int i = 0; i < MAX_NUM_TOPICS; i++) {
        char *topic_name = topic_list[i].topic_name;

        if (topic_list[i].topic_name != NULL && !strcmp(topic_name, string)) {
            return i;
        }
    }
    return -1;
}

static int findFreeTopicListIndex(subscribable_topic_t topic_list[MAX_NUM_TOPICS]) {
    for (int i = 0; i < MAX_NUM_TOPICS; i++)
        if  (topic_list[i].topic_name == NULL)
            return i;
    return -1;
}

// If a connection (client) has already subscribed to topic, return pointer to this connection.
//  Else, return a NULL pointer
static subscription_t *findAlreadySubscribed(connection_t *connection, char *topic) {
    for (int i = 0; i < MAX_NUM_TOPICS; i++) {
        subscription_t *result = connection->client_subscription_list[i];
        if (result != NULL && result->topic_name != NULL && (!strcmp(result->topic_name, topic))) {
            return result;
        }
    }
    return NULL;
}

static int findFreeSubscriptionListIndex(subscription_t subscription_list[MAX_NUM_CLIENTS]) {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++)
        if (subscription_list[i].subscriber_info == NULL)
            return i;
    return -1;
}

static int getNumOfSubscribers(subscription_t subscription_list[MAX_NUM_CLIENTS]) {
    int counter = 0;
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        if (subscription_list[i].subscriber_info != NULL) {
            counter++;
        }
    }
    return counter;
}

static void sendMessageToClient(connection_t *client_connection, message_type_t message_type, void *info, void *pigback_info) {

    message_t *message = malloc(sizeof(message_t));
    message->message_type = message_type;
    message->info = info;
    message->pigback_struct = pigback_info;

    pthread_mutex_t *client_message_queue_lock = client_connection->client_info->client_message_queue_lock;
    message_queue_t *client_message_queue = client_connection->client_info->client_message_queue;
    int *alert_pipe = client_connection->client_info->client_alert_pipe;

    pthread_mutex_lock(client_message_queue_lock);
    enqueueMessage(client_message_queue, message);
    pthread_mutex_unlock(client_message_queue_lock);

    uint8_t byte = 1;
    write(*alert_pipe, &byte, 1);
}

void *manager_routine(void *arg) {

    subscribable_topic_t topic_list[MAX_NUM_TOPICS];
    connection_t connection_list[MAX_NUM_CLIENTS];
    initializeManagerTopicsArray(topic_list);
    initializeManagerConnectionsArray(connection_list);

    while (1) {
        // ! Don't forget to free the message at the end!
        message_t *message;

        pthread_mutex_lock(manager_message_queue_lock);
        // Wait until there's some message available
        while(manager_message_queue->size == 0) {
            pthread_cond_wait(message_size_cond, manager_message_queue_lock);
        }
        message = dequeueMessage(manager_message_queue);
        pthread_mutex_unlock(manager_message_queue_lock);

        if (message->message_type == CONNECT) {

            // Getting info and making changes
            cli_to_man_connect_t *client_info = (cli_to_man_connect_t *) message->info;
            connection_t *client_conn;

            void *pigback_struct = message->pigback_struct;

            int i = findFreeConnectionListIndex(connection_list);

            if (i == -1) {
                // If there's no more space, put error code 0x9F (Connection rate exceeded) on pigback struct
                ((connection_request_t *) message->pigback_struct)->reason_code = 0x9F;
                connection_t tmp = { .client_info = client_info };
                sendMessageToClient(&tmp, CONNACK, NULL, pigback_struct);

            } else {
                client_conn = &connection_list[i];
                // OBS: you can see that client_conn->client_info is just referencing memory allocated on the client thread side
                // but since there's no way to acess it from the client thread and each field has been malloced there, there's no
                // danger in freeing the struct itself, which is done when receiving DISCONNECT message
                client_conn->client_info = client_info;
                sendMessageToClient(client_conn, CONNACK, NULL, pigback_struct);
            }
            //! ATTENTION: Do not free the message->info, since its going to be used until client disconnection time
        }

        else if (message->message_type == SUBSCRIBE) {

            // Connection ID and its corresponding entry on connection list
            char *connection_id = ((cli_to_man_sub_t *) message->info)->client_id;
            int connection_index = findConnectionListIndexByID(connection_list, connection_id);

            connection_t* client_conn = &connection_list[connection_index];

            char **topics = ((cli_to_man_sub_t *) message->info)->topics;
            int num_of_topics = ((cli_to_man_sub_t *) message->info)->num_of_topics;
            uint8_t *new_reason_codes = malloc(num_of_topics * sizeof(uint8_t));

            // Loop through every subscribe request
            for (int i = 0; i < num_of_topics; i++) {
                char *topic_name = topics[i];

                // Tying to find topic on topic_list
                int n;
                new_reason_codes[i] = 0;
                if ((n = findTopicListIndexByName(topic_list, topic_name)) == -1) {
                    if ((n = findFreeTopicListIndex(topic_list)) != -1) {
                        topic_list[n].topic_name = topic_name;
                    } else {
                        new_reason_codes[i] = 0x97; // Quota exceeded
                        free(topic_name); // Cannot register subscription, so drop name
                    }
                } else {
                    free(topic_name); // Topic already exists
                }

                // Finding a specific subscription that the client can occupy
                subscription_t *subscription;
                char *topic;
                int topic_index;
                if (n != -1) {

                    topic = topic_list[n].topic_name;
                    topic_index = n;

                    // Trying to find suitable position for new subscription
                    subscription_t *subscription_list = topic_list[n].subscription_list;

                    // First check if client has alread subscribed to topic. If not, try finding a free
                    // space on the array of subscriptions for that topic. In the end
                    if ((subscription = findAlreadySubscribed(client_conn, topic)) != NULL) {
                        n = -1; // We don't need to set things on the connection structure because it already has this subscripton
                    } else if ((n = findFreeSubscriptionListIndex(subscription_list)) != -1) {
                        subscription = &subscription_list[n];
                        subscription->topic_name = topic_list[topic_index].topic_name;
                    } else {
                        n = -1;
                        subscription = NULL;
                        new_reason_codes[i] = 0x97; // Quota exceeded
                    }
                }

                // Put subscription reference into connection struct, if subscription is new. If subscription already exists
                // do nothing
                if (n != -1 && subscription->subscriber_info == NULL) {
                    subscription->subscriber_info = client_conn;

                    int found = 0;
                    for (int j = 0; i < MAX_NUM_TOPICS; i++) {
                        if (client_conn->client_subscription_list[j] == NULL) {
                            client_conn->client_subscription_list[j] = subscription;
                            found = 1;
                            break;
                        }
                    }
                    if (!found)
                        new_reason_codes[i] = 0x97;
                }
            }

            // Adjusting message
            void *pigback_struct = message->pigback_struct;
            man_to_cli_suback_t *info = malloc(sizeof(man_to_cli_suback_t)); // MALLOCED
            info->new_reason_codes = new_reason_codes;
            info->num_of_topics = num_of_topics;

            sendMessageToClient(client_conn, SUBACK, info, pigback_struct);

            free(message->info);
        }

        else if (message->message_type == PUBLISH) {

            // Topic to publish to and packet to distribute
            char *topic_name = ((cli_to_man_pub_t *) message->info)->topic;
            uint8_t *publish_packet = ((cli_to_man_pub_t *) message->info)->publish_packet;
            size_t packet_size = ((cli_to_man_pub_t *) message->info)->publish_packet_size;

            // Find list of subscriptions on this topic
            int n;
            if ((n = findTopicListIndexByName(topic_list, topic_name)) != -1) {

                // Using the subscription stored on the topic struct, search the current number of subscriptions
                subscription_t *send_to_sub_list = topic_list[n].subscription_list;
                int total_subscribers = getNumOfSubscribers(send_to_sub_list);

                if (total_subscribers > 0) {

                    // Build message
                    man_to_cli_pub_t *publish_info = malloc(sizeof(man_to_cli_pub_t));
                    publish_info->publish_packet = publish_packet;
                    publish_info->packet_size = packet_size;
                    publish_info->counter = total_subscribers;
                    pthread_mutex_init(&publish_info->counter_lock, NULL);

                    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
                        if (send_to_sub_list[i].subscriber_info != NULL) {
                            connection_t *client_conn = send_to_sub_list[i].subscriber_info;
                            sendMessageToClient(client_conn, PUBLISH, publish_info, NULL);
                        }
                    }
                }

            free(message->info);
            }
        }

        else if (message->message_type == DISCONNECT) {

            // Connection ID and its corresponding entry on connection list
            char *connection_id = ((cli_to_man_sub_t *) message->info)->client_id;
            int connection_index = findConnectionListIndexByID(connection_list, connection_id);
            connection_t* client_conn = &connection_list[connection_index];

            for (int i = 0; i < MAX_NUM_TOPICS; i++) {
                if (client_conn->client_subscription_list[i] != NULL) {

                    client_conn->client_subscription_list[i]->subscriber_info = NULL;

                    // If after that, the topic that holds the subscription has no more subscriptions
                    // (all fields 'subscriber_info' are NULL), remove topic identification (free its name)
                    // so that other topics can be generated
                    int n;
                    char *topic_unsub = client_conn->client_subscription_list[i]->topic_name;

                    if ((n = findTopicListIndexByName(topic_list, topic_unsub)) != -1) {
                        if (getNumOfSubscribers(topic_list[n].subscription_list) == 0) {
                            free(topic_list[n].topic_name);
                            topic_list[n].topic_name = NULL;

                        }
                    }
                }
                client_conn->client_subscription_list[i] = NULL;

            }

            sendMessageToClient(client_conn, DISCONNECT, NULL, NULL);

            // As all information contained on this struct is a reference to a part of the client's stack,
            // we can free the struct itself here and let the internal parameters be naturally freed from stack
            free(client_conn->client_info);
            client_conn->client_info = NULL;
        }

        free(message);
    }
    pthread_exit(NULL);
}