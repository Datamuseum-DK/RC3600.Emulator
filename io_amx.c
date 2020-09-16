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

#define NCHAN 8

#define CMD_RECEIVE		0
#define CMD_STOP_RECEIVE	1
#define CMD_TRANSMIT		2
#define CMD_STOP_TRANSMIT	3
#define CMD_SEL_INPUT_BUF	4
#define CMD_SEL_MODEM_STATUS	5
#define CMD_SEL_IN_BUF_STATUS	6
#define CMD_SEL_OUT_BUF_STATUS	7
#define CMD_DTR_ON		8
#define CMD_DTR_OFF		9

struct io_amx {
	int			speed;
	int			chan;
	uint16_t		select[NCHAN];
	struct elastic		*ep[NCHAN];
	struct iodev		iop[1];
};

static void
dev_amx_insfunc(struct iodev *iop, uint16_t ioi, uint16_t *reg)
{
	struct io_amx *ap = iop->priv;
	int cmd;
	ssize_t sz;
	uint8_t buf[2];

	AN(iop);
	AN(iop->cs);

	switch(IO_OPER(ioi)) {
	case DOA:
		ap->chan = (*reg >> 8) & 7;
		cmd = *reg & 0xf;
		switch(cmd) {
		case CMD_RECEIVE:
			break;
		case CMD_TRANSMIT:
			break;
		case CMD_DTR_OFF:
			break;
		case CMD_DTR_ON:
			break;
		case CMD_SEL_INPUT_BUF:
			if (elastic_empty(ap->ep[ap->chan])) {
				iop->ireg_a = 0x0080;
			} else {
				sz = elastic_get(ap->ep[ap->chan], buf, 1);
				assert(sz == 1);
				if (buf[0] == '\n')
					buf[0] = '\r';
				iop->ireg_a = buf[0] | 0x8000;
			}
			break;
		case CMD_SEL_MODEM_STATUS:
			iop->ireg_a = 0x0000;
			break;
		case CMD_SEL_IN_BUF_STATUS:
			iop->ireg_a = 0x0000;
			break;
		case CMD_SEL_OUT_BUF_STATUS:
			iop->ireg_a = 0xc000;
			break;
		default:
			dev_trace(iop, "AMX %d cmd=0x%x\n", ap->chan, cmd);
			break;
		}
		break;
	case DIA:
		*reg = iop->ireg_a;
		break;
	case DOB:
		ap->chan = (*reg >> 8) & 7;
		buf[0] = *reg & 0x7f;
		elastic_put(ap->ep[ap->chan], buf, 1);
		break;
	case DOC:
		break;
		
	default:
		break;
	}
	std_io_ins(iop, ioi, reg);
}

static struct iodev *
new_amx(struct rc3600 *cs, unsigned unit)
{
	struct io_amx *ap;
	int i;

	ap = calloc(1, sizeof *ap);
	AN(ap);
	ap->speed = 2400;

	switch (unit) {
	case 0: ap->iop->unit = 42; break;
	case 1: ap->iop->unit = 43; break;
	case 2: ap->iop->unit = 62; break;
	case 3: ap->iop->unit = 20; break;
	default: break;
	}
	ap->iop->imask = 2;

	for (i = 0; i < NCHAN; i++) {
		ap->ep[i] = elastic_new(cs, O_RDWR);
		AN(ap->ep[i]);
	}
	ap->iop->priv = ap;
	ap->iop->ins_func = dev_amx_insfunc;
	bprintf(ap->iop->name, "AMX%u", unit);
	install_dev(cs, ap->iop, NULL);

	return (ap->iop);
}

void v_matchproto_(cli_func_f)
cli_amx(struct cli *cli)
{
	struct io_amx *ap;
	int port;

	cli->ac--;
	cli->av++;
	ap = get_dev_unit(cli->cs, "AMX", new_amx, cli)->priv;
	AN(ap);

	while (cli->ac && !cli->status) {
		if (cli_dev_trace(ap->iop, cli))
			continue;
		if (!strcasecmp(*cli->av, "speed")) {
			if (cli_n_args(cli, 1))
				return;
			ap->speed = atoi(cli->av[1]);
			cli->av += 2;
			cli->ac -= 2;
			continue;
		}
		if (!strcasecmp(*cli->av, "port")) {
			if (cli->ac < 2) {
				cli_n_args(cli, 1);
				return;
			}
			port = atoi(cli->av[1]);
			if (port < 0 || port > 7) {
				cli_error(cli,
				    "port number out of range [0…7]\n");
				return;
			}
			cli->ac -= 2;
			cli->av += 2;
			if (!cli->ac)
				continue;
			if (cli_elastic(ap->ep[port], cli))
				continue;
			cli_unknown(cli);
			return;
		}
		cli_unknown(cli);
		break;
	}
}
