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

#include <string.h>
#include "rc3600.h"

static void
cpu_update_intr_flag(struct rc3600 *cs)
{
	switch(cs->ins & 0xc0) {
	case 0x00:
		break;
	case 0x40:
		cs->inten[2] = 1;
		break;
	case 0x80:
		memset(cs->inten, 0, sizeof cs->inten);
		break;
	case 0xc0:
		break;
	default: break;
	}
}

static void v_matchproto_(ins_exec_f)
cpu_nova_nop(struct rc3600 *cs)
{
	cpu_update_intr_flag(cs);
}

static void v_matchproto_(ins_exec_f)
cpu_nova_reads(struct rc3600 *cs)
{
	cs->duration += cs->timing->time_io_input;
	cs->acc[(cs->ins >> 11) & 3] = cs->switches;
	cpu_update_intr_flag(cs);
}

static void v_matchproto_(ins_exec_f)
cpu_nova_inta(struct rc3600 *cs)
{
	cs->duration += cs->timing->time_io_inta;
	cs->acc[(cs->ins >> 11) & 3] = intr_inta(cs);
	cpu_update_intr_flag(cs);
}

static void v_matchproto_(ins_exec_f)
cpu_nova_msko(struct rc3600 *cs)
{
	cs->duration += cs->timing->time_io_output;
	intr_msko(cs, cs->acc[(cs->ins >> 11) & 3]);
	cpu_update_intr_flag(cs);
}

static void v_matchproto_(ins_exec_f)
cpu_nova_halt(struct rc3600 *cs)
{
	cs->running = 0;
	cpu_update_intr_flag(cs);
}

static void v_matchproto_(ins_exec_f)
cpu_nova_iorst(struct rc3600 *cs)
{
	unsigned u;

	cs->ext_core = 0;
	AZ(pthread_mutex_lock(&cs->run_mtx));
	TAILQ_INIT(&cs->irq_list);
	TAILQ_INIT(&cs->masked_irq_list);
	for (u = 0; u < IO_MAXDEV; u++)
		cs->iodevs[u]->ipen = 0;
	AZ(pthread_mutex_unlock(&cs->run_mtx));
	for (u = 0; u < IO_MAXDEV; u++)
		if (cs->iodevs[u] != cs->nodev)
			cs->iodevs[u]->io_func(cs->iodevs[u], 0, NULL);
	cpu_update_intr_flag(cs);
}

static void v_matchproto_(ins_exec_f)
cpu_nova_skpbn(struct rc3600 *cs)
{
	cs->duration += cs->timing->time_io_skp;
	if (cs->inten[0]) {
		cs->duration += cs->timing->time_io_skp_skip;
		cs->npc++;
	}
}

static void v_matchproto_(ins_exec_f)
cpu_nova_skpbz(struct rc3600 *cs)
{
	cs->duration += cs->timing->time_io_skp;
	if (!cs->inten[0]) {
		cs->duration += cs->timing->time_io_skp_skip;
		cs->npc++;
	}
}

static void v_matchproto_(ins_exec_f)
cpu_nova_skpdn(struct rc3600 *cs)
{
	// Test for Power Fail == 1
	cs->duration += cs->timing->time_io_skp;
	(void)cs;
}

static void v_matchproto_(ins_exec_f)
cpu_nova_skpdz(struct rc3600 *cs)
{
	// Test for Power Fail == 0
	cs->duration += cs->timing->time_io_skp;
	cs->duration += cs->timing->time_io_skp_skip;
	cs->npc++;
}

void
cpu_nova(struct rc3600 *cs)
{
	unsigned f, a, acc;
	const char *iflg, *nop;

	cs->ins_exec[0x673f] = cpu_nova_skpbn;
	disass_magic(0x673f, "SKPINTN");

	cs->ins_exec[0x677f] = cpu_nova_skpbz;
	disass_magic(0x677f, "SKPINTZ");

	cs->ins_exec[0x67bf] = cpu_nova_skpdn;
	disass_magic(0x67bf, "SKPPWRN");

	cs->ins_exec[0x67ff] = cpu_nova_skpdz;
	disass_magic(0x67ff, "SKPPWRZ");

	for (f = 0; f < 0x100; f += 0x40) {
		switch (f) {
		case 0x40: iflg = ",IEN"; nop = "INTEN"; break;
		case 0x80: iflg = ",IDS"; nop = "INTDS"; break;
		default: iflg = ""; nop = "NOP"; break;
		}
		cs->ins_exec[0x603f | f] = cpu_nova_nop;
		disass_magic(0x603f | f, "%s", nop);
		for (a = 0x0000; a < 0x2000; a += 0x0800) {
			acc = a >> 11;
			cs->ins_exec[0x613f | f | a] = cpu_nova_reads;
			disass_magic(0x613f | f | a, "READS  %u%s", acc, iflg);

			cs->ins_exec[0x633f | f | a] = cpu_nova_inta;
			disass_magic(0x633f | f | a, "INTA   %u%s", acc, iflg);

			cs->ins_exec[0x643f | f | a] = cpu_nova_msko;
			disass_magic(0x643f | f | a, "MSKO   %u%s", acc, iflg);

			cs->ins_exec[0x653f | f | a] = cpu_nova_iorst;
			disass_magic(0x653f | f | a, "IORST  %u%s", acc, iflg);

			cs->ins_exec[0x663f | f | a] = cpu_nova_halt;
			disass_magic(0x663f | f | a, "HALT   %u%s", acc, iflg);
		}
	}
}
