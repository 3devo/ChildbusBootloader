#pragma once

#include <SoftWire.h>
#include <stdint.h>
#include <Arduino.h>

/**
 * Wrapper around SoftWire that prints every byte transferred.
 *
 * SoftWire does not use virtual methods, so any methods SoftWire calls
 * on itself will not be overwritten (e.g. the low-level methods).
 *
 * Additionally, this tracks the current start/stop state and
 * automatically uses repeatedStart when no stop was given before.
 */
class PrintingSoftWire : private SoftWire {
  public:
    bool print = false;

    // Inherit constructors
    using SoftWire::SoftWire;
    using SoftWire::begin;
    using SoftWire::setTimeout_ms;
    using SoftWire::getScl;
    using SoftWire::getSda;
    using SoftWire::_setSclLow;
    using SoftWire::_setSclHigh;
    using SoftWire::_setSdaLow;
    using SoftWire::_setSdaHigh;
    using SoftWire::_readSda;
    using SoftWire::_readScl;
    using SoftWire::setDelay_us;

    void printByte(uint8_t b, const char *sep = " ") {
      if (!print) return;
      if (b < 0x10) Serial.write('0');
      Serial.print(b, HEX);
      Serial.print(sep);
    }

    void printStart(uint8_t address, bool write, bool repStart) {
      if (!print) return;
      Serial.print(repStart ? "Sr" : "S");
      printByte(address, "");
      Serial.print(write ? "+W " : "+R ");
    }

    void stop() {
      if (print)
        Serial.println('P');
      SoftWire::stop();
      started = false;
    }

    result_t startWrite(uint8_t a) {
      printStart(a, true, started);
      if (!started) {
        started = true;
        return SoftWire::startWrite(a);
      } else {
        return SoftWire::repeatedStartWrite(a);
      }
    }

    result_t startRead(uint8_t a) {
      printStart(a, false, started);
      if (!started) {
        started = true;
        return SoftWire::startRead(a);
      } else {
        return SoftWire::repeatedStartRead(a);
      }
    }

    result_t write(uint8_t b) {
      printByte(b);
      return SoftWire::write(b);
    }

    result_t readThenAck(uint8_t& b) {
      result_t res = SoftWire::readThenAck(b);
      printByte(b);
      return res;
    }

    result_t readThenNack(uint8_t& b) {
      result_t res = SoftWire::readThenNack(b);
      printByte(b);
      return res;
    }
  private:
    bool started = false;
};
