#! /usr/bin/python
import os
import sys
import struct
import pysvn
import time
from subprocess import Popen
from subprocess import check_output
from subprocess import check_call

class Cell:
	def __init__(self):
		data = readData(15, 12)
		self.cellId = data[1] + data[2] * 16
		if self.cellId == 0xffff:
			self.cellId
		self.kelvinConnection = data[3] == 1
		self.resistorShunt = data[4] == 1
	
	def __str__(self):
		return "cellId: " + str(self.cellId) + " kelvin connection: " + str(self.kelvinConnection) + " resistorShunt " + str(self.resistorShunt)

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
	
	newConfig = Cell()
	if cellId != newConfig.cellId:
		raise ValueError("expected cell id " + cellId + " but got " + newConfig)
	if kelvinConnection != newConfig.kelvinConnection:
		raise ValueError("expected kelvin connection " + kelvinConnection + " but got " + newConfig)
	if resistorShunt == newConfig.resistorShunt:
		raise ValueError("expected resistor shunt " + str(resistorShunt) + " but got " + str(newConfig))

def readData(address, length):
	# we read 64 bytes and then return the data requested
	if (length + address > 64):
		raise ValueError("cannot read mroe than 64 bytes")
	output = check_output(["pk2cmd", "-PPIC16F688", "-GE00-3f", "-R"])
	lines = output.splitlines()
	if not lines[0].strip().endswith("Read successfully."):
		raise ValueError("'" + lines[0] + "'\n" + output)
	data = []
	for line in lines[3:11]:
		# 0010 4B  00  00  01  CB  01  00  14
		lineData = line.strip().replace("  ", " ").split(" ")
		if len(lineData) != 9:
			raise ValueError("Expected 9 values in " + lineData)
		for value in lineData[1:9]:
			data.append(int(value, 16))
	return data[address:address + length]	
	
initialConfig = Cell();
print "found ", initialConfig

if (len(sys.argv) > 1):
	cellId = int(sys.argv[1])
else:
	cellId = initialConfig.cellId

if cellId == None:
	raise ValueError
	
if initialConfig.kelvinConnection == None:
	kelvinConnection = False
else:
	kelvinConnection = initialConfig.kelvinConnection

if initialConfig.resistorShunt == None:
	resistorShunt = True
else:
	resistorShunt = initialConfig.resistorShunt

print "writing cellId:", cellId, "kelvin connection:", kelvinConnection, "resistorShunt", resistorShunt 
writeData(cellId, kelvinConnection, resistorShunt)

revision = getRevision()
isClean = getIsClean()
programDate = long(time.time());

extra = "-DCELL_ID_LOW=" + str(cellId & 0xff)
extra = extra + " -DCELL_ID_HIGH=" + str(cellId >> 8)
extra = extra + " -DREVISION_LOW=" + str(revision & 0xff)
extra = extra + " -DREVISION_HIGH=" + str(revision >> 8)
extra = extra + " -DIS_CLEAN=" + str(int(isClean))
extra = extra + " -DPROGRAM_DATE_0=" + str(programDate >> 0 & 0xff)
extra = extra + " -DPROGRAM_DATE_1=" + str(programDate >> 8 & 0xff)
extra = extra + " -DPROGRAM_DATE_2=" + str(programDate >> 16 & 0xff)
extra = extra + " -DPROGRAM_DATE_3=" + str(programDate >> 24 & 0xff)
if resistorShunt:
	extra = extra + " -DRESISTOR_SHUNT=1"
makeEnv = os.environ.copy()
makeEnv["EXTRA"] = extra
make = Popen(["make", "clean", "all"], env=makeEnv)
make.wait()
if make.returncode != 0:
	raise ValueError("Make failed")

print check_output(["pk2cmd", "-PPIC16F688", "-E", "-M", "-Z", "-Fevd5.hex", "-R"])
