# BIOFORGE: the everyday commands. The firmware itself is built by ./build.sh (docs/DINGHY_README.md: the toolchain).
#   make test       the engine's and the memory's host tests (plain and ASan/UBSan), the renders: build/audio/
#   make host       the app on the host, the tour of its pages: build/host/bioforge.wav and its screens
#   make tables     firmware/gen/bio_tables.h from tools/gen_tables.py
#   make art        firmware/gen/bio_art.h from tools/gen_art.py (and build/art/*.png to look at)
#   make perf       the engine's instructions a frame (callgrind, -Os): the awakening and everything at once
#   make fw         the FM-1 package build/bioforge.fwsc (JieLi toolchain in ~/.jieli, SDK in ~/fw-AC79_AIoT_SDK)
#   make web        the page: the demos, BIOFORGE for the browser, its check in Node, build/site/ (after make fw)
#   make check      what CI runs after the packages: BIOFORGE's tests, Dinghy's platform tests, fm1-emu
PY ?= python3

.PHONY: test host tables art perf fw web check emu clean
test:
	sh tests/run_bio.sh
host:
	sh tests/run_host.sh tests/scripts/pages.txt
tables:
	$(PY) tools/gen_tables.py
art:
	$(PY) tools/gen_art.py
perf:
	mkdir -p build && cc -Os -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/bio_perf tools/bio_perf.c
	for m in awaken worst perc ritual1 ritual2 ritual3 ritual4 ritual9 ritual10; do valgrind --tool=callgrind --callgrind-out-file=build/cg.$$m.out build/bio_perf $$m 690 2>&1 | \
	    sed -n 's/.*Collected : \([0-9]*\)/\1/p' | awk -v m=$$m '{ printf "%s: %d instructions a frame\n", m, $$1 / 176640 }'; done
fw:
	BOARD=smk37 PYTHON=$(PY) sh build.sh
	PYTHON=$(PY) sh build.sh
	sha256sum build/bioforge.fwsc
web:
	sh tools/make_demos.sh
	sh web/app/build.sh build/web/bioforge.wasm
	node tests/web/app_test.mjs build/web/bioforge.wasm
	$(PY) web/make_site.py build/bioforge.fwsc dev build/site
emu:
	sh tools/emu_check.sh build/bioforge.fwsc
check: fw
	sh tests/run_bio.sh
	sh tests/run_tests.sh
	sh tools/emu_ci.sh
clean:
	rm -rf build/host build/gen build/loader build/audio build/emu build/art build/web build/site build/bioforge* build/bio_*
