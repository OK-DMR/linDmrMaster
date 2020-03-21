//
// Created by smarek on 3/21/20.
//

#ifndef LINDMR_DMR_H
#define LINDMR_DMR_H

#include <bits/types/time_t.h>
#include <sqlite3.h>
#include <stdbool.h>

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
#define DTMF_IND_OFFSET 35
#define DTMF_NUM_OFFSET1 28
#define DTMF_NUM_OFFSET2 29


struct allow {
    bool repeater;
    bool sMaster;
    bool isRange;
    bool isDynamic;
};

struct header {
    bool responseRequested;
    int dataPacketFormat;
    int sapId;
    int appendBlocks;
};

struct state {
    int reflectorNewState;
    int repConnectNewState;
};


void delRepeaterByPos();

void sendTalkgroupInfo();

void sendRepeaterInfo();

void sendReflectorStatus();

bool *convertToBits();

struct header decodeDataHeader();

unsigned char *decodeThreeQuarterRate();

unsigned char *decodeHalfRate();

void decodeHyteraGpsTriggered();

void decodeHyteraGpsCompressed();

void decodeHyteraGpsButton();

void decodeHyteraRrs();

sqlite3 *openDatabase();

void closeDatabase();

void *g711ClientsListener();

void decodeHyteraOffRrs(struct repeater repeater, unsigned char data[300]);

time_t reflectorTimeout, autoReconnectTimer;

#endif //LINDMR_DMR_H
