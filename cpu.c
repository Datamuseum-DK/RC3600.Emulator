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
#include "rc3600.h"

struct cpu_model;
typedef void cpu_setup_f(struct rc3600 *cs, const struct cpu_model *cm);

struct cpu_model {
	const char			*name;
	const char			*desc;
	cpu_setup_f			*setup;
	const struct ins_timing		*timing;
};

/**********************************************************************/

void
cpu_add_dev(struct iodev *iop, iodev_thr *thr)
{
	struct rc3600 *cs;

	AN(iop);
	cs = iop->cs;
	AN(cs);
	AN(iop->devno);
	assert(cs->iodevs[iop->devno] == cs->nodev);
	AN(iop->imask);
	AZ(pthread_mutex_init(&iop->mtx, NULL));
	AZ(pthread_cond_init(&iop->cond, NULL));
	AZ(pthread_cond_init(&iop->sleep_cond, NULL));
	if (iop->io_func == NULL)
		iop->io_func = std_io_ins;
	if (iop->skp_func == NULL)
		iop->skp_func = std_skp_ins;
	if (thr != NULL)
		AZ(pthread_create(&iop->thread, NULL, thr, iop));
	cs->iodevs[iop->devno] = iop;
}

/**********************************************************************/

void
cpu_start(struct rc3600 *cs)
{
	AN(cs)

	if (!cs->running) {
		AZ(pthread_mutex_lock(&cs->run_mtx));
		cs->running = 1;
		AZ(pthread_mutex_unlock(&cs->run_mtx));
		AZ(pthread_cond_signal(&cs->run_cond));
	}
}

void
cpu_stop(struct rc3600 *cs)
{

	if (!cs->running)
		return;
	AZ(pthread_mutex_lock(&cs->run_mtx));
	cs->running = 0;
	AZ(pthread_mutex_lock(&cs->running_mtx));
	AZ(pthread_mutex_unlock(&cs->running_mtx));
	AZ(pthread_mutex_unlock(&cs->run_mtx));
}

void
cpu_instr(struct rc3600 *cs)
{
	if (cs->do_trace)
		trace_state(cs);
	rc3600_exec(cs);
}

static void *
cpu_thread(void *priv)
{
	struct rc3600 *cs = priv;
	struct iodev *iop;
	int time_step;
	int iter = 0;
	nanosec pace;
	nanosec next_tmo;
	nanosec dt;
	nanosec zzz;
	struct timespec ts;

	AZ(pthread_mutex_lock(&cs->run_mtx));
	while (1) {
		time_step = !cs->running;
		while (!cs->running)
			AZ(pthread_cond_wait(&cs->run_cond, &cs->run_mtx));
		AZ(pthread_mutex_lock(&cs->running_mtx));
		cs->real_time = now();
		if (0) {
			if (time_step)
				cs->sim_time = cs->real_time;
		}

		iop = intr_pending(cs);
		AZ(pthread_mutex_unlock(&cs->run_mtx));
		if (iop != NULL) {
			dev_trace(iop, "INTERRUPT %s 0x%02x\n",
			    iop->name, iop->devno);
			core_write(cs, 0, cs->pc, CORE_WRITE);
			cs->pc = core_read(cs, 1, CORE_READ | CORE_INDIR);
		}
		if (cs->do_trace)
			trace_state(cs);
		cs->duration = 0;
		cs->ins_count++;
		cs->ins = core_read(cs, cs->pc, CORE_READ | CORE_INS);
		cs->npc = cs->pc + 1;
		cs->ins_exec[cs->ins](cs);
		cs->pc = cs->npc;
		if (cs->core_size <= 0x8000)
			cs->pc &= 0x7fff;
		cs->inten[0] = cs->inten[1];
		cs->inten[1] = cs->inten[2];
		cs->sim_time += cs->duration;

		next_tmo = callout_poll(cs);
		AZ(pthread_mutex_unlock(&cs->running_mtx));

		pace = 0;
		if (cs->pc < 0x100 &&
		    core_read(cs, cs->pc, CORE_NULL) == cs->pc) {
			if (++iter > 3)
				pace = 100000000;
		} else if (cs->sim_time > cs->real_time + 1000000) {
			// pace = 1000000;
		} else {
			iter = 0;
		}
		if (next_tmo > 0) {
			dt = next_tmo;
			dt -= cs->sim_time;
			if (dt < 0)
				dt = 0;
			if (dt < pace)
				pace = dt;
		}
		AZ(pthread_mutex_lock(&cs->run_mtx));
		if (TAILQ_EMPTY(&cs->irq_list) && pace > 0) {
			zzz = pace + now();
			ts.tv_sec = zzz / 1000000000;
			ts.tv_nsec = zzz % 1000000000;
			(void)pthread_cond_timedwait(&cs->wait_cond, &cs->run_mtx, &ts);
			cs->sim_time += pace;
			cs->last_core = cs->ins_count;
		}

	}
}

