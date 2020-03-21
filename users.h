//
// Created by smarek on 3/21/20.
//

#ifndef LINDMR_USERS_H
#define LINDMR_USERS_H

#include <sqlite3.h>

sqlite3 *openDatabase();

void closeDatabase();

#endif //LINDMR_USERS_H
