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

#include "rc3600.h"

/*
 * NOVA
 * ----
 *
 * From 014-000631 page F-1 of 4 "NOVA".
 *
 * Matches 015-000009-09 page D12 "Nova".
 *
 * The time_jsr number looks strange, but both sources provide it.
 */
const struct ins_timing nova_timing = {
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

/*
 * NOVA 1200
 * ---------
 *
 * From 014-000631 page F-1 of 4 "1200 SERIES"
 *
 * Matches 015-000009-09 page D12 "1200 Series".
 *
 * Matches RCSL_44_RT_1558_RC3600_INSTRUCTION_TIMER_TEST "NOVA 1200 12"
 */
const struct ins_timing nova1200_timing = {
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

/*
 * NOVA 800
 * --------
 *
 * From 014-000631 page F-1 of 4 "800, 820, 840"
 *
 * Matches 015-000009-09 page D12 "800 Series".
 *
 * No match in RCSL_44_RT_1558_RC3600_INSTRUCTION_TIMER_TEST
 */
const struct ins_timing nova800_timing = {
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

/*
 * NOVA 2
 * ------
 *
 * From 014-000631 page F-1 of 4 "NOVA 2, 16K"
 *
 * No match in 015-000009-09 page D12
 *
 * Matches RCSL_44_RT_1558_RC3600_INSTRUCTION_TIMER_TEST "NOVA 2 - 17K 20"
 * apart from "ISZ CWORK" and "DSZ CWORK" which measure 2100 but expect 2300.
 */

const struct ins_timing nova2_timing = {
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

/*
 * RC3608
 * ------
 *
 * RCSL-42-I-1008 RC3803 CPU Programmer's Guide, p126 "RC3608 32K memory
 *
 * Matches RCSL_44_RT_1558_RC3600_INSTRUCTION_TIMER_TEST "RC3603 - 32K 22"
 * with the deltas seen below.
 */
const struct ins_timing rc3608_timing = {
	.time_lda = 1600,
	.time_sta = 1600,
	.time_isz = 2400,
	.time_jmp = 800,
	.time_jsr = 1250,
	.time_indir_adr = 850 - 50,
	.time_auto_idx = 850 - 50,
	.time_alu_1 = 1150 - 50,
	.time_alu_2 = 1150 - 50,
	.time_alu_skip = 200,
	.time_alu_shift = 300,
	.time_alu_swap = 900,
	.time_io_input = 1850 + 150,
	.time_io_nio = 1700 + 300,
	.time_io_output = 2000 + 150,
	.time_io_skp = 1400,
	.time_io_skp_skip = 200,
	.time_io_scp = 0,
	.time_io_inta = 1850 + 150,
};

/*
 * RC3609
 * ------
 *
 * RCSL-42-I-1008 RC3803 CPU Programmer's Guide, p126 "RC3609 16K memory"
 *
 * Matches RCSL_44_RT_1558_RC3600_INSTRUCTION_TIMER_TEST "RC3603 - 16K 20"
 * with the deltas seen below.
 */
const struct ins_timing rc3609_timing = {
	.time_lda = 1400,
	.time_sta = 1400 + 50,
	.time_isz = 2100 + 50,
	.time_jmp = 700,
	.time_jsr = 1200,
	.time_indir_adr = 750,
	.time_auto_idx = 750 - 50,
	.time_alu_1 = 1000 + 50,
	.time_alu_2 = 1000 + 50,
	.time_alu_skip = 200,
	.time_alu_shift = 300,
	.time_alu_swap = 900,
	.time_io_input = 1810 + 140,
	.time_io_nio = 1700 + 250,
	.time_io_output = 2000 + 100,
	.time_io_skp = 1400 - 50,
	.time_io_skp_skip = 200,
	.time_io_scp = 0,
	.time_io_inta = 1810 + 140,
};
