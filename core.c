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

#include <stdlib.h>
#include <stdint.h>

#include "rc3600.h"

struct loc {
	uint16_t		core;
	char			ins[DISASS_BUF];
};

struct core {
	struct loc		loc[1<<16];
	unsigned		size;
	struct core_handlers	handlers;
	pthread_mutex_t		mtx;
};

struct core *
core_new(unsigned size)
{
	struct core *cp;

	assert(size <= 1<<16);
	cp = calloc(sizeof *cp, 1);
	AN(cp);
	TAILQ_INIT(&cp->handlers);
	cp->size = size;
	AZ(pthread_mutex_init(&cp->mtx, NULL));
	return (cp);
}

void
core_setsize(struct core *cp, unsigned siz)
{
	cp->size = siz;
}

const char *
core_disass(const struct rc3600 *cs, uint16_t addr)
{
	if (*cs->core->loc[addr].ins == '\0') {
		(void)disass(
		    cs->core->loc[addr].core,
		    NULL,
		    cs,
		    cs->core->loc[addr].ins,
		    NULL
		);
	}
	return (cs->core->loc[addr].ins);
}

uint16_t *
core_ptr(const struct rc3600 *cs, uint16_t addr)
{
	cs->core->loc[addr].ins[0] = '\0';
	return (&cs->core->loc[addr].core);
}

uint16_t
core_read(struct rc3600 *cs, uint16_t addr, int how)
{
	uint16_t rv;
	int i;
	struct core_handler *ch;

	AN(cs);
	AN(how);
	if (addr >= cs->core->size)
		return (0);
	AZ(pthread_mutex_lock(&cs->core->mtx));
	rv = cs->core->loc[addr].core;
	TAILQ_FOREACH(ch, &cs->core->handlers, next) {
		if (ch->read_func == NULL)
			continue;
		i = ch->read_func(cs, addr, &rv, how);
		if (i > 0)
			break;
	}
	if (!(how & (CORE_NULL | CORE_INS)))
		cs->last_core = cs->ins_count;
	AZ(pthread_mutex_unlock(&cs->core->mtx));
	return (rv);
}

void
core_write(struct rc3600 *cs, uint16_t addr, uint16_t val, int how)
{
	int i;
	struct core_handler *ch;

	AN(cs);
	AN(how);
	AZ(pthread_mutex_lock(&cs->core->mtx));
	TAILQ_FOREACH(ch, &cs->core->handlers, next) {
		if (ch->write_func == NULL)
			continue;
		i = ch->write_func(cs, addr, &val, how);
		if (i > 0)
			break;
	}
	cs->last_core = cs->ins_count;
	cs->core->loc[addr].core = val;
	cs->core->loc[addr].ins[0] = '\0';
	AZ(pthread_mutex_unlock(&cs->core->mtx));
}
