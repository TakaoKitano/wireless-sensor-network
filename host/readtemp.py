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
import shutil
from collections import OrderedDict

LOG_DIR = "/var/tmp/twelog/"
LOG_FILEPATH = os.path.join(LOG_DIR, "temperature.json")
LOG_TMP_FILEPATH = os.path.join(LOG_DIR, "temperature.tmp")
AT_A_GLANCE_FILEPATH = os.path.join(LOG_DIR, "current.json")
AT_A_GLANCE_TMP_FILEPATH = os.path.join(LOG_DIR, "current.tmp")
LOG_PERIOD = 3600 * 24

#
# logs data format
#
# { 
#   epoch:{nodename0:{time:int, route:int, signal:int, temperature:float, battery:int}, 
#          nodename1:{...},
#           ...
#         },
#         ...
# {
#
# current file format
#
# { nodename0:{'datetime': date time string, 
#               'time': epoch int, 
#               'route':int, 
#               'signal': int,
#               'temperature': float,
#               'battery': int },
#   nodename1:{},
#     ... (there are a dozen of nodes)
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
    with open(os.path.join(LOG_DIR, LOG_FILEPATH)) as f:
      return json.load(f, object_pairs_hook=OrderedDict)
  except:
    print("no log file")
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
  try:
    if not hasattr(readtemp, "port"):
      readtemp.port = serial.Serial("/dev/ttyUSB0", 115200)
    data = readtemp.port.readline().decode("utf-8")
  except:
    print("exception while accessing /dev/ttyUSB")
    sys.exit()

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
  #epoch = int(now/60)*60  # normalize to per minute
  epoch = int(now/300)*300 # normalize to every 5 minutes
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
    node = current[nodename]
    # if the existing node is
    #   recent enough
    # and
    #   route is different
    if (node['time'] > (now - 5)) and not (rc & node['route']):
      route |= node['route'] 
      if node['signal'] > signal:   # retain the maximum signal strength
        signal = node['signal']

  current[nodename] = {'datetime': time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(now)),
                       'time': now,
                       'route': route,
                       'signal': signal,
                       'temperature': values['te'],
                       'battery': values['ba']} 
  print("current[" + nodename + "]=", current[nodename]) 

  fLogAdded = False
  if not epoch in logs:
    logs[epoch] = {}
    fLogAdded = True
  logs[epoch][nodename] = current[nodename]
  return fLogAdded

def main():  
  current = {}

  os.makedirs(LOG_DIR, exist_ok=True)
  logs = load_log()

  while True:
    values = readtemp()
    if values:
      fLogUpdated = processValue(current, logs, values)
      # update current file
      with open(AT_A_GLANCE_TMP_FILEPATH, "w") as f:
        json.dump(current, f, indent=2)
      shutil.move(AT_A_GLANCE_TMP_FILEPATH, AT_A_GLANCE_FILEPATH)
      if fLogUpdated:
        # delete old entries
        now = int(time.time())
        for key in logs:
          if int(key) < (now - LOG_PERIOD):
            del logs[key]
        # update log file
        with open(LOG_TMP_FILEPATH, "w") as f:
          json.dump(logs, f, indent=2)
        shutil.move(LOG_TMP_FILEPATH, LOG_FILEPATH)

if __name__ == '__main__':
  main()
