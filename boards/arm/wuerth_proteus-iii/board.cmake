# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

board_runner_args(openocd "--config=board/nordic_nrf52_dk.cfg")

include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
