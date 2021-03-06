#!/usr/bin/python3
#
# publish.py: read current.json and publish to IOT clouds
# it is supposed to be called every one minute or so
#

import os,sys
import json
from pubnub import Pubnub
import requests

import properties

PUBLISH_KEY = properties.PubnubKeys['PUBLISH_KEY']
SUBSCRIBE_KEY = properties.PubnubKeys['SUBSCRIBE_KEY']

def _callback(message):
  print(message)
  
def _error(message):
  print("ERROR : " + str(message))
  
def _connect(message):
  print("CONNECTED")
  
def _reconnect(message):
  print("RECONNECTED")
  
def _disconnect(message):
  print("DISCONNECTED")

def pubnub_publish(data):
  pubnub = Pubnub(publish_key=PUBLISH_KEY, subscribe_key=SUBSCRIBE_KEY)
  pubnub.publish('khaus', data, callback=_callback, error=_error)

def dweet_publish(data):
  for node in data:
    val = data[node]
    param = '?'.join(['temperature=' + str(val['temperature']),
                     '&datetime=' + str(val['datetime']), 
                     '&battery=' + str(val['battery']),
                     '&signal=' + str(val['signal']),
                     '&route=' + str(val['route'])])
    print(param)  
    requests.get('http://dweet.io/dweet/for/khaus_' + node +  param)

def main():
  FILE_PATH = "/var/tmp/twelog/current.json"
  file = open(FILE_PATH, "r")
  with file:
    data = json.load(file)
  print(data)  
  pubnub_publish(data)
  dweet_publish(data)

if __name__ == '__main__':
  main()
