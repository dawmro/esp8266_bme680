#Python Code
import serial
import time
import ast
from datetime import datetime
import sqlite3
import socket
import serial.tools.list_ports
import os


# 
board_name_on_comm_list = "CH340"
handshake_reply_desired = "esp8266_bme680"
serial_baudrate=115200
serial_timeout=5



def tStamp():

    return str("["+datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')+" UTC]")
   
    
def get_bme680_data(my_dict, serialcomm):

    print(tStamp() + " Sending get data request: " + str(serialcomm.write("Get BME680 data;".encode())))
    response = ''
    print(tStamp() + " Starting getting response")
    try:
        response = serialcomm.readline().decode('utf-8').rstrip()
        print(tStamp() + " Got response")
    except:
        print(tStamp() + " Something fucked up with response")
        print(tStamp() + " Will try again in a while")
    
    if response != '':
        print(tStamp() + " Length of response: " + str(len(response)))
        print(response)
        if len(response) > 100:
            try:
                my_dict = ast.literal_eval(response)
            except:    
                print("Cannot eval")
                        
    return my_dict
        

def save_bme680_data_to_db(my_dict):
    try:
        bme680 = "bme680"
        conn = sqlite3.connect("sensor_data.db")
        c = conn.cursor()
        c.execute("""CREATE TABLE IF NOT EXISTS """+bme680+"""(
            timeNow, 
            time_trigger, 
            rawTemperature, 
            pressure,
            rawHumidity,
            gasResistance,
            iaq,
            iaqAccuracy,
            temperature,
            humidity,
            dewPoint,
            staticIaq,
            co2Equivalent,
            breathVocEquivalent
            )""")
            
        c.execute("""INSERT INTO """+bme680+""" VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",(
            my_dict.get('timeNow'),
            my_dict.get('time_trigger'),
            my_dict.get('rawTemperature'),
            my_dict.get('pressure'),
            my_dict.get('rawHumidity'),
            my_dict.get('gasResistance'),
            my_dict.get('iaq'),
            my_dict.get('iaqAccuracy'),
            my_dict.get('temperature'),
            my_dict.get('humidity'),
            my_dict.get('dewPoint'),
            my_dict.get('staticIaq'),
            my_dict.get('co2Equivalent'),
            my_dict.get('breathVocEquivalent')))
            
        conn.commit()  
        c.close()
        conn.close()
        return "True"
    except:
        return "False"

   
def scan_ports_and_send_handshahe(handshake_reply, serialcomm):
    arduino_ports = [
        p.device
        for p in serial.tools.list_ports.comports()
        if board_name_on_comm_list in p.description  # may need tweaking to match new arduinos
    ]
    if not arduino_ports:
        print(tStamp() + " No Arduino found :(")
    elif len(arduino_ports) > 1:
        print(tStamp() + " Multiple Arduinos found, will try each of them:")
    else:
        print(tStamp() + " Single Arduino found")
    
    for p in arduino_ports:
        print(tStamp() + " Trying to connect to: " + p)
        serialcomm = serial.Serial(port = str(p), baudrate = serial_baudrate)
        serialcomm.timeout = serial_timeout
    
        if serialcomm.isOpen():
            print(tStamp() + " Closing serial port: " + str(serialcomm.close()))
            
        print(tStamp() + " Opening serial port: " + str(serialcomm.open()))
        print(tStamp() + " Checking if serial port is open: " + str(serialcomm.isOpen()))
        
        to_send = "who_are_you;"
        print(to_send)
        print(tStamp() + " Sending handshake: " + str(serialcomm.write(to_send.encode())))
        response = ''
        try:
            response = serialcomm.readline().decode('utf-8').rstrip()
        except:
            response = serialcomm.readline()
        print(tStamp() + " Closing serial port: " + str(serialcomm.close()))    
        print(tStamp() + " Length of response: " + str(len(response)))
        print(response)
        
        if response == handshake_reply_desired:
            handshake_reply = response
            print(tStamp() + " Desired Arduino found on: " + p)
        
    return handshake_reply, serialcomm
    

def main():

    my_dict = {}
    handshake_reply = ""
    serialcomm = ""
 
    while True:
        print("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n")
  
        # get hostname
        hostname = socket.gethostname()
        
        if handshake_reply != handshake_reply_desired:
            handshake_reply, serialcomm = scan_ports_and_send_handshahe(handshake_reply, serialcomm)
                
        if handshake_reply == handshake_reply_desired:
        
            if serialcomm.isOpen():
                print(tStamp() + " Closing serial port: " + str(serialcomm.close()))
            try:    
                print(tStamp() + " Opening serial port: " + str(serialcomm.open()))
            except:
                handshake_reply = ""
                continue
            print(tStamp() + " Checking if serial port is open: " + str(serialcomm.isOpen()))
            
            my_dict = {"rigName": hostname, "timeNow": datetime.utcnow()}
            my_dict.update(get_bme680_data(my_dict, serialcomm))
            

            print(tStamp() + " Closing serial port: " + str(serialcomm.close()))
                    
            print(tStamp() + " save_bme680_data_to_db: " + str(save_bme680_data_to_db(my_dict)))
            
        print("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n")
        time.sleep(10)

    
if __name__ == '__main__':
    main()
