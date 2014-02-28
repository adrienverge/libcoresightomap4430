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
#include "libstm.h"

int stm_open(struct stm_handle_t *stm_handle)
{
	stm_handle->base_ctl = map_page(STM_CONFIG);
	if (stm_handle->base_ctl == NULL)
		return -1;

	stm_handle->base_xport =
		map_region(STM_XPORT,
			   STM_MIPI_NUM_CHANNELS * STM_CHAN_RESOLUTION);
	if (stm_handle->base_xport == NULL) {
		unmap_page(stm_handle->base_ctl);
		return -1;
	}

	return 0;
}

void stm_close(struct stm_handle_t *stm_handle)
{
	unmap_page(stm_handle->base_ctl);
	stm_handle->base_ctl = NULL;
	unmap_page(stm_handle->base_xport);
	stm_handle->base_xport = NULL;
}

int stm_config_for_etb(struct stm_handle_t *stm_handle)
{
	int ret = -1;
	void *base_tf;
	int timeout = GLOBAL_TIMEOUT;

	base_tf = map_page(CS_TF_DEBUGSS);
	if (base_tf == NULL)
		goto end;

	// Setup routing to get STM data to the ETB
	coresight_unlock(base_tf);
	__writel(__readl(base_tf + 0)|(1<<7), base_tf + 0);
	coresight_lock(base_tf);

	coresight_unlock(stm_handle->base_ctl);

	// In some cases the STM module can transport a SYNC message
	// thus the reason the ETB must be setup first.

	// Claim it for the Application but leave it in Debug override
	// mode allowing the Debugger to take control of it at any time.
	stm_ctl_write_reg(stm_handle, 0, STM_MIPI_REGOFF_SWMstCntl_0);
	// MOD_CLAIMED | DEBUG_OVERRIDE
	stm_ctl_write_reg(stm_handle, (1<<30)|(1<<29), STM_MIPI_REGOFF_SWMstCntl_0);

	// Check the claim bit or already enabled and application owns the unit
	while (--timeout)
		if ((stm_ctl_read_reg(stm_handle, STM_MIPI_REGOFF_SWMstCntl_0) &
		     ((3<<30)|(1<<28))) == ((1<<30)|(1<<28)))
			break;
	if ((stm_ctl_read_reg(stm_handle, STM_MIPI_REGOFF_SWMstCntl_0) &
	     ((1<<30)|(1<<28))) != ((1<<30)|(1<<28))) {
		printf("error: STM unit ownage not granted");
		goto relock_cs;
	}

	// Enable Masters
	stm_ctl_write_reg(stm_handle, 0x10204400, STM_MIPI_REGOFF_SWMstCntl_1);
	stm_ctl_write_reg(stm_handle, 0x03030303, STM_MIPI_REGOFF_SWMstCntl_2);
	stm_ctl_write_reg(stm_handle, 0x07070707, STM_MIPI_REGOFF_SWMstCntl_3);
	stm_ctl_write_reg(stm_handle, 0x07070707, STM_MIPI_REGOFF_SWMstCntl_4);
	stm_ctl_write_reg(stm_handle, 0x747C7860, STM_MIPI_REGOFF_HWMstCntl);

	// Set STM PTI output to gated mode, 4 bit data and dual edge clocking
	stm_ctl_write_reg(stm_handle, 0x000000A0, STM_MIPI_REGOFF_PTIConfig);
	// Set STM master ID generation frequency for HW messages
	// - not used for any SW messages
	stm_ctl_write_reg(stm_handle, 0xFC, STM_MIPI_REGOFF_PTICntDown);

	// MIPI STM 1.0

	// Enable ATB interface for ETB. Repeat master ID every 8 x 8
	// instrumentation access and repeat channel ID after F x 8
	// instrumentation access from master.
	// This is needed to optimize ETB buffer in circular mode.
	stm_ctl_write_reg(stm_handle, 0x0000F800, STM_MIPI_REGOFF_ATBConfig);
	stm_ctl_write_reg(stm_handle, 0x0001F800, STM_MIPI_REGOFF_ATBConfig);

	// Enable the STM module to export data
	// MOD_ENABLED | STM_TRACE_EN
	stm_ctl_write_reg(stm_handle, (2<<30)|(1<<16), STM_MIPI_REGOFF_SWMstCntl_0);

	ret = 0;

relock_cs:
	coresight_lock(stm_handle->base_ctl);

	unmap_page(base_tf);
end:
	return ret;
}

int stm_flush(struct stm_handle_t *stm_handle)
{
	int ret = -1;
	int timeout = STM_FLUSH_RETRY;

	coresight_unlock(stm_handle->base_ctl);

	while (--timeout)
		if ((stm_ctl_read_reg(stm_handle, 0x014) & (1<<8)) == (1<<8)) {
			ret = 0;
			break;
		}

	coresight_lock(stm_handle->base_ctl);

	return ret;
}
