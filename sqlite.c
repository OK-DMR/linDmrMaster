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

sqlite3 *openDatabase() {
    int rc;
    sqlite3 *db;

    rc = sqlite3_open("Master_Server.sql", &db);
    if (rc) {
        syslog(LOG_NOTICE, "Cant't open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }
    return db;
}

void closeDatabase(sqlite3 *db) {
    if (sqlite3_close(db) != SQLITE_OK) syslog(LOG_NOTICE, "Failed to close SQLITE database");
}

bool isFieldExisting(sqlite3 *db, char *table, char *field) {
    sqlite3_stmt *stmt;
    char SQLQUERY[200];

    syslog(LOG_NOTICE, "Database, checking column '%s' in table '%s'", field, table);
    sprintf(SQLQUERY, "PRAGMA table_info(%s)", table);
    if (sqlite3_prepare_v2(db, SQLQUERY, -1, &stmt, 0) == 0) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (strcmp(sqlite3_column_text(stmt, 1), field) == 0) {
                sqlite3_finalize(stmt);
                return true;
            }
        }
    } else {
        syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    syslog(LOG_NOTICE, "field %s doesn't exist", field);
    return false;
}

bool isTableExisting(sqlite3 *db, char *table) {
    sqlite3_stmt *stmt;
    char SQLQUERY[200];

    syslog(LOG_NOTICE, "Database, checking table '%s'", table);
    sprintf(SQLQUERY, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name = '%s'", table);
    if (sqlite3_prepare_v2(db, SQLQUERY, -1, &stmt, 0) == 0) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (sqlite3_column_int(stmt, 0) == 0) {
                syslog(LOG_NOTICE, "table %s doesn't exist", table);
                sqlite3_finalize(stmt);
                return false;
            }
        }
    } else {
        syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

int initDatabase(sqlite3 *db) {
    char SQLQUERY[1000];
    int r;
    sqlite3_stmt *stmt;

    srand(time(NULL));

    do {
        r = rand() % 99999;
    } while (r < 10000);
    if (!isTableExisting(db, "master")) {
        sprintf(SQLQUERY,
                "CREATE TABLE master (repTS1 VARCHAR(100) default '',repTS2 VARCHAR(100) default '',sMasterTS1 VARCHAR(100) default '',sMasterTS2 VARCHAR(100) default '', timeBase INTEGER default 60, servicePort int default 50000, rdacPort int default 50002,dmrPort int default 50001, baseDmrPort int default 50100, baseRdacPort int default 50200, maxRepeaters int default 20, echoId int default 9990, rrsGpsId int default 500, aprsUrl VARCHAR(100) default '', aprsPort VARCHAR(7) default '8080', echoSlot integer default 1, masterDmrId integer default %i)",
                r);
        if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
            sprintf(SQLQUERY, "INSERT INTO master (repTS1) VALUES ('')");
            if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
                syslog(LOG_NOTICE, "Table master created");
            } else {
                syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
                return 0;
            }
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }
    if (!isTableExisting(db, "sMaster")) {
        sprintf(SQLQUERY,
                "CREATE TABLE sMaster (ownName VARCHAR(100) default '',ownCountryCode VARCHAR(5) default '',ownRegion VARCHAR(2) default '',sMasterIp VARCHAR(100) default '', sMasterPort VARCHAR(5) default '62010', priorityTGTS1 integer default 0, priorityTGTS2 integer default 0, priorityTimeout integer default 10)");
        if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
            sprintf(SQLQUERY, "INSERT INTO sMaster (ownName) VALUES ('')");
            if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
                syslog(LOG_NOTICE, "Table sMaster created");
            } else {
                syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
                return 0;
            }
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isTableExisting(db, "repeaters")) {
        sprintf(SQLQUERY,
                "CREATE TABLE repeaters (repeaterId INTEGER default 0 ,callsign VARCHAR(10) default '',txFreq VARCHAR(10) default '',shift VARCHAR(7) default '', hardware VARCHAR(11) default '', firmware VARCHAR(12) default '', mode VARCHAR(4) default '', currentAddress INTEGER default 0, timeStamp varchar(20) default '1970-1-1 00:00:00', ipAddress VARCHAR(50) default '',language VARCHAR(50) default 'english', geoLocation VARCHAR(20) default '', aprsPass VARCHAR(5) default '0000', aprsBeacon VARCHAR(100) default 'DMR repeater', aprsPHG VARCHHAR(7) default '', currentReflector integer default 0, autoReflector integer default 0, userName default '', upDated integer default 0, lastRdacUpdate varchar(20) default '1970-1-1 00:00:00', online integer default 0, intlRefAllow integer default 1, reflectorTimeout integer default 1800,autoConnectTimer integer default 600)");
        if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
            syslog(LOG_NOTICE, "Table repeater created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isTableExisting(db, "callsigns")) {
        sprintf(SQLQUERY,
                "CREATE TABLE callsigns (radioId INTEGER default 0 PRIMARY KEY, callsign VARCHAR(32) default '', name VARCHAR(32) default '', aprsSuffix VARCHAR(3) default '-9', aprsBeacon VARCHAR(100) default 'DMR terminal', aprsSymbol INTEGER default 62, hasSendAprs INTEGER default 0, messageStore INTEGER default 1, email VARCHAR(100) default '', login VARCHAR(50) default '', password VARCHAR(50) default '', lastAprsTime INTEGER default 0, madeChange INTEGER default 0, city VARCHAR(32) default '', state VARCHAR(32) default '', country VARCHAR(32) default '', radio VARCHAR(32) default '', homeRepeaterId VARCHAR(32) default '', remarks VARCHAR(32) default '')");
        if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
            syslog(LOG_NOTICE, "Table callsigns created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isTableExisting(db, "rrs")) {
        sprintf(SQLQUERY,
                "CREATE TABLE rrs (radioId INTEGER default 0 PRIMARY KEY, callsign VARCHAR(32) default '',name VARCHAR(32) default '', registerTime VARCHAR(20) default '1970-01-01 00:00:00', onRepeater VARCHAR(32) default '', unixTime long)");
        if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
            syslog(LOG_NOTICE, "Table rrs created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isTableExisting(db, "traffic")) {
        sprintf(SQLQUERY,
                "CREATE TABLE traffic (senderId int default 0 PRIMARY KEY,senderCallsign varchar(32) default '',targetId int default 0,targetCallsign varchar(32) default '',channel int default 0,serviceType varchar(15) default 'Voice',callType varchar(15) default 'Group',timeStamp int default 0, onRepeater varchar(32) default '', senderName varchar(32) default '')");
        if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
            syslog(LOG_NOTICE, "Table traffic created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isTableExisting(db, "voiceTraffic")) {
        sprintf(SQLQUERY,
                "CREATE TABLE voiceTraffic (senderId int default 0 PRIMARY KEY,senderCallsign varchar(32) default '',targetId int default 0,targetCallsign varchar(32) default '',channel int default 0,serviceType varchar(15) default 'Voice',callType varchar(15) default 'Group',timeStamp int default 0, onRepeater varchar(32) default '', senderName varchar(32) default '')");
        if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
            syslog(LOG_NOTICE, "Table voiceTraffic created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isTableExisting(db, "localReflectors")) {
        sprintf(SQLQUERY, "CREATE TABLE localReflectors(id int primary key,name varchar(50),type int default 0)");
        if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
            syslog(LOG_NOTICE, "Table localReflectors created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }


    if (!isTableExisting(db, "access")) {
        sprintf(SQLQUERY,
                "CREATE TABLE access(userName varchar(20) default '' PRIMARY KEY, password varchar(20) default '', role varchar(30) default 'log');");
        if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
            syslog(LOG_NOTICE, "Table access created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
        sprintf(SQLQUERY, "INSERT INTO access (userName,password,role) VALUES ('admin','admin','admin')");
        if (sqlite3_exec(db, SQLQUERY, NULL, NULL, NULL) == 0) {
            syslog(LOG_NOTICE, "Default admin user created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }

    }

    //check for new fields added later in development

    if (!isFieldExisting(db, "localReflectors", "type")) {
        sprintf(SQLQUERY, "alter table localReflectors add type integer default 0");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field type in localReflectors created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }


    if (!isFieldExisting(db, "repeaters", "intlRefAllow")) {
        sprintf(SQLQUERY, "alter table repeaters add intlRefAllow integer default 1");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field intlRefAllow in repeaters created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }


    if (!isFieldExisting(db, "repeaters", "reflectorTimeout")) {
        sprintf(SQLQUERY, "alter table repeaters add reflectorTimeout integer default 1800");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field reflectorTimeout in repeaters created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isFieldExisting(db, "repeaters", "autoConnectTimer")) {
        sprintf(SQLQUERY, "alter table repeaters add autoConnectTimer integer default 600");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field autoConnectTimer in repeaters created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }


    if (!isFieldExisting(db, "master", "masterDmrId")) {
        sprintf(SQLQUERY, "alter table master add masterDmrId integer default %i", r);
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field masterDmrId in master created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isFieldExisting(db, "master", "debug")) {
        sprintf(SQLQUERY, "alter table master add debug integer default 0");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field debug in master created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isFieldExisting(db, "master", "dynamicTS1")) {
        sprintf(SQLQUERY, "alter table master add dynamicTS1 varchar(100) default ''");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field dynamicTS1 in master created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isFieldExisting(db, "master", "dynamicTS2")) {
        sprintf(SQLQUERY, "alter table master add dynamicTS2 varchar(100) default ''");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field dynamicTS2 in master created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isFieldExisting(db, "sMaster", "priorityTGTS1")) {
        sprintf(SQLQUERY, "alter table sMaster add priorityTGTS1 integer default 0");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field priorityTGTS1 in sMaster created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isFieldExisting(db, "sMaster", "priorityTGTS2")) {
        sprintf(SQLQUERY, "alter table sMaster add priorityTGTS2 integer default 0");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field priorityTGTS2 in sMaster created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }


    if (!isFieldExisting(db, "sMaster", "priorityTimeout")) {
        sprintf(SQLQUERY, "alter table sMaster add priorityTimeout integer default 10");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field priorityTimeout in sMaster created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isFieldExisting(db, "sMaster", "eMail")) {
        sprintf(SQLQUERY, "alter table sMaster add eMail varchar(100) default 'dmrMaster@test.be'");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field eMail in sMaster created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }

    if (!isFieldExisting(db, "repeaters", "codecRepeater")) {
        sprintf(SQLQUERY, "alter table repeaters add codecRepeater integer default 0");
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "field codecRepeater in repeaters created");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }


    //Fix for random master ID max 5 digits
    bool misMatch = false;
    sprintf(SQLQUERY, "SELECT masterDmrId FROM master");
    if (sqlite3_prepare_v2(db, SQLQUERY, -1, &stmt, 0) == 0) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (sqlite3_column_int(stmt, 0) < 10000 || sqlite3_column_int(stmt, 0) > 99999) {
                misMatch = true;
            }
            sqlite3_finalize(stmt);
        }
    }

    if (misMatch) {
        syslog(LOG_NOTICE, "Correcting random masterID");
        sprintf(SQLQUERY, "update master set masterDmrId = %i", r);
        if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
            syslog(LOG_NOTICE, "Successfully changed masterId");
        } else {
            syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
            return 0;
        }
    }
    //Clean database
    sprintf(SQLQUERY, "update repeaters set currentReflector = 0");
    if (sqlite3_exec(db, SQLQUERY, 0, 0, 0) == 0) {
        syslog(LOG_NOTICE, "Reflector status cleaned");
    } else {
        syslog(LOG_NOTICE, "Database error: %s", sqlite3_errmsg(db));
    }

    return 1;
}
