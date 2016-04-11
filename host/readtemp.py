#!/usr/bin/python3
#
# readte.py: read from the TWE-Lite wireless temperature sensor and store the values
#

import serial
import re
import time
import os
import sys
import json
from collections import OrderedDict

PRIMARY_LOG_DIR = "/var/tmp/twelog/"
LOG_FILENAME = "temperature.json"
PRIMARY_LOG_PERIOD = 3600 * 6
AT_A_GLANCE_FILENAME = "current.json"
SECONDARY_LOG_DIR = "/var/www/twetemp/logs/"
SECONDARY_LOG_DUMP_INTERVAL = 3600    # typical 1hour=3600 
LOG_DUMP_DURATION = 600               # resolution 10minutes=600

#
# initialize 
#   -create log directory
#
def initialize():
  os.makedirs(PRIMARY_LOG_DIR, exist_ok=True)
  os.makedirs(SECONDARY_LOG_DIR, exist_ok=True)

#
# read log file if exists
#
def load_log_file():
  #
  # read from the primary log files
  #
  try:
    with open(os.path.join(PRIMARY_LOG_DIR, LOG_FILENAME)) as f:
      return json.load(f)
  except:
    print("no log file")
  return []

def update_secondary_log_files(logs):
  print("update_secondary_log_files")
  dict = {}
  now = int(time.time())
  for element in logs:
    # [0] : epoch
    # [1] : nodename
    # [2] : route
    # [3] : signal
    # [4] : temperature
    # [5] : battery
    # [6] : flag
    flag = element[6]
    temperature = element[4]
    nodename = element[1]
    epoch = element[0]
    if flag == 0 and int(epoch/LOG_DUMP_DURATION) < int(now/LOG_DUMP_DURATION):
      element[6] = 1
      key = int(epoch/LOG_DUMP_DURATION)*LOG_DUMP_DURATION
      if nodename in dict:
        if time in dict[nodename]:
          dict[nodename][key].append(temperature)
          continue
      else:
        dict[nodename] = OrderedDict()

      dict[nodename][key] = [temperature]
  #####
    
  for nodename in dict:
    for key, values in dict[nodename].items():
      filename = ""
      f = None
      name = time.strftime("%Y-%m-%d", time.localtime(key))
      if name != filename:
        filename = name
        os.makedirs(os.path.join(SECONDARY_LOG_DIR, nodename), exist_ok=True)
        if f:
          f.close()
        print("opening:", os.path.join(SECONDARY_LOG_DIR, nodename, filename))
        f = open(os.path.join(SECONDARY_LOG_DIR, nodename, filename), "a")
        f.write(str(key) + ',' + str(round(sum(values)/len(values), 1)) + '\n')
    if f:
      f.close()

#
# read the value from the USB sensor device
#
# to figure out the device name, dmesg and find a message like this
# "usb 1-1.2: FTDI USB Serial Device converter now attached to ttyUSB0"
#
def readtemp():
  if not hasattr(readtemp, "pattern"):
    readtemp.pattern = re.compile('rc=(\w+):.*lq=(\d+):.*ed=(\w+):.*id=(\w+):.*ba=(\d+):.*:te=(\d+)')
  try:
    if not hasattr(readtemp, "port"):
      readtemp.port = serial.Serial("/dev/ttyUSB0", 115200)
    data = readtemp.port.readline().decode("utf-8")
  except:
    print("exception while accessing /dev/ttyUSB")
    sys.exit()

  print(data)
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

#
# logs data format
#
# [ 
#   0:            1:          2:        3:                4:                 5:
#  [epoch int, nodename str, route int, link_quality int, temperature float, battery int], 
#    ...
# ]
#

# current internal format
# { 
#   nodename1: index in logs
#   ...
# }
#
# current file format
#
# { nodename1:{'datetime': date time string, 
#               'time': epoch int, 
#               'route':int, 
#               'signal': int,
#               'temperature': float,
#               'battery': int },
#   nodename2:{},
#     ... (there are a dozen of nodes)
# }
#
def main():  
  current = {}
  logs = []
  secondary_log_dump_at  = int(time.time())

  initialize()
  logs = load_log_file()

  while True:
    values = readtemp()
    if not values:
      continue

    # rc: routing device
    # lq: link quality(signal strength) int
    # ed: nodename string
    # id: deviceid string (not used, though)
    # ba: battery int (mV)
    # te: temperature float
    now = int(time.time())
    flag = 0 
    nodename = values['ed']
    if values['rc'] == '80000000':           # parent
      rc = 1
    elif values['rc'] == '8100D797':         # router1
      rc = 2
    elif values['rc'] == '8100978C':         # router2
      rc = 4
    else:
      rc = 0
    signal = values['lq']

    route = rc
    if nodename in current:
      index = current[nodename]
      element = logs[index]
      if (element[0] > (now - 2)) and not (rc & element[2]):
        route |= element[2]       # merge route
        if element[3] > signal:   # retain the maximum signal strength
          signal = element[3]
        flag = element[6]
      else:
        logs.append([])
        index = len(logs) - 1
    else:
      logs.append([])
      index = len(logs) - 1
    current[nodename] = index
    logs[index] = [now, nodename, route, signal, values['te'], values['ba'], flag]
      
    #
    # update secondary log files 
    #
    if now > (secondary_log_dump_at + SECONDARY_LOG_DUMP_INTERVAL):
      secondary_log_dump_at  = now
      update_secondary_log_files(logs)

    #
    # update at_a_glance file 
    #
    dict = {}
    for nodename, index in current.items():
      element = logs[index]
      dict[nodename] = { 'datetime': time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(element[0])),
                         'time': element[0],
                         'route': element[2],
                         'signal': element[3],
                         'temperature': element[4],
                         'battery': element[5] }
    with open(os.path.join(PRIMARY_LOG_DIR, AT_A_GLANCE_FILENAME), "w") as f:
      json.dump(dict, f, indent=2)

    #
    # update the primary log files
    #
    recentenough = 0
    for i, element in enumerate(logs):
      if element[0] > (now - PRIMARY_LOG_PERIOD):
        recentenough = i
        break
    del logs[0:recentenough]
    # adjust the indexes in current
    for nodename in current:
      current[nodename] = current[nodename] - recentenough
    with open(os.path.join(PRIMARY_LOG_DIR, LOG_FILENAME), "w") as f:
      json.dump(logs, f, indent=2)

if __name__ == '__main__':
  main()
