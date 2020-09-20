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
 * This file contains the default device unit/imask assignments, as found in
 * Appendix A in "RCSL 42-I-1008 RC 3803 CPU Programmer's Reference Manual"
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rc3600.h"

/* NODEV ============================================================ */

static void v_matchproto_(iodev_io_f)
no_dev_io_ins(struct iodev *iop, uint16_t ioi, uint16_t *reg)
{

if (1) {
	trace(iop->cs,
	    "Unclaimed IO: 0x%04x dev=0x%x\n",
	    iop->cs->ins, iop->cs->ins & 0x3f);
	trace_state(iop->cs);
}
	iop->ireg_a = 0;
	iop->ireg_b = 0;
	iop->ireg_c = 0;
	std_io_ins(iop, ioi, reg);
	iop->busy = 0;
	iop->done = 0;
}

static void v_matchproto_(iodev_skp_f)
no_dev_skp_ins(struct iodev *iop, uint16_t ioi)
{
	trace(iop->cs,
	    "Unclaimed IO-SKP: 0x%04x dev=0x%x\n",
	    iop->cs->ins, iop->cs->ins & 0x3f);
	iop->busy = iop->done = 0;
	std_skp_ins(iop, ioi);
}

void
iodev_init(struct rc3600 *cs)
{
	int i;

	cs->nodev = calloc(sizeof *cs->nodev, 1);
	cs->nodev->cs = cs;
	AN(cs->nodev);
	AZ(pthread_mutex_init(&cs->nodev->mtx, NULL));
	AZ(pthread_cond_init(&cs->nodev->cond, NULL));
	cs->nodev->io_func = no_dev_io_ins;
	cs->nodev->skp_func = no_dev_skp_ins;

	for (i = 0; i < 64; i++)
		cs->iodevs[i] = cs->nodev;
}

/* STANDARD IO DEVICE BEHAVIOUR ===================================== */

void v_matchproto_(iodev_io_f)
std_io_ins(struct iodev *iop, uint16_t ioi, uint16_t *reg)
{
	struct rc3600 *cs;

	cs = iop->cs;

	switch (IO_OPER(ioi)) {
	case 0:
		// IORST
		iop->busy = 0;
		iop->done = 0;
		intr_lower(iop);
		return;
	case IO_SKP:
		assert(0 == __LINE__);
	case IO_NIO:
		cs->duration += cs->timing->time_io_nio;
		break;
	case IO_DIA:
		cs->duration += cs->timing->time_io_input;
		*reg = iop->ireg_a;
		ioi &= 0xe7ff;
		break;
	case IO_DIB:
		cs->duration += cs->timing->time_io_input;
		*reg = iop->ireg_b;
		ioi &= 0xe7ff;
		break;
	case IO_DIC:
		cs->duration += cs->timing->time_io_input;
		*reg = iop->ireg_c;
		ioi &= 0xe7ff;
		break;
	case IO_DOA:
		cs->duration += cs->timing->time_io_output;
		iop->oreg_a = *reg;
		ioi &= 0xe7ff;
		break;
	case IO_DOB:
		cs->duration += cs->timing->time_io_output;
		iop->oreg_b = *reg;
		ioi &= 0xe7ff;
		break;
	case IO_DOC:
		cs->duration += cs->timing->time_io_output;
		iop->oreg_c = *reg;
		ioi &= 0xe7ff;
		break;
	default:
		printf("UNKNOWN IO INS 0x%04x (0x%04x)\n", ioi, ioi & 0xe7ff);
		assert(0 == __LINE__);
	}

	switch(IO_ACTION(ioi)) {
	case IO_CLEAR:
		cs->duration += cs->timing->time_io_scp;
		iop->done = 0;
		iop->busy = 0;
		if (iop != cs->nodev)
			intr_lower(iop);
		break;
	case IO_START:
		cs->duration += cs->timing->time_io_scp;
		iop->done = 0;
		iop->busy = 1;
		if (iop != cs->nodev)
			intr_lower(iop);
		AZ(pthread_cond_signal(&iop->cond));
		break;
	case IO_PULSE:
		cs->duration += cs->timing->time_io_scp;
		iop->pulse = 1;
		AZ(pthread_cond_signal(&iop->cond));
		break;
	default:
		break;
	}
	if (iop->trace > 1)
		trace_state(iop->cs);
}

void v_matchproto_(iodev_skp_f)
std_skp_ins(struct iodev *iop, uint16_t ioi)
{
	int skip;

	assert (IO_OPER(ioi) == IO_SKP);

	if (ioi & 0x0080)
		skip = iop->done;
	else
		skip = iop->busy;

	if (ioi & 0x0040)
		skip = !skip;

	iop->cs->duration += iop->cs->timing->time_io_skp;

	if (skip) {
		iop->cs->duration += iop->cs->timing->time_io_skp_skip;
		iop->cs->npc++;
	}
}

