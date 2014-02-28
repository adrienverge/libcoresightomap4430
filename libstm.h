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
#ifndef LIBSTM_H
#define LIBSTM_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include "libomap4430.h"

#define STM_XPORT		0x54000000
#define STM_CHAN_RES		0x1000
#define STM_CONFIG		0x54161000
#define STM_MIPI_NUM_CHANNELS	256
#define STM_CHAN_RESOLUTION	0x1000
#define STM_FLUSH_RETRY		1024

#define STM_MIPI_REGOFF_Id           0x000
#define STM_MIPI_REGOFF_SysConfig    0x010
#define STM_MIPI_REGOFF_SysStatus    0x014
#define STM_MIPI_REGOFF_SWMstCntl_0  0x024
#define STM_MIPI_REGOFF_SWMstCntl_1  0x028
#define STM_MIPI_REGOFF_SWMstCntl_2  0x02C
#define STM_MIPI_REGOFF_SWMstCntl_3  0x030
#define STM_MIPI_REGOFF_SWMstCntl_4  0x034
#define STM_MIPI_REGOFF_HWMstCntl    0x038
#define STM_MIPI_REGOFF_PTIConfig    0x03C
#define STM_MIPI_REGOFF_PTICntDown   0x040
#define STM_MIPI_REGOFF_ATBConfig    0x044
#define STM_MIPI_REGOFF_ATBSyncCnt   0x048
#define STM_MIPI_REGOFF_ATBHeadPtr   0x048
#define STM_MIPI_REGOFF_ATBid        0x04C

#define CS_TF_DEBUGSS	0x54164000

#define CS_UNLOCK_VALUE	0xC5ACCE55
#define CS_LAR	0xFB0
#define CS_LSR	0xFB4

#define coresight_lock(baseaddr)	__writel(0, (baseaddr) + CS_LAR)
#define coresight_unlock(baseaddr)	\
	__writel(CS_UNLOCK_VALUE, (baseaddr) + CS_LAR)

#define stm_ctl_read_reg(stm_handle, offset) \
	__readl((stm_handle)->base_ctl + (offset))
#define stm_ctl_write_reg(stm_handle, val, offset) \
	__writel((val), (stm_handle)->base_ctl + (offset))

#define stm_xport_addr(stm_handle, channel) \
	((stm_handle)->base_xport + (channel) * STM_CHAN_RESOLUTION)
#define stm_xport_addr_ts(stm_handle, channel) \
	((stm_handle)->base_xport + (channel) * STM_CHAN_RESOLUTION \
	 + STM_CHAN_RESOLUTION / 2)

#define stm_xport_writeb(stm_handle, val, channel) \
	__writeb((val), stm_xport_addr(stm_handle, channel))
#define stm_xport_ts_writeb(stm_handle, val, channel) \
	__writeb((val), stm_xport_addr_ts(stm_handle, channel))
#define stm_xport_writew(stm_handle, val, channel) \
	__writew((val), stm_xport_addr(stm_handle, channel))
#define stm_xport_ts_writew(stm_handle, val, channel) \
	__writew((val), stm_xport_addr_ts(stm_handle, channel))
#define stm_xport_writel(stm_handle, val, channel) \
	__writel((val), stm_xport_addr(stm_handle, channel))
#define stm_xport_ts_writel(stm_handle, val, channel) \
	__writel((val), stm_xport_addr_ts(stm_handle, channel))

#define GLOBAL_TIMEOUT	100

struct stm_handle_t {
	void *base_ctl, *base_xport;
};

int stm_open(struct stm_handle_t *stm_handle);

void stm_close(struct stm_handle_t *stm_handle);

int stm_config_for_etb(struct stm_handle_t *stm_handle);

int stm_flush(struct stm_handle_t *stm_handle);

/*
 * Strangely, it runs really faster with 'static inline function()' than
 * with a '#define function()'. Maybe some gcc magic?
 */
#if 0
#define stm_send_u24_pkt(stm_handle, channel, data) \
	stm_xport_ts_writel(stm_handle, (3 << 24) | ((data) & 0xffffff), channel)
#else
static inline void stm_send_u24_pkt(struct stm_handle_t *stm_handle,
				    int channel, uint32_t data)
{
	stm_xport_ts_writel(stm_handle, (3 << 24) | ((data) & 0xffffff), channel);
}
#endif
static inline void stm_send_u32_pkt(struct stm_handle_t *stm_handle,
				    int channel, uint32_t data)
{
	stm_xport_writel(stm_handle, data, channel);
	stm_xport_ts_writeb(stm_handle, 8, channel);
}

static inline ssize_t stm_send_msg_pkt(struct stm_handle_t *stm_handle,
				       int channel, void *data, size_t len)
{
	void *end = data + len;

	if (len >= 255)
		return -1;

	/*
	 * If needed, align to 32 bit block
	 */
	if (((uint32_t) data) % 2) {
		stm_xport_writeb(stm_handle, *(uint8_t *) data, channel);
		data++;
	}
	if (((uint32_t) data) % 4) {
		stm_xport_writew(stm_handle, *(uint16_t *) data, channel);
		data += 2;
	}

	/*
	 * Send the biggest part but keep at least one byte for the end
	 */
	while (data + 4 <= end) {
		stm_xport_writel(stm_handle, *(uint32_t *) data, channel);
		data += 4;
	}

	/*
	 * Send the last part, terminating by the lenght byte, timestamped
	 */
	if (data + 3 == end) {
		stm_xport_ts_writel(stm_handle, (len << 24) | ((*(uint32_t *) data) & 0xffffff), channel);
	} else if (data + 2 == end) {
		stm_xport_writew(stm_handle, *(uint16_t *) data, channel);
		stm_xport_ts_writeb(stm_handle, len, channel);
	} else if (data + 1 == end) {
		stm_xport_ts_writew(stm_handle, (len << 8) | (*(uint8_t *) data), channel);
	} else {
		stm_xport_ts_writeb(stm_handle, len, channel);
	}

	return len;
}

#endif
