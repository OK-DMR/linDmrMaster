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
#define VCALL 4369                                        //HEX 1111
#define DCALL 26214                                        //HEX 6666
#define ISSYNC 61166										//HEX EEEE
#define VCALLEND 8738										//HEX 2222
//#define CALL 2
//#define CALLEND 3
#define PTYPE_ACTIVE1 2                                        
#define PTYPE_END1 3
#define PTYPE_ACTIVE2 66
#define PTYPE_END2 67
#define VFRAMESIZE 103                                        //UDP PAYLOAD SIZE OF sMaster
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

struct masterData sMaster = {0};

struct allow{
	bool repeater;
	bool sMaster;
	bool isRange;
	bool isDynamic;
};

struct allow checkTalkGroup();

void sendTalkgroupInfo(int sockfd,struct sockaddr_in servaddr){
	char rreqResponse[200];
	char myCCInfo[21];
	char announced[90];
	int i;
	bool cc1Send = false;
	bool cc2Send = false;
	int conferenceDoubles[100] = {0};

	sprintf(myCCInfo,"%s%s",master.ownCountryCode,master.ownRegion);

	/*if (dynTg[1] != 0){
		sprintf(announced,"%s",master.announcedCC1);
		unsigned char strRef[6];
		int CC1len = master.sMasterTS1GroupCount * 4;
		sprintf(strRef,"%4i",dynTg[1]);
		memcpy(announced + CC1len,strRef,4);
		sprintf(rreqResponse,"TS2R%-20s%-20s:%s\n",myCCInfo,master.ownName,announced);
		syslog(LOG_NOTICE,"%s",rreqResponse);
		sendto(sockfd,rreqResponse,107,0,(struct sockaddr *)&servaddr,sizeof(servaddr));
		cc1Send = true;
	}
	if (dynTg[2] != 0){
		sprintf(announced,"%s",master.announcedCC2);
		unsigned char strRef[6];
		int CC2len = master.sMasterTS2GroupCount * 4;
		sprintf(strRef,"%4i",dynTg[2]);
		memcpy(announced + (CC2len -4),strRef,4);
		sprintf(rreqResponse,"TS2R%-20s%-20s:%s\n",myCCInfo,master.ownName,announced);
		syslog(LOG_NOTICE,"%s",rreqResponse);
		sendto(sockfd,rreqResponse,107,0,(struct sockaddr *)&servaddr,sizeof(servaddr));
		cc2Send = true;
	}*/
	
	if (!cc1Send){
		sprintf(rreqResponse,"TS1R%-20s%-20s:%s\n",myCCInfo,master.ownName,master.announcedCC1);
		syslog(LOG_NOTICE,"%s",rreqResponse);
		sendto(sockfd,rreqResponse,107,0,(struct sockaddr *)&servaddr,sizeof(servaddr));
	}
	
	int confCounter = 0, x= 0;
	for (i=0;i<highestRepeater;i++){
		bool confFound = false;
		if (repeaterList[i].address.sin_addr.s_addr !=0){
			if(repeaterList[i].conference[2] !=0 && repeaterList[i].conferenceType[2] == 1){
				for (x=0;x<confCounter;x++){
					if (repeaterList[i].conference[2] == conferenceDoubles[x]){
						confFound = true;
						break;
					}
				}
				if (!confFound){
					sprintf(announced,"%s",master.announcedCC2);
					unsigned char strRef[6];
					int CC2len = strlen(announced);
					sprintf(strRef,"%5i",repeaterList[i].conference[2]);
					memcpy(announced + (CC2len -4),strRef,6);
					sprintf(rreqResponse,"TS2R%-20s%-20s:%s\n",myCCInfo,master.ownName,announced);
					syslog(LOG_NOTICE,"%s",rreqResponse);
					sendto(sockfd,rreqResponse,107,0,(struct sockaddr *)&servaddr,sizeof(servaddr));
					cc2Send = true;
					conferenceDoubles[confCounter] = repeaterList[i].conference[2];
					confCounter++;
				}
			}
		}
	}
	
	if(!cc2Send){
		sprintf(rreqResponse,"TS2R%-20s%-20s:%s\n",myCCInfo,master.ownName,master.announcedCC2);
		syslog(LOG_NOTICE,"%s",rreqResponse);
		sendto(sockfd,rreqResponse,strlen(rreqResponse),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
	}

}

void sendRepeaterInfo(int sockfd,struct sockaddr_in servaddr,int repPos){
	unsigned char ip[INET_ADDRSTRLEN];
	int i;
	unsigned char conference[7];
	unsigned char repInfo[200];

	if (repPos == 100){
		syslog(LOG_NOTICE,"Sending repeater info to sMaster, highest repeater %i",highestRepeater);
		for (i=0;i<highestRepeater;i++){
			if (strlen(repeaterList[i].callsign) !=0){
				inet_ntop(AF_INET, &(repeaterList[i].address.sin_addr), ip, INET_ADDRSTRLEN);
				if(repeaterList[i].conference[2] !=0 && repeaterList[i].conferenceType[2] == 1){
					sprintf(conference,"%i",repeaterList[i].conference[2]);
				}
				else{
					sprintf(conference,"0");
				}
				sprintf(repInfo,"RPT%02i%-15s%i%i%i%i%-4s%-16s%-9s%-6s0     %-6s%-10s%-12s%-3s%i\n",i,ip,repeaterList[i].id,baseDmrPort+i,
				master.ownCCInt,master.ownRegionInt,version,repeaterList[i].callsign,repeaterList[i].txFreq,repeaterList[i].shift,conference,
				repeaterList[i].hardware,repeaterList[i].firmware,repeaterList[i].mode,masterDmrId);
				sendto(sockfd,repInfo,strlen(repInfo),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
				syslog(LOG_NOTICE,"%s",repInfo);
				usleep(60000);
			}
		}
	}
	else{
		inet_ntop(AF_INET, &(repeaterList[repPos].address.sin_addr), ip, INET_ADDRSTRLEN);
		if(repeaterList[repPos].conference[2] !=0 && repeaterList[repPos].conferenceType[2] == 1){
			sprintf(conference,"%i",repeaterList[repPos].conference[2]);
		}
		else{
			sprintf(conference,"0");
		}
		sprintf(repInfo,"RPT%02i%-15s%i%i%i%i%-4s%-16s%-9s%-6s0     %-6s%-10s%-12s%-3s%i",repPos,ip,repeaterList[repPos].id,baseDmrPort+repPos,
		master.ownCCInt,master.ownRegionInt,version,repeaterList[repPos].callsign,repeaterList[repPos].txFreq,repeaterList[repPos].shift,conference,
		repeaterList[repPos].hardware,repeaterList[repPos].firmware,repeaterList[repPos].mode,masterDmrId);
		sendto(sockfd,repInfo,strlen(repInfo),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
		syslog(LOG_NOTICE,"%s",repInfo);
	}
}

void sendReflectorStatus(int sockfd,struct sockaddr_in servaddr,int repPos){
		unsigned char refStatus[50];
		int i;

		if (repPos == 100){
			for(i=0;i<highestRepeater;i++){
				if (repeaterList[i].id != 0 && repeaterList[i].dmrOnline && repeaterList[i].conference[2] !=0 && repeaterList[i].conferenceType[2] == 1){
					sprintf(refStatus,"%6.6s@%6.6i@%4.4i\n",repeaterList[i].callsign,repeaterList[i].id,repeaterList[i].conference[2]);
					sendto(sockfd,refStatus,27,0,(struct sockaddr *)&servaddr,sizeof(servaddr));
					usleep(60000);
				}
			}
		}
		else{
			sprintf(refStatus,"%6.6s@%6.6i@%4.4i\n",repeaterList[repPos].callsign,repeaterList[repPos].id,repeaterList[repPos].conference[2]);
			sendto(sockfd,refStatus,27,0,(struct sockaddr *)&servaddr,sizeof(servaddr));
		}
}

void *sMasterThread(){
	int sockfd = 0,voiceSockfd = 0;
	unsigned char buffer[VFRAMESIZE];
	char ping[70];
	int n,rc,i;
	fd_set fdMaster;
	time_t timeNow,needPingTime,reportTime,pongTime;
	struct timeval timeout;
	int packetType = 0;
	unsigned char slot = 0;
	int slotType = 0;
	int frameType = 0;
	int srcId = 0;
	int dstId = 0;
	int callType = 0;
	int defTG2 = 9;
	unsigned char origC[5][3];
	struct sockaddr_in servaddr,voiceServaddr;
	struct allow toSend = {0};
	bool reflectorTraffic = false;
	bool logSend = false;
	int currentDst[3];

	syslog(LOG_NOTICE,"[sMaster]Starting sMaster voice thread");
	currentDst[1] = 0;
	currentDst[2] = 0;
		
	sockfd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=INADDR_ANY;
	servaddr.sin_port=htons(62010);
	bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
	servaddr.sin_addr.s_addr=inet_addr(master.sMasterIp);

	voiceSockfd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	bzero(&voiceServaddr,sizeof(voiceServaddr));
	voiceServaddr.sin_family = AF_INET;
	voiceServaddr.sin_addr.s_addr=INADDR_ANY;
	voiceServaddr.sin_port=htons(62011);
	bind(voiceSockfd,(struct sockaddr *)&voiceServaddr,sizeof(voiceServaddr));
	voiceServaddr.sin_addr.s_addr=inet_addr(master.sMasterIp);

	sMaster.address = servaddr;
	sMaster.voiceAddress = voiceServaddr;
	sMaster.sockfd = sockfd;
	sMaster.voiceSockfd = voiceSockfd;
	sMaster.online = true;
	sprintf(ping,"3ING%s%s%i ",master.ownCountryCode,master.ownRegion,masterDmrId);
	time(&needPingTime);
	sendto(sockfd,ping,strlen(ping),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
	time(&reportTime);
	
	FD_ZERO(&fdMaster);
	
for(;;){
		FD_SET(sockfd, &fdMaster);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		if (rc = select(sockfd+1, &fdMaster, NULL, NULL, &timeout) == -1) { 
			syslog(LOG_NOTICE,"Select error in sMaster thread");
			close(sockfd);
			//need to reconnect here !!!!
        }
		
		if (FD_ISSET(sockfd,&fdMaster)) {
			n = recv(sockfd,buffer,VFRAMESIZE,0);
			if (n<VFRAMESIZE){
				if (strstr(buffer,"RREQ")){
					syslog(LOG_NOTICE,"RREQ from sMaster");
					sendTalkgroupInfo(sockfd,servaddr);
					sendRepeaterInfo(sockfd,servaddr,100);
					time(&reportTime);
				}
				if (strstr(buffer,"PONG")){
					time(&pongTime);
				}
			}
			else{
				slot = buffer[SLOT_OFFSET1] / 16;
				dstId = buffer[DST_OFFSET3] << 16 | buffer[DST_OFFSET2] << 8 | buffer[DST_OFFSET1];
				srcId = buffer[SRC_OFFSET3] << 16 | buffer[SRC_OFFSET2] << 8 | buffer[SRC_OFFSET1];
				if (buffer[0] == 'R'){
					if (!reflectorTraffic){
						syslog(LOG_NOTICE,"[sMaster]Incoming traffic from reflector %i source id = %i",dstId,srcId);
						reflectorTraffic = true;
					}
					//changed here !!!
					if (buffer[1] == 0x02) buffer[0] = 0x01; else buffer[0] = buffer[1];
					memcpy(buffer+64,(char*)&defTG2,sizeof(int));
					//end
					for (i=0;i<highestRepeater;i++){
						if(dstId == repeaterList[i].conference[2] && repeaterList[i].conferenceType[2] == 1){
							sendto(repeaterList[i].sockfd,buffer,72,0,(struct sockaddr *)&repeaterList[i].address,sizeof(repeaterList[i].address));
						}
					}
				}
				else{
					time(&timeNow);
					if ((dmrState[slot] == IDLE && (difftime(timeNow,voiceIdleTimer[slot]) > master.priorityTimeout || dstId == master.priorityTG[slot])) || sMaster.sending[slot]){
						packetType = buffer[PTYPE_OFFSET] & 0x0F;
						slotType = buffer[SLOT_TYPE_OFFSET1] << 8 | buffer[SLOT_TYPE_OFFSET2];
						frameType  = buffer[FRAME_TYPE_OFFSET1] << 8 | buffer[FRAME_TYPE_OFFSET2];

						switch (packetType){
				
							case 0x01:
							if (slotType == DCALL && dmrState[slot] != DATA){
								callType = buffer[TYP_OFFSET1];
								syslog(LOG_NOTICE,"[sMaster]Data on slot %i src %i dst %i type %i",slot,srcId,dstId,callType);
								break;
							}

							break;
							
							case 0x02:
							if (slotType == 0xeeee && frameType == 0x1111 && dmrState[slot] != VOICE){
								callType = buffer[TYP_OFFSET1];
								toSend = checkTalkGroup(dstId,slot,callType);
								if (toSend.repeater == false){
									continue;
									
								}
								if (toSend.isDynamic == true && dynTg[slot] != dstId){
									continue;
								}
								sMaster.sending[slot] = true;
								dmrState[slot] = VOICE;
								currentDst[slot] = dstId;
								logSend = false;
								syslog(LOG_NOTICE,"[sMaster]Voice call started on slot %i src %i dst %i type %i",slot,srcId,dstId,callType);
								if (buffer[90] !=0) {
									memcpy(origC[slot],buffer+90,4);
									syslog(LOG_NOTICE,"[sMaster]Replacing country code to %s",origC[slot]);
								} 
								else{ 
									memcpy(origC[slot],buffer+64,4);
								}
								break;
							}
							
				
							case 0x03:
							if (slotType == VCALLEND){
								if (dstId == currentDst[slot]){
									currentDst[slot] = 0;
									dstId = 0;  //put dstId to 0 so voice end is send to repeaters 
									syslog(LOG_NOTICE,"[sMaster]Voice call ended on slot %i",slot);
									dmrState[slot] = IDLE;
									sMaster.sending[slot] = false;
								}
							}
							break;
						}
						memcpy(buffer+64,origC[slot],4);
						if (dstId == currentDst[slot]){
							for (i=0;i<highestRepeater;i++){
								if (repeaterList[i].address.sin_addr.s_addr !=0 && !repeaterList[i].sending[slot]){
									sendto(repeaterList[i].sockfd,buffer,72,0,(struct sockaddr *)&repeaterList[i].address,sizeof(repeaterList[i].address));
								}
							}
						}
					}
					else{
						if (!logSend) {
							syslog(LOG_NOTICE,"[sMaster]Incoming traffic on slot %i, but DMR not IDLE",slot);
							logSend = true;
						}
					}
				}
			}
		}
		else{
			time(&timeNow);
			if (sMaster.sending[1] && dmrState[1] != IDLE){
				dmrState[1] = IDLE;
				sMaster.sending[1] = false;
				currentDst[1] = 0;
				syslog(LOG_NOTICE,"[sMaster]Slot 1 IDLE");
			}
			if (sMaster.sending[2] && dmrState[2] != IDLE){
				dmrState[2] = IDLE;
				sMaster.sending[2] = false;
				currentDst[2] = 0;
				syslog(LOG_NOTICE,"[sMaster]Slot 2 IDLE");
			}
			if (reflectorTraffic){
				reflectorTraffic = false;
				syslog(LOG_NOTICE,"[sMaster] Voice call ended on reflector");
			}	
			
			if (difftime(timeNow,needPingTime) > 5){
				time(&needPingTime);
				sendto(sockfd,ping,strlen(ping),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
			}
			if (difftime(timeNow,reportTime) > 120){
				sendTalkgroupInfo(sockfd,servaddr);
				sendRepeaterInfo(sockfd,servaddr,100);
				time(&reportTime);
			}
			
		}
	}
}