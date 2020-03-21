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
#include "users.h"

void loadUsersToFile(){
    CURL *curl;
    FILE *fpu;
    CURLcode res;
    char userfilename[20] = "user.db";
    curl = curl_easy_init();
	syslog(LOG_NOTICE,"Start retrieving users\n");
    if (curl) {
        fpu = fopen(userfilename,"wb");
        curl_easy_setopt(curl, CURLOPT_URL, "http://status.ircddb.net/dmr-user.php" );
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fpu);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fpu);
    }
	syslog(LOG_NOTICE,"End retrieving users\n");
}

void importUsers(){
	FILE *fpr;
	sqlite3 *dbase;
	char line[500];
	char SQLQUERY[500];
	char *radioId,*callsign,*name,*date,*running;
	int rc;
	
	char userfilename[20] = "user.db";
	fpr = fopen(userfilename,"rb");
	bzero(line,sizeof(line));
	
	dbase = openDatabase();
	syslog(LOG_NOTICE,"Start importing users to database\n");
	sprintf(SQLQUERY,"DELETE FROM callsigns");
	if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
		syslog(LOG_NOTICE,"Failed to delete entries in table callsigns: %s\n",sqlite3_errmsg(dbase));
		return;
	}
	sprintf(SQLQUERY,"BEGIN");
	if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
		syslog(LOG_NOTICE,"Failed to start transaction: %s\n",sqlite3_errmsg(dbase));
		return;
	}
	while (!feof(fpr)){
		fgets(line,500,fpr);
		running = strdup(line);
		radioId = strsep(&running,"@");
		callsign = strsep(&running,"@");
		date = strsep(&running,"@");
		name = strsep(&running,"@");
		sprintf(SQLQUERY,"INSERT INTO callsigns (radioId,callsign,name) VALUES (%s,'%s','%s')",radioId,callsign,name);
			if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
			syslog(LOG_NOTICE,"Failed to add user in table callsigns: %s\n",sqlite3_errmsg(dbase));
		}
	}
	sprintf(SQLQUERY,"COMMIT");
	if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
		syslog(LOG_NOTICE,"Failed to commit: %s\n",sqlite3_errmsg(dbase));
	}
	syslog(LOG_NOTICE,"End importing users to database\n");
	fclose(fpr);
	closeDatabase(dbase);
}

void importTalkGroups(){
	FILE *fpr;
	sqlite3 *dbase;
	char line[500];
	char SQLQUERY[500];
	char *id,*name,*running;;
	int rc;

	char userfilename[20] = "talkgroups.db";
	fpr = fopen(userfilename,"rb");
	bzero(line,sizeof(line));

	dbase = openDatabase();
	syslog(LOG_NOTICE,"Start importing talkgroups to database\n");
	sprintf(SQLQUERY,"BEGIN");
	if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
		syslog(LOG_NOTICE,"Failed to start transaction: %s\n",sqlite3_errmsg(dbase));
		return;
	}
	while (!feof(fpr)){
		fgets(line,500,fpr);
		running = strdup(line);
		id = strsep(&running,";");
		name = strsep(&running,";");
		sprintf(SQLQUERY,"INSERT INTO callsigns (radioId,callsign,name) VALUES (%s,'%s','%s')",id,name,name);
			if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
			syslog(LOG_NOTICE,"Failed to add talkgroup in table callsigns: %s\n",sqlite3_errmsg(dbase));
		}
	}
	sprintf(SQLQUERY,"COMMIT");
	if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
		syslog(LOG_NOTICE,"Failed to commit: %s\n",sqlite3_errmsg(dbase));
	}
	syslog(LOG_NOTICE,"End importing talkgroups to database\n");
	fclose(fpr);
	closeDatabase(dbase);

}
