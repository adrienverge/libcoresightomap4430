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
#include <sys/types.h>
#include <sys/stat.h>

#include "libstm.h"

#define BUFSIZE 256

void usage(char *prog)
{
	printf("usage: %s [-c CHANNEL] INPUTFILE\n"
	       "       %s [-c CHANNEL] (reads from stdin)\n", prog, prog);
}

int main(int argc, char **argv)
{
	int c;
	int input;
	ssize_t n;
	char buf[BUFSIZE];
	int channel = 0;
	struct stm_handle_t stm_handle;

	input = STDIN_FILENO;

	while ((c = getopt(argc, argv, "hc:")) != -1)
		switch (c) {
		case 'h':
			usage(argv[0]);
			exit(0);
		case 'c':
			channel = atoi(optarg);
			break;
		case '?':
		default:
			usage(argv[0]);
			exit(1);
		}

	if (optind == argc - 1) {
		input = open(argv[optind], O_RDONLY);
		if (input == -1) {
			perror("open");
			return 1;
		}
	} else if (optind != argc) {
		usage(argv[0]);
		exit(1);
	}

	if (stm_open(&stm_handle)) {
		printf("error: couldn't open STM\n");
		goto end;
	}
	if (stm_config_for_etb(&stm_handle)) {
		printf("error: couldn't configure STM for ETB\n");
		goto end;
	}

	while ((n = read(input, buf, BUFSIZE)) > 0)
		stm_send_msg_pkt(&stm_handle, channel, buf, n);

	stm_flush(&stm_handle);
	stm_close(&stm_handle);
end:
	if (argc == 2)
		close(input);

	return 0;
}

