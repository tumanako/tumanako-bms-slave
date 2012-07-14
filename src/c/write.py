#! /usr/bin/python
import os
import sys
import struct
import pysvn
import time
from subprocess import Popen
from subprocess import check_output
from subprocess import check_call

def getEEData(output, addresses):
	for l in output.splitlines():
		if l.startswith("0010"):
			words = l.split(" ")
			result = []
			for address in addresses:
				result.append(words[address])
			return result
	raise ValueError(output)
	
def getCellId(output):
	data = getEEData(output, [3, 1])
	cellId = int(data[0] + data[1], 16)
	if cellId == 0xffff:
		return None
	return cellId

def getKelvinConnection(output):
	return getBoolean(output, 5)

def getResistorShunt(output):
	return getBoolean(output, 7)

def getBoolean(output, address):
	value = getEEData(output, [address])
	if value[0] == "00":
		return False
	if value[0] == "01":
		return True
	return None

def getRevision():
	client = pysvn.Client()
	entry = client.info('.')
	return entry.revision.number

def getIsClean():
	client = pysvn.Client()
	statuses = client.status(".")
	for status in statuses:
		if status.is_versioned:
			if status.text_status == pysvn.wc_status_kind.modified:
				return False
	return True
	
def makeEEDataHex(address, data):
	# TODO use the address
	h = ":%02x422000" % (len(data) * 2)
	for b in data:
		h = h + b.encode("hex") + "00"
	h = h + "00\n"
	h = h + ":00000001FF"
	return h

def writeData(cellId, kelvinConnection, resistorShunt):
	revision = getRevision()
	isClean = getIsClean()
	print "about to write", cellId, kelvinConnection, resistorShunt, revision, isClean
	
	data = struct.pack("<HBBHBL", cellId, kelvinConnection, resistorShunt, revision, isClean, long(time.time()))
	
	eedata = open("eedata.hex", "w")
	eedata.write(makeEEDataHex(10, data))
	eedata.close()
	print check_output(["pk2cmd", "-PPIC16F688", "-M", "-Feedata.hex", "-R"])
	
	output = check_output(["pk2cmd", "-PPIC16F688", "-GE10-17", "-R"])
	if cellId != getCellId(output):
		raise ValueError("expected cell id " + cellId + " but got " + getCellId(output) + " " + output)
	if kelvinConnection != getKelvinConnection(output):
		raise ValueError("expected kelvin connection " + kelvinConnection + " but got " + str(getKelvinConnection(output)) + " " + output)
	if resistorShunt != getResistorShunt(output):
		raise ValueError("expected resistor shunt " + resistorShunt + " but got " + str(resistorShunt(output)) + " " + output)

output = check_output(["pk2cmd", "-PPIC16F688", "-GE10-17", "-R"])
cellId = getCellId(output)
kelvinConnection = getKelvinConnection(output)
resistorShunt = getResistorShunt(output)

print "found cellId:", cellId, "kelvin connection:", kelvinConnection, "resistorShunt", resistorShunt 

if (len(sys.argv) > 1):
	if (sys.argv[1] != cellId):
		cellId = int(sys.argv[1])

if cellId == None:
	raise ValueError
	
if kelvinConnection == None:
	kelvinConnection = False

if resistorShunt == None:
	resistorShunt = True

print "writing cellId:", cellId, "kelvin connection:", kelvinConnection, "resistorShunt", resistorShunt 
writeData(cellId, kelvinConnection, resistorShunt)

extra = "-DCELL_ID_LOW=" + str(cellId & 0xff) + " -DCELL_ID_HIGH=" + str(cellId >> 8)
if resistorShunt:
	extra = extra + " -DRESISTOR_SHUNT=1"
makeEnv = os.environ.copy()
makeEnv["EXTRA"] = extra
make = Popen(["make", "clean", "all"], env=makeEnv)
make.wait()
if make.returncode != 0:
	raise ValueError("Make failed")

print check_output(["pk2cmd", "-PPIC16F688", "-E", "-M", "-Z", "-Fevd5.hex", "-R"])