/* DEVICE NUMBER ASSIGMENTS ========================================= */

struct dev_assignment {
	const char		*name;
	unsigned		unit;
	unsigned		devno;
	int			imask;
	const char		*comment;
};

#define ASL_TXT	"Automatic System Load"
#define TTY_TXT	"Teletype"
#define PTR_TXT	"Paper Tape Reader"
#define PTP_TXT	"Paper Tape Punch"
#define RTC_TXT	"Real Time Clock"
#define PLT_TXT	"Incremental Plotter"
#define SPC_TXT	"Standard Parallel Controller"
#define CDR_TXT	"Card Reader"
#define LPT_TXT	"Line Printer"
#define DSC_TXT	"Disc Storage Channel"
#define AMX_TXT	"Asynchronous Multiplexor"
#define TMX_TXT	"64 Chan. Async. Multiplexor"
#define MT_TXT	"Magnetic Tape"
#define CLP_TXT	"Charaband Printer"
#define OCP_TXT	"Operator Control Panel"
#define IBM_TXT	"IBM Channel"
#define LPS_TXT	"Serial Printer"
#define BSC_TXT	"BSC Controller"
#define FPA_TXT	"Inter Processor Channel"
#define HLC_TXT	"HDLC Controller"
#define SMX_TXT	"Synchronous Multiplexor"
#define FDD_TXT	"Floppy Disc Drive"
#define CRP_TXT	"Card Reader Punch"
#define DTC_TXT	"Digital Cartridge Controller"
#define DST_TXT "Digital Sense"
#define DOT_TXT "Digital Output"
#define CNT_TXT "Digital Counter"
#define ACU_TXT "Dial-up Controller"
#define DKP_TXT "Moving Head Disc Channel"

static const struct dev_assignment default_assignments[] = {
	{ "ASL",	0,	005,	-1,	ASL_TXT },
	{ "TTI",	0,	010,	14,	TTY_TXT },
	{ "TTO",	0,	011,	15,	TTY_TXT },
	{ "PTR",	0,	012,	11,	PTR_TXT },
	{ "PTP",	0,	013,	13,	PTP_TXT },
	{ "RTC",	0,	014,	13,	RTC_TXT },
	{ "PLT",	0,	015,	12,	PLT_TXT },
	{ "SPC",	2,	015,	 9,	SPC_TXT },
	{ "CDR",	0,	016,	10,	CDR_TXT },
	{ "LPT",	0,	017,	12,	LPT_TXT },
	{ "DSC",	0,	020,	 4,	DSC_TXT },
	{ "SPC",	0,	021,	 9,	SPC_TXT },
	{ "SPC",	1,	022,	 9,	SPC_TXT },
	{ "PTR",	1,	023,	11,	PTR_TXT },
	{ "AMX",	3,	024,	 2,	AMX_TXT },
	{ "TMXO",	1,	024,	 0,	TMX_TXT },
	{ "TMXI",	1,	025,	 1,	TMX_TXT },
	{ "TMXO",	0,	026,	 0,	TMX_TXT },
	{ "TMXI",	0,	027,	 1,	TMX_TXT },
	{ "MT",		0,	030,	 5,	MT_TXT },
	{ "PTP",	1,	031,	13,	PTP_TXT },
	{ "TTI",	2,	032,	14,	TTY_TXT },
	{ "OCP",	0,	032,	-1,	OCP_TXT },
	{ "IBMR",	0,	032,	-1,	IBM_TXT },
	{ "TTO",	2,	033,	15,	TTY_TXT },
	{ "IBMX",	0,	033,	-1,	IBM_TXT },
	{ "TTI",	3,	034,	14,	TTY_TXT },
	{ "TTO",	3,	035,	15,	TTY_TXT },
	{ "DISP",	0,	035,	 7,	OCP_TXT },
	{ "",		0,	036,	-1,	OCP_TXT },
	{ "LPS",	0,	037,	12,	LPS_TXT },
	{ "REC",	0,	040,	 8,	BSC_TXT },
	{ "XMT",	0,	041,	 8,	BSC_TXT },
	{ "REC",	1,	042,	 8,	BSC_TXT },
	{ "XMT",	1,	043,	 8,	BSC_TXT },
	{ "MT",		1,	044,	 5,	MT_TXT },
	{ "CLP",	0,	045,	12,	CLP_TXT },
	{ "FPAR",	0,	046,	 3,	FPA_TXT },
	{ "FPAX",	0,	047,	 3,	FPA_TXT },
	{ "TTI",	1,	050,	14,	TTY_TXT },
	{ "TTO",	1,	051,	15,	TTY_TXT },
	{ "AMX",	0,	052,	 2,	AMX_TXT },
	{ "AMX",	1,	053,	 2,	AMX_TXT },
	{ "HLC",	0,	054,	 8,	HLC_TXT },
	{ "FPAR",	2,	054,	 3,	FPA_TXT },
	{ "HLC",	1,	055,	 8,	HLC_TXT },
	{ "FPAX",	2,	055,	 3,	FPA_TXT },
	{ "CDR",	1,	056,	10,	CDR_TXT },
	{ "LPT",	1,	057,	12,	LPT_TXT },
	{ "LPS",	2,	057,	12,	LPS_TXT },
	{ "SMX",	0,	060,	-1,	SMX_TXT },
	{ "FDD",	0,	061,	 7,	FDD_TXT },
	{ "CRP",	0,	062,	10,	CRP_TXT },
	{ "IBMR",	1,	062,	-1,	IBM_TXT },
	{ "IBMX",	1,	062,	-1,	IBM_TXT },
	{ "CLP",	1,	063,	12,	CLP_TXT },
	{ "FDD",	1,	064,	 7,	FDD_TXT },
	{ "LPS",	3,	065,	12,	LPS_TXT },
	{ "CLP",	2,	065,	-1,	CLP_TXT },
	{ "DTC",	0,	066,	 9,	DTC_TXT },
	{ "LPS",	4,	066,	12,	LPS_TXT },
	{ "LPS",	1,	067,	12,	LPS_TXT },
	{ "CLP",	3,	067,	-1,	CLP_TXT },
	{ "DST",	0,	070,	-1,	DST_TXT },
	{ "DOT",	0,	071,	-1,	DOT_TXT },
	{ "CNT",	0,	072,	-1,	CNT_TXT },
	{ "ACU",	0,	072,	-1,	ACU_TXT },
	{ "DKP",	0,	073,	 7,	DKP_TXT },
	{ "FPAR",	1,	074,	 3,	FPA_TXT },
	{ "FPAX",	1,	075,	 3,	FPA_TXT },
	{ "AMX",	2,	076,	 2,	AMX_TXT },
	// "CPU",	-,	077,	 -,	-
	{ NULL,		0,	0,	0,	NULL},
};

