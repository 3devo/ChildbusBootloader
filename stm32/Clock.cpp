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

#include <libopencm3/stm32/rcc.h>

#include "../bootloader.h"

void ClockInit() {
    // This is essentially a simpler version of rcc_clock_setup() that
    // just switches to HSE. No need to do other setup (flash wait
    // states, voltage scaling) since HSE is also 16Mhz, so the defaults
    // should be fine (and the less we change, the less to revert).
    rcc_osc_on(RCC_HSE);
    rcc_wait_for_osc_ready(RCC_HSE);

    rcc_set_sysclk_source(RCC_HSE);
    rcc_wait_for_sysclk_status(RCC_HSE);
}

void ClockDeinit() {
    rcc_set_sysclk_source(RCC_HSI);
    rcc_wait_for_sysclk_status(RCC_HSI);

    rcc_osc_off(RCC_HSE);
}
