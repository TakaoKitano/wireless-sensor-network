#!/usr/bin/env python3

import glob
import os
import datetime
import calendar
import time
import sys
import json
import shutil
from collections import OrderedDict
from datetime import date, timedelta

DIRECTORY = '/var/www/twetemp/logs/'

def do_records(f, logs, duration):
    records = json.load(f, object_pairs_hook=OrderedDict)
    nodes = {}
    epoch = int(list(records.items())[0][0])
    for key in records:
        # merge multiple keys
        if int(epoch/duration) != int(int(key)/duration):
            #
            #  put nodes values into logs
            #
            logs[epoch] = {}
            for nodename in nodes:
                value = round(sum(nodes[nodename])/len(nodes[nodename]), 1)
                #  put average value into logs
                logs[epoch][nodename] = {"temperature":value}
            epoch = int(key)
            nodes = {}
        for nodename in records[key]:
            if not nodename in nodes:
                nodes[nodename] = []
            # push temperatures in this duration
            temp = records[key][nodename]['temperature']
            if (temp > -10) and (temp < 60):
                nodes[nodename].append(temp)
    return logs

def merge_days(since, to, duration):
    print (since.strftime("%Y-%m-%d"), " - ", to.strftime("%Y-%m-%d"), " duration:", duration)
    logs = OrderedDict()
    day = since
    while day <= to:
        print (day)
        try:
            with open(os.path.join(DIRECTORY, day.strftime("%Y-%m-%d.json"))) as f:
                logs = do_records(f, logs, duration)
        except IOError:
            print ("no such file")
        day = day + timedelta(days=1)
    return logs

def merge_months(year, duration):
    print ("merge ", year, " duration:", duration)
    logs = OrderedDict()
    month = 1
    while month <= 12:
        day = datetime.date(year,month,1)
        print (day.strftime("%Y-%m.json"))
        try:
            with open(os.path.join(DIRECTORY, day.strftime("%Y-%m.json"))) as f:
                logs = do_records(f, logs, duration)
        except IOError:
            print ("no such file")
        month = month + 1
    return logs

def createMonth(year, month):
    duration = 7200
    since = datetime.date(year,month,1)
    to = datetime.date(year,month,calendar.monthrange(year,month)[1])
    logs = merge_days(since, to, duration)
    filepath = os.path.join(DIRECTORY, since.strftime("%Y-%m.json"))
    with open(filepath, "w") as f:
      json.dump(logs,f,indent=2)

def createYear(year):
    duration = 3600 * 24
    logs = merge_months(year, duration)
    day = datetime.date(year,1,1)
    filepath = os.path.join(DIRECTORY, day.strftime("%Y.json"))
    with open(filepath, "w") as f:
      json.dump(logs,f,indent=2)

if __name__ == '__main__':
    yesterday = datetime.date.today() - datetime.timedelta(days=1)
    createMonth(yesterday.year, yesterday.month)
    createYear(yesterday.year)
