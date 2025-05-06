/*
Copyright 2017-2025 3devo (www.3devo.eu)

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <stdint.h>
#include <Arduino.h>

/**
 * Wrapper around Stream that prints every byte transferred.
 *
 * Unlike PrintingSoftwire, this does not subclass Stream (or
 * HardwareSerial) but wraps it, because otherwise we would also need to
 * duplicate the serial ISR.
 */
class PrintingStream : public Stream {
  enum {
    IDLE,
    WRITING,
    READING,
  };

  public:
    bool print = false;

    PrintingStream(Stream &wrapped)
    : wrapped(wrapped) { }

    void printByte(uint8_t b, int state, char prefix) {
      if (!print) return;
      if (state != this->state) {
        if (this->state != IDLE)
          Serial.println();
        Serial.write(prefix);
        this->state = state;
      } else {
        Serial.write(' ');
      }

      if (b < 0x10) Serial.write('0');
      Serial.print(b, HEX);
    }
/*
    void printStart(uint8_t address, bool write, bool repStart) {
      if (!print) return;
      Serial.print(repStart ? "Sr" : "S");
      printByte(address);
      Serial.print(write ? "+W" : "+R");
    }

    result_t printResult(result_t res) {
      if (!print) return res;
      switch(res) {
        case nack:
          Serial.print("n");
          break;
        case timedOut:
          Serial.print("t");
          break;
      }
      Serial.print(" ");
      return res;
    }
*/
    void endOfTransaction() {
      if (print)
        Serial.println();
      this->state = IDLE;
    }

    size_t write(uint8_t b) {
      printByte(b, WRITING, '>');
      return wrapped.write(b);
    }
    using Stream::write;

    int read() {
      int res = wrapped.read();
      if (res >= 0)
        printByte(res, READING, '<');
      return res;
    }

    int available() {
      return wrapped.available();
    }

    int peek() {
      return wrapped.peek();
    }

    virtual void flush() {
      wrapped.flush();
    }

  private:
    int state = IDLE;
    Stream& wrapped;
};
