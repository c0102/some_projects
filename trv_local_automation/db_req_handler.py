import dbat
import sqlite3
import json
import threading
import paho.mqtt.client as mqtt
import heat_automation
import time
import serve
import deviceConfig
stop_flag = threading.Event()

# Here are the topics we'll need to subscribe to.-
#  We'll need to also add a file in which new topics will be written-
# and read. 

listForPack = [] # for ht sensor

# In this list we will store the data that will be stored in the data base
listDataBuffer = []

# This is the function that runs in a thread parallely 
def bufferMessagesforDb(client):
     while not stop_flag.is_set():
        # If the list has data available
        if listDataBuffer:
            # we send the data to the handler, which will filter the message and data acording to it's type
            handleMessagesForDb(client, listDataBuffer.pop())

def heatingAutomation(client):
    while not stop_flag.is_set():
        connection = sqlite3.connect('smart_home.db')
        heat_automation.heat_automation_process(connection, client)
        connection.close()
        time.sleep(360)


# This is the function that will filter out and direct the data where it should go.
def handleMessagesForDb(client, incomming_message):

    # Checking if we're getting the data in a JSON format
    jsonFormat = False
    data = incomming_message
    try:
        data = json.loads(incomming_message)
        jsonFormat = True
    except:
        print("NOT JSON")

# According to the type of the operation read from the JSON the data is saved in the tables where the data belongs.
    if jsonFormat:
        response = {}
        result = False
        if(data['operation'] == "addRoom"):
            connection = sqlite3.connect('smart_home.db')
            result = dbat.insertDataRoomTable(connection, data['type'], data['name'])
            connection.close()

            if result:
                response={
                    "operation":data['operation'],
                    "status":result,
                    "name":f"{data['name']}",
                    "type":f"{data['type']}",
                }
            
            else:
                response={
                    "operation":data['operation'],
                    "status":result,
                    "name":f"{data['name']}",
                    "type":f"{data['type']}",
                    # "info":f"The {data['name']} already exists"
                }
        
            response = json.dumps(response)
            client.publish("hub01/response", response)

            print(result)
            print("finish")

            

        elif(data['operation'] == "giveRooms"):
            connection = sqlite3.connect('smart_home.db')
            yourRooms = serve.serverRooms(connection=connection)
            for rooms in yourRooms:
                client.publish("hub01/response", rooms)
        
        elif(data['operation'] == "giveDevices"):
            
            connection = sqlite3.connect('smart_home.db')
            roomId = dbat.fetchRoomId(connection=connection,roomName = data['roomName'])
            connection = sqlite3.connect('smart_home.db')
            yourRooms = serve.serveDevices(connection=connection, roomId=roomId)
            for rooms in yourRooms:
                client.publish("hub01/response", rooms)
        
        


        elif(data['operation'] == "add_plugs"):
            connection = sqlite3.connect('smart_home.db')
            roomId = dbat.fetchRoomId(connection=connection,roomName = data['roomName'])
            result = dbat.insertDataShellyPlugsTable(connection=connection, plugsId=data['id'], plugsName=data['name'], roomId=roomId)
            if result:
                response={
                    "operation":data['operation'],
                    "status":result,
                    "name":f"{data['name']}",
                    "type":f"{data['type']}",
                }
            
            else:
                response={
                    "operation":data['operation'],
                    "status":result,
                    "name":f"{data['name']}",
                    "type":f"{data['type']}",
                    # "info":f"The {data['name']} already exists"
                }

        elif(data['operation'] == "add_trv"):
            connection = sqlite3.connect('smart_home.db')
            result = dbat.insertDataShellyTrvsTable(connection, data['id'], data['name'], data['room_id'])
            if result:
                client.subscribe(f"shellies/{data['id']}/info")
            connection.close()
            print("finish")
        elif(data['operation'] == "add_hts"):
            connection = sqlite3.connect('smart_home.db')
            result = dbat.insertDataShellyHTsTable(connection, data['id'], data['name'], data['room_id'])
            if result:
                client.subscribe(f"shellies/{data['id']}/#")
            connection.close()
            print("finish")
        elif(data['operation'] == "save_trv_data"):
            connection = sqlite3.connect("smart_home.db")
            result = dbat.insertHistoricalShellyTrvsTable(connection, data["valve_pos"], data["temperature"], data["id"])
            connection.close()
        elif(data['operation'] == "save_ht_data"):
            print("HERE HT")
            connection = sqlite3.connect("smart_home.db")
            result = dbat.insertHistoricalShellyHTsTable(connection, data["temperature"], data["id"])
            connection.close()
            print("FINITO")
    
        
        # if result:
        #     response={
        #         "operation":data['operation'],
        #         "status":"ok",
        #         # "info":f"{data['name']} added successfully"
        #     }
            
        # else:
        #     response={
        #         "operation":data['operation'],
        #         "status":"no",
        #         # "info":f"The {data['name']} already exists"
        #     }
        
        # response = json.dumps(response)
        # client.publish("hub01/response", response)
        
    
    else:
        print("No JSON")

