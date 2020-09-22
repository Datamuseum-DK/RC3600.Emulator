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
 *
 * The extended instruction set of the RC3803/CPU720/CPU721 CPU
 * ------------------------------------------------------------
 *
 * These instructions are presumably specified in
 *
 *     RCSL-42-I-1008 RC 3803 CPU Programmer's Reference Manual
 *
 * but the specification is deficient in a number of ways which
 * make it useless for implementors and buggy for users.
 *
 * Getting:
 *
 *     RCSL-52-AA-899 TEST OF INSTRUCTION SET FOR CPU 720
 *
 * to pass helps, but even that very exhaustive and competent
 * test misses some required behaviour, for instance the value
 * returned in AC1 from the FETCH instruction.
 *
 * More complicted, neither the wrong documentation or the test
 * program explain how other obscure details can possibly work
 * in the first place which leads to undocumented AC1 call value
 * requirement for PLINK.
 *
 * For that I had to consult the microcode documentation:
 *
 *     RCSL-44-RT-1877 CPU720 MICROPROGRAM FLOWCHARTS
 *
 * And should that ever fail, I could resort to the microcode
 * prom listings in:
 *
 *     RCSL-44-RT-1955 CPU720 ROM LISTING
 *
 * and
 *
 *     RCSL-44-RT-2063 CPU 721 ROM Listing
 *
 * But I hope it never gets to that...
 *
 */

#include <string.h>
#include "rc3600.h"

static void v_matchproto_(ins_exec_f)
cpu_720_idfy(struct rc3600 *cs)
{
	cs->acc[(cs->ins >> 11) & 3] = cs->ident;
	cs->duration += 1500;
}

static uint16_t
getbyte(struct rc3600 *cs, uint16_t baddr)
{
	uint16_t u;

	u = core_read(cs, baddr >> 1, CORE_READ);
	if (baddr & 1)
		return (u & 0xff);
	return (u >> 8);
}

static void
putbyte(struct rc3600 *cs, uint16_t baddr, uint16_t data)
{
	uint16_t u;

	u = core_read(cs, baddr >> 1, CORE_NULL);
	if (baddr & 1) {
		u &= 0xff00;
		u |= data & 0xff;
	} else {
		u &= 0x00ff;
		u |= (data & 0xff) << 8;
	}
	core_write(cs, baddr >> 1, u, CORE_MODIFY);
}

static void v_matchproto_(ins_exec_f)
cpu_720_ldb(struct rc3600 *cs)
{

	cs->acc[0] = getbyte(cs, cs->acc[1]);
	if (cs->acc[1] & 1)
		cs->duration += 3100;
	else
		cs->duration += 3700;
}

static void v_matchproto_(ins_exec_f)
cpu_720_stb(struct rc3600 *cs)
{

	putbyte(cs, cs->acc[1], cs->acc[0]);
	if (cs->acc[1] & 1)
		cs->duration += 4400;
	else
		cs->duration += 5000;
}

static void v_matchproto_(ins_exec_f)
cpu_720_bmove(struct rc3600 *cs)
{
	uint16_t u;

	if (cs->acc[3]) {
		if (!(cs->acc[1] & 1) && !(cs->acc[2] & 1))
			cs->duration += 7900;
		else if ((cs->acc[1] & 1) && !(cs->acc[2] & 1))
			cs->duration += 6700;
		else
			cs->duration += 7300;
		u = getbyte(cs, cs->acc[1]);
		if (cs->acc[0]) {
			if ((cs->acc[0] + u) & 1)
				cs->duration += 3100;
			else
				cs->duration += 2500;
			u = getbyte(cs, cs->acc[0] + u);
		}
		putbyte(cs, cs->acc[2], u);
		cs->acc[1]++;
		cs->acc[2]++;
		cs->acc[3]--;
		cs->npc = cs->pc;
	} else {
		cs->duration += 1500;
	}
}

static void v_matchproto_(ins_exec_f)
cpu_720_comp(struct rc3600 *cs)
{
	uint16_t u;
	uint16_t v;

	if (cs->acc[0]) {
		if (!(cs->acc[1] & 1) && !(cs->acc[2] & 1))
			cs->duration += 7500;
		else if ((cs->acc[1] & 1) && !(cs->acc[2] & 1))
			cs->duration += 6200;
		else
			cs->duration += 6800;
		u = getbyte(cs, cs->acc[1]);
		v = getbyte(cs, cs->acc[2]);
		cs->acc[1]++;
		cs->acc[2]++;
		if (u != v) {
			cs->acc[0] = u - v;
			return;
		}
		cs->acc[0]--;
		cs->npc = cs->pc;
	} else {
		cs->duration += 1200;
	}
}

static void v_matchproto_(ins_exec_f)
cpu_720_wmove(struct rc3600 *cs)
{
	uint16_t u;

	if (cs->acc[0]) {
		cs->duration += 2700;
		u = core_read(cs, cs->acc[1], CORE_NULL);
		core_write(cs, cs->acc[2], u, CORE_MODIFY);
		cs->acc[1]++;
		cs->acc[2]++;
		cs->acc[0]--;
		cs->npc = cs->pc;
	} else {
		cs->duration += 1500;
	}
}

