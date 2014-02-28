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
#include "libomap4430.h"

void *map_region(uint32_t hw_addr, size_t size)
{
	void *vaddr = NULL;
	int fd;

	if ((fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0) {
		printf("error: open failed\n");
		goto end;
	}

	vaddr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd,
		(off_t) hw_addr);
	if (vaddr == MAP_FAILED) {
		printf("error: mmap failed\n");
		vaddr = NULL;
		goto close_fd;
	}

close_fd:
	close(fd);
end:
	return vaddr;
}

void *map_page(uint32_t hw_addr)
{
	return map_region(hw_addr, 0x1000);
}

void unmap_region(void *vaddr, size_t size)
{
	munmap(vaddr, size);
}

void unmap_page(void *vaddr)
{
	unmap_region(vaddr, 0x1000);
}

int omap4430_enable_emu()
{
	int ret = -1;
	void *base_cm_emu, *base_l3instr_l3;
	int timeout = GLOBAL_TIMEOUT;

	base_cm_emu = map_page(OMAP4430_CM_EMU);
	if (base_cm_emu == NULL)
		goto end;

	base_l3instr_l3 = map_page(OMAP4430_CM_L3INSTR_L3);
	if (base_l3instr_l3 == NULL)
		goto unmap_cm_emu;

	// Enable clocks
	__writel(0x2, base_cm_emu + 0xA00);
	__writel(0x1, base_l3instr_l3 + 0xE20);
	__writel(0x1, base_l3instr_l3 + 0xE28);
	
	// Check if it worked
	while (--timeout)
		if (((__readl(base_cm_emu + 0xA00) & 0xf00) == 0x300)
			&& (__readl(base_cm_emu + 0xA20) & 0x40000)) {
			ret = 0;
			break;
		}

	unmap_page(base_l3instr_l3);
unmap_cm_emu:
	unmap_page(base_cm_emu);
end:
	return ret;
}
