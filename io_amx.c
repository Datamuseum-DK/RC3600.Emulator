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

#define CMD_RECEIVE		0x0
#define CMD_STOP_RECEIVE	0x1
#define CMD_TRANSMIT		0x2
#define CMD_STOP_TRANSMIT	0x3
#define CMD_SEL_INPUT_BUF	0x4
#define CMD_SEL_MODEM_STATUS	0x5
#define CMD_SEL_IN_BUF_STATUS	0x6
#define CMD_SEL_OUT_BUF_STATUS	0x7
#define CMD_DTR_ON		0x8
#define CMD_DTR_OFF		0x9
#define CMD_CLEAR_ONE_CHAR	0xf

struct amx_chan {
	uint16_t		last_modem;
	uint8_t			out_fifo[40];
	unsigned		outw;
	unsigned		outr;
	struct elastic		*ep;
	pthread_t		out_thread;
	pthread_mutex_t		mtx;
};

struct io_amx {
	int			speed;
	struct amx_chan		chans[8];
	struct iodev		*iop;
	int			chan;
	struct amx_chan		*cp;
};

static void *
dev_amx_out_thread(void *priv)
{
	unsigned nout;
	uint8_t buf[1];
	struct amx_chan *cp = priv;
	AN(cp);
	AN(cp->mtx);

	(void)priv;
	while (1) {
		AZ(pthread_mutex_lock(&cp->mtx));
		if (cp->outr != cp->outw) {
			buf[0] = cp->out_fifo[cp->outr++];
			cp->outr %= sizeof(cp->out_fifo);
			nout = 1;
		} else {
			nout = 0;
		}
		AZ(pthread_mutex_unlock(&cp->mtx));
		if (nout) {
			elastic_put(cp->ep, buf, 1);
		}
		usleep((nsec_per_char(cp->ep) / 1000) + 1);
		//sleep(1);
	}
	return (NULL);
}

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
		ap->cp = &ap->chans[ap->chan];
		cmd = *reg & 0xf;
		switch(cmd) {
		case CMD_RECEIVE:
			dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x CMD_RX\n", ioi, reg ? *reg : 0, ap->chan);
			break;
		case CMD_TRANSMIT:
			dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x CMD_TX\n", ioi, reg ? *reg : 0, ap->chan);
			break;
		case CMD_DTR_OFF:
			dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x CMD_DTR_OFF\n", ioi, reg ? *reg : 0, ap->chan);
			break;
		case CMD_DTR_ON:
			dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x CMD_DTR_ON\n", ioi, reg ? *reg : 0, ap->chan);
			break;
		case CMD_SEL_INPUT_BUF:
			//dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x CMD_SEL_IBUF\n", ioi, reg ? *reg : 0, ap->chan);
			if (elastic_empty(ap->cp->ep)) {
				iop->ireg_a = 0x0080;
				if (!ap->cp->ep->carrier && ap->cp->last_modem) {
					iop->ireg_a |= 0x3000;
				}
				ap->cp->last_modem = ap->cp->ep->carrier;
			} else {
				sz = elastic_get(ap->cp->ep, buf, 1);
				//printf("INP %d %zd %02x\n", ap->chan, sz, buf[0]);
				//buf[0] &= 0x7f;
				assert(sz == 1);
				if (buf[0] == '\n')
					buf[0] = '\r';
				iop->ireg_a = buf[0] | 0x8000;
			}
			break;
		case CMD_SEL_MODEM_STATUS:
			if (!ap->cp->ep->carrier) {
				iop->ireg_a = 0x6000;
			} else {
				iop->ireg_a = 0x0000;
			}
			dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x CMD_MODEM_STATUS\n", ioi, iop->ireg_a, ap->chan);
			break;
		case CMD_SEL_IN_BUF_STATUS:
			dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x CMD_SEL_IBUF_STATUS\n", ioi, reg ? *reg : 0, ap->chan);
			iop->ireg_a = 0x0000;
			break;
		case CMD_SEL_OUT_BUF_STATUS:
			dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x CMD_SEL_OBUF_STATUS\n", ioi, reg ? *reg : 0, ap->chan);
			AZ(pthread_mutex_lock(&ap->cp->mtx));
			iop->ireg_a = 0x0000;
			unsigned nfifo = sizeof(ap->cp->out_fifo) + ap->cp->outw - ap->cp->outr;
			nfifo %= sizeof(ap->cp->out_fifo);
			if (nfifo < 32)
				iop->ireg_a |= 0x8000;
			if (ap->cp->outw == ap->cp->outr)
				iop->ireg_a |= 0x4000;
			AZ(pthread_mutex_unlock(&ap->cp->mtx));
			break;
		case CMD_CLEAR_ONE_CHAR:
			dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x CMD_CLEAR_ONE_CHAR\n", ioi, reg ? *reg : 0, ap->chan);
			AZ(pthread_mutex_lock(&ap->cp->mtx));
			if (ap->cp->outw != ap->cp->outr) {
				ap->cp->outw += sizeof(ap->cp->out_fifo) - 1;
				ap->cp->outw %= sizeof(ap->cp->out_fifo);
			}
			if (ap->cp->outw == ap->cp->outr) {
				iop->ireg_a = 0x0000;
			} else {
				iop->ireg_a = 0x8000;
			}
			AZ(pthread_mutex_unlock(&ap->cp->mtx));
			break;
		default:
			dev_trace(iop, "AMX %d cmd=0x%04x ???\n", ap->chan, cmd);
			break;
		}
		break;
	case IO_DIA:
		*reg = iop->ireg_a;
		dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x DIA\n", ioi, reg ? *reg : 0, ap->chan);
		break;
	case IO_DOB:
		dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x DOB\n", ioi, reg ? *reg : 0, ap->chan);
		ap->chan = (*reg >> 8) & 7;
		AZ(pthread_mutex_lock(&ap->cp->mtx));
		ap->cp->out_fifo[ap->cp->outw++] = *reg & 0xff;
		ap->cp->outw %= sizeof(ap->cp->out_fifo);
		AZ(pthread_mutex_unlock(&ap->cp->mtx));
		break;
	case IO_DOC:
		dev_trace(iop,"AMX ioi=0x%04x *reg=0x%04x chan=0x%x DOC\n", ioi, reg ? *reg : 0, ap->chan);
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
	struct amx_chan *cp;
	int i;

	AN(iop);
	AZ(iop2);

	ap = calloc(1, sizeof *ap);
	AN(ap);
	ap->iop = iop;
	ap->speed = 2400;

	for (i = 0; i < NCHAN; i++) {
		cp = &ap->chans[i];
		cp->ep = elastic_new(ap->iop->cs, O_RDWR);
		AN(cp->ep);
		cp->ep->bits_per_char = 11;
		cp->ep->bits_per_sec = 9600;
		AZ(pthread_mutex_init(&cp->mtx, NULL))
		AN(cp->out_fifo);
		AZ(pthread_create(&cp->out_thread, NULL, dev_amx_out_thread, cp));
	}
	ap->chan = 0;
	ap->cp = &ap->chans[ap->chan];
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
			if (cli_elastic(ap->chans[port].ep, cli))
				continue;
			cli_unknown(cli);
			return;
		}
		cli_unknown(cli);
		break;
	}
}
