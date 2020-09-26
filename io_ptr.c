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

struct io_ptr {
	struct elastic		*ep;
	struct iodev		*iop;
};

static void*
dev_ptr_thread(void *priv)
{
	struct iodev *iod = priv;
	struct io_ptr *tp = iod->priv;
	uint8_t buf[1];
	ssize_t sz;

	while (1) {
		if (tp->ep->bits_per_sec > 0)
			callout_dev_sleep(iod, nsec_per_char(tp->ep));

		sz = elastic_get(tp->ep, buf, 1);
		assert(sz == 1);
		dev_trace(iod, "PTR 0x%02x\n", buf[0]);

		AZ(pthread_mutex_lock(&iod->mtx));
		while (!iod->busy)
			AZ(pthread_cond_wait(&iod->cond, &iod->mtx));
		iod->ireg_a = buf[0];
		iod->busy = 0;
		iod->done = 1;
		intr_raise(iod);
		AZ(pthread_mutex_unlock(&iod->mtx));
	}
}

static void * v_matchproto_(new_dev_f)
new_ptr(struct iodev *iop1, struct iodev *iop2)
{
	struct io_ptr *tp;

	AN(iop1);
	AZ(iop2);
	tp = calloc(1, sizeof *tp);
	AN(tp);
	tp->iop = iop1;
	tp->ep = elastic_new(tp->iop->cs, O_RDONLY);
	tp->ep->bits_per_sec = 8 * 1000;
	AN(tp->ep);
	tp->iop->priv = tp;
	cpu_add_dev(tp->iop, dev_ptr_thread);
	return (tp);
}

void v_matchproto_(cli_func_f)
cli_ptr(struct cli *cli)
{
	struct io_ptr *tp;

	if (cli->help) {
		cli_io_help(cli, "Paper Tape Reader", 1, 1);
		return;
	}

	cli->ac--;
	cli->av++;
	tp = cli_dev_get_unit(cli, "PTR", NULL, new_ptr);
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
