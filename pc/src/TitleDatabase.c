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

#include "TitleDatabase.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strdup _strdup
#endif

struct TitleDatabaseEntry {
	char* key;
	char* name;
	unsigned int sequence;
};

static void FreeEntry(TitleDatabaseEntry* entry)
{
	free(entry->key);
	free(entry->name);
	entry->key = NULL;
	entry->name = NULL;
}

static bool ReserveEntries(TitleDatabase* database, size_t wantedCount)
{
	if (wantedCount <= database->capacity) {
		return true;
	}

	size_t newCapacity = database->capacity ? database->capacity * 2 : 256;
	while (newCapacity < wantedCount) {
		newCapacity *= 2;
	}

	TitleDatabaseEntry* entries = realloc(database->entries, newCapacity * sizeof(*entries));
	if (!entries) {
		return false;
	}
	database->entries = entries;
	database->capacity = newCapacity;
	return true;
}

bool TitleDatabase_CompactKey(const char* titleId, char* outKey, size_t outKeySize)
{
	if (!titleId || !outKey || outKeySize == 0) {
		return false;
	}

	size_t written = 0;
	for (const unsigned char* read = (const unsigned char*)titleId; *read; read++) {
		if (!isalnum(*read)) {
			continue;
		}
		if (written + 1 >= outKeySize) {
			outKey[0] = '\0';
			return false;
		}
		outKey[written++] = (char)toupper(*read);
	}
	outKey[written] = '\0';
	return written > 0;
}

static bool AddEntry(TitleDatabase* database, const char* serial, const char* name)
{
	char key[TITLE_DATABASE_KEY_SIZE];
	if (!TitleDatabase_CompactKey(serial, key, sizeof(key)) || !name[0]) {
		return true;
	}

	if (!ReserveEntries(database, database->count + 1)) {
		return false;
	}

	TitleDatabaseEntry* entry = &database->entries[database->count];
	entry->key = strdup(key);
	entry->name = strdup(name);
	entry->sequence = ++database->sequence;
	if (!entry->key || !entry->name) {
		FreeEntry(entry);
		return false;
	}

	database->count++;
	return true;
}

bool TitleDatabase_LoadFile(TitleDatabase* database, const char* path)
{
	FILE* file = fopen(path, "r");
	if (!file) {
		return false;
	}

	char line[1024];
	bool ok = true;
	bool skip = false;
	while (fgets(line, sizeof(line), file)) {
		bool more = !strchr(line, '\n') && !feof(file);
		if (skip) {
			skip = more;
			continue;
		}
		skip = more;
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
			continue;
		}

		char* tab = strchr(line, '\t');
		if (!tab) {
			continue;
		}

		*tab = '\0';
		char* name = tab + 1;
		name[strcspn(name, "\r\n")] = '\0';
		if (!AddEntry(database, line, name)) {
			ok = false;
			break;
		}
	}

	ok = ok && !ferror(file);
	fclose(file);
	return ok;
}

static int CompareEntriesForSort(const void* lhs, const void* rhs)
{
	const TitleDatabaseEntry* a = lhs;
	const TitleDatabaseEntry* b = rhs;
	int keyCompare = strcmp(a->key, b->key);
	if (keyCompare) {
		return keyCompare;
	}
	return (a->sequence > b->sequence) - (a->sequence < b->sequence);
}

static int CompareEntryKeys(const void* lhs, const void* rhs)
{
	const TitleDatabaseEntry* a = lhs;
	const TitleDatabaseEntry* b = rhs;
	return strcmp(a->key, b->key);
}

void TitleDatabase_Finalize(TitleDatabase* database)
{
	if (database->count == 0) {
		return;
	}

	qsort(database->entries, database->count, sizeof(*database->entries), CompareEntriesForSort);

	size_t writeIndex = 0;
	for (size_t readIndex = 0; readIndex < database->count;) {
		size_t groupEnd = readIndex + 1;
		while (groupEnd < database->count && !strcmp(database->entries[readIndex].key, database->entries[groupEnd].key)) {
			groupEnd++;
		}

		size_t keepIndex = groupEnd - 1; // Later loaded files override earlier entries.
		for (size_t i = readIndex; i < groupEnd; i++) {
			if (i != keepIndex) {
				FreeEntry(&database->entries[i]);
			}
		}
		if (writeIndex != keepIndex) {
			database->entries[writeIndex] = database->entries[keepIndex];
		}
		writeIndex++;
		readIndex = groupEnd;
	}
	database->count = writeIndex;
}

const char* TitleDatabase_Find(const TitleDatabase* database, const char* titleId)
{
	if (database->count == 0) {
		return NULL;
	}

	char key[TITLE_DATABASE_KEY_SIZE];
	if (!TitleDatabase_CompactKey(titleId, key, sizeof(key))) {
		return NULL;
	}

	TitleDatabaseEntry wanted = { .key = key };
	TitleDatabaseEntry* found = bsearch(&wanted, database->entries, database->count, sizeof(*database->entries), CompareEntryKeys);
	return found ? found->name : NULL;
}

void TitleDatabase_Destroy(TitleDatabase* database)
{
	for (size_t i = 0; i < database->count; i++) {
		FreeEntry(&database->entries[i]);
	}
	free(database->entries);
	*database = (TitleDatabase) { 0 };
}
