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

static void v_matchproto_(ins_exec_f)
cpu_extmem_ena(struct rc3600 *cs)
{
	/*
	 * XXX: Not obvious this check is necessary/valid
	 * XXX: See 13755 in RCSL-52-AA-899
	 */
	if (cs->core_size > 0x8000)
		cs->ext_core |= 1;
}

static void v_matchproto_(ins_exec_f)
cpu_extmem_test(struct rc3600 *cs)
{
	if (cs->ext_core)
		cs->npc++;
}


void
cpu_extmem(struct rc3600 *cs)
{
	unsigned a;

	cs->ins_exec[0x6781] = cpu_extmem_test;
	disass_magic(0x6781, "EXMEM   SKP");
	for (a = 0x0000; a < 0x2000; a += 0x0800) {
		cs->ins_exec[0x65c1 | a] = cpu_extmem_ena;
		disass_magic(0x65c1 | a, "EXMEM  %u,ENA", a >> 11);
	}
}
