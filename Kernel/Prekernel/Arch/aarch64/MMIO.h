/*
 * Copyright (c) 2021, Nico Weber <thakis@chromium.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Prekernel {

// Knows about memory-mapped IO addresses on the Broadcom family of SOCs used in Raspberry Pis.
// RPi3 is the first Raspberry Pi that supports aarch64.
// https://datasheets.raspberrypi.org/bcm2836/bcm2836-peripherals.pdf (RPi3)
// https://datasheets.raspberrypi.org/bcm2711/bcm2711-peripherals.pdf (RPi4 Model B)
class MMIO {
public:
    static MMIO& the();

    u32 read(FlatPtr offset) { return *(u32 volatile*)(m_base_address + offset); }
    void write(FlatPtr offset, u32 value) { *(u32 volatile*)(m_base_address + offset) = value; }

private:
    MMIO();

    unsigned int m_base_address;
};

}
