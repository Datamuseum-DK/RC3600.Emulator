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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "rc3600.h"
#include "elastic.h"

struct elastic_fd {
	struct elastic			*ep;
	int				fd;
	pthread_t			rxt;
	struct elastic_subscriber	*ws;
};

static void *
elastic_fd_rxthread(void *priv)
{
	struct elastic_fd *efp = priv;
	char buf[1];
	ssize_t sz;

	while (1) {
		sz = read(efp->fd, buf, 1);
		if (sz != 1)
			break;
		elastic_inject(efp->ep, buf, 1);
	}
	if (efp->ws != NULL)
		elastic_unsubscribe(efp->ep, efp->ws);
	AZ(close(efp->fd));
	free(efp);
	return (NULL);
}

static void
elastic_fd_txfunc(void *priv, const void *src, size_t len)
{
	struct elastic_fd *efp = priv;

	(void)write(efp->fd, src, len);
}

void
elastic_fd_use(struct elastic *ep, int fd, int mode)
{
	struct elastic_fd *efp;

	AN(ep);
	assert(fd > STDERR_FILENO);

	if (mode == -1)
		mode = ep->mode;
	else
		assert(ep->mode == O_RDWR || ep->mode == mode);

	if (mode != O_WRONLY) {
		elastic_inject(ep, "", 1);
		elastic_inject(ep, "", 1);
		elastic_inject(ep, "", 1);
		elastic_inject(ep, "", 1);
		elastic_inject(ep, "", 1);
	}
	efp = calloc(1, sizeof *efp);
	AN(efp);
	efp->fd = fd;
	efp->ep = ep;
	if (mode != O_RDONLY)
		efp->ws = elastic_subscribe(ep, elastic_fd_txfunc, efp);
	if (mode != O_WRONLY)
		AZ(pthread_create(&efp->rxt, NULL, elastic_fd_rxthread, efp));
}

static void
elastic_serial(struct elastic *ep, struct cli *cli)
{
	int fd;
	struct termios tt;

	if (cli->ac < 2) {
		(void)cli_n_args(cli, 1);
		return;
	}

	fd = open(cli->av[1], O_RDWR);
	if (fd < 0) {
		cli_error(cli, "Cannot open %s: %s\n",
		    cli->av[1], strerror(errno));
		return;
	}
	if (tcgetattr(fd, &tt)) {
		cli_error(cli, "Not a tty %s: %s\n",
		    cli->av[1], strerror(errno));
		AZ(close(fd));
		return;
	}
	cfmakeraw(&tt);
	tt.c_cflag |= CLOCAL;
	tt.c_cc[VMIN] = 1;
	tt.c_cc[VTIME] = 0;
	cli->ac -= 2;
	cli->av += 2;
	while (cli->ac > 0) {
		cli_unknown(cli);
	}
	assert(tcsetattr(fd, TCSAFLUSH, &tt) == 0);
	elastic_fd_use(ep, fd, -1);
}

int v_matchproto_(cli_elastic_f)
cli_elastic_fd(struct elastic *ep, struct cli *cli)
{
	int fd;

	AN(cli);
	if (cli->help) {
		cli_printf(cli, "\t< <filename>\n");
		cli_printf(cli, "\t\tRead input from file\n");
		cli_printf(cli, "\t> <filename>\n");
		cli_printf(cli, "\t\tWrite output to file\n");
		cli_printf(cli, "\t>> <filename>\n");
		cli_printf(cli, "\t\tAppend output to file\n");
		cli_printf(cli, "\tserial <tty-device>\n");
		cli_printf(cli, "\t\tConnect to (UNIX) tty-device\n");
		return(0);
	}

	AN(ep);

	if (!strcmp(*cli->av, ">") || !strcmp(*cli->av, ">>")) {
		if (cli_n_args(cli, 1))
			return(1);
		if (ep->mode == O_RDONLY)
			return (cli_error(cli,
			    "Only outputs can '%s'\n", *cli->av));

		if (!strcmp(*cli->av, ">"))
			fd = open(cli->av[1], O_WRONLY|O_CREAT|O_TRUNC, 0644);
		else
			fd = open(cli->av[1], O_WRONLY|O_CREAT|O_APPEND, 0644);

		if (fd < 0)
			return (cli_error(cli, "Cannot open %s: %s\n",
			    cli->av[1], strerror(errno)));

		elastic_fd_use(ep, fd, O_WRONLY);
		cli->av += 2;
		cli->ac -= 2;
		return (1);
	}
	if (!strcmp(*cli->av, "<")) {
		if (cli_n_args(cli, 1))
			return(1);
		if (ep->mode == O_WRONLY)
			return (cli_error(cli,
			    "Only inputs can '%s'\n", *cli->av));

		fd = open(cli->av[1], O_RDONLY);
		if (fd < 0)
			return (cli_error(cli, "Cannot open %s: %s\n",
			    cli->av[1], strerror(errno)));

		elastic_fd_use(ep, fd, O_RDONLY);
		cli->av += 2;
		cli->ac -= 2;
		return (1);
	}
	if (!strcmp(*cli->av, "serial")) {
		elastic_serial(ep, cli);
		return (1);
	}
	return (0);
}
