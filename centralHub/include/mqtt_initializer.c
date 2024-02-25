#include <stdio.h>
#include <stdlib.h>
#include <MQTTClient.h>
#include <string.h>
#include "mqtt_initializer.h"
#include "mqtt_messagehandler.h"


#define ADDRESS     "127.0.0.1:1883"
#define CLIENTID    "CentralHub"
#define QOS         1
#define TIMEOUT     10000L


int mqtt_initialize(MQTTClient* client, MQTTClient_connectOptions* conn_opts) {

    MQTTClient_create(client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    MQTTClient_setCallbacks(*client, NULL, connlost, msgarrvd, NULL);

    conn_opts->keepAliveInterval = 20;
    conn_opts->cleansession = 1;

    if (MQTTClient_connect(*client, conn_opts) != MQTTCLIENT_SUCCESS)
{
        return -1;

}

    return 0;
}


int mqtt_subscribe(MQTTClient* client, const char* topic ){

return MQTTClient_subscribe(*client, topic, 1);

}


void mqtt_cleanup(MQTTClient* client) {
    MQTTClient_disconnect(*client, TIMEOUT);
    MQTTClient_destroy(client);
}
