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

static void
dev_cpu_ins(struct iodev *iop, uint16_t ioi, uint16_t *reg)
{
	unsigned u;
	struct rc3600 *cs;

	cs = iop->cs;

	// printf("IO_CPU: INS 0x%04x 0x%04x\n", cs->ins, ioi);

	switch (ioi) {
	case SKPBN:
		if (iop->cs->inten[0])
			iop->cs->npc++;
		return;
	case SKPBZ:
		if (!iop->cs->inten[0])
			iop->cs->npc++;
		return;
	case SKPDN:
		return;
		break;
	case SKPDZ:
		iop->cs->npc++;
		return;
		break;
	default:
		break;
	}

	switch (IO_ACTION(ioi)) {
	case IO_START:
		cs->inten[2] = 1;
		break;
	case IO_CLEAR:
		memset(cs->inten, 0, sizeof cs->inten);
		break;
	default:
		break;
	}

	switch (IO_OPER(ioi)) {
	case NIO:
		// INTEN/INTDS
		break;
	case DIA:
		// READS
		*reg = cs->switches;
		break;
	case DIB:
		// INTA
		iop->cs->duration += iop->cs->timing->time_io_inta;
		*reg = intr_inta(iop->cs);
		break;
	case DIC:
		// IORST
		TAILQ_INIT(&cs->irq_list);
		TAILQ_INIT(&cs->masked_irq_list);
		for (u = 0; u < 0x3f; u++) {
			if (cs->iodevs[u] == NULL)
				continue;
			if (cs->iodevs[u]->ins_func != NULL) {
				cs->iodevs[u]->ins_func(cs->iodevs[u], 0, NULL);
			} else {
				std_io_ins(cs->iodevs[u], 0, NULL);
			}
		}
		break;
	case DOB:
		// MSKO
		intr_msko(iop->cs, *reg);
		break;
	case DOC:
		printf("HALT\n");
		cs->running = 0;
		break;
	default:
		printf("IO_CPU: 0x%04x\n", ioi);
		assert(0 == __LINE__);
		break;
	}
}

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
dev_cpu_init(struct iodev *iop)
{
	unsigned u, acc, flg;
	const char *iflg;

	(void)iop;
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

			disass_magic(0x613f | u, "READS  %d%s", acc, iflg);
			disass_magic(0x633f | u, "INTA   %d%s", acc, iflg);
			disass_magic(0x643f | u, "MASKO  %d%s", acc, iflg);
			disass_magic(0x653f | u, "IORST  %d%s", acc, iflg);
			disass_magic(0x663f | u, "HALT   %d%s", acc, iflg);

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
	disass_magic(0x607f, "INTEN");
	disass_magic(0x60bf, "INTDS");
	disass_magic(0x673f, "SKPINTN");
	disass_magic(0x677f, "SKPINTZ");
	disass_magic(0x67bf, "SKPPWRN");
	disass_magic(0x67ff, "SKPPWRZ");
}


struct iodev iodev_cpu = {
	.init_func =	dev_cpu_init,
	.ins_func =	dev_cpu_ins,
	.name =		"CPU",
};

struct iodev iodev_cpu721 = {
	.ins_func =	dev_cpu721_ins,
	.name =		"CPU721",
};
