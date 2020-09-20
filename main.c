/*-
 * Copyright (c) 2005-2020 Poul-Henning Kamp
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rc3600.h"
#include "elastic.h"

void
trace_state(struct rc3600 *cs)
{
	char buf[BUFSIZ];

	AN(cs);
	if (cs->fd_trace < 0)
		return;
	bprintf(buf,
	    "I %jd %d %04x %04x %06o  %04x %04x %04x %04x %c"
	    " s%.9f r%.6f d%.6f"
	    " w%8ju %s\n",
	    cs->ins_count,
	    cs->inten[0],
	    cs->pc,
	    core_read(cs, cs->pc, CORE_NULL),
	    core_read(cs, cs->pc, CORE_NULL),
	    cs->acc[0],
	    cs->acc[1],
	    cs->acc[2],
	    cs->acc[3],
	    cs->carry ? 'C' : '.',
	    cs->sim_time * 1e-9,
	    cs->real_time * 1e-9,
	    (cs->sim_time - cs->real_time) * 1e-9,
	    cs->ins_count - cs->last_core,
	    core_disass(cs, cs->pc)
	);
	(void)write(cs->fd_trace, buf, strlen(buf));
}

void
trace(const struct rc3600 *cs, const char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZ];

	AN(cs);
	if (cs->fd_trace < 0)
		return;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	(void)write(cs->fd_trace, buf, strlen(buf));
}

void
dev_trace(const struct iodev *iop, const char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZ];

	AN(iop);
	if (iop->cs->fd_trace < 0 || !iop->trace)
		return;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	(void)write(iop->cs->fd_trace, buf, strlen(buf));
	va_end(ap);
}


int
main(int argc, char **argv)
{
	int ch, i;
	char buf[BUFSIZ];
	char *p;
	int bare = 0;
	struct rc3600 *cs;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	cs = cpu_new();
	AN(cs);

	while ((ch = getopt(argc, argv, "b:htT:")) != -1) {
		switch (ch) {
		case 'b':
			bare = 1;
			break;
		case 't':
			cs->do_trace++;
			break;
		case 'T':
			cs->fd_trace =
			    open(optarg, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (cs->fd_trace < 0) {
				fprintf(stderr, "Cannot open: %s: %s\n",
				    optarg, strerror(errno));
				exit(2);
			}
			break;
		default:
			fprintf(stderr, "Usage...\n");
			exit(0);
		}
	}
	argc -= optind;
	argv += optind;

	if (!bare) {
		AZ(cli_exec(cs, "cpu"));
		AZ(cli_exec(cs, "tty 0"));
		AZ(cli_exec(cs, "rtc 0"));
	}
	for (i = 0; i < argc; i++) {
		printf("CLI <%s>\n", argv[i]);
		if (cli_exec(cs, argv[i]))
			exit(2);
	}

	printf("CLI open\n");

	while (1) {
		if (fgets(buf, sizeof buf, stdin) != buf)
			break;
		p = strchr(buf, '\n');
		if (p != NULL)
			*p = '\0';
		printf("cli <%s>\n", buf);
		if (cli_exec(cs, buf) < 0)
			break;
	}
	return (0);
}
