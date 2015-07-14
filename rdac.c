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


sqlite3 *openDatabase();
void closeDatabase();
void discard();
void delRepeaterByPos();
int findRepeater();

int select_str(char *s)
{
	int i;

	char *accepted_strings[] = {
	"$repeaterId\n",
	"$callsign\n",
	"$hardware\n",
	"$firmware\n",
	"$mode\n",
	"$txfreq\n",
	"$rxfreq\n"
	};
	
	for (i=0; i < sizeof(accepted_strings)/sizeof(*accepted_strings);i++){
		if (!strcmp(s, accepted_strings[i])) return i;
	}
	return -1;
}

bool getRepeaterInfo(int sockfd,int repPos,struct sockaddr_in cliaddrOrg,sqlite3 *dbase){
	FILE *fp;
	struct sockaddr_in cliaddr;
	char *lineread;
	int cc,i,rc,n;
	unsigned char fl[256] = {0};
	unsigned char line[400];
	unsigned char buffer[256] ={0};
	fd_set master;
	struct timeval timeout;
	char callsignUtf16[20] = {0};
	char callsignUtf8[12] = {0};
	char hardwareUtf16[10] = {0};
	char hardwareUtf8[7] = {0};
	char firmwareUtf16[24] = {0};
	char firmwareUtf8[14] = {0};
	char timeStamp[20];
	double txFreq;
	double rxFreq;
	double shift;
	int cnt;
	char str[INET_ADDRSTRLEN];
	socklen_t len;
	size_t srclen,dstlen;
	sqlite3_stmt *stmt;
	char SQLQUERY[300];

	inet_ntop(AF_INET, &(cliaddrOrg.sin_addr), str, INET_ADDRSTRLEN);
	FD_ZERO(&master);
	len = sizeof(cliaddr);
	fp=fopen("rdac.in","r");
	if (fp == NULL){
		syslog(LOG_NOTICE,"rdac.in file not found in Master_Server directory, please copy this file");
		return false;
	}
	while (!feof(fp)){
		if (fgets(line,400,fp) != NULL){
			lineread = strtok(line,":");
			if(!strcmp(lineread,"R")){		//See if line is something we need to receive
				cc = 0;
				while(lineread = strtok(NULL,":")){
					fl[cc] = strtol(lineread,NULL,16);
					cc++;
				}
				cnt = 0;
				do{
					FD_SET(sockfd, &master);
					timeout.tv_sec = 5;
					timeout.tv_usec = 0;
					if (rc = select(sockfd+1, &master, NULL, NULL, &timeout) == -1) { 
						return false;
					}
		
					if (FD_ISSET(sockfd,&master)) {
						n = recvfrom(sockfd,buffer,256,0,(struct sockaddr *)&cliaddr,&len);
						cnt++;
					
						if (cliaddr.sin_addr.s_addr == cliaddrOrg.sin_addr.s_addr){
							if (memcmp(buffer,fl,cc) != 0){ 
								fclose(fp);
								return false;  //did not receive expected info	
							}
							//printf("Received string we are waiting for\n");
						}
					}
					else{
						//timeout
						fclose(fp);
						return false;
					}
				} while(cliaddr.sin_addr.s_addr != cliaddrOrg.sin_addr.s_addr && cnt < 5);
				continue;
			}
			if(!strcmp(lineread,"S")){		//See if line is something we need to send
				cc = 0;
				while(lineread = strtok(NULL,":")){
					fl[cc] = strtol(lineread,NULL,16);
					cc++;
				}
				sendto(sockfd,fl,cc,0,(struct sockaddr *)&cliaddrOrg,sizeof(cliaddrOrg));
				continue;
			}
			if(!strcmp(lineread,"E")){		//See if line is something we need to extract
				while(lineread = strtok(NULL,":")){
					//printf("Extracting %s\n",lineread);
					switch (select_str(lineread)){
						case 0:  //0 = repeaterId
						repeaterList[repPos].id = buffer[20] << 16 | buffer[19] << 8 | buffer[18];
						syslog(LOG_NOTICE,"Assigning id %i to repeater on pos %i [%s]",repeaterList[repPos].id,repPos,str);
						sprintf(SQLQUERY,"SELECT repeaterId FROM repeaters WHERE repeaterId = %i",repeaterList[repPos].id);
						if (sqlite3_prepare_v2(dbase,SQLQUERY,-1,&stmt,0) == 0){
							if (sqlite3_step(stmt) != SQLITE_ROW){
								syslog(LOG_NOTICE,"Repeater with id %i not known in database, removing from list pos %i [%s]",repeaterList[repPos].id,repPos,str);
								delRepeaterByPos(repPos);
								fclose(fp);
								syslog(LOG_NOTICE,"Setting repeater in discard list [%s]",str);
								discard(cliaddrOrg);
								close(sockfd);
								sqlite3_finalize(stmt);
								closeDatabase(dbase);
								pthread_exit(NULL);
							}
							sqlite3_finalize(stmt);
						}
						break;
						
						case 1: {//1 = callsign
						memcpy(callsignUtf16,buffer + 0x58,20);
						char * pIn = callsignUtf16;
						char * pOut = (char*)callsignUtf8;
						srclen = 20;
						dstlen = 10;
						iconv_t conv = iconv_open("UTF-8","UTF-16LE");
						iconv(conv, &pIn, &srclen, &pOut, &dstlen);
						iconv_close(conv);
						sprintf(repeaterList[repPos].callsign,"%s",callsignUtf8);
						break;}
						
						case 2: {//2 = hardware
						memcpy(hardwareUtf16,buffer + 0x78,10);
						char * pIn = hardwareUtf16;
						char * pOut = (char*)hardwareUtf8;
						srclen = 10;
						dstlen = 5;
						iconv_t conv = iconv_open("UTF-8","UTF-16LE");
						iconv(conv, &pIn, &srclen, &pOut, &dstlen);
						iconv_close(conv);
						sprintf(repeaterList[repPos].hardware,"%s",hardwareUtf8);
						break;}

						case 3: {//2 = firmware
						memcpy(firmwareUtf16,buffer + 0x38,24);
						char * pIn = firmwareUtf16;
						char * pOut = (char*)firmwareUtf8;
						srclen = 24;
						dstlen = 12;
						iconv_t conv = iconv_open("UTF-8","UTF-16LE");
						iconv(conv, &pIn, &srclen, &pOut, &dstlen);
						iconv_close(conv);
						sprintf(repeaterList[repPos].firmware,"%s",firmwareUtf8);
						break;}
						
						case 4: { //3 = mode
						if (buffer[26] == 0) sprintf(repeaterList[repPos].mode,"DIG");
						if (buffer[26] == 1) sprintf(repeaterList[repPos].mode,"ANA");
						if (buffer[26] == 2) sprintf(repeaterList[repPos].mode,"MIX");
						break;}
						
						case 5: {//4 = txfreq
						txFreq = buffer[32] << 24 | buffer[31] << 16 | buffer[30] << 8 | buffer[29];
						txFreq = txFreq/1000000;
						sprintf(repeaterList[repPos].txFreq,"%.4f",txFreq);
						break;}
						
						case 6: {//5 = rxfreq
						rxFreq = buffer[36] << 24 | buffer[35] << 16 | buffer[34] << 8 | buffer[33];
						rxFreq = rxFreq/1000000;
						shift = rxFreq - txFreq;
						sprintf(repeaterList[repPos].shift,"%1.1f",shift);
						repeaterList[repPos].rdacUpdated = true;
						break;}
					}
				}
			}
		}
	}
	syslog(LOG_NOTICE,"Updating from RDAC %s %s %s %s %s %s to repeater on pos %i [%s]",repeaterList[repPos].callsign,repeaterList[repPos].hardware
	,repeaterList[repPos].firmware,repeaterList[repPos].mode,repeaterList[repPos].txFreq,repeaterList[repPos].shift,repPos,str);
	sprintf(SQLQUERY,"SELECT count(*) FROM repeaters WHERE repeaterId = %i",repeaterList[repPos].id);
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	strftime(timeStamp,sizeof(timeStamp),"%Y-%m-%d %H:%M:%S",t);
	if (sqlite3_prepare_v2(dbase,SQLQUERY,-1,&stmt,0) == 0){
		if (sqlite3_step(stmt) == SQLITE_ROW){
			sqlite3_finalize(stmt);
			sprintf(SQLQUERY,"UPDATE repeaters SET callsign = '%s', txFreq = '%s', shift = '%s', hardware = '%s', firmware = '%s', mode = '%s', currentAddress = %lu, lastRdacUpdate = '%s', ipAddress = '%s'  WHERE repeaterId = %i",
			repeaterList[repPos].callsign,repeaterList[repPos].txFreq,repeaterList[repPos].shift,repeaterList[repPos].hardware,repeaterList[repPos].
			firmware,repeaterList[repPos].mode,(long)cliaddrOrg.sin_addr.s_addr,timeStamp,str,repeaterList[repPos].id);
			sqlite3_exec(dbase,SQLQUERY,0,0,0);
			sprintf(SQLQUERY,"SELECT language,geoLocation,aprsPass,aprsBeacon,aprsPHG,autoReflector,intlRefAllow,reflectorTimeout,autoConnectTimer FROM repeaters WHERE repeaterId = %i",repeaterList[repPos].id);
			if (sqlite3_prepare_v2(dbase,SQLQUERY,-1,&stmt,0) == 0){
                if (sqlite3_step(stmt) == SQLITE_ROW){
					sprintf(repeaterList[repPos].language,"%s",sqlite3_column_text(stmt,0));
					sprintf(repeaterList[repPos].geoLocation,"%s",sqlite3_column_text(stmt,1));
					sprintf(repeaterList[repPos].aprsPass,"%s",sqlite3_column_text(stmt,2));
					sprintf(repeaterList[repPos].aprsBeacon,"%s",sqlite3_column_text(stmt,3));
					sprintf(repeaterList[repPos].aprsPHG,"%s",sqlite3_column_text(stmt,4));
					repeaterList[repPos].autoReflector = sqlite3_column_int(stmt,5);
					repeaterList[repPos].intlRefAllow = (sqlite3_column_int(stmt,6) == 1 ? true:false);
					repeaterList[repPos].reflectorTimeout = sqlite3_column_int(stmt,7);
					repeaterList[repPos].autoConnectTimer = sqlite3_column_int(stmt,8);
					sqlite3_finalize(stmt);
					syslog(LOG_NOTICE,"Setting repeater language to %s [%s]",repeaterList[repPos].language,str);
				}
			}
		}
		else{
			syslog(LOG_NOTICE,"RDAC, no row [%s]",str);
		}
	}
	else{
		syslog(LOG_NOTICE,"RDAC, bad query [%s]",str);
	}
	fclose(fp);
	return true;
}


