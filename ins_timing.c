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
#include "rc3600.h"

static const struct ins_timing nova_timing = {
	.model = "NOVA",
	.time_lda = 5200,
	.time_sta = 5500,
	.time_isz = 5200,
	.time_jmp = 5600,
	.time_jsr = 3500,
	.time_indir_adr = 2600,
	.time_base_reg = 300,
	.time_alu_1 = 5600,
	.time_alu_2 = 5900,
	.time_io_input = 4400,
	.time_io_nio = 4400,
	.time_io_output = 4700,
	.time_io_skp = 4400,
	.time_io_inta = 4400,
};

/* RCSL_44_RT_1558_RC3600_INSTRUCTION_TIMER_TEST type 12 */
static const struct ins_timing nova1200_timing = {
	.model = "NOVA 1200",
	.time_lda = 2550,
	.time_sta = 2550,
	.time_isz = 3150,
	.time_isz_skp = 1350,
	.time_jmp = 1350,
	.time_jsr = 1350,
	.time_indir_adr = 1200,
	.time_auto_idx = 600,
	.time_alu_1 = 1350,
	.time_alu_2 = 1350,
	.time_alu_skip = 1350,
	.time_io_input = 2550,
	.time_io_nio = 3150,
	.time_io_output = 3150,
	.time_io_skp = 2550,
	.time_io_inta = 2550,
};

static const struct ins_timing nova800_timing = {
	.model = "NOVA 800",
	.time_lda = 1600,
	.time_sta = 1600,
	.time_isz = 1800,
	.time_jmp = 800,
	.time_jsr = 800,
	.time_indir_adr = 800,
	.time_auto_idx = 200,
	.time_alu_1 = 800,
	.time_alu_2 = 800,
	.time_alu_skip = 200,
	.time_io_input = 2200,
	.time_io_nio = 2200,
	.time_io_output = 2200,
	.time_io_scp = 600,
	.time_io_skp = 1400,
	.time_io_skp_skip = 200,
	.time_io_inta = 2200,
};

static const struct ins_timing nova2_timing = {
	.model = "NOVA 2",
	.time_lda = 2000,
	.time_sta = 2000,
	.time_isz = 2100,
	.time_jmp = 1000,
	.time_jsr = 1200,
	.time_indir_adr = 1000,
	.time_auto_idx = 500,
	.time_alu_1 = 1000,
	.time_alu_2 = 1000,
	.time_alu_skip = 200,
	.time_io_input = 1500,
	.time_io_nio = 1700,
	.time_io_output = 1700,
	.time_io_skp = 1200,
	.time_io_skp_skip = 200,
	.time_io_scp = 300,
	.time_io_inta = 1500,
};


/* RCSL-42-I-1008 RC3803 CPU Programmer's Guide, p126 */
static const struct ins_timing rc3608_timing = {
	.model = "RC3608",
	.time_lda = 1600,
	.time_sta = 1600,
	.time_isz = 2400,
	.time_jmp = 800,
	.time_jsr = 1250,
	.time_indir_adr = 850,
	.time_auto_idx = 850,
	.time_alu_1 = 1150,
	.time_alu_2 = 1150,
	.time_alu_skip = 200,
	.time_alu_shift = 300,
	.time_alu_swap = 900,
	.time_io_input = 1850,
	.time_io_nio = 1700,
	.time_io_output = 2000,
	.time_io_skp = 1400,
	.time_io_skp_skip = 200,
	.time_io_scp = 0,
	.time_io_inta = 1850,
};

/* RCSL-42-I-1008 RC3803 CPU Programmer's Guide, p126 */
static const struct ins_timing rc3609_timing = {
	.model = "RC3609",
	.time_lda = 1400,
	.time_sta = 1400,
	.time_isz = 2100,
	.time_jmp = 700,
	.time_jsr = 1200,
	.time_indir_adr = 750,
	.time_auto_idx = 750,
	.time_alu_1 = 1000,
	.time_alu_2 = 1000,
	.time_alu_skip = 200,
	.time_alu_shift = 300,
	.time_alu_swap = 900,
	.time_io_input = 1810,
	.time_io_nio = 1700,
	.time_io_output = 2000,
	.time_io_skp = 1400,
	.time_io_skp_skip = 200,
	.time_io_scp = 0,
	.time_io_inta = 1810,
};

const struct ins_timing * const ins_timings[] = {
	&rc3609_timing,
	&rc3608_timing,
	&nova_timing,
	&nova1200_timing,
	&nova800_timing,
	&nova2_timing,
	NULL
};

int
ins_timing_check(void)
{
	int i, rv = 0;

	for (i = 0; ins_timings[i] != NULL; i++) {
#define TIMING_MACRO(fld, x) \
	if (x && ins_timings[i]->fld == 0) { \
		printf("%s lacks %s timing\n", ins_timings[i]->model, #fld); \
		rv++; \
	}
	TIMINGS
#undef TIMING_MACRO
	}
	return (rv);
}
