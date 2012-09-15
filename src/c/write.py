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
	def read(self):
		data = readData(15, 12)
		if data[0] == 0xff:
			self.configVersion = 0
		else:
			raise ValueError("unknown config verison in " + str(data))
		self.cellId = data[1] + data[2] * 256
		if self.cellId == 0xffff:
			self.cellId
		self.kelvinConnection = data[3] == 1
		self.resistorShunt = data[4] == 1
		self.revision = data[5] + data[6] * 256
		self.isClean = data[7] == 1
		self.whenProgrammed = data[11]
		self.whenProgrammed = self.whenProgrammed * 256 + data[10]
		self.whenProgrammed = self.whenProgrammed * 256 + data[9]
		self.whenProgrammed = self.whenProgrammed * 256 + data[8]
	
	def write(self):
		data = struct.pack("<HBBHBL", self.cellId, self.kelvinConnection, self.resistorShunt, self.revision, self.isClean, self.whenProgrammed)
		writeData(16, data)

		newConfig = Cell()
		newConfig.read()
		if self.cellId != newConfig.cellId:
			raise ValueError("expected cell id " + str(self.cellId) + " but got " + str(newConfig))
		if self.kelvinConnection != newConfig.kelvinConnection:
			raise ValueError("expected kelvin connection " + str(self.kelvinConnection) + " but got " + str(newConfig))
		if self.resistorShunt != newConfig.resistorShunt:
			raise ValueError("expected resistor shunt " + str(self.resistorShunt) + " but got " + str(newConfig))

	def __str__(self):
		return "configVersion: " + str(self.configVersion) + " cellId: " + str(self.cellId) + " kelvin: " + str(self.kelvinConnection) + " resistorShunt: " + str(self.resistorShunt) + " r" + str(self.revision) + " isClean: " + str(self.isClean) + " whenProgrammed: " + str(self.whenProgrammed)

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

def writeData(address, data):
	print "about to write", len(data), "bytes to", address
	eedata = open("eedata.hex", "w")
	eedata.write(makeEEDataHex(address, data))
	eedata.close()
	print check_output(["pk2cmd", "-PPIC16F688", "-M", "-Feedata.hex", "-R"])
	
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
	
config = Cell()
config.read()
print "found  ", config

if (len(sys.argv) > 1):
	config.cellId = int(sys.argv[1])

if config.cellId == None:
	raise ValueError
if config.kelvinConnection == None:
	config.kelvinConnection = False
if config.resistorShunt == None:
	config.resistorShunt = True

config.revision = getRevision()
config.isClean = getIsClean()
config.whenProgrammed = long(time.time());

print "writing", str(config)
config.write()

extra = "-DCELL_ID_LOW=" + str(config.cellId & 0xff)
extra = extra + " -DCELL_ID_HIGH=" + str(config.cellId >> 8)
extra = extra + " -DREVISION_LOW=" + str(config.revision & 0xff)
extra = extra + " -DREVISION_HIGH=" + str(config.revision >> 8)
extra = extra + " -DIS_CLEAN=" + str(int(config.isClean))
extra = extra + " -DPROGRAM_DATE_0=" + str(config.whenProgrammed >> 0 & 0xff)
extra = extra + " -DPROGRAM_DATE_1=" + str(config.whenProgrammed >> 8 & 0xff)
extra = extra + " -DPROGRAM_DATE_2=" + str(config.whenProgrammed >> 16 & 0xff)
extra = extra + " -DPROGRAM_DATE_3=" + str(config.whenProgrammed >> 24 & 0xff)
if config.resistorShunt:
	extra = extra + " -DRESISTOR_SHUNT=1"
makeEnv = os.environ.copy()
makeEnv["EXTRA"] = extra
make = Popen(["make", "clean", "all"], env=makeEnv)
make.wait()
if make.returncode != 0:
	raise ValueError("Make failed")

print check_output(["pk2cmd", "-PPIC16F688", "-E", "-M", "-Z", "-Fevd5.hex", "-R"])
