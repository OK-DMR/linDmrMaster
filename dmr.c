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

#define NUMSLOTS 2                                        //DMR IS 2 SLOT
#define SLOT1 4369                                        //HEX 1111
#define SLOT2 8738                                        //HEX 2222
//#define VCALL 4369                                        //HEX 1111
//#define DCALL 26214                                        //HEX 6666
//#define ISSYNC 61166										//HEX EEEE
//#define VCALLEND 8738										//HEX 2222
//#define CALL 2
//#define CALLEND 3
//#define PTYPE_ACTIVE1 2                                        
//#define PTYPE_END1 3
//#define PTYPE_ACTIVE2 66
//#define PTYPE_END2 67
#define VFRAMESIZE 72                                        //UDP PAYLOAD SIZE OF REPEATER VOICE/DATA TRAFFIC
//#define SYNC_OFFSET1 18                                        //UDP OFFSETS FOR VARIOUS BYTES IN THE DATA STREAM
//#define SYNC_OFFSET2 19                                        //
//#define SYNC_OFFSET3 18                                        //Look for EEEE
//#define SYNC_OFFSET4 19                                        //Look for EEEE
#define SLOT_OFFSET1 16                                        //        
#define SLOT_OFFSET2 17
#define PTYPE_OFFSET 8
#define SRC_OFFSET1 68
#define SRC_OFFSET2 69
#define SRC_OFFSET3 70
#define DST_OFFSET1 64
#define DST_OFFSET2 65
#define DST_OFFSET3 66
#define TYP_OFFSET1 62

#define SLOT_TYPE_OFFSET1 18
#define SLOT_TYPE_OFFSET2 19
#define FRAME_TYPE_OFFSET1 22
#define FRAME_TYPE_OFFSET2 23

struct allow{
	bool repeater;
	bool sMaster;
	bool isRange;
};

struct header{
	bool responseRequested;
    int dataPacketFormat;
    int sapId;
    int appendBlocks;
};


void delRepeaterByPos();
void sendTalkgroupInfo();
void sendRepeaterInfo();
void sendReflectorStatus();
bool * convertToBits();
struct header decodeDataHeader();
unsigned char *  decodeThreeQuarterRate();
unsigned char *  decodeHalfRate();
void decodeHyteraGpsTriggered();
void decodeHyteraGpsCompressed();
void decodeHyteraGpsButton();
void decodeHyteraRrs();
sqlite3 *openDatabase();
void closeDatabase();

int numbersIndex[20] = {25,10,50,10,72,15,95,10,120,15,139,20,170,12,190,15,218,15,240,15};

struct allow checkTalkGroup(int dstId, int slot, int callType){
	struct allow toSend = {0};
	int i;
	
	toSend.isRange = false;
	//First check sMaster talk groups. All sMaster talkgroups apply to the repeaters also
	if (slot == 1){
		for(i=0;i<master.sMasterTS1GroupCount;i++){
			if (dstId >= sMasterTS1List[i][0] && dstId <= sMasterTS1List[i][1]){
				toSend.repeater = true;
				toSend.sMaster = true;
				if (sMasterTS1List[i][0] != sMasterTS1List[i][1]) toSend.isRange = true;
				return toSend;
			}
		}
	}
	else{
		for(i=0;i<master.sMasterTS2GroupCount;i++){
			if (dstId >= sMasterTS2List[i][0] && dstId <= sMasterTS2List[i][1]){
				toSend.repeater = true;
				toSend.sMaster = true;
				if (sMasterTS2List[i][0] != sMasterTS2List[i][1]) toSend.isRange = true;
				return toSend;
			}
		}
	}
	
	if (slot == 1){
		for(i=0;i<master.repTS1GroupCount;i++){
			if (dstId >= repTS1List[i][0] && dstId <= repTS1List[i][1]){
				toSend.repeater = true;
				toSend.sMaster = false;
				if (repTS1List[i][0] != repTS1List[i][1]) toSend.isRange = true;
				return toSend;
			}
		}
	}
	else{
		for(i=0;i<master.repTS2GroupCount;i++){
			if (dstId >= repTS2List[i][0] && dstId <= repTS2List[i][1]){
				toSend.repeater = true;
				toSend.sMaster = false;
				if (repTS2List[i][0] != repTS2List[i][1]) toSend.isRange = true;
				return toSend;
			}
		}
	}
	toSend.repeater = false;
	toSend.sMaster = false;
	return toSend;
}

void updateRepeaterTable(int status, int reflector, int repPos){

	char SQLQUERY[400];
	sqlite3 *dbase;
	sqlite3_stmt *stmt;

	if(status ==2){
		sprintf(SQLQUERY,"UPDATE repeaters set currentReflector = %i where callsign = '%s'",reflector,repeaterList[repPos].callsign);
	}
	else{
		sprintf(SQLQUERY,"UPDATE repeaters set currentReflector = 0 where callsign = '%s'",repeaterList[repPos].callsign);
	}
	dbase = openDatabase();
	if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
                syslog(LOG_NOTICE,"Failed to update repeater table: %s",sqlite3_errmsg(dbase));
                syslog(LOG_NOTICE,"QUERY: %s",SQLQUERY);
        }
	closeDatabase(dbase);
}

