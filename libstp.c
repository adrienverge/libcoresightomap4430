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
#include <stdio.h>
#include <stdlib.h>

#include "libstp.h"
#include "libstm.h"

#define halfbyte(src, pos) \
	(((pos)%2)?((src[(pos)/2]>>4)&0xf):(src[(pos)/2]&0xf))
#define byteat(src, pos) \
	(((pos)%2) ? \
		(((halfbyte(src, (pos))<<0) | \
		 (halfbyte(src, (pos)+1)<<4)) & 0xff) : \
		src[(pos)/2]&0xff )

enum stp_msg_format {
	STP_MASTER = 0x1,
	STP_OVRF = 0x2,
	STP_C8 = 0x3,
	STP_D8 = 0x4, STP_D8TS = 0x8,
	STP_D16 = 0x5, STP_D16TS = 0x9,
	STP_D32 = 0x6, STP_D32TS = 0xa
};

#define STP_MSG_IS_CHANNEL(msg_type) \
	((msg_type)==STP_C8)
#define STP_MSG_IS_DATA(msg_type) \
	((msg_type)==STP_D8||(msg_type)==STP_D16||(msg_type)==STP_D32|| \
	 (msg_type)==STP_D8TS||(msg_type)==STP_D16TS||(msg_type)==STP_D32TS)
#define STP_MSG_IS_TIMESTAMPED(msg_type) \
	((msg_type)==STP_D8TS||(msg_type)==STP_D16TS||(msg_type)==STP_D32TS)
#define STRINGIFY_STP_TYPE(v) \
	v==STP_MASTER?"STP_MASTER":v==STP_OVRF?"STP_OVRF":v==STP_C8?"STP_C8":v\
	==STP_D8?"STP_D8":v==STP_D8TS?"STP_D8TS":v==STP_D16?"STP_D16":v==\
	STP_D16TS?"STP_D16TS":v==STP_D32?"STP_D32":v==STP_D32TS?"STP_D32TS":\
	"(unknown)"

/*
 * Packet layout :
 * ---------------------------------------
 * |      msg      |id|  msg  |id| ...
 * ---------------------------------------
 * with msg of various formats (see enum stp_msg_format),
 * and id a 4-bit identifier.
 *
 * Type of msg inside a packet:
 *   id | packet type
 * ----------------------------
 *    4 | 1B data
 *    5 | 2B data
 *    6 | 4B data
 *    8 | timestamp + 1B data
 *    9 | timestamp + 2B data
 *    a | timestamp + 4B data
 *
 * Timestamp can be either 1B or 2B.
 * When 2B, it seemd that the first byte
 * always starts with a byte &= 0xe
 *
 * When using OST headers only:
 * The first block (header) is always of type 6 or a (4B + optional
 * timestamp)and its first byte (after timestamp contains the payload
 * size).
 * If this payload size >= 0xff, this byte is 0xff and a second block
 * of type 6 or a contains the real size.
 */

void free_stp_pkt_list(struct stp_pkt *list)
{
	if (list->next != NULL)
		free_stp_pkt_list(list->next);
	free(list);
}

static struct stp_pkt *new_stp_pkt(char *data, size_t len, int timestamp)
{
	struct stp_pkt *pkt;

	pkt = malloc(sizeof(struct stp_pkt));
	if (pkt == NULL) {
		perror("malloc");
		return NULL;
	}

	pkt->data = malloc(len);
	if (pkt->data == NULL) {
		perror("malloc");
		free(pkt);
		return NULL;
	}

	pkt->timestamp = timestamp;
	pkt->len = len;
	pkt->channel = 0xff;

	return pkt;
}

/*
 * Takes an ETB buffer and find packets inside.
 * Returns a linked-list of struct stp_pkt.
 *
 * The input buffer is read from the end to the beginning.
 */
struct stp_pkt *stp_read_pkts(char *in, size_t u8size)
{
	size_t u4size = 2 * u8size;
	off_t i, j;
	enum stp_msg_format msg_type;
	ssize_t pkt_len, msg_len, data_len;
	int timestamp;
	uint32_t data;

	struct stp_pkt *pkt = NULL, *pkt_list = NULL;
	off_t pkt_offset = 0;

	if (halfbyte(in, u4size - 1) == 0)
		u4size--;

#if defined(DEBUG)
	for (i = 0; i < u4size; i++)
		fprintf(stderr, "%x ", halfbyte(in, i));
	fprintf(stderr, "\n");
#endif

	i = u4size - 1;

