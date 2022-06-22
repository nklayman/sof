/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0

This implements a backend for Zephyr's GDB stub. Since a serial connection is
not available, we need to implement a GDB communication system that uses an
alternative medium. This uses two circular buffers in shared SRAM within the
DEBUG region, similar to the closed-source FW implementation. The SOF kernel
driver will expose a DebugFS file which allows the GDB client to communicate
with Zephyr's stub through these buffers. To allow the setup sequence to proceed
without interference from the GDB stub, it is not activated until a special IPC
command is received. The SOF driver automatically sends this command when the
DebugFS file is accessed. In the future, this may be configurable to allow for
early-boot debugging. To enable GDB, the CONFIG_GDBSTUB kconfig option must be
enabled.
*/

#include <sof/debug/gdb/ringbuffer.h>

#define BUFFER_OFFSET 0x120

volatile struct ring * const rx = (void *)SRAM_DEBUG_BASE;
volatile struct ring * const tx = (void *)(SRAM_DEBUG_BASE + sizeof(struct ring));

void z_gdb_backend_init(void)
{
	rx->head = rx->tail = 0;
	tx->head = tx->tail = 0;
}

void z_gdb_putchar(unsigned char c)
{
	while (!ring_have_space(tx)) {
		dcache_invalidate_region((__sparse_force char __sparse_cache *)&tx->tail, 1);
		dcache_writeback_region((__sparse_force char __sparse_cache *)&tx->head, 1);
	}
	tx->data[tx->head] = c;
	tx->head = ring_next_head(tx);
	dcache_writeback_region((__sparse_force char __sparse_cache *)&tx->head, 1);
	dcache_writeback_region((__sparse_force char __sparse_cache *)&tx->data, RING_SIZE);
}

unsigned char z_gdb_getchar(void)
{
	unsigned char v;
	while (!ring_have_data(rx)) {
		dcache_invalidate_region((__sparse_force char __sparse_cache *)&rx->head, 1);
		dcache_writeback_region((__sparse_force char __sparse_cache *)&rx->tail, 1);
	}
	dcache_invalidate_region((__sparse_force char __sparse_cache *)&rx->data, RING_SIZE);
	v = rx->data[rx->tail];
	rx->tail = ring_next_tail(rx);
	return v;
}