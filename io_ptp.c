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

struct io_ptp {
	int			speed;
	struct elastic		*ep;
	struct iodev		iop[1];
};

static void*
dev_ptp_thread(void *priv)
{
	struct iodev *iod = priv;
	struct io_ptp *tp = iod->priv;
	uint16_t u;
	char buf[2];

	AZ(pthread_mutex_lock(&iod->mtx));
	while (1) {
		while (!iod->busy)
			AZ(pthread_cond_wait(&iod->cond, &iod->mtx));
		u = iod->oreg_a;
		AZ(pthread_mutex_unlock(&iod->mtx));
		trace(iod->cs, "PTP 0x%02x\n", u);
		buf[0] = u;
		elastic_put(tp->ep, buf, 1);
		if (tp->speed > 0)
			usleep(1000000 / tp->speed);
		AZ(pthread_mutex_lock(&iod->mtx));
		iod->busy = 0;
		iod->done = 1;
		intr_raise(iod);
	}
}

static struct iodev *
new_ptp(struct rc3600 *cs, unsigned unit)
{
	struct io_ptp *tp;

	tp = calloc(1, sizeof *tp);
	AN(tp);
	if (unit == 0) {
		tp->iop->unit = 11;
		tp->iop->imask = 13;
	}
	tp->speed = 1000;
	tp->ep = elastic_new(cs, O_WRONLY);
	AN(tp->ep);
	tp->iop->priv = tp;
	bprintf(tp->iop->name, "PTP%u", unit);
	install_dev(cs, tp->iop, dev_ptp_thread);
	return (tp->iop);
}

void v_matchproto_(cli_func_f)
cli_ptp(struct cli *cli)
{
	struct io_ptp *tp;

	cli->ac--;
	cli->av++;
	tp = get_dev_unit(cli->cs, "PTP", new_ptp, cli)->priv;
	AN(tp);

	while (cli->ac && !cli->status) {
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
