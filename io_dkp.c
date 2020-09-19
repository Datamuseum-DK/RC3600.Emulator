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

#define BPS	512
#define SPT	12
#define TPC	2
#define CPD	203

#define DKP_SIZE	(CPD * TPC * SPT * BPS)

struct dkp_drive {
	unsigned		drive_no;
	struct iodev		*iop;
	uint8_t			img[DKP_SIZE];
	unsigned		cyl;
	pthread_t		seek_thread;
	pthread_mutex_t		seek_mtx;
	pthread_cond_t		seek_cond;
};

struct io_dkp {
	struct dkp_drive	drive[4];
	uint16_t		drv;
	uint16_t		cyl;
	uint16_t		hd;
	uint16_t		sec;
	uint16_t		nsec;
	uint16_t		core_adr;
	struct iodev		*iop;
};

static void
dev_dkp_iofunc(struct iodev *iop, uint16_t ioi, uint16_t *reg)
{
	struct io_dkp *tp = iop->priv;
	struct dkp_drive *dp = NULL;
	int clr = 0, clrall = 0, clrstatus = 0;

	iop->ireg_a &= 0x7fff;
	if (iop->done)
		iop->ireg_a |= 0x8000;

	std_io_ins(iop, ioi, reg);

	if (ioi == 0)
		clr = 1;
	if (IO_ACTION(ioi) == IO_CLEAR)
		clr = 1;
	if (IO_ACTION(ioi) == IO_START)
		clrall = 1;

	if (IO_OPER(ioi) == IO_DOA && *reg & 0x8000) {
		clrstatus = 1;
		iop->ireg_a &= ~(*reg & 0xf800);
	}

	if (clr)
		clrall = 1;

	if (clrall)
		clrstatus = 1;

	if (clrstatus)
		iop->done = 0;

	if (clr)
		iop->busy = 0;

	if (clr || clrstatus)
		iop->ireg_a &= ~0x7800;

	if (!(iop->ireg_a & 0xf800))
		intr_lower(iop);

	switch(IO_OPER(ioi)) {
	case IO_DOA:
		tp->cyl = *reg & 0xff;
		break;
	case IO_DOB:
		tp->core_adr = *reg;
		break;
	case IO_DOC:
		tp->drv = *reg >> 14;
		tp->hd = (*reg >> 8) & 0x3f;
		tp->sec = (*reg >> 4) & 0xf;
		tp->nsec = *reg & 0xf;
		break;
	case IO_DIB:
		*reg = tp->core_adr;
		break;
	case IO_DIC:
		AZ(tp->hd & ~0x3f);
		AZ(tp->sec & ~0xf);
		AZ(tp->nsec & ~0xf);
		*reg =
		    (tp->drv << 14) |
		    (tp->hd << 8) |
		    (tp->sec << 4) |
		    tp->nsec;
		break;
	default:
		break;
	}

	if ((iop->oreg_a & 0x200) && IO_ACTION(ioi) == IO_PULSE) {
		dp = &tp->drive[tp->drv];
		AZ(pthread_mutex_lock(&dp->seek_mtx));
		if (iop->oreg_a & 0x100)
			dp->cyl = 0;
		else
			dp->cyl = tp->cyl;
		AZ(pthread_mutex_unlock(&dp->seek_mtx));

		// Seeking
		iop->ireg_a |= 0x0200 >> dp->drive_no;

		// Not Seek Complete
		iop->ireg_a &= ~(0x4000 >> dp->drive_no);

		AZ(pthread_cond_signal(&dp->seek_cond));
	}

}

static void
do_xfer(struct iodev *iop, struct io_dkp *tp, int do_read)
{
	unsigned u;
	uint8_t *p;
	struct dkp_drive *dd;

	AN(iop);
	AN(tp);
	callout_dev_sleep(iop, 200000);
	dd = &tp->drive[tp->drv];
	do {
		assert(dd->cyl < 0xff);
		u = ((((dd->cyl * TPC) + tp->hd) * SPT) + tp->sec) * BPS;
		p = dd->img + u;
		for(u = 0; u < 256; u++, p += 2, tp->core_adr++) {
			if (do_read)
				core_write(iop->cs, tp->core_adr, be16dec(p), CORE_DMA);
			else
				be16enc(p, core_read(iop->cs, tp->core_adr, CORE_DMA | CORE_DATA));
		}

		if (++tp->sec == SPT) {
			tp->sec = 0;
			tp->hd++;
		}

		tp->nsec++;
		tp->nsec &= 0xf;
		AZ(pthread_cond_signal(&iop->cs->wait_cond));

	} while(tp->nsec);
	//printf("DKP %s DONE\n", read ? "READ" : "WRITE");
	AZ(pthread_mutex_lock(&iop->mtx));

	// RW done
	iop->ireg_a |= 0x8000;

	// Seek complete
	//iop->ireg_a |= 0x4000 >> tp->drive[0].drive_no;

	if (!do_read)
		tp->core_adr += 2;

	AZ(pthread_mutex_unlock(&iop->mtx));
	callout_dev_sleep(iop, 1000000);
}

