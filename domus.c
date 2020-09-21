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

// RCSL-43-GL-7538 MUPAR.01
static const char *domus_call[] = {
	[0006002] = "WAIT",
	[0006003] = "WAITINTERRUPT",
	[0006004] = "SENDMESSAGE",
	[0006005] = "WAITANSWER",
	[0006006] = "WAITEVENT",
	[0006007] = "SENDANSWER",
	[0006010] = "SEARCHITEM",
	[0006011] = "CLEANPROCESS",
	[0006012] = "BREAKPROCESS",
	[0006013] = "STOPPROCESS",
	[0006014] = "STARTPROCESS",
	[0006015] = "RECHAIN",

	[0006164] = "NEXTOPERATION",
	[0006167] = "WAITOPERATION",
	[0006165] = "RETURNANSWER",
	[0006170] = "SETINTERRUPT",
	[0006171] = "SETRESERVATION",
	[0006172] = "SETCONVERSION",
	[0006173] = "CONBYTE",
	[0006174] = "GETBYTE",
	[0006175] = "PUTBYTE",
	[0006176] = "MULTIPLY",
	[0006177] = "DIVIDE",

	[0002164] = ".NEXTOPERATION",
	[0002165] = ".RETURNANSWER",
	[0002166] = ".CLEARDEVICE",
	[0100166] = "CLEAR",
	[0002170] = ".SETINTERRUPT",
	[0002171] = ".SETRESERVATION",
	[0002172] = ".SETCONVERSION",
	[0002173] = ".CONBYTE",
	[0002174] = ".GETBYTE",
	[0002175] = ".PUTBYTE",
	[0002176] = ".MULTIPLY",
	[0002177] = ".DIVIDE",

	[0006232] = "BINDEC",
	[0006233] = "DECBIN",
	[0006200] = "GETREC",
	[0006201] = "PUTREC",
	[0006202] = "WAITTRANSFER",
	[0006204] = "TRANSFER",
	[0006205] = "INBLOCK",
	[0006206] = "OUTBLOCK",
	[0006207] = "INCHAR",
	[0006210] = "FREESHARE",
	[0006211] = "OUTSPACE",
	[0006212] = "OUTCHAR",
	[0006213] = "OUTNL",
	[0006214] = "OUTEND",
	[0006215] = "OUTTEXT",
	[0006216] = "OUTOCTAL",
	[0006217] = "SETPOSITION",
	[0006220] = "CLOSE",
	[0006221] = "OPEN",
	[0006223] = "INNAME",
	[0006222] = "WAITZONE",
	[0006224] = "MOVE",
	[0006225] = "INTERPRETE",

	[0002200] = ".GETREC",
	[0002201] = ".PUTREC",
	[0002202] = ".WAITTRANSFER",
	[0002203] = ".REPEATSHARE",
	[0002204] = ".TRANSFER",
	[0002205] = ".INBLOCK",
	[0002206] = ".OUTBLOCK",
	[0002210] = ".FREESHARE",
	[0002207] = ".INCHAR",
	[0002211] = ".OUTSPACE",
	[0002212] = ".OUTCHAR",
	[0002213] = ".OUTNL",
	[0002214] = ".OUTEND",
	[0002215] = ".OUTTEXT",
	[0002216] = ".OUTOCTAL",
	[0002217] = ".SETPOSITION",
	[0002220] = ".CLOSE",
	[0002221] = ".OPEN",

	[0000226] = "INTGIVEUP",
	[0000230] = "INTBREAK",

	[0006332] = "NEWCAT",
	[0006333] = "FREECAT",

	[0006334] = "CDELAY",
	[0006335] = "WAITSEM",
	[0006336] = "WAITCHAINED",
	[0006337] = "CWANSWER",
	[0006340] = "CTEST",
	[0006341] = "CPRINT",
	[0006342] = "CTOUT",
	[0006343] = "SIGNAL",
	[0006344] = "SIGCHAINED",
	[0006345] = "CPASS",

	[0006346] = "CREATEENTRY",
	[0006347] = "LOOKUPENTRY",
	[0006350] = "CHANGEENTRY",
	[0006351] = "REMOVEENTRY",
	[0006352] = "INITCATALOG",
	[0006353] = "SETENTRY",

	[0006254] = "COMON",
	[0006255] = "CALL",
	[0006256] = "GOTO",
	[0006257] = "GETADR",
	[0006260] = "GETPOINT",
	[0006264] = "CSENDM",
	[0006265] = "SIGGEN",
	[0006266] = "WAITGE",
	[0006267] = "CTOP",
};