	while (i > 0) {
		msg_type = halfbyte(in, i);

		switch (msg_type) {
		case STP_MASTER:
		case STP_OVRF:
		case STP_C8:
		case STP_D8:
		case STP_D8TS:
			data_len = 2;
			break;
		case STP_D16:
		case STP_D16TS:
			data_len = 4;
			break;
		case STP_D32:
		case STP_D32TS:
			data_len = 8;
			break;
		default:
			data_len = 0;
			fprintf(stderr, "ERROR: unknown STP message type: %x",
				msg_type);
			goto end;
		}

		if (i - data_len < 0 ||
		    (STP_MSG_IS_TIMESTAMPED(msg_type) && i - data_len - 2 < 0)) {
			fprintf(stderr, "ERROR: %s packet truncated at the "
				"beggining of buffer\n",
				STRINGIFY_STP_TYPE(msg_type));
			goto end;
		}

		msg_len = data_len;
		if (STP_MSG_IS_TIMESTAMPED(msg_type)) {
			msg_len += 2;
			if (halfbyte(in, i - msg_len - 1) == 0xe) {
				int hb0, b1;
				msg_len += 2;
				hb0 = halfbyte(in, i - msg_len),
				b1 = byteat(in, i - msg_len + 2);
				if (hb0 < 7)
					timestamp = (1 << (7 + hb0)) + ((b1 ^ 0x80) << (hb0));
				else
					timestamp = (1 << hb0) + (b1 << (2 * hb0 - 6));
			} else {
				timestamp = byteat(in, i - msg_len);
			}
		}

		data = 0;
		for (j = data_len - 2; j >= 0; j -= 2) {
			data = data << 8;
			data |= byteat(in, i - data_len + j);
		}

		if (STP_MSG_IS_DATA(msg_type)) {
			if (pkt_offset <= 0) {
				pkt_len = data >> (4 * (data_len - 2));

				if (2 * i - pkt_len < 0) {
					fprintf(stderr, "ERROR: packet "
						"overflowing on left\n");
					goto end;
				}

				pkt = new_stp_pkt(NULL, pkt_len, timestamp);
				if (pkt == NULL) {
					fprintf(stderr, "ERROR: pkt == NULL\n");
					goto end;
				}
				pkt_offset = pkt_len;

				pkt->next = pkt_list;
				pkt_list = pkt;

				data_len -= 2;
			}

			pkt_offset -= data_len / 2;
			if (data_len == 2) // 1 byte
				*((uint8_t *) &pkt->data[pkt_offset])
					= data;
			else if (data_len == 4) // 2 bytes
				*((uint16_t *) &pkt->data[pkt_offset])
					= (uint16_t) data;
			else if (data_len == 6) { // 3 bytes
				*((uint16_t *) &pkt->data[pkt_offset + 1])
					= (uint16_t) (data >> 8);
				*((uint8_t *) &pkt->data[pkt_offset])
					= (uint8_t) data;
			} else if (data_len == 8) // 4 bytes
				*((uint32_t *) &pkt->data[pkt_offset])
					= (uint32_t) data;
		} else if (STP_MSG_IS_CHANNEL(msg_type)) {
			if (pkt != NULL)
				pkt->channel = data;
#if defined(DEBUG)
		} else {
			fprintf(stderr, "%-10s: ", STRINGIFY_STP_TYPE(msg_type));
			fprintf(stderr, "%x\n", data);
#endif
		}

		i -= (msg_len + 1);
	}

end:
	return pkt_list;
}

size_t stp_count_pkts(char *in, size_t u8size)
{
	size_t u4size = 2 * u8size;
	off_t i;
	enum stp_msg_format msg_type;
	ssize_t pkt_len, msg_len, data_len;

	off_t pkt_offset = 0;

	size_t count = 0;

	if (halfbyte(in, u4size - 1) == 0)
		u4size--;

	i = u4size - 1;

	while (i > 0) {
		msg_type = halfbyte(in, i);

		switch (msg_type) {
		case STP_MASTER:
		case STP_OVRF:
		case STP_C8:
		case STP_D8:
		case STP_D8TS:
			data_len = 2;
			break;
		case STP_D16:
		case STP_D16TS:
			data_len = 4;
			break;
		case STP_D32:
		case STP_D32TS:
			data_len = 8;
			break;
		default:
			data_len = 0;
			fprintf(stderr, "ERROR: unknown STP message type: %x",
				msg_type);
			goto end;
		}

		if (i - data_len < 0 ||
		    (STP_MSG_IS_TIMESTAMPED(msg_type) && i - data_len - 2 < 0)) {
			fprintf(stderr, "ERROR: %s packet truncated at the "
				"beggining of buffer\n",
				STRINGIFY_STP_TYPE(msg_type));
			goto end;
		}

		msg_len = data_len;
		if (STP_MSG_IS_TIMESTAMPED(msg_type)) {
			msg_len += 2;
			if (halfbyte(in, i - msg_len - 1) == 0xe)
				msg_len += 2;
		}

		if (STP_MSG_IS_DATA(msg_type)) {
			if (pkt_offset <= 0) {
				pkt_len = byteat(in, i - 2);

				if (2 * i - pkt_len < 0) {
					fprintf(stderr, "ERROR: packet "
						"overflowing on left\n");
					goto end;
				}

				count++;

				pkt_offset = pkt_len;

				data_len -= 2;
			}
			pkt_offset -= data_len / 2;
		}

		i -= (msg_len + 1);
	}

end:
	return count;
}

