# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Python owns configuration and build-state derivation. Make only keeps the
# bootstrap variables and stable command names used by developers and CI.
PYTHON ?= python3
TOPDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

phony+=all
all: build

.EXPORT_ALL_VARIABLES:

phony+=menu
menu:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" menu --root "$(TOPDIR)"

phony+=config
config:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target config

phony+=build
build:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target build

phony+=test
test:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target test

phony+=install
install:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target install

phony+=package
package:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target package

phony+=package-source
package-source:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target package-source

phony+=clean
clean:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target clean

phony+=distclean
distclean:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target distclean

phony+=run
run:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" run --root "$(TOPDIR)" --execute

phony+=run-direct
run-direct:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target run-direct

phony+=benchmark-update benchmark-svg
benchmark-update:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target benchmark-update

benchmark-svg:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target benchmark-svg

phony+=profile-lttng
profile-lttng:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target profile-lttng

phony+=list-tests
list-tests:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target list-tests

phony+=cppcheck-report
cppcheck-report:
	$(PYTHON) "$(TOPDIR)/script/menuconfig.py" execute --root "$(TOPDIR)" --target cppcheck-report

.PHONY: $(phony)
