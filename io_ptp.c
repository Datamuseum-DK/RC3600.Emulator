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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rc3600.h"
#include "elastic.h"

struct io_ptp {
	struct elastic		*ep;
	struct iodev		*iop;
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
		if (tp->ep->bits_per_sec > 0)
			usleep(nsec_per_char(tp->ep) / 1000);
		AZ(pthread_mutex_lock(&iod->mtx));
		iod->busy = 0;
		iod->done = 1;
		intr_raise(iod);
	}
}

static void * v_matchproto_(new_dev_f)
new_ptp(struct iodev *iop1, struct iodev *iop2)
{
	struct io_ptp *tp;

	AN(iop1);
	AZ(iop2);
	tp = calloc(1, sizeof *tp);
	AN(tp);
	tp->iop = iop1;
	tp->ep = elastic_new(tp->iop->cs, O_WRONLY);
	tp->ep->bits_per_sec = 8 * 1000;
	AN(tp->ep);
	tp->iop->priv = tp;
	cpu_add_dev(tp->iop, dev_ptp_thread);
	return (tp);
}

void v_matchproto_(cli_func_f)
cli_ptp(struct cli *cli)
{
	struct io_ptp *tp;

	if (cli->help) {
		cli_io_help(cli, "Paper Tape Punch", 1, 1);
		return;
	}

	cli->ac--;
	cli->av++;
	tp = cli_dev_get_unit(cli, "PTP", NULL, new_ptp);
	if (tp == NULL)
		return;
	AN(tp);

	while (cli->ac && !cli->status) {
		if (cli_dev_trace(tp->iop, cli))
			continue;
		if (cli_elastic(tp->ep, cli))
			continue;
		cli_unknown(cli);
		break;
	}
}
