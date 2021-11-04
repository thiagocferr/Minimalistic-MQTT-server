
#include "broker_client.h"

static void sendMessageToManager(message_t *msg) {
    pthread_mutex_lock(manager_message_queue_lock);
    enqueueMessage(manager_message_queue, msg);
    if (manager_message_queue->size == 1) {
        pthread_cond_signal(message_size_cond);
    }
    pthread_mutex_unlock(manager_message_queue_lock);
}

static void sendConnectToManager(connection_request_t *connect_struct, char *client_id,
message_queue_t *client_message_queue, pthread_mutex_t *client_message_queue_lock, int *client_alert_pipe) {

    message_t *msg = malloc(sizeof(message_t));
    cli_to_man_connect_t *conn = malloc(sizeof(cli_to_man_connect_t));

    inititializeMessageQueue(client_message_queue);

    // Filling messages
    conn->client_id = client_id;
    conn->client_message_queue = client_message_queue;
    conn->client_message_queue_lock = client_message_queue_lock;
    conn->client_alert_pipe = client_alert_pipe;

    pthread_mutex_init(conn->client_message_queue_lock, NULL);

    msg->message_type = CONNECT;
    msg->info = conn;
    msg->pigback_struct = connect_struct;

    // Sending message
    sendMessageToManager(msg);
}

static void sendSubscribeToManager(subscribe_request_t *sub_struct, char *client_id) {
    message_t *msg = malloc(sizeof(message_t));
    cli_to_man_sub_t *sub = malloc(sizeof(cli_to_man_sub_t));

    // Filling message
    sub->client_id = client_id;
    sub->topics = malloc(sub_struct->num_of_topics * sizeof (char*));

    for (int i = 0; i < (int) sub_struct->num_of_topics; i++) {
        sub->topics[i] = sub_struct->topics[i].topic_name;
    }
    sub->num_of_topics = sub_struct->num_of_topics;

    msg->message_type = SUBSCRIBE;
    msg->info = sub;
    msg->pigback_struct = sub_struct;

    // Sending message
    sendMessageToManager(msg);
}

static void sendPublishToManager(publish_request_t *pub_struct, uint8_t *packet) {
    message_t *msg = malloc(sizeof(message_t));
    cli_to_man_pub_t *pub = malloc(sizeof(cli_to_man_pub_t));

    // Filling message
    pub->topic = pub_struct->topic_name;
    pub->publish_packet_size = pub_struct->package_size;
    pub->publish_packet = packet;

    msg->message_type = PUBLISH;
    msg->info = pub;
    msg->pigback_struct = pub_struct;

    // Sending message
    sendMessageToManager(msg);
}

static void sendDisconnectToManager(char *client_id) {
    message_t *msg = malloc(sizeof(message_t));
    cli_to_man_disconnect_t *disc = malloc(sizeof(cli_to_man_disconnect_t));

    // Filling message
    disc->client_id = client_id;

    msg->message_type = DISCONNECT;
    msg->info = disc;
    msg->pigback_struct = NULL;

    // Sending message
    sendMessageToManager(msg);
}

