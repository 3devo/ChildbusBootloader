# This builds board info files for testing
BOARD_INFO_SRC    = $(wildcard *.txt)
BOARD_INFO_H      = $(BOARD_INFO_SRC:.txt=.h)
BOARD_INFO_HEX    = $(BOARD_INFO_SRC:.txt=.hex)
BOARD_INFO_BIN    = $(BOARD_INFO_SRC:.txt=.bin)

all: $(BOARD_INFO_HEX) $(BOARD_INFO_H) $(BOARD_INFO_BIN)

%.bin: %.txt Makefile
	srec_cat "$<" -VMem -offset - -minimum-addr "$<" -VMem -o "$@" -binary
	@ if which pycrc > /dev/null && which xxd > /dev/null; then \
		echo -n "Checksum for $< (*not* written to file - update manually if needed): "; \
		pycrc --check-hexstring "$$(head -c 62 "$@" | xxd -p -c0)" --width 16 --poly 0x1021 --xor-in 0xffff --xor-out 0 --reflect-in true --reflect-out true; \
	else \
		echo "Missing pycrc or xxd, not calculating CRC"; \
	fi

%.hex: %.txt Makefile
	srec_cat "$<" -VMem -o "$@" -Intel

%.h: %.bin Makefile
	xxd -name board_info -include "$<" "$@"
