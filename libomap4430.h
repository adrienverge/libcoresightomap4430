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
#ifndef LIBOMAP4430_H
#define LIBOMAP4430_H

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define OMAP4430_CM_EMU	0x4A307000
#define	OMAP4430_CM_L3INSTR_L3	0x4A008000

//#define __read_reg(addr)	*((uint32_t *) (addr))
//#define __write_reg(val, addr)	*((uint32_t *) (addr)) = (val)

#define __readb(addr)		*((volatile uint8_t *) (addr))
#define __writeb(val, addr)	*((volatile uint8_t *) (addr)) = (val)
#define __readw(addr)		*((volatile uint16_t *) (addr))
#define __writew(val, addr)	*((volatile uint16_t *) (addr)) = (val)
#define __readl(addr)		*((volatile uint32_t *) (addr))
#define __writel(val, addr)	*((volatile uint32_t *) (addr)) = (val)

#define GLOBAL_TIMEOUT	100

void *map_region(uint32_t hw_addr, size_t size);

void *map_page(uint32_t hw_addr);

void unmap_region(void *vaddr, size_t size);

void unmap_page(void *vaddr);

int omap4430_enable_emu();

#endif
