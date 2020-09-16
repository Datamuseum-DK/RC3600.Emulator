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

struct io_ptr {
	int			speed;
	struct elastic		*ep;
	struct iodev		iop[1];
};

static void*
dev_ptr_thread(void *priv)
{
	struct iodev *iod = priv;
	struct io_ptr *tp = iod->priv;
	uint8_t buf[1];
	ssize_t sz;

	while (1) {
		if (tp->speed > 0)
			usleep(1000000 / tp->speed);
		sz = elastic_get(tp->ep, buf, 1);
		assert(sz == 1);
		trace(iod->cs, "PTR 0x%02x\n", buf[0]);

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

static struct iodev *
new_ptr(struct rc3600 *cs, unsigned unit)
{
	struct io_ptr *tp;

	tp = calloc(1, sizeof *tp);
	AN(tp);
	if (unit == 0) {
		tp->iop->unit = 10;
		tp->iop->imask = 11;
	}
	tp->speed = 2000;
	tp->ep = elastic_new(cs, O_RDONLY);
	AN(tp->ep);
	tp->iop->priv = tp;
	bprintf(tp->iop->name, "PTR%u", unit);
	install_dev(cs, tp->iop, dev_ptr_thread);
	return (tp->iop);
}

void v_matchproto_(cli_func_f)
cli_ptr(struct cli *cli)
{
	struct io_ptr *tp;

	cli->ac--;
	cli->av++;
	tp = get_dev_unit(cli->cs, "PTR", new_ptr, cli)->priv;
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
