import sqlite3

def createRoomsTable():
    # Create a table for rooms
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS rooms (
        id INTEGER PRIMARY KEY,
        type TEXT,
        name TEXT,
        moneySpent REAL,
        percentage24h REAL,
        percentaged REAL,
        optimal_temperature REAL
        )
    ''')

    # Make sure that every room has a unique name 
    cursor.execute("CREATE UNIQUE INDEX IF NOT EXISTS unique_room_name ON rooms (name)")

def insertDataRoomTable(connection, roomType, roomName, moneySpent=0.0, percentage24h=0.0,  percentaged=0.0,  optimalTemperature=22):
    cursor = connection.cursor()
        # Now here we use a try except that will be triggered when the name of the inserted room is a duplicate
    try:
        cursor.execute("INSERT INTO rooms (type, name, moneySpent, percentage24h, percentaged, optimal_temperature) VALUES (?,?,?,?,?,?)", (roomType, roomName, moneySpent, percentage24h, percentaged, optimalTemperature))
        connection.commit()
        print("ADDED SUCCESFULLY")
        cursor.close()
        return True
        
    except:
        print("no name duplications allowed")
        cursor.close()
        return False
    

def fetchRoomId(connection, roomName):
    cursor = connection.cursor()
    cursor.execute("SELECT id FROM rooms WHERE name=?", (roomName,))
    row = cursor.fetchone()
    print(row)
    if row is not None:
        room_id=row[0]
        return room_id
    else:
        print("That ID doesn't exist")
        return 0
    

def createShellyPlugsTable():

    cursor.execute('''
    CREATE TABLE IF NOT EXISTS shelly_plugs (
        id TEXT PRIMARY KEY,
        name TEXT,
        deviceState TEXT,
        moneySpent REAL,
        type TEXT,
        room_id INTEGER,
        FOREIGN KEY (room_id) REFERENCES rooms(id)
        )
    ''')
    
    cursor.execute("CREATE UNIQUE INDEX IF NOT EXISTS unique_plugs_name ON shelly_plugs (name)")


def insertDataShellyPlugsTable(connection, plugsId, plugsName, roomId, deviceState = "off", moneySpent = 0.0, deviceType = "shellyplug-s" ):
    cursor = connection.cursor()
    # Now here we use a try except that will be triggered when the name of the inserted shelly TRV is a duplicate
    try:
        cursor.execute("INSERT INTO shelly_plugs (id, name, deviceState, moneySpent,type ,room_id) VALUES (?, ?, ?, ?, ?, ?)", (plugsId, plugsName, deviceState, moneySpent,deviceType, roomId))
        connection.commit()
        cursor.close()
        return True
    except:
        print("no duplications allowed")
        cursor.close()
        return False

    


def createShellysTrvTable():
    # Create a table for Shelly TRVs
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS shelly_trvs (
            id TEXT PRIMARY KEY,
            name TEXT,
            room_id INTEGER,
            FOREIGN KEY (room_id) REFERENCES rooms(id)
        )
    ''')

    

    # Make sure that every Shelly TRV has a unique name
    cursor.execute("CREATE UNIQUE INDEX IF NOT EXISTS unique_trv_name ON shelly_trvs (name)")

def insertDataShellyTrvsTable(connection,trvId, trvName, roomId):
    cursor = connection.cursor()
    # Now here we use a try except that will be triggered when the name of the inserted shelly TRV is a duplicate
    try:
        cursor.execute("INSERT INTO shelly_trvs (id, name, room_id) VALUES (?, ?, ?)", (trvId, trvName, roomId))
        connection.commit()
        cursor.close()
        return True
    except:
        print("no duplications allowed")
        cursor.close()
        return False


