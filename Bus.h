/*
 * Copyright (C) 2015-2017 Erin Tomson <erin@rgba.net>
 * Copyright (C) 2017-2025 3devo (http://www.3devo.eu)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BUS_H_
#define BUS_H_

#include <stdint.h>
#include "Config.h"

static_assert(MAX_PACKET_LENGTH >= 32, "Protocol requires at least 32-byte packets");

void BusUpdate();
void BusInit();
void BusDeinit();
void BusSetDeviceAddress(uint8_t address);
void BusResetDeviceAddress();

int BusCallback(uint8_t address, uint8_t *buffer, uint8_t len, uint8_t maxLen);
#endif /* BUS_H_ */