void playVoiceReflector(int sockfd, struct sockaddr_in address, char fileName[100],int repPos,int reflector){
	FILE *file;
	int slotType=0,frameType=0,packetType = 0;;
	unsigned char buffer[VFRAMESIZE];
	unsigned char endBuffer[VFRAMESIZE];
	int startPos,frames,i,x;
	int seq = 0;

	sleep(1);
	syslog(LOG_NOTICE,"[%s]Playing %s",repeaterList[repPos].callsign,fileName);
	if (file = fopen(fileName,"rb")){
		while (fread(buffer,VFRAMESIZE,1,file)){
			slotType = buffer[SLOT_TYPE_OFFSET1] << 8 | buffer[SLOT_TYPE_OFFSET2];
			frameType = buffer[FRAME_TYPE_OFFSET1] << 8 | buffer[FRAME_TYPE_OFFSET2];
			if (slotType != 0x2222 && packetType !=3){
				if (slotType != 0xeeee && frameType != 0x1111) buffer[4] = seq;
				sendto(sockfd,buffer,VFRAMESIZE,0,(struct sockaddr *)&address,sizeof(address));
				if (slotType != 0xeeee && frameType != 0x1111){
					seq++;
					if (seq == 256) seq= 0;
				}
			}
			else{
				memcpy(endBuffer,buffer,VFRAMESIZE);
			}
			if (slotType != 0xeeee && frameType != 0x1111) usleep(60000);
		}
        fclose(file);
	}
	else{
		syslog(LOG_NOTICE,"File %s not found",fileName);
	}
	
	if (file = fopen("numbers.voice","rb")){
		int counter = 0;
		int numbers[6];
		syslog(LOG_NOTICE,"[%s]Analyzing reflector number %i",repeaterList[repPos].callsign,reflector); 
		while (reflector > 0) {
			int digit = reflector % 10;
			counter ++;
			numbers[counter] = digit;
			reflector /= 10;
		}
		for (x = counter;x>0;x--){
			startPos = numbersIndex[numbers[x] * 2];
			frames = numbersIndex[(numbers[x] * 2) + 1];
			fseek(file,startPos * VFRAMESIZE,SEEK_SET);
			for(i=0;i<frames;i++){
				if (fread(buffer,VFRAMESIZE,1,file)){
					slotType = buffer[SLOT_TYPE_OFFSET1] << 8 | buffer[SLOT_TYPE_OFFSET2];
					frameType = buffer[FRAME_TYPE_OFFSET1] << 8 | buffer[FRAME_TYPE_OFFSET2];
					packetType = buffer[PTYPE_OFFSET] & 0x0F;
					if (slotType != 0x2222 && packetType !=3){
						if (slotType != 0xeeee && frameType != 0x1111) buffer[4] = seq;
						sendto(sockfd,buffer,VFRAMESIZE,0,(struct sockaddr *)&address,sizeof(address));
						if (slotType != 0xeeee && frameType != 0x1111){
							seq++;
							if (seq == 256) seq= 0;
						}
					}
					if (slotType != 0xeeee && frameType != 0x1111) usleep(60000);
				}
				else{
					syslog(LOG_NOTICE,"[%s] fread failed from numbers.voice",repeaterList[repPos].callsign);
				}
			}
		}
		sendto(sockfd,endBuffer,VFRAMESIZE,0,(struct sockaddr *)&address,sizeof(address));
		fclose(file);
	}
	else{
		syslog(LOG_NOTICE,"[%s] Failed to open numbers.voice",repeaterList[repPos].callsign);
	}

}

void playVoiceRepeater(int sockfd, struct sockaddr_in address, char fileName[100],int repPos, int pearRepPos){
	FILE *file,file1,file2;
	int slotType=0,frameType=0,packetType=0;
	unsigned char buffer[VFRAMESIZE];
	unsigned char endBuffer[VFRAMESIZE];
	int seq = 0;
	int x;
	
	sleep(1);
	syslog(LOG_NOTICE,"[%s]Playing %s",repeaterList[repPos].callsign,fileName);
	if (file = fopen(fileName,"rb")){
		while (fread(buffer,VFRAMESIZE,1,file)){
			slotType = buffer[SLOT_TYPE_OFFSET1] << 8 | buffer[SLOT_TYPE_OFFSET2];
			frameType = buffer[FRAME_TYPE_OFFSET1] << 8 | buffer[FRAME_TYPE_OFFSET2];
			if (slotType != 0x2222 && packetType !=3){
				if (slotType != 0xeeee && frameType != 0x1111) buffer[4] = seq;
				sendto(sockfd,buffer,VFRAMESIZE,0,(struct sockaddr *)&address,sizeof(address));
				if (slotType != 0xeeee && frameType != 0x1111){
					seq++;
					if (seq == 256) seq= 0;
				}
			}
			else{
				memcpy(endBuffer,buffer,VFRAMESIZE);
			}
			if (slotType != 0xeeee && frameType != 0x1111) usleep(60000);
		}
        fclose(file);
	}
	else{
		syslog(LOG_NOTICE,"File %s not found",fileName);
	}
	//play callsign
	/*for (x=0;i<strlen(repeaterList(pearRepPos).callsign;i++){
		if (isalpha(repeaterList(pearRepPos).callsign[x])){
			int position = pearRepPos).callsign[x] - 65;
		}
		if (isdigit(repeaterList(pearRepPos).callsign[x])){
			int position = repeaterList(pearRepPos).callsign[x]);
		}
	
	}*/
}


void reflectorStatus(int sockfd, struct sockaddr_in address,int status,int reflector, int repPos){

	char fileName[100];

	switch (status) {
		case 1:
		sprintf(fileName,"disconnected.voice");
		updateRepeaterTable(status,reflector,repPos);
		break;

		case 2:
		sprintf(fileName,"connected.voice");
		updateRepeaterTable(status,reflector,repPos);
		break;

		case 3:
		sprintf(fileName,"intl_not_allowed.voice");
		break;
		
		case 4:
		if (reflector != 0){
			sprintf(fileName,"current_ref.voice");
		}
		else{
			sprintf(fileName,"no_reflector.voice");
		}
		break;	

		case 5:
		sprintf(fileName,"unknown_ref.voice");
		break;	

	}
	
	playVoiceReflector(sockfd,address,fileName,repPos,reflector);	
}

void repConnectStatus(int sockfd, struct sockaddr_in address,int status, int repPos, int pearRepPos){
	char fileName[100];

	switch (status) {
		case 1:
		sprintf(fileName,"connect_own.voice");
		break;

		case 2:
		sprintf(fileName,"deffered_reflector.voice");
		break;

		case 3:
		sprintf(fileName,"connected_repeater.voice");
		break;

		case 4:
		sprintf(fileName,"repeater_busy.voice");
		break;

		case 5:
		sprintf(fileName,"disconnected_repeater.voice");
		break;
		
	}
	playVoiceRepeater(sockfd,address,fileName,repPos,pearRepPos);
}