static void
cpu_init_instructions(struct rc3600 *cs)
{
	int i;
	for (i = 0; i < (1<<16); i++)
		cs->ins_exec[i] = rc3600_exec;
}

static void
cpu_setup_nova1200(struct rc3600 *cs, const struct cpu_model *cm)
{
	cpu_init_instructions(cs);
	cpu_nova(cs);
	cs->timing = cm->timing;
	cs->cpu_model = cm->name;
}

/*
 * From: RCSL 43-GL-10561 MUI13 source code:
 *
 *	THE FOLLOWING CPU-TYPES ARE HANDLED:
 *		0 : RC3603
 *		1 : RC3703
 *		2 : RC3803
 *		3 : RC3703 WITH MEMORY FACILITY AS RC3803
 *		4 : RC3603 WITH MEMORY FACILITY AS RC3803
 *
 * From: RCSL 44-RT-1557 INSTRUCTION TIMER TEST source code
 *
 *	FIRST MEM MODULE, CPU TYPE
 *	NOVA 1200		12
 *	NOVA 2 - 8K		16
 *	NOVA2 - 16K		17
 *	RC3603 - 16K		20
 *	  WITH BREAK		21
 *	RC3603 - 32K		22
 *	  WITH BREAK		23
 */

static const struct cpu_model cpu_models[] = {
	{
	.name = "Nova",
	.desc = "Data General Nova",
	.setup = cpu_setup_nova1200,
	.timing = &nova_timing,
	},
	{
	.name = "Nova1200",
	.desc = "Data General Nova 1200",
	.setup = cpu_setup_nova1200,
	.timing = &nova1200_timing,
	},
	{
	.name = "Nova2",
	.desc = "Data General Nova 2",
	.setup = cpu_setup_nova1200,
	.timing = &nova2_timing,
	},
	{
	.name = "Nova800",
	.desc = "Data General Nova 800",
	.setup = cpu_setup_nova1200,
	.timing = &nova800_timing,
	},
	{
	.name = "RC7000",
	.desc = "Regnecentralen RC7000 (=Nova 1200)",
	.setup = cpu_setup_nova1200,
	.timing = &nova1200_timing,
	},
	{
	/*
	 * RCSL-42-I-1008 "RC3803 CPU, Programmer's Reference Manual"
	 * has keyword "CPU 708"
	 */
	.name = "RC3603",
	.desc = "Regnecentralen RC3600 (CPU708)",
	.setup = cpu_setup_nova1200,
	.timing = &rc3609_timing,
	},
	{
	.name = "RC3703",
	.desc = "Regnecentralen RC3700 (CPU720 ?)",
	.setup = cpu_setup_nova1200,
	.timing = &rc3608_timing,
	},
	{
	/*
	 * RCSL-42-I-1008 "RC3803 CPU, Programmer's Reference Manual"
	 * has keyword "CPU 720"
	 */
	.name = "RC3803",
	.desc = "Regnecentralen RC3800 (CPU721 ?)",
	.setup = cpu_setup_nova1200,
	.timing = &rc3608_timing,
	},
	{ NULL, NULL, NULL, NULL },
};

struct rc3600 *
cpu_new(void)
{
	struct rc3600 *cs;

	cs = calloc(1, sizeof *cs);
	AN(cs);

	AZ(pthread_mutex_init(&cs->run_mtx, NULL));
	AZ(pthread_mutex_init(&cs->running_mtx, NULL));
	AZ(pthread_cond_init(&cs->run_cond, NULL));
	AZ(pthread_cond_init(&cs->wait_cond, NULL));
	AZ(pthread_mutex_init(&cs->callout_mtx, NULL));

	cs->core_size = 0x8000;
	cs->core = core_new(cs->core_size);

	iodev_init(cs);

	TAILQ_INIT(&cs->irq_list);
	TAILQ_INIT(&cs->masked_irq_list);
	TAILQ_INIT(&cs->callouts);
	cs->fd_trace = -1;

	cpu_models[0].setup(cs, &cpu_models[0]);

	AZ(pthread_create(&cs->cthread, NULL, cpu_thread, cs));

	return (cs);
}

