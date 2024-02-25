import sqlite3
import json

def serverRooms(connection):
    cursor = connection.cursor()

    listData = []
    cursor.execute("SELECT * FROM rooms")
    rooms = cursor.fetchall()

    for room in rooms:
        data = {
            "operation": "serveRoom",
            "name":room[2],
            "moneySpent":room[3],
            "percentage24h":room[4],
            "percentaged":room[5],
        }
    
        dataJson = json.dumps(data)
        listData.append(dataJson)
    cursor.close()
    return listData

def serveDevices(connection, roomId):
    cursor = connection.cursor()
    listData = []

    cursor.execute("SELECT * FROM shelly_plugs WHERE room_id = (?)",(roomId,))
    shellyplugs = cursor.fetchall()

    for plug in shellyplugs:
        data = {
    "operation": "serveDevice",
    "id":plug[0],
    "name":plug[1],
    "deviceState":plug[2],
    "moneySpent":plug[3],
    "type": plug[4]
}
        dataJson = json.dumps(data)
        listData.append(dataJson)

    cursor.close()
    return(listData)


# connection = sqlite3.connect('smart_home.db')

# serveDevices(connection=connection, roomId=1)
