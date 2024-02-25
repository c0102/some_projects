#ifndef MQTT_MESSAGEHANDLER_H
#define MQTT_MESSAGEHANDLER_H

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void connlost(void *context, char *cause);

#endif
