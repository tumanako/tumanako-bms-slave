#! /usr/bin/python
import os

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

def booleanToString(b):
	if (b):
		return "01"
	return "00"

def writeData(cellId, kelvinConnection, resistorShunt):
	cellIdHigh = hex((cellId & 0xff00) >> 8)[2:].zfill(2)
	cellIdLow = hex(cellId & 0x00ff)[2:].zfill(2)
	kelvinConnection = booleanToString(kelvinConnection)
	resistorShunt = booleanToString(resistorShunt)
	checksum = "00";

	print "about to write ", cellIdLow, cellIdHigh, kelvinConnection, resistorShunt
	eedata = open("eedata.hex", "w")
	eedata.write(":08422000" + cellIdLow + "00" + cellIdHigh + "00" + kelvinConnection + "00" + resistorShunt + "00" + checksum + "\n")
	eedata.write(":00000001FF")
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

print "cellId:", cellId, "kelvin connection:", kelvinConnection, "resistorShunt", resistorShunt 

if cellId == None or kelvinConnection == None or resistorShunt == None:
	if cellId == None:
		raise ValueError
	
	if kelvinConnection == None:
		kelvinConnection = False

	if resistorShunt == None:
		resistorShunt = True

	writeData(cellId, kelvinConnection, resistorShunt)

print "invoking make"
makeEnv = os.environ.copy()
makeEnv["EXTRA"] = "-DRESISTOR_SHUNT=1"
make = Popen(["make", "clean", "all"], env=makeEnv)
make.wait()
if make.returncode != 0:
	raise ValueError("Make failed")

print check_output(["pk2cmd", "-PPIC16F688", "-E", "-M", "-Z", "-Fevd5.hex", "-R"])
