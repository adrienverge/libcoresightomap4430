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
#include "libetb.h"

int etb_open(struct etb_handle_t *etb_handle)
{
	int ret = -1;

	etb_handle->base = map_page(CS_ETB);
	if (etb_handle->base == NULL)
		goto end;

	coresight_unlock(etb_handle->base);

	/* ETB FIFO reset by writing 0 to ETB RAM Write Pointer Register. */
	etb_write_reg(0, ETB_RWP);
	/* Disable formatting and put ETB formatter into bypass mode. */
	etb_write_reg(0, ETB_FFCR);
	/* Setup Trigger counter. */
	etb_write_reg(0, ETB_TRIG);

	ret = 0;

	coresight_lock(etb_handle->base);

end:
	return ret;
}

void etb_close(struct etb_handle_t *etb_handle)
{
	unmap_page(etb_handle->base);
	etb_handle->base = NULL;
}

int etb_enable(struct etb_handle_t *etb_handle)
{
	int ret = -1;
	int timeout = GLOBAL_TIMEOUT;

	coresight_unlock(etb_handle->base);

	etb_write_reg(0, ETB_RRP); // RAM Read Pointer Register
	etb_write_reg(0, ETB_RWP); // RAM Write Pointer Register

	/* Enable ETB data capture by writing ETB Control Register. */
	etb_write_reg(1, ETB_CTL);

	/* Put some delays in here - make sure we can read back. */
	while (--timeout)
		if ((etb_read_reg(ETB_CTL) & 0x1) == 0x1) {
			ret = 0;
			break;
		}

	coresight_lock(etb_handle->base);

	return ret;
}

int etb_disable(struct etb_handle_t *etb_handle)
{
	int ret = -1;
	int timeout = GLOBAL_TIMEOUT;

	coresight_unlock(etb_handle->base);

	/* Manual flush */
	etb_write_reg(etb_read_reg(ETB_FFCR)|(1<<6), ETB_FFCR);

	/* Disable ETB data capture by writing ETB Control Register. */
	etb_write_reg(0, ETB_CTL);

	while (--timeout)
		if ((etb_read_reg(ETB_CTL) & 0x1) == 0x0) {
			ret = 0;
			break;
		}

	coresight_lock(etb_handle->base);

	return ret;
}

int etb_status(struct etb_handle_t *etb_handle)
{
	int ret = -1;

	coresight_unlock(etb_handle->base);

	/*// try to enable interrupts
	etb_write_reg((TI_ETB_IRST_FULL | TI_ETB_IRST_HALF_FULL), ETB_IER);
	etb_write_reg(1, ETB_IRST);*/

	printf("ETB: 0x4    RDP == 0x%04x\n", etb_read_reg(ETB_RDP));
	printf("ETB: 0x8    WDH == 0x%04x\n", etb_read_reg(ETB_WIDTH));
	printf("ETB: 0xC    STS == 0x%04x\n", etb_read_reg(ETB_STS));
	//printf("ETB: 0x10   RRD == 0x%08x\n", etb_read_reg(ETB_RRD));
	printf("ETB: 0x14   RRP == 0x%08x\n", etb_read_reg(ETB_RRP));
	printf("ETB: 0x18   RWP == 0x%08x\n", etb_read_reg(ETB_RWP));
	printf("ETB: 0x1C   TRG == 0x%08x\n", etb_read_reg(ETB_TRIG));
	printf("ETB: 0x20   CTL == 0x%08x\n", etb_read_reg(ETB_CTL));
	//printf("ETB: 0x24   RWD == 0x%08x\n", etb_read_reg(ETB_RWD));
	printf("ETB_TI_CTL      == 0x%08x\n", etb_read_reg(ETB_TI_CTL));
	printf("ETB_IRST (int)  == 0x%08x\n", etb_read_reg(ETB_IRST));
	printf("ETB_ICST (int)  == 0x%08x\n", etb_read_reg(ETB_ICST));
	printf("ETB_IER  (int)  == 0x%08x\n", etb_read_reg(ETB_IER));

	ret = 0;

	coresight_lock(etb_handle->base);

	return ret;
}

ssize_t etb_retrieve(struct etb_handle_t *etb_handle, void *buf0, size_t bufsize /* in bytes */)
{
	char *buf = (char *) buf0;
	ssize_t size = -1; /* in bytes */
	ssize_t	size_in_u32; /* in uint32_t */
	void *start, *end;
	off_t offset;

	coresight_unlock(etb_handle->base);

	/*
	 * ETB Formatter and Flush Control Register FFCR
	 *
	 * bit 1: Continuous Formatting. Continuous mode in the ETB corresponds
	 *        to normal mode with the embedding of triggers. Can only be
	 *        changed when FtStopped is HIGH. This bit is clear on reset.
	 * bit 0: Enable Formatting. Do not embed Triggers into the formatted
	 *        stream. Trace disable cycles and triggers are indicated by
	 *        TRACECTL, where fitted. Can only be changed when FtStopped is
	 *        HIGH. This bit is clear on reset.
	 *
	 * Par défaut, les deux bits valent 0, donc le mode serait 'Bypass'...
	 * (In this mode, no formatting information is inserted into the trace
	 * stream and a raw reproduction of the incoming trace stream is stored.)
	 */
	//uint32_t ffcr = etb_read_reg(0x304);
	//printf("ffcr = 0x%x\n", ffcr);
	//etb_write_reg(0, 0x304);

	start = (void *) etb_read_reg(ETB_RRP);
	end = (void *) etb_read_reg(ETB_RWP); // number of available words

	size_in_u32 = end - start;
	/*printf("start = %p\n", start);
	printf("end   = %p\n", end);//*/
	etb_write_reg(0, ETB_RRP);

	/*etb_status();
	printf("%08x ", (uint32_t) etb_read_reg(0x10));
	etb_write_reg(4, 0x14);
	printf("%08x ", (uint32_t) etb_read_reg(0x10));
	printf("%08x ", (uint32_t) etb_read_reg(0x10));
	printf("%08x ", (uint32_t) etb_read_reg(0x10));
	printf("\n");
	etb_status();*/

	// read addr > write addr: error
	if (size_in_u32 < 0) {
		goto end;
	//} else if (start == end) {
	//} else if (start + 4 >= end) {
	//	size = 0;
	} else {
		size = 4 * size_in_u32;
		if (size > bufsize)
			size = bufsize;

		for (offset = 0; offset < size; offset += 4)
			*((uint32_t *) &buf[offset]) = etb_read_reg(ETB_RRD);

		/*
		 * Coresight ETB adds 0x01 0x00 0x00 0x00... (up to 15 bytes
		 * of 0x00) so we do a little trick to remove them
		 */
		/*
		off_t i;
		for (i = size - 1; i > size - 1 - 16; i--) {
			if ((uint8_t) buf[i] == 0x0)
				size--;
			else if ((uint8_t) buf[i] == 0x1) {
				size--;
				break;
			} else
				break;
		}*/
		if (size == 16 &&
		    *((uint32_t *) &buf[0]) == 0x00000001 &&
		    *((uint32_t *) &buf[4]) == 0x00000000 &&
		    *((uint32_t *) &buf[8]) == 0x00000000 &&
		    *((uint32_t *) &buf[12]) == 0x00000000) {
			size = 0;
		}
	}

end:
	coresight_lock(etb_handle->base);

	return size;
}
