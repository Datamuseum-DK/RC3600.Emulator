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
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/endian.h>

#include "rc3600.h"
/* ALU Instructions --------------------------------------------------*/

static void
Insn_ALU(struct rc3600 *cs)
{
	unsigned tc, u;
	uint16_t *rps, *rpd, t;
	uint32_t tt;

	switch((cs->ins >> 4) & 3) {
	case 0:	tc = cs->carry;	break;
	case 1:	tc = 0;		break;
	case 2:	tc = 1;		break;
	case 3:	tc = 1 - cs->carry;	break;
	default:
		assert(0 == __LINE__);
	}
	rps = &cs->acc[(cs->ins >> 13) & 0x3];
	rpd = &cs->acc[(cs->ins >> 11) & 0x3];
	switch (cs->ins & 0x0700) {
	case 0x0000:	/* COM */
		cs->duration += cs->timing->time_alu_1;
		t = ~(*rps);
		break;
	case 0x0100:	/* NEG */
		cs->duration += cs->timing->time_alu_1;
		t = -(*rps);
		if (*rps == 0)
			tc ^= 1;
		break;
	case 0x0200:	/* MOV */
		cs->duration += cs->timing->time_alu_1;
		t = (*rps);
		break;
	case 0x0300:	/* INC */
		cs->duration += cs->timing->time_alu_1;
		t = (*rps) + 1;
		if (*rps == 0xffff)
			tc ^= 1;
		break;
	case 0x0400:	/* ADC */
		cs->duration += cs->timing->time_alu_2;
		t = ~(*rps) + (*rpd);
		if (*rpd > *rps)
			tc ^= 1;
		break;
	case 0x0500:	/* SUB */
		cs->duration += cs->timing->time_alu_2;
		t = (*rpd) - (*rps);
		if (*rpd >= *rps)
			tc ^= 1;
		break;
	case 0x0600:	/* ADD */
		cs->duration += cs->timing->time_alu_2;
		tt = *rps;
		tt += *rpd;
		if (tt & (1<<16))
			tc ^= 1;
		t = tt;
		break;
	case 0x0700:	/* AND */
		cs->duration += cs->timing->time_alu_2;
		t = (*rps) & (*rpd);
		break;
	default:
		assert(0 == __LINE__);
	}
	switch((cs->ins >> 6) & 3) {
	case 0:
		break;
	case 1:
		cs->duration += cs->timing->time_alu_shift;
		tt = t;
		tt <<= 1;
		tt |= tc & 1;
		tc = (tt >> 16) & 1;
		t = tt;
		break;
	case 2:
		cs->duration += cs->timing->time_alu_shift;
		tt = t;
		tt |= tc << 16;
		tc = tt & 1;
		t = tt >> 1;
		break;
	case 3:
		cs->duration += cs->timing->time_alu_swap;
		t = bswap16(t);
		break;
	default:
		assert(0 == __LINE__);
	}
	u = 0;
	switch(cs->ins & 7) {
	case 0: break;				/* "   " */
	case 1:	u++; break;			/* SKP */
	case 2: if (!tc) u++; break;		/* SZC */
	case 3: if (tc) u++; break;		/* SNC */
	case 4: if (t == 0) u++; break;		/* SZR */
	case 5: if (t != 0) u++; break;		/* SNR */
	case 6: if (!tc || t == 0) u++; break;	/* SEZ */
	case 7: if (tc && t != 0) u++; break;	/* SBN */
	default: assert(0 == __LINE__);
	}
	if (u) {
		cs->duration += cs->timing->time_alu_skip;
		cs->npc++;
	}
	if (!(cs->ins & 0x8)) {
		*rpd = t;
		cs->carry = tc;
	}
}

/* I/O Instructions --------------------------------------------------*/

static void
Insn_IO(struct rc3600 *cs)
{
	uint16_t *rpd, ioi;
	struct iodev *iop;
	int unit;

	unit = cs->ins & 0x3f;
	ioi = cs->ins & 0xe7c0;
	rpd = &cs->acc[(cs->ins >> 11) & 0x3];
	iop = cs->iodevs[unit];
	AN(iop);

if (iop->cs != cs) printf("XXXX CS %s\n", iop->name);
if (iop->skp_func == NULL) printf("XXXX SKP %s\n", iop->name);
if (iop->io_func == NULL) printf("XXXX IO %s\n", iop->name);

	assert(iop->cs == cs);
	AN(iop->skp_func);
	AN(iop->io_func);

	AZ(pthread_mutex_lock(&iop->mtx));
	if (IO_OPER(ioi) == IO_SKP)
		iop->skp_func(iop, ioi);
	else
		iop->io_func(iop, ioi, rpd);
	AZ(pthread_mutex_unlock(&iop->mtx));
}

