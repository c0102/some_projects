import sqlite3

conn = sqlite3.connect('smart_home.db')
cursor = conn.cursor()


cursor.execute("SELECT * FROM rooms")
row = cursor.fetchall()

print(row)

# cursor.execute("SELECT * FROM shelly_hts_his WHERE ht_id = (?)",("shellyht-02475A",))
# row = cursor.fetchall()
# print(row)


# cursor.execute("SELECT * FROM shelly_hts")
# row = cursor.fetchall()
# print(row)



# cursor.execute("SELECT id, name FROM shelly_hts")
# row = cursor.fetchall()
# print(row)

# for device in row:
#     print(device[0])

# cursor.execute("SELECT id, name FROM shelly_trvs")
# row = cursor.fetchall()
# if row:
#     print(row)




conn.commit()
conn.close()