def createTableHistoricalDataShellyTrvs():

    # Create a table for historical data of Shelly TRVs
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS shelly_trvs_his (
            id INTEGER PRIMARY KEY,
            valve_pos INTEGER,
            temperature REAL,
            trv_id TEXT,
            timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (trv_id) REFERENCES shelly_trvs(id)
        )
    ''')


def fetchTrvId(trvName):
    cursor.execute("SELECT id FROM shelly_trvs WHERE name=?", (trvName,))
    row = cursor.fetchone()
    print(row)
    if row is not None:
        trv_id=row[0]
        return trv_id
    else:
        print("That ID doesn't exist")
        return 0

def insertHistoricalShellyTrvsTable(connection, valvePos, temperature, trvId):
    cursor = connection.cursor()

    print("Insert historical data for trvs")
    # Now here we use a try except that will be triggered when the name of the inserted shelly TRV is a duplicate
    cursor.execute("INSERT INTO shelly_trvs_his (valve_pos, temperature, trv_id) VALUES (?, ?, ?)", (valvePos, temperature, trvId))
    connection.commit()
    cursor.close()

    return True
    
        


def createSHellyHtsTable():
    # Create a table for Shelly H&T
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS shelly_hts (
            id TEXT PRIMARY KEY,
            name TEXT,
            room_id INTEGER,
            FOREIGN KEY (room_id) REFERENCES rooms(id)
        )
    ''')

    # Make sure that every Shelly TRV has a unique name
    cursor.execute("CREATE UNIQUE INDEX IF NOT EXISTS unique_ht_name ON shelly_hts (name)")


def insertDataShellyHTsTable(connection, htId ,htName, roomId):
    cursor = connection.cursor()
    try:
        cursor.execute("INSERT INTO shelly_hts (id, name, room_id) VALUES (?, ?, ?)", (htId, htName, roomId))
        connection.commit()
        cursor.close()
        return True
        

    except:
        print("no duplications allowed")
        cursor.close()
        return False
    

def fetchHtId(htName):
    cursor.execute("SELECT id FROM shelly_hts WHERE name=?", (htName,))
    row = cursor.fetchone()
    print(row)
    if row is not None:
        ht_id=row[0]
        return ht_id
    else:
        print("That ID doesn't exist")
        return 0

def createTableHistoricalDataShellyHTs():

    # Create a table for historical data of Shelly TRVs
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS shelly_hts_his (
            id INTEGER PRIMARY KEY,
            temperature REAL,
            ht_id TEXT,
            timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (ht_id) REFERENCES shelly_hts(id)
        )
    ''')

def insertHistoricalShellyHTsTable(connection, temperature, htId):
    cursor = connection.cursor()


    print("Insert historical data for temp&hum sens")
    # Now here we use a try except that will be triggered when the name of the inserted shelly TRV is a duplicate
    cursor.execute("INSERT INTO shelly_hts_his (temperature, ht_id) VALUES (?, ?)", (temperature, htId))
    connection.commit()
    cursor.close()


    
    

# Connect to smart_home database
conn = sqlite3.connect('smart_home.db')
cursor = conn.cursor()

createRoomsTable()
createShellyPlugsTable()
createShellysTrvTable()
createTableHistoricalDataShellyTrvs()
createSHellyHtsTable()
createTableHistoricalDataShellyHTs()



# insertDataRoomTable("BedRoom", 'MyBedRoom', 22.5)
# room_id = fetchRoomId("MyBedRoom")
# insertDataShellyTrvsTable('BedRoomTRV2', room_id)

# insertDataShellyHTsTable('BedRoomHtSens',room_id)

# trv_id = fetchTrvId("BedRoomTRV2")
# insertHistoricalShellyTrvsTable(61, 33.9, trv_id)

# ht_id = fetchHtId('BedRoomHtSens')
# insertHistoricalShellyHTsTable(23.3, ht_id)







# # Here we are printing out all the Shelly TRVs that are linked with the MyLivingRoom id 
# cursor.execute("SELECT name FROM shelly_trvs WHERE room_id = (?)",(room_id,))
# row = cursor.fetchall()
# print(row)

# Here we are printing out all the Shelly TRVs that are linked with the MyLivingRoom id 
# cursor.execute("SELECT name FROM shelly_hts WHERE room_id = (?)",(room_id,))
# row = cursor.fetchall()
# print("here",row)


# # Here we are printing historical data for a particular shelly TRV 
# cursor.execute("SELECT * FROM shelly_trvs_his WHERE trv_id = (?)",(trv_id,))
# row = cursor.fetchall()
# print(row)


# cursor.execute("SELECT * FROM shelly_hts_his WHERE ht_id = (?)",(ht_id,))
# row = cursor.fetchall()
# print(row)



# Commit the transaction and close the connection
conn.commit()
conn.close()