/*
 * In ETB, blocks are separated by sync packets that consist of
 * 0x01 followed by up to 15 bytes of 0x00.
 * Generally, the number of 0x00 is made to align to 4 bytes.
 *
 * Examples:
 * de 56 38 a9 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 23 f8
 * de 56 01 00 00 00 00 00 00 00 00 00 00 00 00 00 23 f8
 *
 * Implementation: detects 0x01 followed by 12 to 15 bytes
 */
static int stp_find_first_sync(char *buf, size_t u8size, off_t start,
			       off_t *out_off, size_t *out_size)
{
	off_t off;
	size_t size;

	off_t i = start;
	uint32_t a, y, z;

	for (i = start + 4; i <= u8size - 8; i += 4) {
		if (   *((uint32_t *) &buf[i])     == 0x00000000
		    && *((uint32_t *) &buf[i + 4]) == 0x00000000) {
			a = *((uint32_t *) &buf[i - 4]);

			/* First, find beginning position */
			if ((a >> 24) == 0x01)
				off = i - 1;
			else if ((a >> 16) == 0x0001)
				off = i - 2;
			else if ((a >> 8) == 0x000001)
				off = i - 3;
			else if (a == 0x00000001)
				off = i - 4;
			else
				continue;

			/* Then, find the length (between 13 and 16) */
			y = z = 0;
			if (i + 8 < u8size)
			        y = *((uint32_t *) &buf[off + 8]);
				if (i + 12 < u8size)
					z = *((uint32_t *) &buf[off + 12]);
			if (y == 0x00000000) {
				if (z == 0x00000000)
					size = 16;
				else if ((z & 0xffffff) == 0x000000)
					size = 15;
				else if ((z & 0xffff) == 0x0000)
					size = 14;
				else if ((z & 0xff) == 0x00)
					size = 13;
				else
					continue;

				*out_off = off;
				*out_size = size;
				return 0;
			}
		}
	}

	return -1;
}

/*
 * Finds the first block, delimited by sync packets.
 */
static int stp_find_first_block(char *buf, size_t u8size, off_t start,
				off_t *out_off, size_t *out_size)
{
	off_t  sync_off, head = start;
	size_t sync_len;

	while (head < u8size) {
		if (stp_find_first_sync(buf, u8size, head,
					&sync_off, &sync_len) != 0) {
			// no more sync packet
			*out_off = head;
			*out_size = u8size - head;
			return 0;
		} else if (sync_off > head) {
			// space between two sync packets
			*out_off = head;
			*out_size = sync_off - head;
			return 0;
		}
		head = sync_off + sync_len;
	}

	return -1;
}

struct stp_pkt *stp_read_pkts_in_raw_etb(char *buf, size_t u8size)
{
	off_t start = 0;
	off_t block_off;
	size_t block_len;

	struct stp_pkt *pkts, *pkt_list = NULL, *last_pkt;

	while (stp_find_first_block(buf, u8size, start, &block_off, &block_len) == 0) {
		/*fprintf(stderr, "found block %x -> %x\n",
			(int) block_off, (int) block_off + block_len);//*/
		pkts = stp_read_pkts(&buf[block_off], block_len);

		if (pkt_list == NULL)
			pkt_list = pkts;
		else
			last_pkt->next = pkts;

		for (last_pkt = pkts; last_pkt->next != NULL; last_pkt = last_pkt->next) ;

		start = block_off + block_len;
	}

	return pkt_list;
}

size_t stp_count_pkts_in_raw_etb(char *buf, size_t u8size)
{
	off_t start = 0;
	off_t block_off;
	size_t block_len;

	size_t count = 0;

	while (stp_find_first_block(buf, u8size, start, &block_off, &block_len) == 0) {
		count += stp_count_pkts(&buf[block_off], block_len);

		start = block_off + block_len;
	}

	return count;
}
