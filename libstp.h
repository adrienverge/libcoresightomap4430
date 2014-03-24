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
#ifndef LIBSTP_H
#define LIBSTP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct stp_pkt {
	struct stp_pkt *next;
	char *data;
	size_t len;
	int timestamp;
	unsigned char channel;
};

void free_stp_pkt_list(struct stp_pkt *list);

struct stp_pkt *stp_read_pkts(char *in, size_t size);
size_t stp_count_pkts(char *in, size_t u8size);

struct stp_pkt *stp_read_pkts_in_raw_etb(char *buf, size_t u8size);
size_t stp_count_pkts_in_raw_etb(char *buf, size_t u8size);

#ifdef __cplusplus
}
#endif

#endif