static void v_matchproto_(ins_exec_f)
cpu_720_schel(struct rc3600 *cs)
{
	uint16_t u;

	u = core_read(cs, cs->acc[1] + 2, CORE_NULL);
	if (u == 0) {
		cs->acc[2] = u;
		cs->acc[3] = core_read(cs, 0x20, CORE_NULL);
		cs->duration += 8700;
		return;
	}
	do {
		if (core_read(cs, cs->acc[2], CORE_NULL) !=
		    core_read(cs, u + 4, CORE_NULL)) {
			cs->duration += 1700;	// XXX
			break;
		}
		if (core_read(cs, cs->acc[2] + 1, CORE_NULL) !=
		    core_read(cs, u + 5, CORE_NULL)) {
			cs->duration += 1700;	// XXX
			break;
		}
		if (core_read(cs, cs->acc[2] + 2, CORE_NULL) !=
		    core_read(cs, u + 6, CORE_NULL)) {
			cs->duration += 1700;	// XXX
			break;
		}
		cs->acc[1] = u + 6;	/* RCSL 52-AA-899, 017234 */
		cs->acc[2] = u;
		cs->acc[3] = core_read(cs, 0x20, CORE_NULL);
		cs->duration += 8700;
		return;
	} while (0);
	cs->acc[1] = u;
	cs->npc = cs->pc;
}

static void v_matchproto_(ins_exec_f)
cpu_720_sfree(struct rc3600 *cs)
{
	uint16_t u;

	if (cs->acc[2]) {
		cs->duration += 2300;
		u = core_read(cs, cs->acc[2] + 5, CORE_NULL);
		if (u != 0) {
			cs->acc[2] = core_read(cs, cs->acc[2] + 2, CORE_NULL);
			cs->npc = cs->pc;
		}
	} else {
		cs->duration += 2600;
	}
}

static void v_matchproto_(ins_exec_f)
cpu_720_link(struct rc3600 *cs)
{
	uint16_t oldtail;

	cs->acc[3] = cs->acc[1];
	oldtail = core_read(cs, cs->acc[1] + 1, CORE_NULL);
	cs->acc[0] = oldtail;		/* RCSL 52-AA-899, 017606 */
	core_write(cs, cs->acc[1] + 1, cs->acc[2], CORE_MODIFY);
	core_write(cs, cs->acc[2], cs->acc[1], CORE_MODIFY);
	core_write(cs, cs->acc[2] + 1, oldtail, CORE_MODIFY);
	core_write(cs, oldtail, cs->acc[2], CORE_MODIFY);
	cs->duration += 7200;
}

static void v_matchproto_(ins_exec_f)
cpu_720_remel(struct rc3600 *cs)
{

	cs->acc[3] = core_read(cs, cs->acc[2], CORE_NULL);
	cs->acc[0] = core_read(cs, cs->acc[2] + 1, CORE_NULL);
	core_write(cs, cs->acc[0], cs->acc[3], CORE_MODIFY);
	core_write(cs, cs->acc[3] + 1, cs->acc[0], CORE_MODIFY);
	core_write(cs, cs->acc[2], cs->acc[2], CORE_MODIFY);
	core_write(cs, cs->acc[2] + 1, cs->acc[2], CORE_MODIFY);
	cs->duration += 8100;
}

static void v_matchproto_(ins_exec_f)
cpu_720_plink(struct rc3600 *cs)
{
	uint16_t q, pre, elem;

	if (cs->acc[1]) {	/* RCSL 30-M-328 FC19 */
		cs->duration += 5400;
		core_write(cs, cs->acc[2] + 013, 0, CORE_MODIFY);
		cs->acc[3] = core_read(cs, cs->acc[2] + 015, CORE_NULL);
		cs->acc[0] = core_read(cs, 054, CORE_NULL);
		cs->acc[1] = 0;
		cs->npc = cs->pc;
		return;
	}

	elem = core_read(cs, cs->acc[0], CORE_NULL);
	q = core_read(cs, elem + 015, CORE_NULL);
	if (q >= cs->acc[3]) {
		cs->duration += 2300;
		cs->acc[0] = elem;		/* RCSL 52-AA-899, 020060 */
		cs->npc = cs->pc;
		return;
	}
	cs->duration += 7200;
	pre = core_read(cs, elem + 1, CORE_NULL);
	core_write(cs, elem + 1, cs->acc[2], CORE_MODIFY);
	core_write(cs, cs->acc[2], elem, CORE_MODIFY);
	core_write(cs, cs->acc[2] + 1, pre, CORE_MODIFY);
	core_write(cs, pre, cs->acc[2], CORE_MODIFY);
	cs->acc[3] = elem;		/* RCSL 52-AA-899, 020064 */
	cs->acc[1] = elem;		/* RCSL 52-AA-899, 020067 */
}