static uint16_t
EA(struct rc3600 *cs)
{
	int8_t displ;
	uint16_t t, u;
	int i;

	displ = cs->ins;
	switch((cs->ins >> 8) & 3) {
	case 0:
		t = (uint8_t)displ;
		break;
	case 1:
		t = cs->pc + displ;
		break;
	case 2:
		cs->duration += cs->timing->time_base_reg;
		t = cs->acc[2] + displ;
		break;
	case 3:
		cs->duration += cs->timing->time_base_reg;
		t = cs->acc[3] + displ;
		break;
	default:
		assert(0 == __LINE__);
	}
	if (!cs->ext_core)
		t &= 0x7fff;
	/* @ bit */
	i = cs->ins & 0x400;
	while (i) {
		cs->duration += cs->timing->time_indir_adr;
		if (cs->do_trace > 1)
			trace(cs, "EA 0x%04x = 0x%04x\n", t, core_read(cs, t, CORE_NULL));
		u = core_read(cs, t, CORE_READ | CORE_INDIR);
		if (!cs->ext_core)
			i = u & 0x8000;
		else
			i = 0;
		if (t >= 020 && t <= 027) {
			cs->duration += cs->timing->time_auto_idx;
			core_write(cs, t, ++u, CORE_MODIFY);
		} else if (t >= 030 && t <= 037) {
			cs->duration += cs->timing->time_auto_idx;
			core_write(cs, t, --u, CORE_MODIFY);
		}
		if (!cs->ext_core)
			u &= 0x7fff;
		t = u;
	}
	if (cs->do_trace > 1)
		trace(cs, "EA 0x%04x = 0x%04x\n", t, core_read(cs, t, CORE_NULL));
	return (t);
}

void v_matchproto_(ins_exec_f)
rc3600_exec(struct rc3600 *cs)
{
	uint16_t t, u, *rpd;

	AN(cs);
	AN(cs->core);
	AN(cs->timing);
	assert(cs->pc || cs->ins);

	assert((cs->carry & ~1) == 0);
	switch((cs->ins >> 13) & 7) {
	case 0:	/* DSZ, ISZ, JMP, JSR */
		t = EA(cs);
		switch((cs->ins >> 11) & 3) {
		case 0:	/* JMP */
			cs->duration += cs->timing->time_jmp;
			cs->npc = t;
			break;
		case 1: /* JSR */
			cs->duration += cs->timing->time_jsr;
			cs->acc[3] = cs->npc;
			cs->npc = t;
			break;
		case 2: /* ISZ */
			cs->duration += cs->timing->time_isz;
			u = core_read(cs, t, CORE_READ | CORE_DATA);
			core_write(cs, t, ++u, CORE_MODIFY);
			if (u == 0) {
				cs->duration += cs->timing->time_isz_skp;
				cs->npc++;
			}
			break;
		case 3: /* DSZ */
			cs->duration += cs->timing->time_isz;
			u = core_read(cs, t, CORE_READ | CORE_DATA);
			core_write(cs, t, --u, CORE_MODIFY);
			if (u == 0) {
				cs->duration += cs->timing->time_isz_skp;
				cs->npc++;
			}
			break;
		default:
			assert(0 == __LINE__);
		}
		break;
	case 1:	/* LDA */
		cs->duration += cs->timing->time_lda;
		rpd = &cs->acc[(cs->ins >> 11) & 0x3];
		t = EA(cs);
		*rpd = core_read(cs, t, CORE_READ | CORE_DATA);
		break;
	case 2:	/* STA */
		cs->duration += cs->timing->time_sta;
		rpd = &cs->acc[(cs->ins >> 11) & 0x3];
		t = EA(cs);
		core_write(cs, t, *rpd, CORE_WRITE);
		break;
	case 3:
		Insn_IO(cs);
		break;
	default:
		Insn_ALU(cs);
		break;
	}
}
