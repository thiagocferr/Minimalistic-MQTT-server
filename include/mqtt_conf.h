#ifndef _MQTT_CONF_H
#define _MQTT_CONF_H

#define MAX_MESSAGE_QUEUE_SIZE 1000

#define MAX_NUM_CLIENTS 100

#define MAX_NUM_TOPICS 10

/**
 * Maximum size of a client ID
 */
#define MAX_CLIENT_ID_SIZE 40

/**
 * Maximum number of user properties on CONNECT and CONNACK messages
 */
//#define MAX_NUM_USER_PROPERTIES 50

/**
 * File with some Server configuration macros. Uncomment a macro if you which to set it
 */

// Maximum number of characters on a string
#define MAX_STR_SIZE 65535

/**
 * The maximum size, per MQTT specification, is 128^4 bytes (approximately 256 MB). Other values
 * can be set here (they must be lower, however)
 */
#define MAX_PACKAGE_SIZE 128*128*128*128

/**
 * Maximum level of QoS supported by server. Used on the CONNACK response.
 */
#define MAX_QOS 0

#endif