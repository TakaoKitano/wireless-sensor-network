#!/usr/bin/python3
#
# pubambient.py: read current.json and publish to ambient server
# it is supposed to be called every one minute or so
#

import os,sys
import json
import requests
import properties
from ambient import ambient
from collections import OrderedDict

USERKEY=properties.AmbientKeys['USERKEY']
CHANNELID=properties.AmbientKeys['CHANNELID']
READKEY=properties.AmbientKeys['READKEY']
WRITEKEY=properties.AmbientKeys['WRITEKEY']

def main():
  FILE_PATH = "/var/tmp/twelog/current.json"
  file = open(FILE_PATH, "r")
  with file:
    data = json.load(file)
  #print(data)  
  # the logic below assumes that data contains extactly 8 nodes
  send_data = {}
  # 'F390':('d1', "3Fベッドルーム"),
  # '14AF':('d2', "EXT和室床下"),
  # 'DFA5':('d3', "3Fバスルーム"),  
  # '15AC':('d4', "1F書斎"),
  # 'F2B6':('d5', "3Fアトリウム"), 
  # 'B356':('d6', "2Fダイニング"),
  # 'B0E9':('d7', "2Fリビング"),
  # '1618':('d8', "EXT室外"),
  labels = {'F390':'d1','14AF':'d2','DFA5':'d3','15AC':'d4','F2B6':'d5','B356':'d6','B0E9':'d7','1618':'d8'}
  for node in data:
    #print(node, data[node]['temperature'])  
    send_data[labels[node]]= data[node]['temperature']
  print(send_data)

  am = ambient.Ambient(CHANNELID, WRITEKEY)
  am.send(send_data)

if __name__ == '__main__':
  main()