void echoTest(unsigned char buffer[VFRAMESIZE],int sockfd, struct sockaddr_in address, int srcId, int repPos){
	struct frame{
		unsigned char buf[VFRAMESIZE];
	};
	struct frame record[2000];
	long frames = 0;
	int n,rc,i;
	int slotType=0,frameType=0;
	fd_set fdMaster;
	struct timeval timeout;
	struct sockaddr_in cliaddr;
	socklen_t len;
	bool timedOut = false;
	FILE *referenceFile;
	FILE *recordFile;
	char fileName[100];

	sprintf(fileName,"%i.record",srcId);
	
	recordFile = fopen(fileName,"wb");
	
	FD_ZERO(&fdMaster);
	
	memcpy(record[frames].buf,buffer,VFRAMESIZE);
	fwrite(buffer,VFRAMESIZE,1,recordFile);
	len = sizeof(cliaddr);
	do{
		FD_SET(sockfd, &fdMaster);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		if (rc = select(sockfd+1, &fdMaster, NULL, NULL, &timeout) == -1) { 
			close(sockfd);
			pthread_exit(NULL);
        }
		if (FD_ISSET(sockfd,&fdMaster)) {
			n = recvfrom(sockfd,buffer,VFRAMESIZE,0,(struct sockaddr *)&cliaddr,&len);
		
			if (n>2){
				slotType = buffer[SLOT_TYPE_OFFSET1] << 8 | buffer[SLOT_TYPE_OFFSET2];
				frameType = buffer[FRAME_TYPE_OFFSET1] << 8 | buffer[FRAME_TYPE_OFFSET2];
				if (frames < 2000){
					frames++;
					memcpy(record[frames].buf,buffer,VFRAMESIZE);
					fwrite(buffer,VFRAMESIZE,1,recordFile);
				} 
			}
		}
		else{
			timedOut = true;
		}
	}while ((slotType != 0x2222 && frameType != 0xaaaa) || timedOut == false);
	fclose(recordFile);
	sleep(1);
	syslog(LOG_NOTICE,"Playing echo back for radio %i",srcId);
	
	for (i=0;i<=frames;i++){
		sendto(sockfd,record[i].buf,VFRAMESIZE,0,(struct sockaddr *)&address,sizeof(address));
		slotType = record[i].buf[SLOT_TYPE_OFFSET1] << 8 | record[i].buf[SLOT_TYPE_OFFSET2];
		frameType = record[i].buf[FRAME_TYPE_OFFSET1] << 8 | record[i].buf[FRAME_TYPE_OFFSET2];
		if (slotType != 0xeeee && frameType != 0x1111) usleep(60000);
	}
	sprintf(fileName,"reference_%s.voice",repeaterList[repPos].language);
        if (referenceFile = fopen(fileName,"rb")){
		sleep(1);
		syslog(LOG_NOTICE,"Playing reference file %s",fileName);
		while (fread(buffer,VFRAMESIZE,1,referenceFile)){
			sendto(sockfd,buffer,VFRAMESIZE,0,(struct sockaddr *)&address,sizeof(address));
			slotType = buffer[SLOT_TYPE_OFFSET1] << 8 | buffer[SLOT_TYPE_OFFSET2];
			frameType = buffer[FRAME_TYPE_OFFSET1] << 8 | buffer[FRAME_TYPE_OFFSET2];
			if (slotType != 0xeeee && frameType != 0x1111) usleep(60000);
		}
	fclose(referenceFile);
	}
	else{
		syslog(LOG_NOTICE,"reference file %s not found",fileName); 
	}
}


void logTraffic(int srcId,int dstId,int slot,unsigned char serviceType[16],int callType, unsigned char repeater[17]){
	char SQLQUERY[400];
	sqlite3 *dbase;
	sqlite3_stmt *stmt;
	unsigned char callsign[33] = "";
	unsigned char name[33] = "";
	unsigned char callGroup[16];

	if (callType == 1) sprintf(callGroup ,"Group"); else sprintf(callGroup,"Private");
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
                        closeDatabase(dbase);
			syslog(LOG_NOTICE,"Failed to find DMR ID in table callsigns");
                        return;
                }
        }
        else{
                closeDatabase(dbase);
		syslog(LOG_NOTICE,"Failed to prepare SQL statement");
                return;

        }

	sprintf(SQLQUERY,"REPLACE into traffic (senderId,senderCallsign,senderName,targetId,channel,serviceType,callType,timeStamp,onRepeater) VALUES (%i,'%s','%s',%i,%i,'%s','%s',%lu,'%s')",srcId,callsign,name,dstId,slot,serviceType,callGroup,time(NULL),repeater);
        if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
            syslog(LOG_NOTICE,"Failed to update traffic table: %s",sqlite3_errmsg(dbase));
			syslog(LOG_NOTICE,"QUERY: %s",SQLQUERY);
        }
	if(strcmp(serviceType,"Voice")== 0){
		sprintf(SQLQUERY,"REPLACE into voiceTraffic (senderId,senderCallsign,senderName,targetId,channel,serviceType,callType,timeStamp,onRepeater) VALUES (%i,'%s','%s',%i,%i,'%s','%s',%lu,'%s')",srcId,callsign,name,dstId,slot,serviceType,callGroup,time(NULL),repeater);
        	if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
                syslog(LOG_NOTICE,"Failed to update voiceTraffic table: %s",sqlite3_errmsg(dbase));
				syslog(LOG_NOTICE,"QUERY: %s",SQLQUERY);
        	}
	}
        closeDatabase(dbase);

}

void updateRepeaterStatus(unsigned char repeater[17],int status){
	char SQLQUERY[400];
	sqlite3 *dbase;
	sqlite3_stmt *stmt;
	char timeStamp[20];

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	strftime(timeStamp,sizeof(timeStamp),"%Y-%m-%d %H:%M:%S",t);
	syslog(LOG_NOTICE,"[%s]Setting repeater to status %i in database",repeater,status);
	dbase = openDatabase();
	sprintf(SQLQUERY,"UPDATE repeaters set online = %i, timeStamp = '%s' where callsign = '%s'",status,timeStamp,repeater);
	if (sqlite3_exec(dbase,SQLQUERY,0,0,0) != 0){
		syslog(LOG_NOTICE,"Failed to update repeater table: %s",sqlite3_errmsg(dbase));
		syslog(LOG_NOTICE,"QUERY: %s",SQLQUERY);
	}
	closeDatabase(dbase);
}

