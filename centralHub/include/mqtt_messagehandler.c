#include <MQTTClient.h>
#include <cJSON.h>
#include <string.h>
#include "db_handler.h"
#include "shellyplugs.h"
#include "trv.h"
#include "motion.h"
#include "hts2.h"
#include "shellyPlus1Pm.h"
#include "shellyPlus2Pm.h"

/*
 *
 * the mqtt_messagehandler.c has the duty to classify the messages coming from 
   the devices or mobile app using their topic names. After filtering it will
   send the messages to the db_handler.c to save the in the sqlite database
 *
 *
*/

//This function handles the incoming messages from the devices or mobile app
// by checking their topic.
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
   // printf("Message arrived\n");
   // printf("     topic: %s\n", topicName);
   // printf("   message: %.*s\n", message->payloadlen, (char*)message->payload);




    // check if shellyplug-s is available in the topic's message
    if(strstr(topicName,"shellies") != NULL){

    if(strstr(topicName,"shellytrv") != NULL ){
	
	trv_handle(topicName , (char*)message->payload);
	
     }
     else if(strstr(topicName,"shellymotion2") != NULL ){

	  //   motion_handle(topicName,(char*)message->payload);

     }

      else if(strstr(topicName,"shellyplug-s") != NULL ){

	     //shellyplugs_handle(topicName,(char*)message->payload);

     }

}else if(strstr(topicName, "shellyplusht") != NULL ){

//	hts2_handle(topicName, (char*)message->payload);

}
else if(strstr(topicName, "shellyplus1pm")!=NULL){

    //shellyPlus1Pm_handle(topicName, (char*)message->payload);

}
else if(strstr(topicName, "shellyplus2pm")!=NULL){

    //shellyPlus2Pm_handle(topicName, (char*)message->payload);


}






    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);    
    return 1;
}


void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    if (cause)
        printf("     cause: %s\n", cause);
}
