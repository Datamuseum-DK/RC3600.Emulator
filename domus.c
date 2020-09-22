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
static const char * const domus_call[] = {
	[0006002] = "WAIT",
	[0006003] = "WAITINTERRUPT",
	[0006004] = "SENDMESSAGE|1M2N",
	[0006005] = "WAITANSWER",
	[0006006] = "WAITEVENT",
	[0006007] = "SENDANSWER",
	[0006010] = "SEARCHITEM|2N",
	[0006011] = "CLEANPROCESS|2P",
	[0006012] = "BREAKPROCESS|2P",
	[0006013] = "STOPPROCESS|2P",
	[0006014] = "STARTPROCESS|2P",
	[0006015] = "RECHAIN",

	[0006164] = "NEXTOPERATION",
	[0006167] = "WAITOPERATION",
	[0006165] = "RETURNANSWER",
	[0006170] = "SETINTERRUPT",
	[0006171] = "SETRESERVATION",
	[0006172] = "SETCONVERSION",
	[0006173] = "CONBYTE",
	[0006174] = "GETBYTE|1B",
	[0006175] = "PUTBYTE|0b",
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
	[0002174] = ".GETBYTE|1B",
	[0002175] = ".PUTBYTE|0b",
	[0002176] = ".MULTIPLY",
	[0002177] = ".DIVIDE",

	[0006232] = "BINDEC",
	[0006233] = "DECBIN",
	[0006200] = "GETREC|2Z",
	[0006201] = "PUTREC|2Z",
	[0006202] = "WAITTRANSFER|2Z",
	[0006204] = "TRANSFER|2Z",
	[0006205] = "INBLOCK|2Z",
	[0006206] = "OUTBLOCK|2Z",
	[0006207] = "INCHAR|2Z",
	[0006210] = "FREESHARE",
	[0006211] = "OUTSPACE|2Z",
	[0006212] = "OUTCHAR|2Z1b",
	[0006213] = "OUTNL|2Z",
	[0006214] = "OUTEND|2Z1b",
	[0006215] = "OUTTEXT|2Z0s",
	[0006216] = "OUTOCTAL|2Z",
	[0006217] = "SETPOSITION|2Z",
	[0006220] = "CLOSE|2Z",
	[0006221] = "OPEN|2Z",
	[0006223] = "INNAME|2Z",
	[0006222] = "WAITZONE|2Z",
	[0006224] = "MOVE",
	[0006225] = "INTERPRETE",
	[0006236] = "TAKEA",
	[0006237] = "TAKEV",

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

	[0006332] = "NEWCAT|2Z",
	[0006333] = "FREECAT|2Z",

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

	[0006346] = "CREATEENTRY|2Z",
	[0006347] = "LOOKUPENTRY|2Z",
	[0006350] = "CHANGEENTRY|2Z",
	[0006351] = "REMOVEENTRY|2Z",
	[0006352] = "INITCATALOG|2Z",
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

static const unsigned ndomus_call = sizeof(domus_call) / sizeof(domus_call[0]);

static uint16_t
getbyte(struct rc3600 *cs, uint16_t baddr)
{
	uint16_t u;

	u = core_read(cs, baddr >> 1, CORE_READ);
	if (baddr & 1)
		return (u & 0xff);
	return (u >> 8);
}

static char *
ascii(char *p, const char *e, uint16_t a)
{
	a &= 0xff;
	switch (a) {
	case '\0':
		p += snprintf(p, e - p, "\\0");
		return (p);
	case '\n':
		p += snprintf(p, e - p, "\\n");
		return (p);
	case '\r':
		p += snprintf(p, e - p, "\\r");
		return (p);
	default:
		if (0x20 < a && a < 0x7f)
			p += snprintf(p, e - p, "%c", a);
		else
			p += snprintf(p, e - p, "\\x%02x", a);
		return (p);
	}
}

static char *
wname(struct rc3600 *cs, char *p, const char *e, uint16_t a)
{
	int i;
	char *q = p;
	uint16_t u;

	*p++ = '"';
	for (i = 0; i < 3; i++) {
		u = core_read(cs, a, CORE_NULL);
		if (!(u >> 8))
			break;
		p = ascii(p, e, u >> 8);
		if (!(u & 0xff))
			break;
		p = ascii(p, e, u);
		a++;
	}
	*p++ = '"';
	while (p < q + 8)
		*p++ = ' ';
	return (p);
}

static void v_matchproto_(ins_exec_f)
exec_domus(struct rc3600 *cs)
{
	char buf[BUFSIZ], *p, *e;
	const char *n;
	uint16_t v, w;

	AN(cs);
	assert(cs->ins < ndomus_call);
	n = domus_call[cs->ins];
	AN(n);
	p = buf;
	e = buf + sizeof buf;
	p += snprintf(p, e - p, "DOMUS 0x%04x 0x%04x 0x%04x ",
	    cs->acc[0], cs->acc[1], cs->acc[2]);
	v = core_read(cs, 0x0020, CORE_NULL);
	p = wname(cs, p, e, v + 4);
	*p++ = ' ';
	while (*n != '\0' && *n != '|')
		*p++ = *n++;
	*p = '\0';
	for (;*n != '\0';n++) {
		switch (*n) {
		case '|':
			break;
		case '0': v = cs->acc[0]; break;
		case '1': v = cs->acc[1]; break;
		case '2': v = cs->acc[2]; break;
		case '3': v = cs->acc[3]; break;
		case 'b':
			p += snprintf(p, e - p, " b 0x%02x '", v);
			p = ascii(p, e, v);
			p += snprintf(p, e - p, "'");
			break;
		case 'B':
			w = getbyte(cs, v);
			p += snprintf(p, e - p, " B 0x%02x '", w);
			p = ascii(p, e, w);
			p += snprintf(p, e - p, "'");
			break;
		case 'M':
			p += snprintf(p, e - p,
			    " M [ 0x%04x 0x%04x 0x%04x 0x%04x ]",
			    core_read(cs, v, CORE_NULL),
			    core_read(cs, v + 1, CORE_NULL),
			    core_read(cs, v + 2, CORE_NULL),
			    core_read(cs, v + 3, CORE_NULL)
			);
			break;
		case 'N':
			p += snprintf(p, e - p, " N ");
			p = wname(cs, p, e, v);
			break;
		case 'P':
			p += snprintf(p, e - p, " P ");
			p = wname(cs, p, e, v + 4);
			break;
		case 's':
			p += snprintf(p, e - p, " s  \"");
			while (1) {
				w = getbyte(cs, v++);
				if (!w)
					break;
				p = ascii(p, e, w);
			}
			p += snprintf(p, e - p, "\"");
			break;
		case 'Z':
			p += snprintf(p, e - p, " Z 0x%04x ", v);
			p = wname(cs, p, e, v);
			break;
		default:
			assert(0 == __LINE__);
		}
	}
	*p = '\0';
	trace(cs, "%s\n", buf);
	rc3600_exec(cs);
}

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
	for (u = 0; u < ndomus_call; u++) {
		if (domus_call[u] != NULL)
			cs->ins_exec[u] = exec_domus;
	}
}
