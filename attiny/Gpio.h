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
#include <avr/io.h>

struct Pin {
	bool read() const;
	void hiz() const;
	void write(bool value) const;

	volatile uint8_t* port;
	volatile uint8_t* pin;
	volatile uint8_t* ddr;
	volatile uint8_t* pue;
	uint8_t mask;
};

inline bool Pin::read() const {
	*this->ddr &= ~this->mask;
	return (*this->pin) & this->mask;
}

inline void Pin::hiz() const {
	// Hi-Z is just input mode on attiny
	*this->ddr &= ~this->mask;
}

inline void Pin::write(bool value) const {
	if (value)
		*this->port |= this->mask;
	else
		*this->port &= ~this->mask;

	*this->ddr |= this->mask;
}
