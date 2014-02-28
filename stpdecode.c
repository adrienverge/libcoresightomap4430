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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "libstp.h"

#define BUFSIZE 512

#define TIME_MAGICK ('t' | 'i'<<8 | 'm'<<16 | 'e'<<24)
#define OMAP4430_FREQ	133400000.0 // ??
#define timeval2double(tv) ((double) tv.tv_sec + (double) tv.tv_usec/1000000.0)

void usage(char *prog)
{
	printf("usage: %s [-c] INPUTFILE\n", prog);
}

int main(int argc, char **argv)
{
	int ret = EXIT_FAILURE;
	int c;
	int action_count = 0;

	int fd;
	struct stat filestat;
	void *data;

	struct stp_pkt *pkt_list,
		       *pkt;

	/*
	 * Parse args
	 */
	while ((c = getopt(argc, argv, "hc")) != -1)
		switch (c) {
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		case 'c':
			action_count = 1;
			break;
		case '?':
		default:
			usage(argv[0]);
			goto end;
		}

	if (optind != argc - 1) {
		usage(argv[0]);
		goto end;
	}

	fd = open(argv[optind], O_RDONLY);
	if (fd == -1) {
		perror("open");
		goto end;
	}
	if (fstat(fd, &filestat) == -1) {
		perror("fstat");
		goto err_close;
	}
	if (filestat.st_size == 0) {
		fprintf(stderr, "error: file is empty\n");
		goto err_close;
	}
	data = mmap(NULL, filestat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED) {
		perror("mmap");
		goto err_close;
	}

	if (action_count) {
		printf("%d\n", stp_count_pkts_in_raw_etb(data, filestat.st_size));
		goto exit_success;
	}

	int incremental_cycles = 0;
	double last_sync_ts = 0.0;
	unsigned char channel = 0xff;

	pkt_list = stp_read_pkts_in_raw_etb(data, filestat.st_size);

	for (pkt = pkt_list; pkt != NULL; pkt = pkt->next) {
		incremental_cycles += pkt->timestamp;

		if (pkt->channel != 0xff)
			channel = pkt->channel;

		if (pkt->len == 12 && *((uint32_t *) pkt->data) == TIME_MAGICK) {
			struct timeval new_tv;
			double new_ts;

			new_tv = *((struct timeval *) &pkt->data[4]);
			new_ts = timeval2double(new_tv);
			printf("[%2.8f] [%02x] --- sync ---\n", new_ts,
			       channel);

			if (new_ts < last_sync_ts + incremental_cycles / OMAP4430_FREQ)
				fprintf(stderr, "warning: timestamp in SYNC is "
					"lower than incremental timestamp:\n"
					"      SYNC = %2.8f\n"
					"should be >= %2.8f   (INCR = %d cycles)\n",
					new_ts, last_sync_ts + incremental_cycles / OMAP4430_FREQ,
					incremental_cycles);

			last_sync_ts = new_ts;
			incremental_cycles = 0;
			continue;
		}


		printf("[%2.8f] [%02x] ", last_sync_ts + incremental_cycles / OMAP4430_FREQ,
		       channel);
		fwrite(pkt->data, 1, pkt->len, stdout);
		printf("\n");
	}

	if (pkt_list != NULL)
		free_stp_pkt_list(pkt_list);

exit_success:
	ret = EXIT_SUCCESS;

	munmap(data, filestat.st_size);
err_close:
	close(fd);
end:
	exit(ret);
}
