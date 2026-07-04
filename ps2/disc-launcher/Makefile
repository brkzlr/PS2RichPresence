EE_BIN = ps2rp.elf
EE_OBJS = build/main.o build/ps2dev9.o build/netman.o build/smap.o build/ps2ip.o build/ps2ips.o
EE_INCS += -I../../protocol
EE_LIBS += -lnetman -lps2ips -lcdvd -lpatches

BIN2C = $(PS2SDK)/bin/bin2c
OUT_DIR = ../../bin
OUT_BIN = $(OUT_DIR)/$(EE_BIN)

.PHONY: all clean

all: $(OUT_BIN)

$(OUT_BIN): $(EE_BIN) | $(OUT_DIR)
	$(EE_STRIP) --strip-all $<
	cp $< $@

$(OUT_DIR):
	mkdir -p $@

build/main.o: main.c | build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/%.o: build/%.c | build
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

build/ps2dev9.c: $(PS2SDK)/iop/irx/ps2dev9.irx | build
	$(BIN2C) $< $@ ps2dev9_irx

build/netman.c: $(PS2SDK)/iop/irx/netman.irx | build
	$(BIN2C) $< $@ netman_irx

build/smap.c: $(PS2SDK)/iop/irx/smap.irx | build
	$(BIN2C) $< $@ smap_irx

build/ps2ip.c: $(PS2SDK)/iop/irx/ps2ip-nm.irx | build
	$(BIN2C) $< $@ ps2ip_irx

build/ps2ips.c: $(PS2SDK)/iop/irx/ps2ips.irx | build
	$(BIN2C) $< $@ ps2ips_irx

build:
	mkdir -p $@

clean:
	rm -rf build $(EE_BIN) $(OUT_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
