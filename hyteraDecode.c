/*
 *  Linux DMR Master server
    Copyright (C) 2014 Wim Hofman

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "master_server.h"
#include "hyteraDecode.h"

void decodeHyteraGpsTriggered(int radioId,int destId,struct repeater repeater, unsigned char data[300]){

	struct gpsCoordinates gpsData = {0};

	memcpy(gpsData.latitude,data+33,3);
	memcpy(gpsData.latitude+3,data+38,4);
	memcpy(gpsData.latitude+7,data+32,1);
	memcpy(gpsData.longitude,data+45,8);
	memcpy(gpsData.longitude+8,data+44,1);
	memcpy(gpsData.speed,data+57,3);
	memcpy(gpsData.heading,data+60,3);

	syslog(LOG_NOTICE,"[%s]Decoded GPS data triggered(Hytera): LAT(%s) LONG(%s) SPEED(%s) HEADING(%s)",repeater.callsign,gpsData.latitude,gpsData.longitude,gpsData.speed,gpsData.heading);
	
	if (checkCoordinates(gpsData,repeater) == 1) sendAprs(gpsData,radioId,destId,repeater);
}

void decodeHyteraGpsButton(int radioId,int destId,struct repeater repeater, unsigned char data[300]){

        struct gpsCoordinates gpsData = {0};

        memcpy(gpsData.latitude,data+35,1);
        memcpy(gpsData.latitude+1,data+38,6);
        memcpy(gpsData.latitude+7,data+34,1);
        memcpy(gpsData.longitude,data+47,7);
        memcpy(gpsData.longitude+7,data+56,1);
        memcpy(gpsData.longitude+8,data+46,1);
        memcpy(gpsData.speed,data+59,3);
        memcpy(gpsData.heading,data+62,3);

        syslog(LOG_NOTICE,"[%s]Decoded GPS data button(Hytera): LAT(%s) LONG(%s) SPEED(%s) HEADING(%s)",repeater.callsign,gpsData.latitude,gpsData.longitude,gpsData.speed,gpsData.heading);

        if (checkCoordinates(gpsData,repeater) == 1) sendAprs(gpsData,radioId,destId,repeater);
}


void decodeHyteraGpsCompressed(int radioId,struct repeater repeater, unsigned char data[300]){

	syslog(LOG_NOTICE,"[%s]Compressed GPS data(Hytera) not decoding",repeater.callsign);
}

void decodeHyteraRrs(struct repeater repeater, unsigned char data[300]){
	sqlite3 *dbase;
	sqlite3_stmt *stmt;
	unsigned char callsign[33];
	unsigned char name[33];
	char SQLQUERY[200];
	char timeStamp[20];

	int srcId = 0;

	srcId = data[8] << 16 | data[9] << 8 | data[10];

	dbase = openDatabase();
	sprintf(SQLQUERY,"SELECT callsign,name FROM callsigns WHERE radioId = %i",srcId);
	if (sqlite3_prepare_v2(dbase,SQLQUERY,-1,&stmt,0) == 0){
		if (sqlite3_step(stmt) == SQLITE_ROW){
			sprintf(callsign,"%s",sqlite3_column_text(stmt,0));
			sprintf(name,"%s",sqlite3_column_text(stmt,1));
			sqlite3_finalize(stmt);

		}
		else{
			sqlite3_finalize(stmt);
			syslog(LOG_NOTICE,"[%s]DMR ID %i not found in database",repeater.callsign,srcId);
			closeDatabase(dbase);
			return;
		}
	}
	else{
		syslog(LOG_NOTICE,"[%s]Bad query %s",repeater.callsign,SQLQUERY);
		closeDatabase(dbase);
		return;

	}

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        strftime(timeStamp,sizeof(timeStamp),"%Y-%m-%d %H:%M:%S",t);
	sprintf(SQLQUERY,"REPLACE into rrs (radioId,callsign,name,registerTime,onRepeater,unixTime) VALUES (%i,'%s','%s','%s','%s',%lu)",srcId,callsign,name,timeStamp,repeater.callsign,now);
	if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
		syslog(LOG_NOTICE,"Failed to update rrs in database: %s",sqlite3_errmsg(dbase));
	}
	syslog(LOG_NOTICE,"[%s]Hytera RADIO REGISTER from %i %s",repeater.callsign,srcId,callsign);
	closeDatabase(dbase);
}

void decodeHyteraOffRrs(struct repeater repeater, unsigned char data[300]){
	sqlite3 *dbase;
	sqlite3_stmt *stmt;
	unsigned char callsign[33];
	char SQLQUERY[200];

	int srcId = 0;

	srcId = data[8] << 16 | data[9] << 8 | data[10];

	dbase = openDatabase();
	sprintf(SQLQUERY,"SELECT callsign FROM callsigns WHERE radioId = %i",srcId);
	if (sqlite3_prepare_v2(dbase,SQLQUERY,-1,&stmt,0) == 0){
		if (sqlite3_step(stmt) == SQLITE_ROW){
			sprintf(callsign,"%s",sqlite3_column_text(stmt,0));
			sqlite3_finalize(stmt);

		}
		else{
			sqlite3_finalize(stmt);
			syslog(LOG_NOTICE,"[%s]DMR ID %i not found in database",repeater.callsign,srcId);
			closeDatabase(dbase);
			return;
		}
	}
	else{
		syslog(LOG_NOTICE,"[%s]Bad query %s",repeater.callsign,SQLQUERY);
	}
	sprintf(SQLQUERY,"DELETE FROM rrs where radioId = %i",srcId);
	if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
		syslog(LOG_NOTICE,"Failed to delete rrs in database: %s",sqlite3_errmsg(dbase));
		closeDatabase(dbase);
		return;
	}

	syslog(LOG_NOTICE,"[%s]Hytera RADIO OFFLINE from %i %s",repeater.callsign,srcId,callsign);
	closeDatabase(dbase);
}
