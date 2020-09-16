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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/endian.h>
#include "rc3600.h"

#define FDD_BPS	128
#define FDD_SPT	26
#define FDD_TPD	77

#define FDD_SIZE	(FDD_TPD * FDD_SPT * FDD_BPS)

struct io_fdd {
	int			speed;
	struct iodev		*iop;
	uint8_t			img[FDD_SIZE];
	uint8_t			wbuf[FDD_BPS];
	uint8_t			sect;
	uint8_t			track;
	unsigned		r_ptr;
	unsigned		w_ptr;
};

static void
dev_fdd_iofunc(struct iodev *iop, uint16_t ioi, uint16_t *reg)
{
	struct io_fdd *fp;

	fp = iop->priv;

	if (IO_OPER(ioi) == DIB) {
		*reg = fp->img[fp->r_ptr++];
		return;
	}
	if (IO_OPER(ioi) == DOB) {
		fp->wbuf[fp->w_ptr] = *reg;
		if (fp->w_ptr < FDD_BPS)
			fp->w_ptr++;
		return;
	}

	//iop->ireg_c = (fp->track << 8) | 0xfb;
	iop->ireg_c = 0xfb;

	std_io_ins(iop, ioi, reg);
	if (IO_ACTION(ioi) == IO_START) {
		switch (iop->oreg_a & 0x0300) {
		case 0x0000:
			fp->sect = iop->oreg_a & 0x00ff;
			dev_trace(iop, "FDD READ track %u sector %u\n",
			    fp->track, fp->sect);
			assert(fp->sect >= 1);
			assert(fp->sect <= FDD_SPT);
			fp->r_ptr = fp->sect - 1;
			fp->r_ptr *= FDD_BPS;
			fp->r_ptr += fp->track * FDD_SPT * FDD_BPS;
			fp->w_ptr = 0;
			break;
		case 0x0100:
			fp->sect = iop->oreg_a & 0x00ff;
			dev_trace(iop, "FDD WRITE track %u sector %u\n",
			    fp->track, fp->sect);
			assert(fp->sect >= 1);
			assert(fp->sect <= FDD_SPT);
			fp->r_ptr = fp->sect - 1;
			fp->r_ptr *= FDD_BPS;
			fp->r_ptr += fp->track * FDD_SPT * FDD_BPS;
			memcpy(fp->img + fp->r_ptr, fp->wbuf, FDD_BPS);
			fp->w_ptr = 0;
			break;
		case 0x0200:
			dev_trace(iop, "FDD RECAL\n");
			fp->track = 0;
			break;
		case 0x0300:
			fp->track = iop->oreg_a & 0x00ff;
			dev_trace(iop, "FDD SEEK track %u\n", fp->track);
			assert(fp->track < FDD_TPD);
			break;
		default:
			assert(__LINE__ == 0);
		}
		callout_dev_is_done(iop, fp->speed);
	}
	if (IO_ACTION(ioi) == IO_PULSE) {
		fp->w_ptr = 0;
	}
}

static void * v_matchproto_(new_dev_f)
new_fdd(struct rc3600 *cs, struct iodev *iop1, struct iodev *iop2)
{
	struct io_fdd *fp;

	AN(cs);
	AN(iop1);
	AZ(iop2);

	fp = calloc(1, sizeof *fp);
	AN(fp);
	fp->iop = iop1;
	fp->speed = 10000;

	fp->iop->ins_func = dev_fdd_iofunc;
	fp->iop->priv = fp;
	install_dev(cs, fp->iop, NULL);
	return (fp);
}

static void
fdd_load_save(struct io_fdd *fp, struct cli *cli, int save)
{
	int fd;
	int e;
	ssize_t sz;

	if (cli_n_args(cli, 1))
		return;
	fd = open(cli->av[1],
	    save ?  O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY,
	    0644);
	if (fd < 0) {
		cli_error(cli, "Cannot open %s: %s\n",
		    cli->av[1], strerror(errno));
		return;
	}
	if (save)
		sz = write(fd, fp->img, sizeof fp->img);
	else
		sz = read(fd, fp->img, sizeof fp->img);
	e = errno;
	AZ(close(fd));
	if (sz < 0 || (size_t)sz != sizeof fp->img) {
		cli_error(cli, "%s error %s: %s\n",
		    save ? "Write" : "Read",
		    cli->av[1], strerror(e));
		return;
	}
	cli->av += 2;
	cli->ac -= 2;
}


void v_matchproto_(cli_func_f)
cli_fdd(struct cli *cli)
{
	struct io_fdd *fp;

	cli->ac--;
	cli->av++;
	fp = cli_dev_get_unit(cli, "FDD", NULL, new_fdd);
	if (fp == NULL)
		return;
	AN(fp);

	while (cli->ac && !cli->status) {
		if (cli_dev_trace(fp->iop, cli))
			continue;
		if (!strcasecmp(*cli->av, "load")) {
			fdd_load_save(fp, cli, 0);
			return;
		}
		if (!strcasecmp(*cli->av, "save")) {
			fdd_load_save(fp, cli, 1);
			return;
		}
		cli_unknown(cli);
		break;
	}
}
