//
// Created by smarek on 3/21/20.
//

#ifndef LINDMR_HYTERADECODE_H
#define LINDMR_HYTERADECODE_H

#include <sqlite3.h>

void sendAprs();
int checkCoordinates();
sqlite3 *openDatabase();
void closeDatabase();
void decodeHyteraGpsTriggered(int radioId,int destId,struct repeater repeater, unsigned char data[300]);
void decodeHyteraGpsButton(int radioId,int destId,struct repeater repeater, unsigned char data[300]);

#endif //LINDMR_HYTERADECODE_H
