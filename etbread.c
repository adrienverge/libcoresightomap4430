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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libomap4430.h"
#include "libetb.h"
#include "libstm.h"

#define BUFSIZE 512

#define halfbyte(src, pos) \
	(((pos)%2)?((src[(pos)/2]>>4)&0xf):(src[(pos)/2]&0xf))

static int keep_going;

static void catch_exit(int sig)
{
	keep_going = 0;
	signal(SIGINT, SIG_DFL);
}

int main(int argc, char **argv)
{
	int ret = -1;
	char buf[BUFSIZE];
	ssize_t n;
	int nowait = 0;
	struct etb_handle_t etb_handle = { .base = NULL };
	int output_debug = 0;

	/*
	 * Parse args
	 */
	if (argc >= 2) {
		if (strcmp("--nowait", argv[1]) == 0)
			nowait = 1;
		else if (strcmp("--dbg", argv[1]) == 0)
			output_debug = 1;
	}

	/*
	 * Enable clocks
	 */
	if (omap4430_enable_emu()) {
		printf("error: couldn't enable OMAP4430 EMU clocks\n");
		goto end;
	}

	/*
	 * Open and configure ETB
	 */
	if (etb_open(&etb_handle)) {
		printf("error: couldn't open ETB\n");
		goto end;
	}

	if (etb_enable(&etb_handle)) {
		printf("error: couldn't enable ETB\n");
		goto close_etb;
	}

	/*
	 * Ok, now let's wait for data coming in the ETB
	 */
	keep_going = 1;
	signal(SIGINT, catch_exit);

	//etb_status();

	while (keep_going) {
		// Possible de lire ETB sans désactiver l'écriture dedans ?
		etb_disable(&etb_handle);
		n = etb_retrieve(&etb_handle, buf, BUFSIZE);
		//fprintf(stderr, "n = %d\n", n);
		etb_enable(&etb_handle);

		if (n < 0)
			fprintf(stderr, "error: etb_retrieve returned -1\n");
		else if (n > 0) {
			if (output_debug) {
				off_t i;
				for (i = 0; i < 2 * n; i++) {
					printf("%x ", halfbyte(buf, i));
				}
				printf("\n");
			} else
				write(STDOUT_FILENO, buf, n);
		}

		if (nowait)
			break;

		sleep(1);
	}

	ret = 0;

	etb_disable(&etb_handle);
close_etb:
	etb_close(&etb_handle);
end:
	return ret;
}
