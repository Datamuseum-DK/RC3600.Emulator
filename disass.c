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
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#include "rc3600.h"

static const char * const alu[8]	= {"COM", "NEG", "MOV", "INC", "ADC", "SUB", "ADD", "AND" };
static const char * const carry[4]	= {" ", "Z", "O", "C" };
static const char * const shift[4]	= {" ", "L", "R", "S" };
static const char * const skip[8]	= {"    ", ",SKP", ",SZC", ",SNC", ",SZR", ",SNR", ",SEZ", ",SBN" };
static const char * const hash[2]	= {"   ", " # " };
static const char * const jjid[4]	= {"JMP", "JSR", "ISZ", "DSZ" };
static const char * const at[2]	= {"   ", " @ " };
static const char * const ldst[4]	= {NULL, "LDA", "STA", NULL};
static const char * const io[8]	= {"NIO", "DIA", "DOA", "DIB", "DOB", "DIC", "DOC", "SKP" };
static const char * const test[4]	= {"BN", "BZ", "DN", "DZ" };
static const char * const func[4]	= {"  ", "S ", "C ", "P " };

static char *disass_magics[1<<16];

void
disass_magic(uint16_t u, const char *fmt, ...)
{
	va_list ap;
	char buf[DISASS_BUF];

	va_start(ap, fmt);
	assert(vsnprintf(buf, sizeof buf, fmt, ap) <= sizeof buf);
	disass_magics[u] = strdup(buf);
	AN(disass_magics[u]);
}

static void
displ(char *buf, u_int u, int *dsp, const char * const *pz)
{
	int i, d;

	d = (u >> 8) & 3;
	i = u & 0x3ff;

	if (d == 0 && pz != NULL && pz[i] != NULL) {
		strcpy(buf, pz[i]);
		return;
	}

	i = u & 0xff;
	if (d != 0 && i > 0x7f) {
		i -= 256;
		sprintf(buf, "-%02x,%d ", -i, d);
	} else {
		sprintf(buf, "+%02x,%d ", i, d);
	}
	if (d == 1 && dsp != NULL)
		*dsp = i;
}

char *
disass(uint16_t u,
    const char * const *pz, const struct rc3600 *sc, char *buf, int *offset)
{
	int i;
	static char mybuf[DISASS_BUF];
	char iobuf[7];
	const char *iodp = NULL;

	if (buf == NULL)
		buf = mybuf;

	if (disass_magics[u] != NULL) {
		strcpy(buf, disass_magics[u]);
		return (buf);
	}

	if (offset != NULL)
		*offset = Rc3600Disass_NO_OFFSET;

	if (u & 0x8000) {
		strcpy(buf, alu[(u >> 8) & 7]);
		strcat(buf, carry[(u >> 4) & 3]);
		strcat(buf, shift[(u >> 6) & 3]);
		strcat(buf, hash[(u >> 3) & 1]);
		sprintf(buf + strlen(buf), "%d,%d",
		    (u >> 13) & 3, (u >> 11) & 3);
		strcat(buf, skip[u & 7]);
	} else if ((u & 0xe000) == 0x0000) {
		strcpy(buf, jjid[(u >> 11) & 3]);
		strcat(buf, at[(u >> 10) & 1]);
		displ(buf + strlen(buf), u, offset, pz);
		strcat(buf, "  ");
	} else if ((u & 0xe000) == 0x6000) {
		i = u & 0x3f;
		if (sc != NULL && sc->iodevs[i] != NULL)
			iodp = sc->iodevs[i]->name;
		if (iodp == NULL || *iodp == '\0') {
			bprintf(iobuf, "%02x", i);
			iodp = iobuf;
		}
		strcpy(buf, io[(u >> 8) & 7]);
		if (((u >> 8) & 7) == 7) {
			strcat(buf, test[(u >> 6) & 3]);
			sprintf(buf + strlen(buf), "   %s     ", iodp);
		} else {
			strcat(buf, func[(u >> 6) & 3]);
			sprintf(buf + strlen(buf), " %d,%s     ",
			   (u >> 11) & 3, iodp);
		}
	} else {
		strcpy(buf, ldst[(u >> 13) & 3]);
		strcat(buf, at[(u >> 10) & 1]);
		sprintf(buf + strlen(buf), "%d,",
		    (u >> 11) & 3);
		displ(buf + strlen(buf), u, offset, pz);
	}
	return (buf);
}

#ifdef LAGUD_MAIN
int
main(int argc __unused, char **argv __unused)
{
	char buf[40], ascii[2];
	uint16_t u, v, a;
	int i, d;

	for(a = 0; ; a++) {
		i = read(0, &v, sizeof v);
		if (i != sizeof v)
			break;
		i = v & 0xff;
		if (i > 0x20 && i < 0x7f)
			ascii[0] = i;
		else
			ascii[0] = ' ';
		i = v >> 8;
		if (i > 0x20 && i < 0x7f)
			ascii[1] = i;
		else
			ascii[1] = ' ';
		u = be16dec(&v);
		LagudDisass(buf, u, &d);
		printf("%05d %04x %c%c %04x  %s  ", a, a, ascii[1], ascii[0], u, buf);
		if (d != 0)
			printf("->%04x", a + d);
		printf("\n");
	}
	return(0);
}
#endif