def subscribeToDevices(client):

    client.subscribe('hub01/db')

    conn = sqlite3.connect('smart_home.db')
    cursor = conn.cursor()

    cursor.execute("SELECT id, name FROM shelly_hts")
    row = cursor.fetchall()
    if row:
        for device in row:
            client.subscribe(f"shellies/{device[0]}/#")


    cursor.execute("SELECT id, name FROM shelly_trvs")
    row = cursor.fetchall()
    if row:
        for device in row:
            client.subscribe(f"shellies/{device[0]}/info")

    conn.commit()
    conn.close()


# Callback function for when a connection is established
def on_connect(client, userdata, flags, rc):
    print("Connected: " + str(rc))
    # Subscribe to the topics from the list of topics
    subscribeToDevices(client)


# Callback function for when a message is received
def on_message(client, userdata, msg):

    # print("Received message: " + msg.payload.decode())
    incomming_message = msg.payload.decode()

    # this checks if the topic is the topic intended for the data that is comming from the-
    # mobile phone to the central for room, trv, and ht sensors registration
    if(msg.topic == "hub01/db"):
        listDataBuffer.append(incomming_message)

    # If we got shellytrv in the topic then we know that the device sending the data in that-
    # topic is a shellyTrv.
    elif("shellytrv" in msg.topic):
        # We conver the JSON into a python object 
        data = json.loads(incomming_message)

        # We create a python object suitable for our database
        packForDb = {
            "operation":"save_trv_data",
            "id": data["fw_info"]["device"],
            "valve_pos": int(round(data["thermostats"][0]["pos"])),
            "temperature": data["thermostats"][0]["tmp"]["value"]
        }

        # We convert the python object into a JSON
        packForDb = json.dumps(packForDb)
        # append it to the listDataBuffer, then to be saved in th database
        listDataBuffer.append(packForDb)

    # In this part we handle the data comming from the humidity&temperature sensor.
    # The data doesn't arrive in a json bus as normal textand in thrree different topics
    elif ("shellyht" in msg.topic):
        # handle the announce topic from the ht sensor from where we get it's id
        if("announce" in msg.topic):
            # since the data in the three topics comes at the same time
            # We first clear the list
            if listForPack:
                listForPack.clear()
            data = json.loads(incomming_message)
            listForPack.append(data["id"])
        elif "temperature" in msg.topic:
            listForPack.append(incomming_message)
        elif "humidity" in msg.topic:
            listForPack.append(incomming_message)

        # Here we check if we've got all the neccesary data
        if(len(listForPack)==3):
            htId, temp, hum = listForPack
            print(htId, temp, hum)
            packForDb = {
            "operation":"save_ht_data",
            "id": htId,
            "temperature": float(temp),
        }
            # Create JSON, append to the list data buffer.
            packForDb = json.dumps(packForDb)
            print(packForDb)
            listDataBuffer.append(packForDb)
            # clear the list for the next upcomming data.
            listForPack.clear()


# Create a MQTT client
client = mqtt.Client()

# Set up callback functions
client.on_connect = on_connect
client.on_message = on_message

# Connect to the MQTT broker
client.connect("192.168.0.109", 1883, 60)


# Start the MQTT client loop
client.loop_start()

# Declare intialize and start thread
bufferThread = threading.Thread(target=bufferMessagesforDb, args=(client,))
bufferThread.start()

heatAutomationThread = threading.Thread(target=heatingAutomation, args=(client,))
# heatAutomationThread.start()

# Keep the script running to receive messages
try:
    while True:
        pass
except KeyboardInterrupt:
    # Cleanup or termination steps
    print("Script stopped by user.")

stop_flag.set()

# Wait for the thread to finish
bufferThread.join()

heatAutomationThread.join()
# Stop the MQTT client loop
client.loop_stop()






