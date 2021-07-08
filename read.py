#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import sys
import serial

def device_exists(path):
	"""Test whether a path exists.	Returns False for broken symbolic links"""
	try:
		os.stat(path)
	except OSError:
		return False
	return True

device = "/dev/ttyACM0"
if (device_exists(device) == False):
	print('{} not fond'.format(device))
	sys.exit()

ser = serial.Serial(device, 115200)
loop = True
while(loop) :
	try:
		msg = ser.readline()
		if (type(msg) is bytes):
			msg=msg.decode('utf-8')
		msg = msg.rstrip('\r')
		msg = msg.rstrip('\n')
		print(msg)
	except:
		print('close port')
		loop = False
