#!/usr/bin/env python3

import paho.mqtt.client as mqtt
import argparse
import json
import base64

class Responder():
    def __init__(self, server, username, password):
        self.server = server
        self.username = username
        self.password = password
        self.client = mqtt.Client()
        self.client.username_pw_set(username, password)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        
    def rssi2color(self, rssi):
        if (rssi > -100):
            return bytes([0xFF, 0x00, 0x00])
        if (rssi > -105):
            return bytes([0xFF, 0x7F, 0x00])
        if (rssi > -110):
            return bytes([0xFF, 0xFF, 0x00])
        if (rssi > -115):
            return bytes([0x00, 0xFF, 0x00])
        if (rssi > -120):
            return bytes([0x00, 0xFF, 0xFF])
        return bytes([0x00, 0x00, 0xFF])

    def on_message(self, client, userdata, msg):
        try:
            print(f"Receive on topic {msg.topic}:")
            
            # decode
            up = json.loads(msg.payload)
            print(f"{up}")
            
            # calculate max rssi
            maxrssi = -1000;
            for g in up['metadata']['gateways']:
                rssi = g["rssi"]
                if rssi > maxrssi:
                    maxrssi = rssi
            
            # check button press
            payload_raw = base64.b64decode(up["payload_raw"])
            if ((payload_raw[0] & 128) != 0):
                # if button was pressed, send back color code
                color = self.rssi2color(maxrssi)
                topic = up['app_id'] + '/devices/' + up['dev_id'] + '/down'
                down = {}
                down["port"] = 1
                down["confirmed"] = False
                down["payload_raw"] = base64.b64encode(color).decode('utf-8')
                print(f"Publish on topic {topic}: {down}")
                client.publish(topic, json.dumps(down))
        except Exception as e:
            print("Caught exception:", e)
        
    def on_connect(self, client, userdata, flags, rc):
        try:
            topic = "+/devices/+/up";
            print(f"Subscribing to uplink topic {topic}...")
            client.subscribe(topic)
        except Exception as e:
            print(e)

    def run(self):
        print(f"Connecting to '{self.server}' as '{self.username}'...")
        self.client.connect(self.server)
        while True:
            self.client.loop()

def main():
    """ The main entry point """
    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--server", help="The MQTT server, e.g. eu.thethings.network",
                        default="eu.thethings.network")
    parser.add_argument("-u", "--username", help = "The MQTT user name (TTN application name)", default = "lorakiss-app", required = True)
    parser.add_argument("-p", "--password", help = "The MQTT password (TTN access key)", default="ttn-account-v2.wsHo81XnbhmQ5NJYm6UCHhfupunktulDH-1AXVE3YI4")
    args = parser.parse_args()

    responder = Responder(args.server, args.username, args.password)
    responder.run()

if __name__ == "__main__":
    main()
    
