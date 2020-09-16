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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rc3600.h"

static void
dev_rtc_iofunc(struct iodev *iop, uint16_t ioi, uint16_t *reg)
{
	uint16_t u;
	nanosec r, t;

	std_io_ins(iop, ioi, reg);

	if (IO_ACTION(ioi) == IO_START) {
		AN(iop->busy);
		AZ(iop->done);
		u = iop->oreg_a;
		switch (u & 3) {
		case 0x0: r = 1000000000 / 50; break;
		case 0x1: r = 1000000000 / 10; break;
		case 0x2: r = 1000000000 / 100; break;
		case 0x3: r = 1000000000 / 1000; break;
		default: assert(0 == __LINE__);
		}
		t = (1 + iop->cs->sim_time / r) * r;
		callout_dev_is_done_abs(iop, t);
	}
}

static void * v_matchproto_(new_dev_f)
new_rtc(struct iodev *iop1, struct iodev *iop2)
{

	AN(iop1);
	AZ(iop2);
	iop1->ins_func = dev_rtc_iofunc;
	install_dev(iop1, NULL);
	return (iop1);
}

void v_matchproto_(cli_func_f)
cli_rtc(struct cli *cli)
{
	struct iodev *iop;

	cli->ac--;
	cli->av++;
	iop = cli_dev_get_unit(cli, "RTC", NULL, new_rtc);
	if (iop == NULL)
		return;

	while (cli->ac && !cli->status) {
		if (cli_dev_trace(iop, cli))
			continue;
		cli_unknown(cli);
		break;
	}
}
