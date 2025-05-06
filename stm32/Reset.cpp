/*
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

#include "../Config.h"

#if defined(USE_LL_HAL)
#include "stm32g0xx.h"
#else
#include <libopencm3/cm3/scb.h>
#endif

#include "../BaseProtocol.h"

void resetSystem() {
	#if defined(USE_LL_HAL)
		NVIC_SystemReset();
	#else
		scb_reset_system();
	#endif
}

