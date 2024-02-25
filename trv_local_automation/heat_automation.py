import sqlite3

def heat_automation_process(connection, client):

    print("HEAT AUTOMATION STARTED !")

    cursor = connection.cursor()
    # Digging the data for the apartament or the house

    # First we fetch all the ids and op.temperature for every room
    cursor.execute("SELECT id, optimal_temperature FROM rooms")

    # Save them on a list
    row = cursor.fetchall()
    print("rooms:", row)

    # Do something if there is any room
    if row:
        # Loop the list,
        # The list is a list of tuples where [(roomid, optimaltemp)...]
        for idRoom in row:
            # Now we use the first id we fetch to get the ids of the shelly trvs that are-
            # located in that room. The room_id in the shelly_trvs is linked with the room id -
            # with this command we are getting [(shellytrvID,)...]
            cursor.execute("SELECT id FROM shelly_trvs WHERE room_id = (?)",(idRoom[0],))
            row = cursor.fetchall()
            print(row)
            # We initialized a list to append the informations about the TRVs in the particular room
            listTrv =[]
            if row:
                # There could be more than one TRV in a room hence we're going to loop through the fetched IDs-
                # We will use them to access their historical data on the shelly_trvs_his table.
                for idTrv in row:
                    # from the historical data of the TRVs we will fetch the temperature and latest valve position
                    cursor.execute("SELECT temperature, valve_pos FROM shelly_trvs_his WHERE trv_id = (?)",(idTrv[0],))
                    row = cursor.fetchall()
                    if row:
                    # We reverse the list since we are interested in the latest inputs
                        row = row[::-1]
                        # In the list there will be added dicitationaries where the main keys will be the TRVs id-
                        # inside every dictationary there will be the temperature and the current valve position
                        listTrv.append({
                            idTrv[0]:{
                                "temperatureTrv":row[0][0],
                                "valvePosTrv":row[0][1]
                                }
                        })

            # We apply the same for the Temperature&Humidity sensors...
            cursor.execute("SELECT id FROM shelly_hts WHERE room_id = (?)",(idRoom[0],))
            row = cursor.fetchall()

            # Similarly, here we'll use just a list, such as when there are more than one h&t sensors-
            # we'll get the avegrage temperature from the room
            listHT =[]
            if row:
                for idHt in row:
                    cursor.execute("SELECT temperature FROM shelly_hts_his WHERE ht_id = (?)",(idHt[0],))
                    row = cursor.fetchall()
                    if row:
                        row = row[::-1]
                        listHT.append(row[0][0])

                        # This would list be very handy to check the amount of change in temperature -
                        # between the last two measurements
                        deltaTemperature = [item[0] for item in row[:2]]
                        print("*************", deltaTemperature)


            if (listTrv and listHT):
                optimalRoomTemperature = idRoom[1]
                averageTemp = sum(listHT)/len(listHT)

                print("optimal temeperature:", optimalRoomTemperature, " averageTemperature:", averageTemp,
                    " trvs status:", listTrv)
                
                ### Automation Process ###

                # Here we calculate the temperature difference the desired temperature minus the temperature-
                # average measured in the room using the temperature sensors 
                temperatureDifference = (optimalRoomTemperature - averageTemp) * 10

                # Here we go through the list of TRVs available in a room. get the latest valve position-
                # and open more ore close the valve according to the calculation made.-
                # then we send the message to the trv via MQTT
                for trv in listTrv:
                    for key, value in trv.items():
                        print(key)
                        print(value['valvePosTrv'])
                        lvl = value['valvePosTrv'] + int(round(temperatureDifference))
                        print(lvl)
                        client.publish(f"shellies/{key}/thermostat/0/command/valve_pos", lvl)

    cursor.close()
    
        ### Automation Process ###
