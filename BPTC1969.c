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

struct header {
    bool responseRequested;
    int dataPacketFormat;
    int sapId;
    int appendBlocks;
};

bool *extractInfo(bool bits[264]) {

    static bool info[196];
    int bytePos = 0;
    int i;
    unsigned char infoBits[200];

    memset(infoBits, 0, 200);

    //printf("Info bits: ");
    for (i = 0; i < 98; i++) {
        info[bytePos] = bits[i];
        if (debug == 1) sprintf(infoBits, "%s%i", infoBits, info[bytePos]);
        bytePos++;
    }

    for (i = 166; i < 264; i++) {
        info[bytePos] = bits[i];
        if (debug == 1) sprintf(infoBits, "%s%i", infoBits, info[bytePos]);
        bytePos++;
    }

    if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-extractInfo]%s", infoBits);
    return info;
}

bool *deInterleave(bool *bits) {
    static bool deInterleavedBits[196];
    int a, interleaveSequence;

    for (a = 0; a < 196; a++) {
        interleaveSequence = (a * 181) % 196;
        deInterleavedBits[a] = *(bits + interleaveSequence);
    }
    return deInterleavedBits;
}

bool *extractPayload(bool *deInterData) {
    int a, pos = 0;
    static bool outData[96];
    for (a = 4; a <= 11; a++) {
        outData[pos] = *(deInterData + a);
        pos++;
    }
    for (a = 16; a <= 26; a++) {
        outData[pos] = *(deInterData + a);
        pos++;
    }
    for (a = 31; a <= 41; a++) {
        outData[pos] = *(deInterData + a);
        pos++;
    }
    for (a = 46; a <= 56; a++) {
        outData[pos] = *(deInterData + a);
        pos++;
    }
    for (a = 61; a <= 71; a++) {
        outData[pos] = *(deInterData + a);
        pos++;
    }
    for (a = 76; a <= 86; a++) {
        outData[pos] = *(deInterData + a);
        pos++;
    }
    for (a = 91; a <= 101; a++) {
        outData[pos] = *(deInterData + a);
        pos++;
    }
    for (a = 106; a <= 116; a++) {
        outData[pos] = *(deInterData + a);
        pos++;
    }
    for (a = 121; a <= 131; a++) {
        outData[pos] = *(deInterData + a);
        pos++;
    }
    return outData;
}


struct header decodeDataHeader(bool bits[264]) {

    bool *infoBits; //196 info bits
    bool *deInterleavedBits; //196 bits
    static bool *payloadBits; //96  bits
    int blocksToFollow = 0, a;
    unsigned char dpf = 0, sap = 0, bitPadding = 0;
    struct header headerDecode;
    unsigned char stringPayload[100];

    memset(stringPayload, 0, 100);

    infoBits = extractInfo(bits);
    deInterleavedBits = deInterleave(infoBits);
    payloadBits = extractPayload(deInterleavedBits);

    if (debug == 1) {
        for (a = 0; a < 96; a++) {
            sprintf(stringPayload, "%s%i", stringPayload, *(payloadBits + a));
        }
        syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]%s", stringPayload);
    }
    if (*(payloadBits + 1) == 1) {
        headerDecode.responseRequested = true;
        if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]response requested");
    } else {
        headerDecode.responseRequested = false;
        if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]NO response requested");
    }

    for (a = 4; a < 8; a++) {
        if (*(payloadBits + a) == true) dpf = dpf + (char) (8 / pow(2, a - 4));
    }
    if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Data Packet Format: ");
    headerDecode.dataPacketFormat = dpf;
    switch (dpf) {
        case 0:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Unified Data Transport\n");
            break;

        case 1:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Response packet\n");
            break;

        case 2:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Data packet with unconfirmed delivery\n");
            break;

        case 3:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Data packet with confirmed delivery\n");
            break;

        case 13:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Short Data: Defined\n");
            break;
        case 14:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Short Data: Raw or Status/Precoded\n");
            break;

        case 15:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Proprietary Data Packet\n");
            break;

    }

    for (a = 8; a < 12; a++) {
        if (*(payloadBits + a) == true) sap = sap + (char) (8 / pow(2, a - 8));
    }

    if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]SAP id: ");
    headerDecode.sapId = sap;
    switch (sap) {

        case 0:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Unified Data Transport\n");
            break;

        case 2:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]TCP/IP header compression\n");
            break;

        case 3:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]UDP/IP header compression\n");
            break;

        case 4:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]IP based Packet data\n");
            break;

        case 5:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Address Resolution Protocol(ARP)\n");
            break;

        case 9:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Proprietary Packet data\n");
            break;

        case 10:
            if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Short Data\n");
            break;

    }
    if (dpf == 13) {
        for (a = 12; a < 16; a++) {//only AB in 2nd octet
            if (*(payloadBits + a) == true) blocksToFollow = blocksToFollow + (char) (8 / pow(2, a - 12));
        }
        headerDecode.appendBlocks = blocksToFollow;
        if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeDataHeader]Appended blocks : %i\n", blocksToFollow);

        for (a = 72; a < 80; a++) {
            if (*(payloadBits + a) == true) bitPadding = bitPadding + (char) (128 / pow(2, a - 12));
        }
    }

    return headerDecode;
}


unsigned char *decodeHalfRate(bool bits[264]) {

    bool *infoBits; //196 info bits
    bool *deInterleavedBits; //196 bits
    static bool *payloadBits; //96  bits
    int i, a, x = 0;
    static unsigned char bb[12] = {0};
    unsigned char bbb[100];

    infoBits = extractInfo(bits);
    deInterleavedBits = deInterleave(infoBits);
    payloadBits = extractPayload(deInterleavedBits);

    memset(bbb, 0, 100);
    for (a = 0; a < 96; a = a + 8) {
        bb[x] = 0;
        for (i = 0; i < 8; i++) {
            if (payloadBits[a + i] == true) bb[x] = bb[x] + (char) (128 / pow(2, i));
        }
        if (debug == 1) sprintf(bbb, "%s(%02X)%c", bbb, bb[x], bb[x]);
        x++;
    }
    if (debug == 1) syslog(LOG_NOTICE, "[BPTC1969-decodeHalfRate]%s", bbb);
    return bb;
}
