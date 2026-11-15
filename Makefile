# clangd needs a fresh compile_commands.json whenever platformio.ini or
# lib_deps change, so always regenerate it before building.

build:
	pio run -t compiledb
	pio run

upload:
	pio run -t upload

monitor:
	pio device monitor -b 115200

db:
	pio run -t compiledb

.PHONY: build upload monitor db
