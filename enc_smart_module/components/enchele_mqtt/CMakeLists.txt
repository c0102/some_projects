idf_component_register(SRCS "mqtt.c"
                    INCLUDE_DIRS "include"
                    REQUIRES enchele_app_helpers message_parsing mqtt
                    EMBED_TXTFILES certs/cacert.pem certs/clientcert.crt certs/privatekey.key)
                    
