#!/usr/bin/python3
#
# readte.py: read from the TWE-Lite wireless temperature sensor and store the values
#

import serial
import re
import time
import os,sys
import glob
import json

PRIMARY_LOG_DIR = "/var/tmp/twelog/"
PRIMARY_LOG_PERIOD = 3600 * 6
AT_A_GLANCE_FILENAME = "current.json"
SECONDARY_LOG_DIR = "/var/www/twetemp/logs/"
SECONDARY_LOG_DUMP_INTERVAL = 60    # typical 1hour=3600 
LOG_DUMP_DURATION = 60              # resolution 10minutes=600
g_devices = {}
g_secondary_log_dump_at = 0

#
# initialize 
#   -create log directory
#   -read log files if exist
#
def init():
  global g_devices

  os.makedirs(PRIMARY_LOG_DIR, exist_ok=True)
  os.makedirs(SECONDARY_LOG_DIR, exist_ok=True)

  #
  # read from the primary log files
  #
  try:
    for file in glob.glob(os.path.join(PRIMARY_LOG_DIR, "*.json")):
      f = open(file)
      with f:
        # the json should be the list of array like
        # [['2016/03/22 13:07:00', 1458619620, 5, '810149B3', 3330, 1801, 111, 0],...]
        #    date/time, epoch time, route, ed, battery, temperature, lq, flag
        dev = os.path.splitext(os.path.basename(f.name))[0]
        g_devices[dev] = json.load(f)
  except:
    print("something wrong with log files")

  #print(g_devices)

#
# read the value from the USB sensor device
#
# to figure out the device name, dmesg and find a message like this
# "usb 1-1.2: FTDI USB Serial Device converter now attached to ttyUSB0"
#
def readtemp():
  try:
    if not hasattr(readtemp, "pattern"):
      readtemp.pattern = re.compile('rc=(\w+):.*lq=(\d+):.*ed=(\w+):.*ba=(\d+):.*:te=(\d+)')
    if not hasattr(readtemp, "port"):
      readtemp.port = serial.Serial("/dev/ttyUSB0", 115200)
    data = readtemp.port.readline().decode("utf-8")
    #print(data)
    m = readtemp.pattern.search(data)
    if m:
      epoch = int(time.time())
      rc = m.group(1)
      lq = int(m.group(2))
      end_device = m.group(3)
      battery = int(m.group(4))
      temperature = int(m.group(5))
      route = 0
      if rc == '80000000':           # parent
        route = 1
      elif rc == '8100D797':         # router1
        route = 2
      elif rc == '8100978C':         # router2
        route = 4
      timestr = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(epoch))
      return [timestr, epoch, route, end_device, battery, temperature, lq, 0]
    else:
      print("read invalid line")
  except:
    print("exception while accessing /dev/ttyUSB")
    sys.exit()

def update_secondary_log_files(now):
  print("update_secondary_log_files")
  global g_devices
  for devname in g_devices:
    dict = {}
    for values in g_devices[devname]:
      # flag:0 means has not been dumped yet
      if values[7] == 0 and int(values[1]/LOG_DUMP_DURATION) < int(now/LOG_DUMP_DURATION):
        key = int(values[1]/LOG_DUMP_DURATION)*LOG_DUMP_DURATION
        if not key in dict:
          dict[key] = []
        dict[key].append(values[5])
        values[7] = 1
    
    filename = ""
    f = None
    for key, values in dict.items():
      name = time.strftime("%Y-%m-%d", time.localtime(key))
      if name != filename:
        filename = name
        os.makedirs(os.path.join(SECONDARY_LOG_DIR, devname), exist_ok=True)
        if f:
          f.close()
        print("opening:", os.path.join(SECONDARY_LOG_DIR, devname, filename))
        f = open(os.path.join(SECONDARY_LOG_DIR, devname, filename), "a")
      f.write(str(key) + ',' + str(round(sum(values)/(100.0*len(values)), 1)) +'\n')
    if f:
      f.close()

def main():  
  global g_devices
  global g_secondary_log_dump_at

  init()
  g_secondary_log_dump_at  = int(time.time())

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
    # 6: link_quality(signal strength) int
    # 7: flag int
    #print(values)
    devname = values[3]

    if not devname in g_devices:
      device = []
      device.append(values)
      g_devices[devname] = device
    else:
      device = g_devices[devname]
      latest = device[-1]
      if latest[1] > (values[1] - 2):
        # add route
        values[2] |= latest[2]
        # retain the maximum signal strength
        if values[6] < latest[6]:
          values[6] = latest[6]  
        device[-1] = values
      else:
        device.append(values)
    print(values)

    #
    # update secondary log files 
    #
    now = int(time.time()) 
    if now > (g_secondary_log_dump_at + SECONDARY_LOG_DUMP_INTERVAL):
      g_secondary_log_dump_at  = now
      update_secondary_log_files(now)

    #
    # update the primary log files
    #
    # mark old entries
    i = 0
    recentenough = 0
    for item in device:
      if item[1] > (device[-1][1] - PRIMARY_LOG_PERIOD):
        recentenough = i
        break
    #print("recentenough=", recentenough)
    f = open(os.path.join(PRIMARY_LOG_DIR, devname + ".json"), "w")
    with f:
      json.dump(device[recentenough:], f, indent=2)

    #
    # update at_a_glance file 
    #
    try:
      f = open(os.path.join(PRIMARY_LOG_DIR, AT_A_GLANCE_FILENAME), "r+")
      all = json.load(f)
      all[devname] = values
      f.seek(0,0)
    except:
      f = open(os.path.join(PRIMARY_LOG_DIR, AT_A_GLANCE_FILENAME), "w+")
      all = {}
    with f:
      json.dump(all, f, indent=2)


if __name__ == '__main__':
  main()
