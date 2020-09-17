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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rc3600.h"
#include "elastic.h"

struct io_tty {
	int			speed;
	struct elastic		*ep;
	struct iodev		*i_dev;
	struct iodev		*o_dev;
};

static void*
dev_tti_thread(void *priv)
{
	struct iodev *iod = priv;
	struct io_tty *tp = iod->priv;
	char buf[1];
	ssize_t sz;

	while (1) {
		sz = elastic_get(tp->ep, buf, 1);
		assert(sz == 1);
		AZ(pthread_mutex_lock(&iod->mtx));
		dev_trace(tp->i_dev, "TTI 0x%02x\n", buf[0]);
		iod->ireg_a = buf[0];
		iod->busy = 0;
		iod->done = 1;
		intr_raise(iod);
		while (iod->done)
			AZ(pthread_cond_wait(&iod->cond, &iod->mtx));
		AZ(pthread_mutex_unlock(&iod->mtx));
	}
}

static void
dev_tto_iofunc(struct iodev *iop, uint16_t ioi, uint16_t *reg)
{
	struct io_tty *tp = iop->priv;
	char buf[2];

	std_io_ins(iop, ioi, reg);

	if (IO_ACTION(ioi) == IO_START) {
		AN(iop->busy);
		AZ(iop->done);
		buf[0] = iop->oreg_a & 0x7f;
		elastic_put(tp->ep, buf, 1);
		switch (buf[0]) {
		case 0x00:
			break;
		case 0x0a:
			printf("%c", buf[0]);
			break;
		case 0x0d:
			printf("%c", buf[0]);
			break;
		default:
			if (buf[0] < 0x20)
				printf("\x1b[1m「%02x」\x1b[m", buf[0]);
			else
				printf("\x1b[1m%c\x1b[m", buf[0]);
			break;
		}
		callout_dev_is_done(iop, 11 * (1000000000 / tp->speed));
	}
}

static void * v_matchproto_(new_dev_f)
new_tty(struct iodev *iop1, struct iodev *iop2)
{
	struct io_tty *tp;

	AN(iop1);
	AN(iop2);
	tp = calloc(1, sizeof *tp);
	AN(tp);
	tp->i_dev = iop1;
	tp->o_dev = iop2;
	tp->speed = 2400;
	tp->ep = elastic_new(tp->i_dev->cs, O_RDWR);
	AN(tp->ep);

	tp->i_dev->priv = tp;
	install_dev(tp->i_dev, dev_tti_thread);

	tp->o_dev->priv = tp;
	tp->o_dev->ins_func = dev_tto_iofunc;

	install_dev(tp->o_dev, NULL);
	return (tp);
}

void v_matchproto_(cli_func_f)
cli_tty(struct cli *cli)
{
	struct io_tty *tp;

	if (cli->help) {
		cli_io_help(cli, "TTI+TTO device pair", 1, 1);
		return;
	}

	cli->ac--;
	cli->av++;
	tp = cli_dev_get_unit(cli, "TTI", "TTO", new_tty);
	if (tp == NULL)
		return;

	while (cli->ac && !cli->status) {
		if (cli_dev_trace(tp->i_dev, cli)) {
			tp->o_dev->trace = tp->i_dev->trace;
			continue;
		}
		if (!strcasecmp(*cli->av, "speed")) {
			if (cli_n_args(cli, 1))
				return;
			tp->speed = atoi(cli->av[1]);
			cli->av += 2;
			cli->ac -= 2;
			continue;
		}
		if (cli_elastic(tp->ep, cli))
			continue;
		cli_unknown(cli);
		break;
	}
}
