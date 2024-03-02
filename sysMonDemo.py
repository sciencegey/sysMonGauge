#!/usr/bin/env python3

# MIT License
# 
# Copyright (c) 2024 sciencegey
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import sys
import os

import psutil
import time
import serial, serial.serialutil, serial.tools.list_ports
import struct
import random

serWait = True
serPort = "COM1"
while serWait:
    for port in list(serial.tools.list_ports.comports()):
        if port.vid == 0x2e8a and port.pid == 0x4826:
            serPort = port.device
            serWait = False
    
    time.sleep(5)

ser = serial.Serial(port=serPort, baudrate=115200)
diskPartitions = psutil.disk_partitions()

for i in range(len(psutil.disk_partitions())):
    if os.name == 'nt' and psutil.disk_partitions()[i].fstype == '':
        diskPartitions.remove(psutil.disk_partitions()[i])

diskReadLast = 0
diskWriteLast = 0
networkSentLast = 0
networkRecvLast = 0

# cpuUsage, ramUsage, diskUsage, diskRead, diskWrite, networkSent, networkRecv, systemUptime
values = [0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00]

def translateValue(inValue=0,inMin=0,inMax=100,outMin=0,outMax=255):
    oldValue = inValue
    oldMin = inMin
    oldMax = inMax
    newMin = outMin
    newMax = outMax
    
    newValue = (((oldValue - oldMin) * (newMax - newMin)) / (oldMax - oldMin)) + newMin
    
    if newValue > outMax:
        newValue = outMax
    
    return int(newValue)

now = int(time.time())

cpuUsage = 0
ramUsage = 0
cpuFreq = 0
cpuFreqMax = 0
diskRead = 0
diskWrite = 0
networkSent = 0
networkRecv = 0
diskUsage = [0]*len(diskPartitions)

while True:
    try:
        if int(time.time()) - now >= 0.25:
            cpuUsage = random.uniform(0,100)
            cpuFreq = random.uniform(0,4500)
            cpuFreqMax = 4500.0
            ramUsage = random.uniform(0,100)
        
            now = int(time.time())

            diskRead = random.randint(0,1000)
            diskWrite = random.randint(0,1000)
            
            networkSent = random.randint(0,1000)
            networkRecv = random.randint(0,1000)
        systemUptime = int(time.time())-int(psutil.boot_time()) # Difference in time of system startup vs current time
        

        # Check if the counters returned a value, and if they did then store them in the array
        if isinstance(cpuUsage, float): values[0] = translateValue(int(cpuUsage), inMax=100)
        if isinstance(ramUsage, float): values[1] = translateValue(int(ramUsage), inMax=100)
        #if isinstance(diskUsageTotal, float): values[6] = translateValue(int(diskUsageTotal/len(diskPartitions)), inMax=100)   # Gets an average of all disk usage across the system
        if isinstance(diskRead, int): values[2] = translateValue(diskRead, inMax=1000)
        if isinstance(diskWrite, int): values[3] = translateValue(diskWrite, inMax=1000)
        if isinstance(networkSent, int): values[4] = translateValue(networkSent, inMax=1000)
        if isinstance(networkRecv, int): values[5] = translateValue(networkRecv, inMax=1000)
        
        # The variables will be encoded as a "long" (4 bytes)
        if isinstance(systemUptime, int): values[7] = int(systemUptime)
        if isinstance(diskRead, int): values[8] = int(diskRead)
        if isinstance(diskWrite, int): values[9] = int(diskWrite)
        if isinstance(networkSent, int): values[10] = int(networkSent)
        if isinstance(networkRecv, int): values[11] = int(networkRecv)
        
        # But these ones will be encoded as a "short" (2 bytes)
        if isinstance(cpuFreq, float): values[12] = int(cpuFreq)
        if isinstance(cpuFreqMax, float): values[13] = int(cpuFreqMax)
        
        # ser.write(bytes(values[:7])+struct.pack('<5L2H', *values[7:])+bytes(len(diskPartitions))+bytes(diskUsage))
        ser.write(bytes(values[:7])+struct.pack('<5L2H', *values[7:]))
        # print("Sent!")
        # print(values)

    except KeyboardInterrupt:
        break

    except serial.serialutil.SerialException:
        # If the pico is unplugged, for now just exit cleanly
        break

# Close the serial connection nicely 
try:
    ser.close()
except:
    print("Couldn't close the port!")