static packet_queue_t *getPacket(int socket, int *size) {

    // Reading packet(s) from socket (non-blocking)
    int first_read = 1;
    int total_bytes_read = 0;
    int total_bytes = 0;
    int n = 0;
    packet_queue_t *packet_queue = malloc(sizeof(packet_queue_t));
    inititializePacketQueue(packet_queue);
    uint8_t *buf = malloc(MAX_PACKAGE_SIZE);

    do {
        // NOTE: This read opeartion can, in fact, return more than one packet at the time (for example,
        // usually a PUBLISH packet is followed by a DISCONNECT one from mosquitto_pub client). If this happens,
        // we split this packet into multiple ones and interpret than one after the other
        int i = read(socket, buf + total_bytes_read, MAX_PACKAGE_SIZE);
        if (i < 0) {
            if ((errno == EAGAIN || errno == EWOULDBLOCK) && total_bytes != 0)
                continue;
            else if ((errno == EAGAIN || errno == EWOULDBLOCK) && total_bytes == 0) {
                n = -2; // There was never anything to read!
                break;
            }
            else {
                n = -1; // Some other error occured!
                break;
            }
        }

        // End of file. Socket has been closed
        if (i == 0) {
            n = 0;
            break;
        }

        if (first_read) {
            // Packet size is stored starting at second byte of packet
            total_bytes = decodeVariableByteIntVal(&buf[1]) + 1 + variableByteIntSizeFromMessage(&buf[1]);
            first_read = 0;
        }

        total_bytes_read += i;
        n += i;

        // If there's more bytes read than the predicted total number of bytes, it means another packet has
        // come with the previous packet. If that's the case, increase the total number of bytes predicted to include
        // the other packet (but only if we at least two bytes from other packet, so we can get other packet's size from
        // fixed header)
        if (total_bytes_read >= total_bytes + 2) {
            total_bytes += 1 + decodeVariableByteIntVal(&buf[total_bytes + 1]) + variableByteIntSizeFromMessage(&buf[total_bytes + 1]);
        }
    } while (total_bytes_read != total_bytes);

    // Store packet size on argument
    *size = n;
    // Avoiding a large consumption of unecessary memory for long
    if (n < 0) {
        free(buf);
        return packet_queue;
    }
    else if (total_bytes_read < MAX_PACKAGE_SIZE) {
        buf = (uint8_t *) realloc(buf, total_bytes_read);
    }

    for (int i = 0; i < total_bytes_read;) {
        int packet_size = 1 + decodeVariableByteIntVal(&buf[i + 1])
            + variableByteIntSizeFromMessage(&buf[i + 1]);

        uint8_t *packet = malloc(packet_size);
        memcpy(packet, buf + i, packet_size);
        packet_t *packet_struct = malloc(sizeof(packet_t));
        packet_struct->packet = packet;
        packet_struct->packet_size = packet_size;

        enqueuePacket(packet_queue, packet_struct);
        i += packet_size;
    }

    // As messages are allocated on queue, free buffer
    free(buf);

    return packet_queue;
}

static void writePacket(int socket, uint8_t *packet, size_t size) {

    struct pollfd pfds[1];
    int pos = 0;
    int n = 0;

    pfds[0].fd = socket;
    pfds[0].events = POLLOUT;

    while (1) {
        poll(pfds, 1, -1);

        if (pfds[0].revents & POLLOUT) {
            n = write(socket, packet + pos, size - pos);
            if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                continue;

            pos += n;

            // If there's nothing more to write, end loop
            if (size - pos == 0)
                break;
        }
        // If there's an error, exit this loop
        else if (pfds[0].revents & POLLERR) {
            break;
        }
    }
}

static void resolveConnectionInnexistance(char *client_id, int socket) {
    // If there's no client id, no connection was first stablished, so send DISCONNECT
    // to client
    if (client_id == NULL) {
        int tmp = 3; // It's known that the packet is 3 bytes long
        uint8_t *subtle_disconnect = getDisconnectFromCode(0x82, &tmp); // Protocol Error
        writePacket(socket, subtle_disconnect, tmp);
    }
}

