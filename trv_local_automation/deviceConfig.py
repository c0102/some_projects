import subprocess
import re
import time
import json
import paho.mqtt.client as mqtt
import urllib.parse


def get_wlan0_ssid():
    command = ["iwconfig", "wlan0"]
    result = subprocess.run(command, capture_output=True, text=True)

    if result.returncode == 0:
        return parse_iwconfig_output(result.stdout)
    else:
        print(f"Error: {result.stderr}")
        return None

def parse_iwconfig_output(output):
    lines = output.split("\n")

    for line in lines:
        match = re.search(r'ESSID:"([^"]+)"', line)
        if match:
            ssid = match.group(1)
            return ssid

    return None

def get_wifi_ssids(interface):
    command = ["iwlist", interface, "scan"]
    result = subprocess.run(command, capture_output=True, text=True)

    if result.returncode == 0:
        return parse_wifi_ssids(result.stdout)
    else:
        print(f"Error: {result.stderr}")

def parse_wifi_ssids(output):
    ssids = []
    lines = output.split("\n")

    for line in lines:
        line = line.strip()
        if re.match(r"ESSID:\".*\"", line):
            ssid = re.search(r"ESSID:\"(.*)\"", line).group(1)
            ssids.append(ssid)

    return ssids


operations = []
client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))
    client.subscribe("hub01/db")
def on_message(client, userdata, msg):
    print("Received message: " + msg.payload.decode())
    incommingMessage = msg.payload.decode()
    if(msg.topic == "hub01/db"):
        data = json.loads(incommingMessage)
        operations.append(data)


def setSettingsShellPlugs(client, data, device):
    data = json.dumps(data)
    data = json.loads(data)

    response = {"operation": data["operation"], "type": data["type"], "status": "Configuring"}
    json_response = json.dumps(response)
    client.publish("hub01/response",json_response)

    print("HERE")
    #command = ["curl", "http://192.168.33.1/settings"]
    #tryIt = 0
    #while tryIt <= 3:
    #    print(tryIt)
    #    try:
    #        result = subprocess.run(command, capture_output=True, text=True, timeout=7)
    #        print(result.stdout)
    #        tryIt=3
    #    except subprocess.TimeoutExpired:
    #        print("Timeout occurred. The command took too long to execute.")
    #    tryIt +=1
    ssid = "Enchele Connect"
    encoded_ssid = urllib.parse.quote(ssid)

    configureCommands = ["/settings?mqtt_enable=1&mqtt_server=192.168.0.109:1883", "/settings?mqtt_update_period=2", f"/settings/sta?enabled=1&ssid={encoded_ssid}&key=11111111"]
    confirm = "OK"
    for config in configureCommands:

        command = ["curl","-s", "-o", "/dev/null", "-w", "%{http_code}", f"http://192.168.33.1{config}"]

        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=7)
            print(result.stdout)

        except subprocess.TimeoutExpired:
            print("Timeout occurred. The command took too long to execute.")
            confirm = "NO"
    if confirm == "OK":
        response = {"operation": data["operation"], "type": data["type"], "status": "Device found", "id": device, "name":data["name"]}
        json_response = json.dumps(response)
        client.publish("hub01/response",json_response)

        response = {"operation": "add_plugs", "type": data["type"], "id": device, "name":data["name"], "roomName":data["roomName"]}
        json_response = json.dumps(response)
        client.publish("hub01/db",json_response)


    #command = ["curl", "http://192.168.33.1/settings"]
    #result = subprocess.run(command, stdout=subprocess.PIPE, text=True)
    #output = result.stdout
    #print(output)


    #command = ["curl", "http://192.168.33.1/settings"]
    #result = subprocess.run(command, capture_output=True, text=True)
    #output = result.stdout
    #print(output)

    print("HERE")
#   command = ["sudo", "-s","-o","/dev/null","-w", "%{http_code}", "http://192.168.33.1/settings"]
#    result = subprocess.run(command, capture_output=True, text=True)
#    print(result.stdout)
    print("HERE")




def connect2Device(client, data, device):
    response = {"operation": data["operation"], "type": data["type"], "status": "Connecting To Device"}
    json_response = json.dumps(response)
    client.publish("hub01/response",json_response)

    command = ["sudo", "ifconfig", "wlan0", "up"]
    result = subprocess.run(command, capture_output=True, text=True)
    print(result)

    file_path = "/etc/netplan/50-cloud-init.yaml"
    #file_path = "test.txt"
    with open(file_path, 'w') as file:
        # Write content to the file
        networkSetting=f"""# This file is generated from information provided by the datasource.  Changes
# to it will not persist across an instance reboot.  To disable cloud-init's
# network configuration capabilities, write a file
# /etc/cloud/cloud.cfg.d/99-disable-network-config.cfg with the following:
# network: config: disabled
network:
    ethernets:
        eth0:
            dhcp4: true
    version: 2
    wifis:
        wlan0:
            dhcp4: true
            optional: true
            access-points:
                "{device}": """+'{}'
        file.write(networkSetting)


    command = ["sudo", "netplan", "apply"]
    result = subprocess.run(command, capture_output=True, text=True)
    print(result)

    command = ["sudo", "ip", "route", "add", "192.168.33.1", "dev", "wlan0"]
    result = subprocess.run(command, capture_output=True, text=True)
    print(result)

    #time.sleep(5)

    #command = ["sudo", "ifconfig", "wlan0", "up"]
    #result = subprocess.run(command, capture_output=True, text=True)
    #print(result)

    time.sleep(20)

    ssid = get_wlan0_ssid()
    if ssid == device:
        response = {"operation": data["operation"], "type": data["type"], "status": "Connected"}
        json_response = json.dumps(response)
        client.publish("hub01/response",json_response)
        time.sleep(2)
        print("ok")
        if(data["type"]=='shellyplug-s'):
            setSettingsShellPlugs(client, data, device)


   # command = ["sudo", "curl", "http://192.168.33.1/settings"]
   # result = subprocess.run(command, capture_output=True, text=True)
   # print(result.stdout)



def startScan(client, data):
    data = json.loads(data)
    
    response = {"operation": data["operation"], "type": data["type"], "status": "Scanning"}
    json_response = json.dumps(response)
    client.publish("hub01/response",json_response)
    print(data)
    device = ""
    if(data["operation"] == "addDevice"):
        ssids = get_wifi_ssids("wlan0")
        print(ssids)
        for ssid in ssids:
            print(type(ssid))
            if(data["type"] in ssid):
                device = ssid
    time.sleep(2)
    if device == "":
        response = {"operation": data["operation"], "type": data["type"], "status": "Not found try again"}
        json_response = json.dumps(response)
        client.publish("hub01/response",json_response)
        time.sleep(3)
        response = {"operation": data["operation"], "type": data["type"], "status": "End"}
        json_response = json.dumps(response)
        client.publish("hub01/response",json_response)


    else:
        print(device)
        connect2Device(client, data, device)
        #response = {"operation": data["operation"], "type": data["type"], "status": "Device found", "id": device, "name":data["name"]}
        #json_response = json.dumps(response)
        #client.publish("hub01/response",json_response)


