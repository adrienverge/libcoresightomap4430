/*
 * Copyright (C) 2013 - Adrien Vergé <adrienverge@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 only, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <signal.h>
#ifndef LIBETB_H
#define LIBETB_H

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

#define	CS_ETB	0x54162000

#define CS_UNLOCK_VALUE	0xC5ACCE55
#define CS_LAR	0xFB0
#define CS_LSR	0xFB4

/* Registers common for both TI-ETB and TBR implementations */
#define ETB_RDP		0x004 /* ETB RAM Depth Register RDP */
#define ETB_STS		0x00C /* ETB Status Register STS */
#define ETB_RRD		0x010 /* ETB RAM Read Data Register RRD */
#define ETB_RRP		0x014 /* ETB RAM Read Pointer Register RRP */
#define ETB_RWP		0x018 /* ETB RAM Write Pointer Register RWP */
#define ETB_TRIG	0x01C /* ETB Trigger counter register */
#define ETB_CTL		0x020 /* ETB Control Register CTL */
#define ETB_RWD		0x024 /* ETB RAM Write Data Register RWD */
#define ETB_FFSR	0x300 /* ETB Formatter and Flush Status Register FFSR */
#define ETB_FFCR	0x304 /* ETB Formatter and Flush Control Register FFCR */
#define ETB_DEVID	0xFC8 /* ETB device ID Register */

/* Registers specific to TI-ETB implementation */
#define ETB_WIDTH	0x008 /* ETB RAM Width Register STS */
#define ETB_RBD		0xA00 /* ETB RAM burst read Register */
#define ETB_TI_CTL	0xE20 /* ETB TI Control Register */
#define ETB_IRST	0xE00 /* ETB TI Interrupt Raw Status Register */
#define ETB_ICST	0xE04 /* ETB TI Interrupt Raw Status Register */
#define ETB_IER		0xE0C /* ETB TI Interrupt Enable Register */
#define ETB_IECST	0xE10 /* Clear interrupt enable bits */

#define TI_ETB_IRST_UNDERFLOW (1 << 3)
#define TI_ETB_IRST_OVERFLOW  (1 << 2)
#define TI_ETB_IRST_FULL      (1 << 1)
#define TI_ETB_IRST_HALF_FULL (1 << 0)

#define coresight_lock(baseaddr)	__writel(0, (baseaddr) + CS_LAR)
#define coresight_unlock(baseaddr)	\
	__writel(CS_UNLOCK_VALUE, (baseaddr) + CS_LAR)

#define etb_read_reg(offset)	__readl(etb_handle->base + (offset))
#define etb_write_reg(val, offset)	__writel((val), etb_handle->base + (offset))

#define GLOBAL_TIMEOUT	100

struct etb_handle_t {
	void *base;
};

int etb_open(struct etb_handle_t *etb_handle);

void etb_close(struct etb_handle_t *etb_handle);

int etb_enable(struct etb_handle_t *etb_handle);

int etb_disable(struct etb_handle_t *etb_handle);

int etb_status(struct etb_handle_t *etb_handle);

ssize_t etb_retrieve(struct etb_handle_t *etb_handle, void *buf, size_t bufsize);

#endif