static void v_matchproto_(ins_exec_f)
cpu_720_fetch(struct rc3600 *cs)
{
	uint16_t q, m;

	cs->acc[2] = core_read(cs, 0x20, CORE_NULL);
	m = core_read(cs, cs->acc[2] + 033, CORE_NULL);
	core_write(cs, cs->acc[2] + 033, m + 1, CORE_MODIFY);
	q = core_read(cs, m, CORE_NULL);
	cs->npc = core_read(cs, cs->npc + (q >> 8), CORE_NULL);
	cs->acc[0] = q & 0xff;			/* RCSL 52-AA-899, 020342 */
	cs->acc[1] = q >> 8;			/* By hand, req'ed */
	cs->duration += 6700;
}

static void v_matchproto_(ins_exec_f)
cpu_720_takea(struct rc3600 *cs)
{
	uint16_t m, q, q1;

	m = core_read(cs, cs->acc[2] + 033, CORE_NULL);
	core_write(cs, cs->acc[2] + 033, m + 1, CORE_MODIFY);
	cs->acc[1] = core_read(cs, m, CORE_NULL);
	switch(cs->acc[0] & 0x3) {
	case 0: cs->duration += 4700; break;
	case 1: cs->duration += 4900; break;
	case 2: cs->duration += 4700; break;
	case 3:
		cs->duration += 7000;
		q = cs->acc[1] & 0xff;
		cs->acc[1] =  cs->acc[1] >> 8;
		q1 = cs->acc[1] + cs->acc[2];
		q1 = core_read(cs, q1 + 041, CORE_NULL);
		q1 = core_read(cs, q1 + 017, CORE_NULL);
		cs->acc[1] = q + q1;
		break;
	default:
		assert(0 == __LINE__);
	}
	cs->acc[0] >>= 2;
	cs->acc[2] = core_read(cs, 0x20, CORE_NULL);
				/* RCSL 52-AA-899, 020576 */
	cs->carry = 0;		/* By hand, not req'ed, reduces diffs */
}

static void v_matchproto_(ins_exec_f)
cpu_720_takev(struct rc3600 *cs)
{
	uint16_t m;

	if (cs->acc[0] & 1) {
		cs->acc[1] = core_read(cs, cs->acc[2] + 032, CORE_NULL);
		cs->duration += 2900;
	} else {
		cs->duration += 5100;
		m = core_read(cs, cs->acc[2] + 033, CORE_NULL);
		core_write(cs, cs->acc[2] + 033, m + 1, CORE_MODIFY);
		cs->acc[1] = core_read(cs, m, CORE_NULL);
		if (cs->acc[0] & 2) {
			cs->duration += 2600;
			cs->acc[1] = core_read(cs, cs->acc[1], CORE_NULL);
		}
	}
	cs->acc[0] >>= 2;
}

void
cpu_720(struct rc3600 *cs)
{
	unsigned a;

	for (a = 0x0000; a < 0x2000; a += 0x0800) {
		cs->ins_exec[0x6102 | a] = cpu_720_idfy;
		disass_magic(0x6102 | a, "IDFY    %u", a >> 11);
		cs->ins_exec[0x6581 | a] = cpu_720_ldb;
		disass_magic(0x6581 | a, "LDB     %u", a >> 11);
		cs->ins_exec[0x6681 | a] = cpu_720_stb;
		disass_magic(0x6681 | a, "STB     %u", a >> 11);
		cs->ins_exec[0x6502 | a] = cpu_720_bmove;
		disass_magic(0x6502 | a, "BMOVE   %u", a >> 11);
		cs->ins_exec[0x6542 | a] = cpu_720_wmove;
		disass_magic(0x6542 | a, "WMOVE   %u", a >> 11);
		cs->ins_exec[0x6782 | a] = cpu_720_comp;
		disass_magic(0x6782 | a, "COMP    %u", a >> 11);
		cs->ins_exec[0x6582 | a] = cpu_720_schel;
		disass_magic(0x6582 | a, "SCHEL   %u", a >> 11);
		cs->ins_exec[0x65c2 | a] = cpu_720_sfree;
		disass_magic(0x65c2 | a, "SFREE   %u", a >> 11);
		cs->ins_exec[0x6602 | a] = cpu_720_link;
		disass_magic(0x6602 | a, "LINK   %u", a >> 11);
		cs->ins_exec[0x6642 | a] = cpu_720_remel;
		disass_magic(0x6642 | a, "REMEL  %u", a >> 11);
		cs->ins_exec[0x6682 | a] = cpu_720_plink;
		disass_magic(0x6682 | a, "PLINK  %u", a >> 11);
		cs->ins_exec[0x66c2 | a] = cpu_720_fetch;
		disass_magic(0x66c2 | a, "FETCH  %u", a >> 11);
		cs->ins_exec[0x6702 | a] = cpu_720_takea;
		disass_magic(0x6702 | a, "TAKEA  %u", a >> 11);
		cs->ins_exec[0x6742 | a] = cpu_720_takev;
		disass_magic(0x6742 | a, "TAKEV  %u", a >> 11);
	}
}
