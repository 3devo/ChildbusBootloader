#pragma once

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

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

struct Pin {
	bool read() const;
	void hiz() const;
	void write(bool value) const;

	enum rcc_periph_clken clock;
	uint32_t port;
	uint16_t pin_mask;
};

inline bool Pin::read() const {
	rcc_periph_clock_enable(this->clock);
	gpio_mode_setup(this->port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, this->pin_mask);
	bool val = gpio_get(this->port, this->pin_mask);
	// Disable clock again, to remove need for deinit function
	rcc_periph_clock_disable(this->clock);
	return val;
}

inline void Pin::hiz() const {
	rcc_periph_clock_enable(this->clock);
	gpio_mode_setup(this->port, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, this->pin_mask);
	// Disable clock again, to remove need for deinit function
	rcc_periph_clock_disable(this->clock);
}

inline void Pin::write(bool value) const {
	rcc_periph_clock_enable(this->clock);
	if (value)
		gpio_set(this->port, this->pin_mask);
	else
		gpio_clear(this->port, this->pin_mask);
	gpio_mode_setup(this->port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, this->pin_mask);
	// Disable clock again, to remove need for deinit function
	rcc_periph_clock_disable(this->clock);
}
