/*
    Copyright (C) 2024 brkzlr <brksys@icloud.com>

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
#ifndef SAMBAREADER_H
#define SAMBAREADER_H

#include "types.h"

#include <stdbool.h>
#include <time.h>

void SmbReader_Cleanup(void);
void SmbReader_Init(const char* sharePath);
void SmbReader_ReadStatus(void);

const string_t* SmbReader_GetGameName(void);
time_t SmbReader_GetGameTimestamp(void);
bool SmbReader_IsInfoValid(void);

#endif // SAMBAREADER_H
