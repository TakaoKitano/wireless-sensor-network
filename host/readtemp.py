#!/usr/bin/python3
#
# readte.py: read from the TWE-Lite wireless temperature sensor and store the values
#

import serial
import re
import time
import datetime
import os
import sys
import json
import shutil
from collections import OrderedDict

LOG_DIR = "/var/tmp/twelog/"
LOG_FILEPATH = os.path.join(LOG_DIR, "temperature.json")
LOG_TMP_FILEPATH = os.path.join(LOG_DIR, "temperature.tmp")
AT_A_GLANCE_FILEPATH = os.path.join(LOG_DIR, "current.json")
AT_A_GLANCE_TMP_FILEPATH = os.path.join(LOG_DIR, "current.tmp")
LOG_PERIOD = 3600 * 24 + 180

SECONDARY_LOG_DIR = "/var/www/twetemp/logs/"
SECONDARY_LOG_INTERVAL = 3600

#
# logs data format
#
# { 
#     epoch:{nodename0:{time:int, route:int, signal:int, temperature:float, battery:int}, 
#                    nodename1:{...},
#                     ...
#                 },
#                 ...
# {
#
# current file format
#
# { nodename0:{'datetime': date time string, 
#                             'time': epoch int, 
#                             'route':int, 
#                             'signal': int,
#                             'temperature': float,
#                             'battery': int },
#     nodename1:{},
#         ... (there are a dozen of nodes)
# }
#


#
# read log file if exists
#
def load_log():
    #
    # read from the primary log files
    #
    try:
        with open(LOG_FILEPATH) as f:
            print("reading data from", LOG_FILEPATH)
            return json.load(f, object_pairs_hook=OrderedDict)
    except:
        try:
            filepath = os.path.join(SECONDARY_LOG_DIR, datetime.date.today().strftime("%Y-%m-%d.json"))
            with open(filepath) as f:
                print("reading data from", filepath)
                return json.load(f, object_pairs_hook=OrderedDict)
        except:
            print("no log file")
    #
    # last resort, null ordered dict
    #
    return OrderedDict()

#
# read the value from the USB sensor device
#
# to figure out the device name, dmesg and find a message like this
# "usb 1-1.2: FTDI USB Serial Device converter now attached to ttyUSB0"
#
def readtemp():
    if not hasattr(readtemp, "pattern"):
        readtemp.pattern = re.compile('rc=(\w+):.*lq=(\d+):.*ed=(\w+):.*id=(\w+):.*ba=(\d+):.*:te=(\d\d\d\d)')

    data = None
    while data is None:
        try:
            if not hasattr(readtemp, "port"):
                readtemp.port = serial.Serial("/dev/ttyUSB0", 115200)
            data = readtemp.port.readline().decode("utf-8")
        except:
            print("exception while accessing /dev/ttyUSB")


    print(data.rstrip('\n'),)
    m = readtemp.pattern.search(data)
    if m:
        rc = m.group(1)
        lq = int(m.group(2))
        ed = m.group(3)[4:]
        id = m.group(4)
        ba = int(m.group(5))
        te = round(int(m.group(6))/100, 2)
        return {'rc':rc, 'lq':lq, 'ed':ed, 'id':id, 'ba':ba, 'te':te}
    else:
        print("read invalid line")


def processValue(current, logs, values):
    # rc: routing device
    # lq: link quality(signal strength) int
    # ed: nodename string
    # id: deviceid string (not used, though)
    # ba: battery int (mV)
    # te: temperature float
    now = int(time.time())
    #epoch = int(now/60)*60    # normalize to per minute
    epoch = int(now/300)*300 # normalize to every 5 minutes
    nodename = values['ed']
    if values['rc'] == '80000000':                     # parent
        rc = 1
    elif values['rc'] == '8100D797':                 # router1
        rc = 2
    elif values['rc'] == '8100978C':                 # router2
        rc = 4
    else:
        rc = 0
    signal = values['lq']
    route = rc
    fLogUpdated = False
    if nodename in current:
        node = current[nodename]
        # if the existing node is
        #     recent enough
        # and
        #     route is different
        if (node['time'] > (now - 5)) and not (rc & node['route']):
            route |= node['route'] 
            if node['signal'] > signal:     # retain the maximum signal strength
                signal = node['signal']
        else:
            fLogUpdated = True

    current[nodename] = {'datetime': time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(now)),
                                             'time': now,
                                             'route': route,
                                             'signal': signal,
                                             'temperature': values['te'],
                                             'battery': values['ba']} 
    print("current[" + nodename + "]=", current[nodename]) 

    if not epoch in logs:
        logs[epoch] = {}
        fLogUpdated = True
    logs[epoch][nodename] = current[nodename]
    return fLogUpdated

def update_primary_log_file(logs):
    # delete old entries
    now = int(time.time())
    for key in list(logs.keys()):
        if int(key) < (now - LOG_PERIOD):
            del logs[key]
    # update log file
    with open(LOG_TMP_FILEPATH, "w") as f:
        json.dump(logs, f, indent=2)
    shutil.move(LOG_TMP_FILEPATH, LOG_FILEPATH)

def update_secondary_log_file(logs):
    # date of 5 minutes ago, thus, action at 24:00 - 24:04 goes to previous day
    # (fromtimestamp returns local date)
    now = time.time()
    date = datetime.date.fromtimestamp(now - 300)
    for key in list(logs.keys()):
        if datetime.date.fromtimestamp(int(key)).day != date.day:
            del logs[key]
    filepath = os.path.join(SECONDARY_LOG_DIR, date.strftime("%Y-%m-%d.json"))
    temppath = os.path.join(SECONDARY_LOG_DIR, date.strftime("%Y-%m-%d.tmp"))
    with open(temppath, "w") as f:
        json.dump(logs, f, indent=2)
    shutil.move(temppath, filepath)
 
def main():    
    current = {}

    os.makedirs(LOG_DIR, exist_ok=True)
    os.makedirs(SECONDARY_LOG_DIR, exist_ok=True)
    logs = load_log()
    secondary_log_at = int(time.time()/SECONDARY_LOG_INTERVAL)

    while True:
        values = readtemp()
        if not values:
            continue
 
        fLogUpdated = processValue(current, logs, values)
        # update current file
        with open(AT_A_GLANCE_TMP_FILEPATH, "w") as f:
            json.dump(current, f, indent=2)
        shutil.move(AT_A_GLANCE_TMP_FILEPATH, AT_A_GLANCE_FILEPATH)
        if fLogUpdated:
            update_primary_log_file(logs)
        now = int(time.time()/SECONDARY_LOG_INTERVAL)
        if now > secondary_log_at:
            secondary_log_at = now
            update_secondary_log_file(logs.copy())

if __name__ == '__main__':
    main()
