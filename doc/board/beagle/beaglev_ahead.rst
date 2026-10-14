.. SPDX-License-Identifier: GPL-2.0+

BeagleBoard.org BeagleV-Ahead
=============================

BeagleV-Ahead is an open-source single board computer based on the T-Head
TH1520 SoC (4x Xuantie C910 @ 1.85GHz), featuring 4 GB of LPDDR4X and 16 GB of
eMMC flash.

 - SoC            T-Head TH1520
 - System Memory  4 GB LPDDR4X (single rank, 3733 MT/s)
 - Storage        16 GB eMMC flash
 -                external microSD slot
 - Networking     Gigabit Ethernet
 -                WiFi + BT
 - Display        HDMI 2.0, 4-lane MIPI DSI
 - USB            4x USB 3.0 Type-A, 1x USB 2.0 Type-C
 - Expansion      BeagleBone-compatible P8/P9 headers

Mainline support
----------------

The following features are currently supported:

1. ns16550 UART driver
2. eMMC and microSD card
3. Gigabit Ethernet (DesignWare GMAC with Realtek PHY)
4. LPDDR4X controller and PHY initialization in SPL

Building
~~~~~~~~

1. Add the RISC-V toolchain to your PATH.
2. Setup ARCH & cross compilation environment variable:

.. code-block:: none

   export CROSS_COMPILE=<riscv64 toolchain prefix>

3. Build DDR firmware

The DDR driver requires a firmware blob to initialize the controller and the
PHY. BeagleV-Ahead has a single rank of LPDDR4X, which needs the single-rank
variant of the configuration:

.. code-block:: bash

	git clone --depth 1 https://github.com/ziyao233/th1520-firmware
	cd th1520-firmware
	lua5.4 ddr-generate.lua src/lpddr4x-3733-singlerank.lua th1520-ddr-firmware.bin

4. Build OpenSBI firmware

The proper U-Boot runs in S mode, thus OpenSBI is required as SBI firmware.
TH1520 support in OpenSBI requires v1.2 or a more recent version:

.. code-block:: bash

	git clone https://github.com/riscv-software-src/opensbi.git
	cd opensbi
	make PLATFORM=generic

5. Build U-Boot images

The DDR firmware must be found by binman. The simplest way is to copy it to
the U-Boot source directory before building:

.. code-block:: bash

	cd <U-Boot-dir>
	cp <path-to-ddr-firmware> th1520-ddr-firmware.bin
	make beaglev_ahead_defconfig
	make OPENSBI=<opensbi_dir>/build/platform/generic/firmware/fw_dynamic.bin

Alternatively, binman can be pointed to the directory holding the firmware
with BINMAN_INDIRS, which keeps the source tree clean and works with
out-of-tree builds:

.. code-block:: bash

	cd <U-Boot-dir>
	make O=<build-dir> beaglev_ahead_defconfig
	make O=<build-dir> \
		BINMAN_INDIRS=<dir-containing-th1520-ddr-firmware.bin> \
		OPENSBI=<opensbi_dir>/build/platform/generic/firmware/fw_dynamic.bin

This generates u-boot-with-spl.bin, which contains SPL, the DDR firmware, the
OpenSBI firmware and the proper U-Boot.

Booting
~~~~~~~

u-boot-with-spl.bin is loaded to SRAM through fastboot. Hold the USB button
while connecting the board to the host through the Type-C port, then release
the button and run:

.. code-block:: bash

	fastboot flash ram u-boot-with-spl.bin
	fastboot reboot

The serial console is available on the debug UART header at 115200 8n1.