static void*
dev_dkp_thread(void *priv)
{
	struct iodev *iop = priv;
	struct io_dkp *tp;

	AN(iop);
	tp = iop->priv;
	AN(tp);
	AZ(pthread_mutex_lock(&iop->mtx));
	while (1) {
		while (!iop->busy)
			AZ(pthread_cond_wait(&iop->cond, &iop->mtx));
		AZ(pthread_mutex_unlock(&iop->mtx));

		switch ((iop->oreg_a >> 8) & 3) {
		case 0x0:
			//printf("DKP READ\n");
			do_xfer(iop, tp, 1);
			break;
		case 0x1:
			//printf("DKP WRITE\n");
			do_xfer(iop, tp, 0);
			break;
		case 0x2:
			dev_trace(iop, "DKP SEEK w/START\n");
			exit(2);
			break;
		case 0x3:
			dev_trace(iop, "DKP RECALIBRATE w/START\n");
			exit(2);
			break;
		default:
			assert(0);
		}
		AZ(pthread_mutex_lock(&iop->mtx));
		iop->busy = 0;
		iop->done = 1;
		dev_trace(iop, "DKP Xfer Complete\n");
		intr_raise(iop);
	}
}

static void *
dkp_seek_thread(void *priv)
{
	struct dkp_drive *dp = priv;

	while (1) {
		AZ(pthread_mutex_lock(&dp->seek_mtx));
		AZ(pthread_cond_wait(&dp->seek_cond, &dp->seek_mtx));
		AZ(pthread_mutex_unlock(&dp->seek_mtx));

		//printf("SEEK/RECAL BEGIN\n");
		usleep(200);
		//printf("SEEK/RECAL END\n");

		AZ(pthread_mutex_lock(&dp->iop->mtx));

		// Not seeking
		dp->iop->ireg_a &= ~(0x0200 >> dp->drive_no);

		// Seek complete
		dp->iop->ireg_a |= 0x4000 >> dp->drive_no;

		dev_trace(dp->iop, "DKP Seek Complete\n");
		dp->iop->done = 1;
		intr_raise(dp->iop);
		AZ(pthread_mutex_unlock(&dp->iop->mtx));
	}
}

static void
new_drive(struct iodev *iop, unsigned drive, struct dkp_drive *dp)
{
	memset(dp, 0, sizeof *dp);
	dp->drive_no = drive;
	dp->iop = iop;
	AZ(pthread_mutex_init(&dp->seek_mtx, NULL));
	AZ(pthread_cond_init(&dp->seek_cond, NULL));
	AZ(pthread_create(&dp->seek_thread, NULL, dkp_seek_thread, dp));
}

static void * v_matchproto_(new_dev_f)
new_dkp(struct iodev *iop1, struct iodev *iop2)
{
	struct io_dkp *tp;

	AN(iop1);
	AZ(iop2);

	tp = calloc(1, sizeof *tp);
	AN(tp);
	tp->iop = iop1;
	tp->iop->io_func = dev_dkp_iofunc;
	new_drive(tp->iop, 0, &tp->drive[0]);
	new_drive(tp->iop, 1, &tp->drive[1]);
	new_drive(tp->iop, 2, &tp->drive[2]);
	new_drive(tp->iop, 3, &tp->drive[3]);
	// Disk Ready
	tp->iop->ireg_a |= 0x40;

	tp->iop->priv = tp;
	install_dev(tp->iop, dev_dkp_thread);
	return (tp);
}

static void
dkp_load_save(struct io_dkp *tp, struct cli *cli, int save)
{
	int drive;
	struct dkp_drive *dp;
	int fd;
	int e;
	ssize_t sz;

	if (cli_n_args(cli, 2))
		return;
	drive = atoi(cli->av[1]);
	if (drive < 0 || drive > 3) {
		cli_error(cli, "Drive number must be [0…3]\n");
		return;
	}
	dp = &tp->drive[drive];
	fd = open(cli->av[2],
	    save ?  O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY,
	    0644);
	if (fd < 0) {
		cli_error(cli, "Cannot open %s: %s\n",
		    cli->av[2], strerror(errno));
		return;
	}
	if (save)
		sz = write(fd, dp->img, sizeof dp->img);
	else
		sz = read(fd, dp->img, sizeof dp->img);
	e = errno;
	AZ(close(fd));
	if (sz != sizeof dp->img) {
		cli_error(cli, "%s error %s: %s\n",
		    save ? "Write" : "Read",
		    cli->av[2], strerror(e));
		return;
	}
	cli->av += 3;
	cli->ac -= 3;
}


void v_matchproto_(cli_func_f)
cli_dkp(struct cli *cli)
{
	struct io_dkp *tp;

	if (cli->help) {
		cli_io_help(cli, "RC3652 \"Diablo\" disk controller", 0, 0);
		cli_printf(cli, "\tload <0…3> <filename>\n");
		cli_printf(cli, "\t\tLoad disk-image from file\n");
		cli_printf(cli, "\tsave <0…3> <filename>\n");
		cli_printf(cli, "\t\tSave disk-image to file\n");
		return;
	}

	cli->ac--;
	cli->av++;
	tp = cli_dev_get_unit(cli, "DKP", NULL, new_dkp);
	if (tp == NULL)
		return;
	AN(tp);
	AN(tp->iop->priv);

	while (cli->ac && !cli->status) {
		if (!strcasecmp(*cli->av, "load")) {
			dkp_load_save(tp, cli, 0);
			return;
		}
		if (!strcasecmp(*cli->av, "save")) {
			dkp_load_save(tp, cli, 1);
			return;
		}
		cli_unknown(cli);
		break;
	}
}