static unsigned ndomus_call = sizeof(domus_call) / sizeof(domus_call[0]);

static const char *
getname(struct rc3600 *cs, uint16_t na)
{
	static char buf[7];
	uint16_t u;

	u = core_read(cs, na, CORE_NULL);
	buf[0] = u >> 8;
	buf[1] = u & 0xff;
	u = core_read(cs, na + 1, CORE_NULL);
	buf[2] = u >> 8;
	buf[3] = u & 0xff;
	u = core_read(cs, na + 2, CORE_NULL);
	buf[4] = u >> 8;
	buf[5] = u & 0xff;
	buf[6] = '\0';
	return (buf);
}


static const char *
cur(struct rc3600 *cs)
{
	static char buf[7];
	uint16_t na, u;

	na = core_read(cs, 0x0020, CORE_NULL);
	na += 4;
	u = core_read(cs, na, CORE_NULL);
	buf[0] = u >> 8;
	buf[1] = u & 0xff;
	u = core_read(cs, na + 1, CORE_NULL);
	buf[2] = u >> 8;
	buf[3] = u & 0xff;
	u = core_read(cs, na + 2, CORE_NULL);
	buf[4] = u >> 8;
	buf[5] = u & 0xff;
	buf[6] = '\0';
	return (buf);
}

static char *
ascii(uint16_t u)
{
	static char buf[2];

	if (u < 0x20 || u > 0x7e)
		return "☐";
	buf[0] = u;
	buf[1] = '\0';
	return (buf);
}

static void v_matchproto_(ins_exec_f)
exec_domus(struct rc3600 *cs)
{
	AN(cs);
	assert(cs->ins < ndomus_call);
	AN(domus_call[cs->ins]);
	trace(cs, "DOMUS CUR %-6s %s\n", cur(cs), domus_call[cs->ins]);
	rc3600_exec(cs);
}

static void v_matchproto_(ins_exec_f)
exec_domus_sendm(struct rc3600 *cs)
{
	uint16_t preacc[4];

	memcpy(preacc, cs->acc, sizeof preacc);
	rc3600_exec(cs);
	trace(cs,
	    "DOMUS CUR %-6s SENDMESSAGE  [ %04x %04x %04x %04x ] -> '%s' = %04x\n",
	    cur(cs),
	    core_read(cs, preacc[1] + 0, CORE_NULL),
	    core_read(cs, preacc[1] + 1, CORE_NULL),
	    core_read(cs, preacc[1] + 2, CORE_NULL),
	    core_read(cs, preacc[1] + 3, CORE_NULL),
	    getname(cs, preacc[2]),
	    cs->acc[2]
	);
}

static void v_matchproto_(ins_exec_f)
exec_domus_inchar(struct rc3600 *cs)
{
	rc3600_exec(cs);
	trace(cs,
	    "DOMUS CUR %-6s INCHAR Z=%04x -> 0x%02x '%s'\n",
	    cur(cs),
	    cs->acc[2],
	    cs->acc[1],
	    ascii(cs->acc[1] & 0xff)
	);
}

static ins_exec_f *domus_exec[] = {
	[0006004] = exec_domus_sendm,
	[0006207] = exec_domus_inchar,
};

static unsigned ndomus_exec = sizeof(domus_exec) / sizeof(domus_exec[0]);

void v_matchproto_(cli_func_f)
cli_domus(struct cli *cli)
{
	struct rc3600 *cs;
	unsigned u;

	AN(cli);
	cs = cli->cs;
	AN(cs);
	cli->ac--;
	cli->av++;
	for (u = 0; u < ndomus_call || u < ndomus_exec; u++) {
		if (u < ndomus_exec && domus_exec[u] != NULL)
			cs->ins_exec[u] = domus_exec[u];
		else if (u < ndomus_call && domus_call[u] != NULL)
			cs->ins_exec[u] = exec_domus;
	}
}