void v_matchproto_(cli_func_f)
cli_cpu(struct cli *cli)
{
	struct rc3600 *cs;
	const struct cpu_model *mp;
	const char *ptr;

	AN(cli);
	cs = cli->cs;
	AN(cs);
	cli->ac--;
	cli->av++;
	if (cli->ac == 1 && !strcmp(cli->av[0], "model")) {
		for (mp = cpu_models; mp->name != NULL; mp++) {
			if (mp->name == cs->cpu_model)
				ptr = "-->";
			else
				ptr = "   ";
			cli_printf(cli, "%s %-20s %s\n",
			    ptr, mp->name, mp->desc);
		}
		return;
	}
	if (cli->ac > 0 && !strcmp(cli->av[0], "model")) {
		if (cli_n_args(cli, 1))
			return;
		for (mp = cpu_models; mp->name != NULL; mp++)
			if (!strcasecmp(mp->name, cli->av[1]))
				break;
		if (mp->name == NULL) {
			(void)cli_error(cli, "CPU model not found.\n");
			return;
		}
		cli->ac -= 2;
		cli->av += 2;
		mp->setup(cs, mp);
		return;
	}
}

#if 0

static void
dev_cpu721_ins(struct iodev *iop, uint16_t ioi, uint16_t *reg)
{
	uint16_t u;
	const char *what = "?";

	if ((iop->cs->ins & ~0x1800) == 0x6102) {
		// IDFY
		what = "IDFY";
		*reg = 4;
	} else if (iop->cs->ins == 0x6781) {
		// CHECK MEM EXPANSION
		if (iop->cs->core_size > 0x8000)
			iop->cs->npc += 1;
	} else if (iop->cs->ins == 0x6581) {
		// LDB
		what = "LDB";
		u = core_read(iop->cs, iop->cs->acc[1] >> 1, CORE_READ);
		if (iop->cs->acc[1] & 1) {
			iop->cs->acc[0] = u & 0xff;
		} else {
			iop->cs->acc[0] = u >> 8;
		}
	} else if (iop->cs->ins == 0x6681) {
		// STB
		what = "STB";
		u = core_read(iop->cs, iop->cs->acc[1] >> 1, CORE_NULL);
		if (iop->cs->acc[1] & 1) {
			u &= 0xff00;
			u |= iop->cs->acc[0] & 0xff;
		} else {
			u &= 0x00ff;
			u |= (iop->cs->acc[0] & 0xff) << 8;
		}
		core_write(iop->cs, iop->cs->acc[1] >> 1, u, CORE_MODIFY);
	} else if (iop->cs->ins == 0x65c1) {
		// ENABLE HIGH MOBY
		iop->cs->core_size = 1 << 16;
		core_setsize(iop->cs->core, iop->cs->core_size);
	} else {
		printf("CPU721: 0x%04x 0x%04x %s\n", ioi, iop->cs->ins, what);
	}
}

static void
dev_cpu_init(void)
{
	unsigned u, acc, flg;
	const char *iflg;

	for (acc = 0; acc < 4; acc++) {
		for (flg = 0; flg < 4; flg++) {
			if (flg == 1)
				iflg = ",IEN";
			else if (flg == 2)
				iflg = ",IDS";
			else
				iflg = "";
			u = acc << 11;
			u |= flg << 6;

			u = acc << 11;
			disass_magic(0x6102 | u, "@IDFY  %d", acc);
			disass_magic(0x6581 | u, "@LDB   %d", acc);
			disass_magic(0x6681 | u, "@STB   %d", acc);
			disass_magic(0x6502 | u, "@BMOVE %d", acc);
			disass_magic(0x6542 | u, "@WMOVE %d", acc);
			disass_magic(0x6582 | u, "@SCHEL %d", acc);
			disass_magic(0x65c2 | u, "@SFREE %d", acc);
			disass_magic(0x6602 | u, "@LINK  %d", acc);
			disass_magic(0x6642 | u, "@REMEL %d", acc);
			disass_magic(0x6682 | u, "@PLINK %d", acc);
			disass_magic(0x66c2 | u, "@FETCH %d", acc);
			disass_magic(0x6702 | u, "@TKADD %d", acc);
			disass_magic(0x6742 | u, "@TKVAL %d", acc);
			disass_magic(0x6782 | u, "@COMP  %d", acc);
		}
	}
}

#endif