void *rdacListener(void* f){
	int sockfd,n,i,rc;
	struct sockaddr_in servaddr;
	socklen_t len;
	unsigned char buffer[500];
	unsigned char response[500] ={0};
	//int port = (intptr_t)f;
	struct sockInfo* param = (struct sockInfo*) f;
	int port = param->port;
	struct sockaddr_in cliaddrOrg = param->address;
	struct sockaddr_in cliaddr;
	int repPos = 0;
	fd_set fdMaster;
	struct timeval timeout;
	time_t timeNow,pingTime;
	char str[INET_ADDRSTRLEN];
	sqlite3 *dbase;

	inet_ntop(AF_INET, &(cliaddrOrg.sin_addr), str, INET_ADDRSTRLEN);
	
	syslog(LOG_NOTICE,"Listener for port %i started [%s]",port,str);
	repPos = findRepeater(cliaddrOrg);
	repeaterList[repPos].rdacOnline = true;
	sockfd=socket(AF_INET,SOCK_DGRAM,0);
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(port);
	bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
	FD_ZERO(&fdMaster);
	time(&pingTime);
	
	len = sizeof(cliaddr);
	for (;;){
		FD_SET(sockfd, &fdMaster);
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		if (rc = select(sockfd+1, &fdMaster, NULL, NULL, &timeout) == -1) { 
			syslog(LOG_NOTICE,"Select error, closing socket port %i",port);
			close(sockfd);
			pthread_exit(NULL);
        }
		if (FD_ISSET(sockfd,&fdMaster)) {
			n = recvfrom(sockfd,buffer,500,0,(struct sockaddr *)&cliaddr,&len);
			if (n>2){
			}
			else{
				time(&pingTime);
				response[0] = 0x41;
				if (repPos !=99 && cliaddr.sin_addr.s_addr == cliaddrOrg.sin_addr.s_addr) sendto(sockfd,response,n,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
				if (repPos !=99 && !repeaterList[repPos].rdacUpdated && cliaddr.sin_addr.s_addr == cliaddrOrg.sin_addr.s_addr && repeaterList[repPos].rdacUpdateAttempts < 10){
					repeaterList[repPos].rdacUpdateAttempts++;
					if (repeaterList[repPos].rdacUpdateAttempts == 10){
						syslog(LOG_NOTICE,"Failed to update from RDAC on port %i [%s]",port,str);
					}
					else{
						syslog(LOG_NOTICE,"Requesting RDAC info from repeater on port %i [%s]",port,str);
					}
					dbase = openDatabase();
					getRepeaterInfo(sockfd,repPos,cliaddr,dbase);
					closeDatabase(dbase);
				}
			}
		}
		else{
			time(&timeNow);
			if (difftime(timeNow,pingTime) > 60) {
				repPos = findRepeater(cliaddr);
				if (repPos !=99){
					syslog(LOG_NOTICE,"PING timeout on RDAC port %i repeater %s, exiting thread [%s]",port,repeaterList[repPos].callsign,str);
					syslog(LOG_NOTICE,"Setting RDAC offline for repeater on position %i [%s]",repPos,str);
					repeaterList[repPos].rdacOnline = false;
				}
				else{
					syslog(LOG_NOTICE,"PING timeout on RDAC port %i repeater already removed from list, exiting thread",port);
				}
				close(sockfd);
				pthread_exit(NULL);
			}
		}
	}
}