/* CLI ============================================================== */

static struct iodev *
cli_mk_iop(struct cli *cli, const char *drvname, unsigned unit)
{
	const struct dev_assignment *da;
	struct iodev *iop;

	for (da = default_assignments; da->name != NULL; da++)
		if (!strcmp(da->name, drvname) && da->unit == unit)
			break;
	if (da->name == NULL) {
		(void)cli_error(cli, "No default address+imask for %s%u\n", drvname, unit);
		// ... use 'create 7 bla bla bla'
		return (NULL);
	}
	iop = calloc(sizeof *iop, 1);
	AN(iop);
	iop->devno = da->devno;
	iop->imask = da->imask;
	bprintf(iop->name, "%s%u", drvname, unit);
	iop->cs = cli->cs;
	return (iop);
}

void *
cli_dev_get_unit(struct cli *cli, const char *drv1, const char *drv2, new_dev_f *func)
{
	unsigned long unit = 0;
	char *p;
	struct iodev *iop1, *iop2 = NULL;
	char nbuf[6];
	int i;
	void *rv;

	AN(cli);
	AN(drv1);
	AN(func);

	p = NULL;
	if (cli->ac > 0) {
		// XXX: implement "create" where unit+mask is specified.
		unit = strtoul(cli->av[0], &p, 0);
		if (p != NULL && *p == '\0' && unit <= IO_MAXDEV) {
			cli->ac--;
			cli->av++;
		} else {
			unit = 0;
		}
	}
	assert(unit <= IO_MAXDEV);
	bprintf(nbuf, "%s%lu", drv1, unit);
	for (i = 0; i < 63; i++) {
		iop1 = cli->cs->iodevs[i];
		if (iop1 != NULL && !strcmp(iop1->name, nbuf))
			return (iop1->priv);
	}
	iop1 = cli_mk_iop(cli, drv1, (unsigned)unit);
	if (iop1 == NULL)
		return (NULL);
	if (drv2 != NULL) {
		iop2 = cli_mk_iop(cli, drv2, unit);
		if (iop2 == NULL)
			return (NULL);
	}
	rv = func(iop1, iop2);
	return (rv);
}