void *client_connection_routine(void *arg) {

    int connfd = *((int *)arg);
    // client_alert_pipe[0] is for reading, client_alert_pipe[1], for writing
    int client_alert_pipe[2];

    if (pipe(client_alert_pipe) == -1) {
        perror("Creation of alert pipe");
        exit(7);
    }

    // Making pipe non-blocking
    if (fcntl(client_alert_pipe[1], F_SETFL, fcntl(client_alert_pipe[1], F_GETFL) | O_NONBLOCK) == -1) {
        perror("Change of pipe write side to non blocking");
        exit(8);
    }

    if (fcntl(client_alert_pipe[0], F_SETFL, fcntl(client_alert_pipe[0], F_GETFL) | O_NONBLOCK) == -1) {
        perror("Change of pipe read side to non blocking");
        exit(8);
    }

    // Making socket non-blocking for avoiding spurious readiness of poll() and subsequenctial possibly being block on an
    // non-existant packet from socket (see man page of select() for description of the bug)
    if (fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL) | O_NONBLOCK) == -1) {
        perror("Change of socket to non blocking");
        exit(8);
    }

    // Each thread will only have 2 associated files: the client socket and ALERT pipe
    int thread_nfds = 2;
    struct pollfd *pfds;
    if ((pfds = calloc(thread_nfds, sizeof(struct pollfd))) == NULL) {
        perror("Malloc array of poll file descriptors");
        exit(9);
    }

    pfds[0].fd = connfd;
    pfds[0].events = POLLIN;
    pfds[1].fd = client_alert_pipe[0];
    pfds[1].events = POLLIN;

    // Where client thread receives messages from manager thread
    message_queue_t *client_message_queue = malloc(sizeof(message_queue_t));
    pthread_mutex_t *client_message_queue_lock = malloc(sizeof(pthread_mutex_t));

    // Where client thread can see its connection properties (on manager thread)
    //connection_t *client_connection = NULL;
    char *client_id = NULL;

    // To avoid bigger crises because of read int NULL, allocate a one byte memory block to place pipe alert received
    char *alert_dump = malloc(1 * sizeof(char));

    // When we are preparing to disconnect, make special treat to incoming messages from manager (basically dumping the responses)
    int dumpMode = 0;

    for (;;) {

        if (poll(pfds, thread_nfds, -1) == -1) {
            perror("Malloc array of poll file descriptors");
            break;
        }

        // To avoind doing busy-waiting while things are closed on the manager side (this would
        // keep catching the POLLHUP event), just do polling at the alert pipe
        if ((pfds[0].revents & POLLHUP || pfds[0].revents & POLLNVAL) && thread_nfds == 2) {
            free(pfds);
            thread_nfds = 1;
            pfds = calloc(thread_nfds, sizeof(struct pollfd));
            pfds[0].fd = client_alert_pipe[0];
            pfds[0].events = POLLIN;
        }

        // PACKETS COMING FROM SOCKET
        if (pfds[0].revents & POLLIN && thread_nfds == 2) {

            int read_buf_size;
            packet_queue_t *packet_queue = getPacket(connfd, &read_buf_size);
            if (read_buf_size == 0) {
                close(connfd);
                if (dumpMode == 0) { // Disconnect without DISCONNECT packet
                    if (client_id != NULL) {
                        sendDisconnectToManager(client_id);
                    } else {
                       break;
                    }
                    dumpMode = 1;
                }

                if (packet_queue->size == 0) {
                    free(packet_queue);
                    continue;
                }

            } else if (read_buf_size < 0) {
                continue;
            }

            while (packet_queue->size > 0) {
                packet_t *packet_struct = dequeuePacket(packet_queue);
                uint8_t *packet = packet_struct->packet;
                size_t packet_size = packet_struct->packet_size;
                free(packet_struct);

                message_type_t packet_type = getMessageType(packet);

                // NOTE: Check for duplicate connection tries?
                if (packet_type == CONNECT) {
                    connection_request_t *connect_struct = getConnectionFromMessage(packet, packet_size);

                    // Because struct 'connect_struct' needs to have its 'client_id' field with a non-null-terminated string
                    // and we need a null-terminated one for internal operations, do operation below
                    client_id = malloc((connect_struct->client_id_len + 1) * sizeof (char));
                    memcpy(client_id, connect_struct->client_id, connect_struct->client_id_len);
                    client_id[connect_struct->client_id_len] = '\0';

                    sendConnectToManager(connect_struct, client_id, client_message_queue,
                        client_message_queue_lock, &client_alert_pipe[1]);

                    free(packet);
                }

                // NOTE: FOR NOW TREATING ONLY CASE OF A SINGLE SUBSCRIBE TOPIC ON THE PACKET
                else if (packet_type == SUBSCRIBE) {
                    resolveConnectionInnexistance(client_id, connfd);

                    subscribe_request_t *sub_struct = getSubscribeFromMessage(packet, packet_size);
                    sendSubscribeToManager(sub_struct, client_id);
                    free(packet);
                }

                else if (packet_type == PUBLISH) {
                    resolveConnectionInnexistance(client_id, connfd);

                    publish_request_t *pub_struct = getPublishFromMessage(packet, packet_size);
                    sendPublishToManager(pub_struct, packet);
                    //! MUST NOT FREE PACKET, AS IT WILL BE SENT TO OTHER CLIENT THREADS
                }

                else if (packet_type == PINGREQ) {
                    transformPing(packet);
                    write(connfd, packet, packet_size);
                    free(packet);
                }

                else if (packet_type == DISCONNECT) {
                    sendDisconnectToManager(client_id);
                    // Cut reference here, while manager clean things on his side
                    dumpMode = 1;
                    free(packet);
                }
            }
        }

        // RESPONSES FROM MANAGER (on client_message_queue)
        if (pfds[1].revents & POLLIN) {

            // If there's no actual message available, just return polling
            if (read(client_alert_pipe[0], alert_dump, 1) == -1) {
                continue;
            }
            if (client_message_queue->size == 0) {
                continue;
            }

            pthread_mutex_lock(client_message_queue_lock);
            message_t *msg = dequeueMessage(client_message_queue);
            pthread_mutex_unlock(client_message_queue_lock);

            message_type_t msg_type = msg->message_type;

            if (msg_type == CONNACK) {

                if (dumpMode) {
                    freeConnectMessageStruct((connection_request_t *) msg->pigback_struct);
                } else {

                    // Acknowledging connection
                    size_t socket_message_size;
                    uint8_t *socket_message = generateMessageFromConnection((connection_request_t *) msg->pigback_struct, &socket_message_size);

                    writePacket(connfd, socket_message, socket_message_size);

                    free(socket_message);
                    freeConnectMessageStruct((connection_request_t *) msg->pigback_struct);
                }
            }

            else if (msg_type == SUBACK) {

                if (!dumpMode) {
                    // If some reason code was decided during the manager processing, change
                    // subscribe_request_t reason code to this new one
                    uint8_t *codes = ((man_to_cli_suback_t *)msg->info)->new_reason_codes;
                    int num_of_topics = ((man_to_cli_suback_t *)msg->info)->num_of_topics;

                    for (int i = 0; i < num_of_topics; i++) {
                        if (codes[i] != 0) {
                            ((subscribe_request_t *) msg->pigback_struct)->topics[i].reason_code = codes[i];
                        }
                    }

                    size_t socket_message_size;
                    uint8_t *socket_message = generateMessageFromSub((subscribe_request_t *) msg->pigback_struct, &socket_message_size);

                    writePacket(connfd, socket_message, socket_message_size);

                    free(socket_message);
                }

                free(((man_to_cli_suback_t *) msg->info)->new_reason_codes);
                free((man_to_cli_suback_t *) msg->info);
                freeSubscribeMessageStruct((subscribe_request_t *) msg->pigback_struct);
            }

            else if (msg_type == PUBLISH) {

                man_to_cli_pub_t *received_pub = (man_to_cli_pub_t *) msg->info;
                if (!dumpMode) {
                    writePacket(connfd, received_pub->publish_packet, received_pub->packet_size);
                }

                // The last thread to finish sending packet should free packet and destry mutex
                int isLastClient = 0;
                pthread_mutex_lock(&received_pub->counter_lock);

                received_pub->counter--;
                if (received_pub->counter == 0)
                    isLastClient = 1;

                pthread_mutex_unlock(&received_pub->counter_lock);

                if (isLastClient) {

                    free(received_pub->publish_packet);
                    pthread_mutex_destroy(&received_pub->counter_lock);

                    free((man_to_cli_pub_t *) msg->info);
                    free((publish_request_t *) msg->pigback_struct);
                }
            }

            else if (msg_type == DISCONNECT) {
                free(client_id);
                break;
            }
        }
    }

    free(client_message_queue);
    pthread_mutex_destroy(client_message_queue_lock);
    free(client_message_queue_lock);
    free(alert_dump);
    free(pfds);

    close(client_alert_pipe[0]);
    close(client_alert_pipe[1]);

    pthread_exit(NULL);

}