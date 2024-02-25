#ifndef MQTT_INITIALIZER_H
#define MQTT_INITIALIZER_H


int mqtt_initialize(MQTTClient* ,MQTTClient_connectOptions*);

int mqtt_subscribe(MQTTClient*, const char* topic);

void mqtt_cleanup(MQTTClient*);


#endif
