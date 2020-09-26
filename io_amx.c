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
	struct iodev		*iop;
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
	case IO_DOA:
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
	case IO_DIA:
		*reg = iop->ireg_a;
		break;
	case IO_DOB:
		ap->chan = (*reg >> 8) & 7;
		buf[0] = *reg & 0xff;
		elastic_put(ap->ep[ap->chan], buf, 1);
		break;
	case IO_DOC:
		break;
	default:
		break;
	}
	std_io_ins(iop, ioi, reg);
}

static void * v_matchproto_(new_dev_f)
new_amx(struct iodev *iop, struct iodev *iop2)
{
	struct io_amx *ap;
	int i;

	AN(iop);
	AZ(iop2);

	ap = calloc(1, sizeof *ap);
	AN(ap);
	ap->iop = iop;
	ap->speed = 2400;

	for (i = 0; i < NCHAN; i++) {
		ap->ep[i] = elastic_new(ap->iop->cs, O_RDWR);
		AN(ap->ep[i]);
		ap->ep[i]->bits_per_char = 11;
		ap->ep[i]->bits_per_sec = 9600;
	}
	ap->iop->io_func = dev_amx_insfunc;
	ap->iop->priv = ap;
	cpu_add_dev(iop, NULL);
	return (ap);
}

void v_matchproto_(cli_func_f)
cli_amx(struct cli *cli)
{
	struct io_amx *ap;
	int port;

	if (cli->help) {
		cli_io_help(cli, "Asynchronous multiplexor", 1, 0);
		cli_printf(cli, "\tport <0…7> <elastic>\n");
		cli_printf(cli, "\t\tPer port elastic buffer arguments\n");
		return;
	}

	cli->ac--;
	cli->av++;
	ap = cli_dev_get_unit(cli, "AMX", NULL, new_amx);
	if (ap == NULL)
		return;

	while (cli->ac && !cli->status) {
		if (cli_dev_trace(ap->iop, cli))
			continue;
		if (!strcasecmp(*cli->av, "port")) {
			if (cli->ac < 2) {
				(void)cli_n_args(cli, 1);
				return;
			}
			port = atoi(cli->av[1]);
			if (port < 0 || port >= NCHAN) {
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