void *dmrListener(void *f){
	int sockfd,n,i,rc,ii,l;
	struct sockaddr_in servaddr,cliaddr;
	socklen_t len;
	unsigned char buffer[VFRAMESIZE];
	unsigned char holdBuffer[3][VFRAMESIZE];
	unsigned char response[VFRAMESIZE] ={0};
	struct sockInfo* param = (struct sockInfo*) f;
	int repPos = param->port - baseDmrPort;
	struct sockaddr_in cliaddrOrg = param->address;
	int packetType[3] = {0};
	int frameType[3] = {0};
	int slotType[3] = {0};
	int srcId[3] = {0};
	int dstId[3] = {0};
	int callType[3] = {0};
	unsigned char slot = 0;
	fd_set fdMaster;
	struct timeval timeout;
	time_t timeNow,pingTime,reflectorTimeout,autoReconnectTimer;
	struct allow toSend = {0};
	bool block[3];
	bool releaseBlock[3];
	bool receivingData[3] = {0};
	unsigned char sMasterFrame[103];
	char myId[11];
	unsigned char webUserInfo[100];
	unsigned char dmrPacket[33];
	bool *bits;
	struct header headerDecode[3];
	int dataBlocks[3] = {0};
	unsigned char *decoded34[3];
	unsigned char *decoded12[3];
	unsigned char decodedString[3][300];
	int reflectorNewState = 0;
	bool txStart;
	int repConnectNewState = 0;
	unsigned char reflectorState[200];
	unsigned char ip[INET_ADDRSTRLEN];


	unsigned char gpsStringHyt[4] = {0x08,0xD0,0x03,0x00};
	unsigned char gpsStringButtonHyt[4] = {0x08,0xA0,0x02,0x00};
	unsigned char gpsCompressedStringHyt[4] = {0x01,0xD0,0x03,0x00};
	unsigned char rrsHyt[4] = {0x00,0x11,0x00,0x03};
	unsigned char rrsOffHyt[4] = {0x00,0x11,0x00,0x01};

	syslog(LOG_NOTICE,"DMR thread for port %i started",baseDmrPort + repPos);
	sockfd=socket(AF_INET,SOCK_DGRAM,0);
	repeaterList[repPos].sockfd = sockfd;
	repeaterList[repPos].dmrOnline = true;
	repeaterList[repPos].sending[1] = false;
	repeaterList[repPos].sending[2] = false;
	block[1] = false;
	block[2] = false;
	releaseBlock[1] = false;
	releaseBlock[2] = false;
	txStart = false;
	//create frame to append after packet for sMaster
	memset(sMasterFrame,0,103);
	memcpy(myId,(char*)&repeaterList[repPos].id,sizeof(int));
	//memcpy(myId+4,master.ownCountryCode,4);
	//memcpy(myId+7,master.ownRegion,1);
	//memcpy(myId+8,version,3);
	memset(decodedString,0,300);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(baseDmrPort + repPos);
	bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
	FD_ZERO(&fdMaster);
	time(&pingTime);
	len = sizeof(cliaddr);

	autoReconnectTimer = 0;
	updateRepeaterStatus(repeaterList[repPos].callsign,1);
	if(repeaterList[repPos].autoReflector != 0){
		updateRepeaterTable(2,repeaterList[repPos].autoReflector,repPos);
		syslog(LOG_NOTICE,"[%s]Adding repeater to conference %i by auto reflector",repeaterList[repPos].callsign,repeaterList[repPos].autoReflector);
	
		//If autoreflector is intl, send needed info to sMaster
		for(l=0;l<numReflectors;l++){
			if(localReflectors[l].id == repeaterList[repPos].autoReflector){
				repeaterList[repPos].conferenceType[2] = localReflectors[l].type;
				if(localReflectors[l].type == 1){
					if(sMaster.online){
						sendRepeaterInfo(sMaster.sockfd,sMaster.address,repPos);
						sendReflectorStatus(sMaster.sockfd,sMaster.address,repPos);
						sendTalkgroupInfo(sMaster.sockfd,sMaster.address,repeaterList[repPos].conference[2]);
					}
				}
			}
		}
	}
	for (;;){
		FD_SET(sockfd, &fdMaster);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		if (rc = select(sockfd+1, &fdMaster, NULL, NULL, &timeout) == -1) { 
			syslog(LOG_NOTICE,"Select error, closing socket port %i",baseDmrPort + repPos);
			close(sockfd);
			pthread_exit(NULL);
        	}

		if (FD_ISSET(sockfd,&fdMaster)) {
			n = recvfrom(sockfd,buffer,VFRAMESIZE,0,(struct sockaddr *)&cliaddr,&len);
			if (n>2){
				slot = buffer[SLOT_OFFSET1] / 16;
				if (dmrState[slot] == IDLE || repeaterList[repPos].sending[slot]){
					packetType[slot] = buffer[PTYPE_OFFSET] & 0x0F;
					slotType[slot] = buffer[SLOT_TYPE_OFFSET1] << 8 | buffer[SLOT_TYPE_OFFSET2];
					frameType[slot]  = buffer[FRAME_TYPE_OFFSET1] << 8 | buffer[FRAME_TYPE_OFFSET2];
					switch (packetType[slot]){

						case 0x02:

						if (slotType[slot] == 0xdddd){
							memcpy(holdBuffer[slot],buffer,n);
							block[slot] = true;
							releaseBlock[slot] = true;
							txStart = true;
							break;
						}

						if (slotType[slot] == 0xeeee && frameType[slot] == 0x6666){//Sometimes start of data and no CSBK afterwards
							srcId[slot] = buffer[SRC_OFFSET3] << 16 | buffer[SRC_OFFSET2] << 8 | buffer[SRC_OFFSET1];
							dstId[slot] = buffer[DST_OFFSET3] << 16 | buffer[DST_OFFSET2] << 8 | buffer[DST_OFFSET1];
							callType[slot] = buffer[TYP_OFFSET1];
							toSend.sMaster = false;
							if (dstId[slot] == rrsGpsId || (dstId[slot] > 5049 && dstId[slot] < 5061) || srcId[slot] == repeaterList[repPos].id) block[slot] = true;
							break;
						}

						if (slotType[slot] == 0xeeee && frameType[slot] == 0x1111 && dmrState[slot] != VOICE && block[slot] == false){ //Hytera voice sync packet
							//Sync packet is send before Voice LC header and every time the embedded LC (4 packets) in a voice superframe has been send
							//When voice call starts, this is the first packet where we can see src and dst)
							//sMasterFrame[98] = slot;
							srcId[slot] = buffer[SRC_OFFSET3] << 16 | buffer[SRC_OFFSET2] << 8 | buffer[SRC_OFFSET1];
							dstId[slot] = buffer[DST_OFFSET3] << 16 | buffer[DST_OFFSET2] << 8 | buffer[DST_OFFSET1];
							callType[slot] = buffer[TYP_OFFSET1];
							repeaterList[repPos].sending[slot] = true;
							if (sMaster.online){
								if (repeaterList[repPos].conference[2] !=0 && repeaterList[repPos].conferenceType[2] ==1 && slot == 2 && dstId[slot] == 9){
									sprintf(webUserInfo,"RX_Slot=%i,GROUP=%i,USER_ID=%i,TYPE=Voice,VERS=%s,RPTR=%i,%s\n",slot,repeaterList[repPos].conference[2],srcId[slot],version,repeaterList[repPos].id,master.ownName);
								}
								else{
									sprintf(webUserInfo,"RX_Slot=%i,GROUP=%i,USER_ID=%i,TYPE=Voice,VERS=%s,RPTR=%i,%s\n",slot,dstId[slot],srcId[slot],version,repeaterList[repPos].id,master.ownName);
								}
								sendto(sMaster.sockfd,webUserInfo,strlen(webUserInfo),0,(struct sockaddr *)&sMaster.address,sizeof(sMaster.address));
							}
							if ((dstId[slot] == echoId && slot == echoSlot) || (dstId[slot] == 9 && srcId[slot] == 4000)){
								syslog(LOG_NOTICE,"[%s]Echo test started on slot %i src %i",repeaterList[repPos].callsign,slot,srcId[slot]);
								echoTest(buffer,sockfd,repeaterList[repPos].address,srcId[slot],repPos);
								repeaterList[repPos].sending[slot] = false;
								break;
							}
							if(dstId[2] == 4000 && repeaterList[repPos].pearRepeater[2] != 0){
								syslog(LOG_NOTICE,"[%s]Disconnecting from repeater %i",repeaterList[repPos].callsign,repeaterList[repPos].pearRepeater[2]);
								repeaterList[repPos].pearRepeater[2] = 0;
								repeaterList[repeaterList[repPos].pearPos[2]].pearRepeater[2] = 0;
								repConnectNewState = 5;
							}
							if(dstId[2] == 4000 && repeaterList[repPos].conference[2] != 0){
								syslog(LOG_NOTICE,"[%s]Removing repeater from conference %i",repeaterList[repPos].callsign,repeaterList[repPos].conference[2]);
								repeaterList[repPos].conference[2] = 0;
								reflectorNewState = 1;
								if(repeaterList[repPos].autoReflector !=0) time(&autoReconnectTimer);
								if(repeaterList[repPos].conferenceType[2] ==1){
									if(sMaster.online){
										sendRepeaterInfo(sMaster.sockfd,sMaster.address,repPos);
									}
								}
							}
							
							if(dstId[2] == 5000 && repeaterList[repPos].pearRepeater[2] == 0){
								syslog(LOG_NOTICE,"[%s]Requesting info on conference %i %s type %i",repeaterList[repPos].callsign,repeaterList[repPos].conference[2],localReflectors[l].name,localReflectors[l].type);
								reflectorNewState = 4;
							}
							
							if(dstId[2] > 4000 && dstId[2] < 5000){
								for(l=0;l<numReflectors;l++){
									if(localReflectors[l].id == dstId[2]){
										if (localReflectors[l].type == 1 && !repeaterList[repPos].intlRefAllow){
											syslog(LOG_NOTICE,"[%s]Not allowed to connect to intl reflector on this repeater %i",repeaterList[repPos].callsign,localReflectors[l].id);
											reflectorNewState = 3;
											break;
										}
										repeaterList[repPos].conference[2] = dstId[2];
										repeaterList[repPos].conferenceType[2] = localReflectors[l].type;
										syslog(LOG_NOTICE,"[%s]Adding repeater to conference %i %s type %i",repeaterList[repPos].callsign,repeaterList[repPos].conference[2],localReflectors[l].name,localReflectors[l].type);
										time(&reflectorTimeout);
										reflectorNewState = 2;
										if(repeaterList[repPos].autoReflector !=0) time(&autoReconnectTimer);
										if (localReflectors[l].type == 1){
											if(sMaster.online){
												sendRepeaterInfo(sMaster.sockfd,sMaster.address,repPos);
												sendReflectorStatus(sMaster.sockfd,sMaster.address,repPos);
												sendTalkgroupInfo(sMaster.sockfd,sMaster.address);
											}
										}
										break;
									}
								}
								if (l == numReflectors){
									syslog(LOG_NOTICE,"[%s]Conference %i not found",repeaterList[repPos].callsign,dstId[2]);
									reflectorNewState = 5;
								}
							}
							if (slot == 2){
								for (l=0;l<highestRepeater;l++){
									if(dstId[2] == repeaterList[l].id){
										if(dstId[2] == repeaterList[repPos].id){
											syslog(LOG_NOTICE,"[%s]repeater cannot connect to itself",repeaterList[repPos].callsign);
											repConnectNewState = 1;
											break;
										}
										if(repeaterList[l].conference[2] != 0){
											if(repeaterList[repeaterList[repPos].pearPos[2]].pearRepeater[2] != 0){
												syslog(LOG_NOTICE,"[%s]Need to disconnecting from repeater %i first",repeaterList[repPos].callsign,repeaterList[repPos].pearRepeater[2]);
												repeaterList[repPos].pearRepeater[2] = 0;
												repeaterList[repeaterList[repPos].pearPos[2]].pearRepeater[2] = 0;
											}
											syslog(LOG_NOTICE,"[%s]connecting to repeater %s, but that repeater is connected to reflector %i, redirecting",repeaterList[repPos].callsign,repeaterList[l].callsign,repeaterList[l].conference[2]);
											repeaterList[repPos].conference[2] = repeaterList[l].conference[2];
											time(&reflectorTimeout);
											repConnectNewState = 2;
											reflectorNewState = 2;
											if(repeaterList[repPos].autoReflector !=0) time(&autoReconnectTimer);
											break;
										}
										if(repeaterList[l].pearRepeater[2] == 0){
											if(repeaterList[repeaterList[repPos].pearPos[2]].pearRepeater[2] != 0){
												syslog(LOG_NOTICE,"[%s]Need to disconnecting from repeater %i first",repeaterList[repPos].callsign,repeaterList[repPos].pearRepeater[2]);
												repeaterList[repPos].pearRepeater[2] = 0;
												repeaterList[repeaterList[repPos].pearPos[2]].pearRepeater[2] = 0;
											}
											repeaterList[repPos].pearRepeater[2] = repeaterList[l].id;
											repeaterList[l].pearRepeater[2] = repeaterList[repPos].id;
											repeaterList[repPos].pearPos[2] = l;
											repeaterList[l].pearPos[2] = repPos;
											if (repeaterList[repPos].conference[2] != 0){
												repeaterList[repPos].conference[2] = 0;
												reflectorNewState = 1;
											}
											if(repeaterList[repPos].autoReflector !=0) time(&autoReconnectTimer);
											syslog(LOG_NOTICE,"[%s]connecting to repeater %s",repeaterList[repPos].callsign,repeaterList[l].callsign);
											repConnectNewState = 3;
											time(&repeaterList[repPos].pearTimeout);
											time(&repeaterList[l].pearTimeout);
										}
										else{
											syslog(LOG_NOTICE,"[%s]connect to repeater %s failed,busy",repeaterList[repPos].callsign,repeaterList[l].callsign);
											repConnectNewState = 4;
										}
										break;
									}
								}
							}
							toSend = checkTalkGroup(dstId[slot],slot,callType[slot]);
							if (toSend.repeater == false){
								block[slot] = true;
								if(repeaterList[repPos].conference[2] != 0 && slot == 2 && dstId[2] == 9){
									syslog(LOG_NOTICE,"[%s]Voice call started, sending to conference %i",repeaterList[repPos].callsign,repeaterList[repPos].conference[2]);
									time(&reflectorTimeout);
									if (autoReconnectTimer !=0) time(&autoReconnectTimer);
								}
								else if(repeaterList[repPos].pearRepeater[2] != 0 && slot == 2 && dstId[2] == 9){
									syslog(LOG_NOTICE,"[%s]Voice call started, sending to repeater %i",repeaterList[repPos].callsign,repeaterList[repPos].pearRepeater[2]);
									time(&repeaterList[repPos].pearTimeout);
									time(&repeaterList[repeaterList[repPos].pearPos[2]].pearTimeout);
									if (autoReconnectTimer !=0) time(&autoReconnectTimer);
								}
								else{
									syslog(LOG_NOTICE,"[%s]Talk group %i not configured on slot %i so not relaying",repeaterList[repPos].callsign,dstId[slot],slot);
									if(autoReconnectTimer !=0 && dstId[2] == 9) time(&autoReconnectTimer);
								}
								break;
							}

							if(toSend.isRange && dstId[slot] != master.ownCCInt){
								memcpy(sMasterFrame+90,(char*)&master.ownCCInt,sizeof(int));
							}
							else{
								memset(sMasterFrame+90,0,4);
							}
							dmrState[slot] = VOICE;
							syslog(LOG_NOTICE,"[%s]Voice call started on slot %i src %i dst %i type %i",repeaterList[repPos].callsign,slot,srcId[slot],dstId[slot],callType[slot]);
							//break;
						}
						break; 

						case 0x41: //V6 of Hytera repeater firmware sends data header with packet type 41 iso 1
						
						if (slotType[slot] == 0x4444){  //Data header
							callType[slot] = buffer[TYP_OFFSET1];
							memcpy(dmrPacket,buffer+26,34);  //copy the dmr part out of the Hyetra packet
							bits = convertToBits(dmrPacket); //convert it to bits
							repeaterList[repPos].sending[slot] = true;
							receivingData[slot] = true;
							dataBlocks[slot] = 0;
							if (debug == 1) syslog(LOG_NOTICE,"------------------------------------------------------------------------------------------");
							headerDecode[slot] = decodeDataHeader(bits);
							syslog(LOG_NOTICE,"[%s]Data header on slot %i src %i dst %i type %i appendBlocks %i",repeaterList[repPos].callsign,slot,srcId[slot],dstId[slot],callType[slot],headerDecode[slot].appendBlocks);
							break;
						}

						
						case 0x01:
						if (slotType[slot] == 0x3333){  //CSBK (first slot type for data where we can see src and dst)
							srcId[slot] = buffer[SRC_OFFSET3] << 16 | buffer[SRC_OFFSET2] << 8 | buffer[SRC_OFFSET1];
							dstId[slot] = buffer[DST_OFFSET3] << 16 | buffer[DST_OFFSET2] << 8 | buffer[DST_OFFSET1];
							toSend.sMaster = false;
							if (dstId[slot] == rrsGpsId || (dstId[slot] > 5049 && dstId[slot] < 5061) || srcId[slot] == repeaterList[repPos].id) block[slot] = true;
							break;
						}
						

						if (slotType[slot] == 0x4444){  //Data header
							memcpy(dmrPacket,buffer+26,34);  //copy the dmr part out of the Hyetra packet
							bits = convertToBits(dmrPacket); //convert it to bits
							callType[slot] = buffer[TYP_OFFSET1];
							repeaterList[repPos].sending[slot] = true;
							receivingData[slot] = true;
							dataBlocks[slot] = 0;
							if (debug == 1) syslog(LOG_NOTICE,"------------------------------------------------------------------------------------------");
							headerDecode[slot] = decodeDataHeader(bits);
							syslog(LOG_NOTICE,"[%s]Data header on slot %i src %i dst %i type %i appendBlocks %i",repeaterList[repPos].callsign,slot,srcId[slot],dstId[slot],callType[slot],headerDecode[slot].appendBlocks);
							break;
						}

						if (slotType[slot] == 0x5555 && !receivingData[slot]){ // 1/2 rate data without valid header
							block[slot] = true;
							releaseBlock[slot] = true;
							break;
						}
						if (slotType[slot] == 0x6666 && !receivingData[slot]){ // 3/4 rate data without valid header
							block[slot] = true;
							releaseBlock[slot] = true;
							break;
						}
						if (slotType[slot] == 0x5555 && receivingData[slot]){ // 1/2 rate data continuation
							memcpy(dmrPacket,buffer+26,34);  //copy the dmr part out of the Hyetra packet
							bits = convertToBits(dmrPacket); //convert it to bits
							decoded12[slot] = decodeHalfRate(bits);
							memcpy(decodedString[slot]+(12*dataBlocks[slot]),decoded12[slot],12);
							dataBlocks[slot]++;
							if(headerDecode[slot].appendBlocks == dataBlocks[slot]){
								if (debug == 1) syslog(LOG_NOTICE,"------------------------------------------------------------------------------------------");
								receivingData[slot] = false;
								releaseBlock[slot] = true;
								dataBlocks[slot] = 0;
								repeaterList[repPos].sending[slot] = false;
								syslog(LOG_NOTICE,"[%s]1/2 rate data continuation all data blocks received on slot %i src %i dst %i type %i",repeaterList[repPos].callsign,slot,srcId[slot],dstId[slot],callType[slot]);
								if(dstId[slot] == rrsGpsId || (dstId[slot] > 5049 && dstId[slot] < 5061)){
									if(memcmp(decodedString[slot] + 1,rrsHyt,4) == 0) decodeHyteraRrs(repeaterList[repPos],decodedString[slot]);
									if(memcmp(decodedString[slot] + 1,rrsOffHyt,4) == 0) decodeHyteraOffRrs(repeaterList[repPos],decodedString[slot]);
									logTraffic(srcId[slot],dstId[slot],slot,"RRS",callType[slot],repeaterList[repPos].callsign);
								}
								else{
									logTraffic(srcId[slot],dstId[slot],slot,"Data 1/2",callType[slot],repeaterList[repPos].callsign);
								}
								memset(decodedString[slot],0,300);
							}
							break;
						}
						if (slotType[slot] == 0x6666 && receivingData[slot]){ // 3/4 rate data continuation
							memcpy(dmrPacket,buffer+26,34);  //copy the dmr part out of the Hyetra packet
							bits = convertToBits(dmrPacket); //convert it to bits
							decoded34[slot] = decodeThreeQuarterRate(bits);
							memcpy(decodedString[slot]+(18*dataBlocks[slot]),decoded34[slot],18);
							dataBlocks[slot]++;
							if(headerDecode[slot].appendBlocks +1 == dataBlocks[slot]){  //Hytera sends last datablock twice
								if (debug == 1) syslog(LOG_NOTICE,"------------------------------------------------------------------------------------------");
								releaseBlock[slot] = true;
								receivingData[slot] = false;
								dataBlocks[slot] = 0;
								repeaterList[repPos].sending[slot] = false;
								syslog(LOG_NOTICE,"[%s]3/4 rate data continuation all data blocks received on slot %i src %i dst %i type %i",repeaterList[repPos].callsign,slot,srcId[slot],dstId[slot],callType[slot]);
								if(dstId[slot] == rrsGpsId || (dstId[slot] > 5049 && dstId[slot] < 5061)){
									if(memcmp(decodedString[slot] + 4,gpsStringHyt,4) == 0) decodeHyteraGpsTriggered(srcId[slot],dstId[slot],repeaterList[repPos],decodedString[slot]);
									if(memcmp(decodedString[slot] + 4,gpsStringButtonHyt,4) == 0) decodeHyteraGpsButton(srcId[slot],dstId[slot],repeaterList[repPos],decodedString[slot]);
									if(memcmp(decodedString[slot] + 4,gpsCompressedStringHyt,4) == 0) decodeHyteraGpsCompressed(srcId[slot],repeaterList[repPos],decodedString[slot]);
									logTraffic(srcId[slot],dstId[slot],slot,"GPS",callType[slot],repeaterList[repPos].callsign);
								}
								else{
									logTraffic(srcId[slot],dstId[slot],slot,"Data 3/4",callType[slot],repeaterList[repPos].callsign);
								}
								memset(decodedString[slot],0,300);
							}
						}
						break;
						
						case 0x03:
						if (slotType[slot] == 0x2222){  //Terminator with LC
							dmrState[slot] = IDLE;
							repeaterList[repPos].sending[slot] = false;
							syslog(LOG_NOTICE,"[%s]Voice call ended on slot %i type %i",repeaterList[repPos].callsign,slot,callType[slot]);
							logTraffic(srcId[slot],dstId[slot],slot,"Voice",callType[slot],repeaterList[repPos].callsign);
							if (block[slot] == true){
								releaseBlock[slot] = true;
							}
							if (repConnectNewState !=0 && slot ==2){
								repConnectStatus(sockfd,repeaterList[repPos].address,repConnectNewState,repPos,repeaterList[repPos].pearPos[2]);
								repConnectNewState = 0;
							}
							if (reflectorNewState !=0 && slot ==2){
								reflectorStatus(sockfd,repeaterList[repPos].address,reflectorNewState,repeaterList[repPos].conference[2],repPos);
								reflectorNewState = 0;
							}
						}
						break;
					}
					
					if (repeaterList[repPos].pearRepeater[2] != 0){
						sendto(repeaterList[repeaterList[repPos].pearPos[2]].sockfd,buffer,n,0,(struct sockaddr *)&repeaterList[repeaterList[repPos].pearPos[2]].address,sizeof(repeaterList[repeaterList[repPos].pearPos[2]].address));
					}
					if (repeaterList[repPos].conference[2] !=0 && slot == 2 && dstId[2] == 9){
						for (i=0;i<highestRepeater;i++){
							if (repeaterList[i].conference[2] == repeaterList[repPos].conference[2] && repeaterList[i].address.sin_addr.s_addr != cliaddrOrg.sin_addr.s_addr){
								sendto(repeaterList[i].sockfd,buffer,n,0,(struct sockaddr *)&repeaterList[i].address,sizeof(repeaterList[i].address));
							}
						}
						if (repeaterList[repPos].conferenceType[2] == 1 && sMaster.online){
							memcpy(buffer+64,(char*)&repeaterList[repPos].conference[2],sizeof(int));
							memcpy(sMasterFrame,buffer,n);
							memcpy(sMasterFrame + n,myId,11);
							sMasterFrame[0] = 'R';
							sendto(sMaster.sockfd,sMasterFrame,103,0,(struct sockaddr *)&sMaster.address,sizeof(sMaster.address));
						}
					}

					if (!block[slot]){
						if (toSend.sMaster && sMaster.online){
							if (txStart){
								memcpy(sMasterFrame,holdBuffer[1],n);
								memcpy(sMasterFrame + n,myId,11);
								sendto(sMaster.sockfd,sMasterFrame,103,0,(struct sockaddr *)&sMaster.address,sizeof(sMaster.address));
								memcpy(sMasterFrame,holdBuffer[2],n);
								memcpy(sMasterFrame + n,myId,11);
								sendto(sMaster.sockfd,sMasterFrame,103,0,(struct sockaddr *)&sMaster.address,sizeof(sMaster.address));
							}
							memcpy(sMasterFrame,buffer,n);
							memcpy(sMasterFrame + n,myId,11);
							sendto(sMaster.sockfd,sMasterFrame,103,0,(struct sockaddr *)&sMaster.address,sizeof(sMaster.address));
						}
						if(txStart){
							txStart = false;
							for (i=0;i<highestRepeater;i++){
								if (repeaterList[i].address.sin_addr.s_addr !=0 && repeaterList[i].address.sin_addr.s_addr != cliaddrOrg.sin_addr.s_addr && repeaterList[repPos].conference[slot] == 0){
									sendto(repeaterList[i].sockfd,holdBuffer[1],n,0,(struct sockaddr *)&repeaterList[i].address,sizeof(repeaterList[i].address));
									sendto(repeaterList[i].sockfd,holdBuffer[2],n,0,(struct sockaddr *)&repeaterList[i].address,sizeof(repeaterList[i].address));
								}
							}
						}
						for (i=0;i<highestRepeater;i++){
							if (repeaterList[i].address.sin_addr.s_addr !=0 && repeaterList[i].address.sin_addr.s_addr != cliaddrOrg.sin_addr.s_addr && repeaterList[repPos].conference[slot] == 0){
								sendto(repeaterList[i].sockfd,buffer,n,0,(struct sockaddr *)&repeaterList[i].address,sizeof(repeaterList[i].address));
							}
						}
					}
					else{
						if (releaseBlock[slot]){
							block[slot] = false;
							releaseBlock[slot] = false;
						}
					}
				}
				else{
					syslog(LOG_NOTICE,"[%s]Incomming traffic on slot %i, but DMR not IDLE",repeaterList[repPos].callsign,slot);
				}
			}
			else{
				response[0] = 0x41;
				sendto(sockfd,response,n,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
				if(repeaterList[repPos].address.sin_port != cliaddr.sin_port){
					syslog(LOG_NOTICE,"[%s]IP port changed !!! old port:%i new port:%i",repeaterList[repPos].callsign,ntohs(repeaterList[repPos].address.sin_port),ntohs(cliaddr.sin_port));
					repeaterList[repPos].address.sin_port = cliaddr.sin_port;
				}
				time(&pingTime);
			}
		}
		else{
			time(&timeNow);
			if ((repeaterList[repPos].sending[1] && dmrState[1] != IDLE) || receivingData[1]){
				if (dmrState[1] == VOICE){
					syslog(LOG_NOTICE,"[%s]Voice call ended after timeout on slot 1",repeaterList[repPos].callsign);
					logTraffic(srcId[1],dstId[1],slot,"Voice",callType[1],repeaterList[repPos].callsign);
				}
				if (receivingData[1]){
					if (debug == 1) syslog(LOG_NOTICE,"------------------------------------------------------------------------------------------");
					syslog(LOG_NOTICE,"[%s]Data call ended after timeout on slot 1",repeaterList[repPos].callsign);
					dataBlocks[1] = 0;
					receivingData[1] = false;
					memset(decodedString[1],0,300);
					logTraffic(srcId[1],dstId[1],slot,"Data",callType[1],repeaterList[repPos].callsign);
				}
				dmrState[1] = IDLE;
				repeaterList[repPos].sending[1] = false;
				block[1] = false;
				releaseBlock[1] = false;
				syslog(LOG_NOTICE,"[%s]Slot 1 IDLE",repeaterList[repPos].callsign);
			}
			if ((repeaterList[repPos].sending[2] && dmrState[2] != IDLE) || receivingData[2]){
				if (dmrState[2] == VOICE){
					syslog(LOG_NOTICE,"[%s]Voice call ended after timeout on slot 2",repeaterList[repPos].callsign);
					logTraffic(srcId[2],dstId[2],slot,"Voice",callType[2],repeaterList[repPos].callsign);
					if (repConnectNewState !=0 && slot ==2){
						repConnectStatus(sockfd,repeaterList[repPos].address,repConnectNewState,repPos,repeaterList[repPos].pearPos[2]);
						repConnectNewState = 0;
					}
					if (reflectorNewState !=0 && slot ==2){
						reflectorStatus(sockfd,repeaterList[repPos].address,reflectorNewState,repeaterList[repPos].conference[2],repPos);
						reflectorNewState = 0;
					}
				}
				if (receivingData[2]){
					if (debug == 1) syslog(LOG_NOTICE,"------------------------------------------------------------------------------------------");
					syslog(LOG_NOTICE,"[%s]Data call ended after timeout on slot 2",repeaterList[repPos].callsign);
					dataBlocks[2] = 0;
					receivingData[2] = false;
					memset(decodedString[2],0,300);
					logTraffic(srcId[2],dstId[2],slot,"Data",callType[2],repeaterList[repPos].callsign);
				}
				dmrState[2] = IDLE;
				repeaterList[repPos].sending[2] = false;
				block[2] = false;
				releaseBlock[2] = false;
				syslog(LOG_NOTICE,"[%s]Slot 2 IDLE",repeaterList[repPos].callsign);
			}
			if (difftime(timeNow,pingTime) > 60 && !repeaterList[repPos].sending[slot]){
				syslog(LOG_NOTICE,"[%s]PING timeout on DMR port %i, exiting thread",repeaterList[repPos].callsign,baseDmrPort + repPos);
				syslog(LOG_NOTICE,"[%s]Removing repeater from list position %i",repeaterList[repPos].callsign,repPos);
				bool found = false; 
				for(i=0;i<highestRepeater;i++){
					if (repeaterList[repPos].id == repeaterList[i].id && repPos !=i){
						syslog(LOG_NOTICE,"[%s]Repeater online in other thread, keep online in database",repeaterList[repPos].callsign);
						found = true;
					}
				}
				if (!found){
					updateRepeaterTable(1,repeaterList[repPos].autoReflector,repPos);
					updateRepeaterStatus(repeaterList[repPos].callsign,0);
					found = false;
				}
				delRepeaterByPos(repPos);
				if (repPos + 1 == highestRepeater) highestRepeater--;
				close(sockfd);
				pthread_exit(NULL);
			}
			if (difftime(timeNow,repeaterList[repPos].pearTimeout) > 1800 && repeaterList[repPos].pearRepeater[2] !=0){
				syslog(LOG_NOTICE,"[%s]Disconnecting from repeater %i after timeout",repeaterList[repPos].callsign,repeaterList[repPos].pearRepeater[2]);
				repeaterList[repPos].pearRepeater[2] = 0;
				repeaterList[repeaterList[repPos].pearPos[2]].pearRepeater[2] = 0;
			}
			if (difftime(timeNow,reflectorTimeout) > 1800 && repeaterList[repPos].conference[2] !=0 && repeaterList[repPos].autoReflector == 0){
				syslog(LOG_NOTICE,"[%s]Remove repeater from conference %i after conference timeout",repeaterList[repPos].callsign,repeaterList[repPos].conference[2]);
				repeaterList[repPos].conference[2] = 0;
				reflectorStatus(sockfd,repeaterList[repPos].address,1,repeaterList[repPos].conference[2],repPos);
				//if intl send to sMaster
				if (localReflectors[l].type == 1){
					if(sMaster.online){
						sendRepeaterInfo(sMaster.sockfd,sMaster.address,repPos);
					}
				}

			}
			if (difftime(timeNow,autoReconnectTimer) > 600 && autoReconnectTimer != 0){
				syslog(LOG_NOTICE,"[%s]Adding repeater to conference %i due to auto reconnect timer",repeaterList[repPos].callsign,repeaterList[repPos].autoReflector);
				repeaterList[repPos].conference[2] = repeaterList[repPos].autoReflector;
				reflectorStatus(sockfd,repeaterList[repPos].address,2,repeaterList[repPos].conference[2],repPos);
				autoReconnectTimer = 0;
				//if intl send to sMaster
				for(l=0;l<numReflectors;l++){
					if(localReflectors[l].id == repeaterList[repPos].autoReflector){
						repeaterList[repPos].conferenceType[2] = localReflectors[l].type;
						if(localReflectors[l].type == 1){
							if(sMaster.online){
								sendRepeaterInfo(sMaster.sockfd,sMaster.address,repPos);
								sendReflectorStatus(sMaster.sockfd,sMaster.address,repPos);
								sendTalkgroupInfo(sMaster.sockfd,sMaster.address,repeaterList[repPos].conference[2]);
							}
						}
					}
				}
			}
		}
	}
}
