//
// Created by smarek on 3/21/20.
//

#ifndef LINDMR_SCHEDULER_H
#define LINDMR_SCHEDULER_H

#include <sqlite3.h>
#include <netinet/in.h>
#include "dmr.h"

#define VFRAMESIZE 72
#define SLOT_TYPE_OFFSET1 18
#define SLOT_TYPE_OFFSET2 19
#define FRAME_TYPE_OFFSET1 22
#define FRAME_TYPE_OFFSET2 23
#define PTYPE_OFFSET 8

int repeater,oldStartPos = 0,startPos=0,oldFrames = 0,frames=0;
char startFile[100];

struct url_data {
    size_t size;
    char* data;
};

void sendAprsBeacon();
sqlite3 *openDatabase();
void closeDatabase();
void loadUsersToFile();
void importUsers();
void importTalkGroups();
void sendReflectorStatus();

#endif //LINDMR_SCHEDULER_H
