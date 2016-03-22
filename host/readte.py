#!/usr/bin/python3
#
# readte.py: read from the TWE-Lite wireless temperature sensor and store the values
#

import serial
import re
import time
import os
import glob
import json

PRIMARY_LOG_DIR = "/var/tmp/twelog/"
devices = {}

#
# initialize 
#   -create log directory
#   -read log files if exist
#
def init():
  global devices
  #print("init0\n")
  os.makedirs(PRIMARY_LOG_DIR, exist_ok=True)
  try:
    for file in glob.glob(os.path.join(PRIMARY_LOG_DIR, "*.json")):
      f = open(file)
      with f:
        # the result of json.load should be the list of array like
        # [['2016/03/22 13:07:00', 1458619620, 5, '810149B3', 3330, 1801],...]
        #    date/time, epoch time, route, ed, battery, temperature
        dev = os.path.splitext(os.path.basename(f.name))[0]
        devices[dev] = json.load(f)
  except:
    print("something wrong with log files")
  print(devices)

#
# read the value from the USB sensor device
#
# to figure out the device name, dmesg and find a message like this
# "usb 1-1.2: FTDI USB Serial Device converter now attached to ttyUSB0"
#
def readtemp():
  try:
    if not hasattr(readtemp, "pattern"):
      readtemp.pattern = re.compile('rc=(\w+):.*ed=(\w+):.*ba=(\d+):.*:te=(\d+)')
    if not hasattr(readtemp, "port"):
      readtemp.port = serial.Serial("/dev/ttyUSB0", 115200)
    data = readtemp.port.readline().decode("utf-8")
    #print(data)
    m = readtemp.pattern.search(data)
    if m:
      epoch = int(time.time())
      rc = m.group(1)
      end_device = m.group(2)
      battery = int(m.group(3))
      temperature = int(m.group(4))
      route = 0
      if rc == '80000000':
        route = 1
      elif rc == '8100D797':
        route = 2
      elif rc == '8100978C':
        route = 4
      timestr = time.strftime("%Y/%m/%d %H:%M:%S", time.localtime(epoch))
      return [timestr, epoch, route, end_device, battery, temperature]
    else:
      print("read invalid line")
  except:
    print("something wrong with /dev/ttyUSB")
    import sys
    sys.exit()

def main():  
  global devices
  init()
  while True:
    values = readtemp()
    if not values:
      continue
    # 0: date/time string
    # 1: epoch int
    # 2: route int
    # 3: devicename string
    # 4: battery int
    # 5: temperature int
    #print(values)
    if not values[3] in devices:
      device = []
      device.append(tuple(values))
      devices[values[3]] = device
    else:
      device = devices[values[3]]
      latest = device[-1]
      if latest[1] > (values[1] - 2):
        values[2] |= latest[2]
        device[-1] = tuple(values)
      else:
        device.append(tuple(values))
    print(device[-1])

    #
    # update the primary log files
    #
    # mark old entries
    i = 0
    recentenough = 0
    for i in range(len(device)):
      if device[i][1] > (device[-1][1] - (3600 * 6)):
        recentenough = i
        break
    #print("recentenough=", recentenough)
    f = open(os.path.join(PRIMARY_LOG_DIR, values[3]+".json"), "w")
    with f:
      json.dump(device[recentenough:], f, indent=2)

    #
    # update at_a_glance file 
    #
    try:
      f = open(os.path.join(PRIMARY_LOG_DIR, "current.json"), "r+")
      all = json.load(f)
      all[values[3]] = tuple(values)
      f.seek(0,0)
    except:
      f = open(os.path.join(PRIMARY_LOG_DIR, "current.json"), "w+")
      all = {}
    with f:
      json.dump(all, f, indent=2)

    #
    # update secondary log files 
    #

if __name__ == '__main__':
  main()
