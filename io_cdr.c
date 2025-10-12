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
 *
 * ireg_a 0x4000 -> NOT INTERFACED
 * ireg_a 0x2000 -> NOT OPERABLE
 * ireg_a 0x1000 -> REJECT FAILED
 * ireg_a 0x0400 -> FEED ERROR
 * ireg_a 0x0200 -> ILLEGAL COMMAND
 * ireg_a 0x0100 -> HOPPER EMPTY
 * ireg_a 0x0040 -> DATA LATE
 * ireg_a 0x0020 -> REED ERROR
 *        0x7760 -> BAD MASK
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/endian.h>
#include "rc3600.h"

struct io_cdr {
	struct iodev		*iop;
	int			fd;
	unsigned		card_no;
};

static void*
dev_cdr_thread(void *priv)
{
	struct iodev *iod = priv;
	struct io_cdr *cp = iod->priv;

	uint8_t buf[160];
	int i;

	AZ(pthread_mutex_lock(&iod->mtx));
	while (1) {
		while (!iod->busy)
			AZ(pthread_cond_wait(&iod->cond, &iod->mtx));
		assert(cp->fd >= 0);

		printf("CDR @%d>@0x%04x\n", cp->card_no + 1, iod->oreg_b);
		dev_trace(iod, "CDR >@0x%04x\n", iod->oreg_b);
		callout_dev_sleep_locked(iod, 25000000);

		i = read(cp->fd, buf, sizeof buf);
		if (i == sizeof buf) {
			iod->ireg_a &= ~0x0100;
			for (i = 0; i < sizeof buf; i += 2) {
				uint16_t u = (buf[i] | (buf[i+1]<<8)) >> 4;
				core_write(iod->cs, iod->oreg_b, u, CORE_DMA);
				iod->oreg_b += 1;
				iod->ireg_b = iod->oreg_b;
				callout_dev_sleep_locked(iod, 2000000);
			}
			cp->card_no++;
			dev_trace(iod, "CDR #%u <@0x%04x\n", cp->card_no, iod->oreg_b);
			callout_dev_sleep_locked(iod, 25000000);
		} else {
			iod->ireg_a |= 0x0100;
		}
		iod->done = 1;
		intr_raise(iod);
		iod->busy = 0;
	}
}

static void
dev_cdr_insfunc(struct iodev *iop, uint16_t ioi, uint16_t *reg)
{

	std_io_ins(iop, ioi, reg);

	switch(IO_OPER(ioi)) {
	case 0:	// IORST
		iop->ireg_b = iop->oreg_b = 0;
		break;
	case IO_DOB:
		iop->ireg_b = iop->oreg_b & 0x7fff;
		break;
	}
}


static void * v_matchproto_(new_dev_f)
new_cdr(struct iodev *iop1, struct iodev *iop2)
{

	struct io_cdr *tp = NULL;

	AN(iop1);
	AZ(iop2);

	tp = calloc(1, sizeof *tp);
	AN(tp);
	tp->fd = -1;
	tp->iop = iop1;
	tp->iop->priv = tp;
	tp->iop->io_func = dev_cdr_insfunc;
	cpu_add_dev(tp->iop, dev_cdr_thread);
	return (tp);
}

void v_matchproto_(cli_func_f)
cli_cdr(struct cli *cli)
{
	struct io_cdr *tp;

	if (cli->help) {
		cli_io_help(cli, "RCxxxx Card Reader Controller", 0, 0);
		cli_printf(cli, "\tload <filename>\n");
		return;
	}

	cli->ac--;
	cli->av++;
	tp = cli_dev_get_unit(cli, "CDR", NULL, new_cdr);
	if (tp == NULL)
		return;
	AN(tp);
	AN(tp->iop->priv);

	while (cli->ac && !cli->status) {
		if (cli_dev_trace(tp->iop, cli))
			continue;
		if (!strcasecmp(*cli->av, "load")) {
			tp->fd = open(cli->av[1], O_RDONLY);
			assert(tp->fd >= 0);
			return;
		}
		cli_unknown(cli);
		break;
	}
}
