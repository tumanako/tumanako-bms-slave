/*
    Copyright 2009 Tom Parker

    This file is part of the Tumanako EVD5 BMS.

    The Tumanako EVD5 BMS is free software: you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    The Tumanako EVD5 BMS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with the Tumanako EVD5 BMS.  If not, see 
    <http://www.gnu.org/licenses/>.
*/

struct evd5_status_t {
	unsigned short cellAddress;
	unsigned short iShunt;
	unsigned short vCell;
	unsigned short vShunt;
	unsigned short temperature;
	unsigned short minCurrent;
	unsigned char sequenceNumber;
	// lower numbers == less gain
	char gainPot;
	// lower numbers == less voltage
	char vShuntPot;
	// true if we have received a character since the last time loopCounter overflowed
	unsigned char hasRx;
	// true if we are doing software addressing
	unsigned char softwareAddressing;
	// true if we are controlling the shunt current automatically
	unsigned char automatic;
};

#define EVD5_STATUS_LENGTH 18