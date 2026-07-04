/*
    Copyright (C) 2026 brkzlr <brksys@icloud.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef TITLE_DATABASE_H
#define TITLE_DATABASE_H

#include <stdbool.h>
#include <stddef.h>

#define TITLE_DATABASE_KEY_SIZE 32

typedef struct TitleDatabaseEntry TitleDatabaseEntry;

typedef struct {
	TitleDatabaseEntry* entries;
	size_t count;
	size_t capacity;
	unsigned int sequence;
} TitleDatabase;

bool TitleDatabase_LoadFile(TitleDatabase* database, const char* path);
bool TitleDatabase_Finalize(TitleDatabase* database);
const char* TitleDatabase_Find(const TitleDatabase* database, const char* titleId);
void TitleDatabase_Destroy(TitleDatabase* database);

bool TitleDatabase_CompactKey(const char* titleId, char* outKey, size_t outKeySize);

#endif // TITLE_DATABASE_H
