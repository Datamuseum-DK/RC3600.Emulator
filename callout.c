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
#include "rc3600.h"

struct callout {
	TAILQ_ENTRY(callout)		next;
	struct rc3600			*cs;
	nanosec				when;
	const struct callout_how	*how;
	void				*priv;
};

typedef void callout_func_f(const struct callout *);

struct callout_how {
	const char			*name;
	callout_func_f			*func;
};

/**********************************************************************/

nanosec
now(void)
{
	struct timespec ts;

	AZ(clock_gettime(CLOCK_REALTIME_FAST, &ts));
	return (ts.tv_sec * 1000000000L + ts.tv_nsec);
}

/**********************************************************************/

static void
callout_func_wake_dev(const struct callout *co)
{
	struct iodev *iop;

	iop = co->priv;
	AZ(pthread_cond_signal(&iop->sleep_cond));
}

static const struct callout_how callout_wake_dev_how = {
	.name =				"Wake Device",
	.func =				callout_func_wake_dev,
};

#if 0
static const struct callout_how callout_callback = {
	.name =				"Callback",
};
#endif

static void
callout_insert(struct callout *co)
{
	struct callout *co2;

	AZ(pthread_mutex_lock(&co->cs->callout_mtx));
	TAILQ_FOREACH(co2, &co->cs->callouts, next)
		if (co2->when > co->when)
			break;
	if (co2 == NULL)
		TAILQ_INSERT_TAIL(&co->cs->callouts, co, next);
	else
		TAILQ_INSERT_BEFORE(co2, co, next);
	AZ(pthread_mutex_unlock(&co->cs->callout_mtx));
}

static struct callout *
callout_wake_dev_rel(struct iodev *iop, nanosec when)
{
	struct callout *co;

	co = calloc(sizeof *co, 1);
	AN(co);
	co->cs = iop->cs;
	AZ(pthread_mutex_lock(&co->cs->run_mtx));
	co->when = when + co->cs->sim_time;
	AZ(pthread_mutex_unlock(&co->cs->run_mtx));
	co->priv = iop;
	co->how = &callout_wake_dev_how;
	callout_insert(co);
	return (co);
}

void
callout_dev_sleep(struct iodev *iop, nanosec when)
{
	struct callout *co;

	AZ(pthread_mutex_lock(&iop->mtx));
	co = callout_wake_dev_rel(iop, when);
	AN(co);
	AZ(pthread_cond_wait(&iop->sleep_cond, &iop->mtx));
	AZ(pthread_mutex_unlock(&iop->mtx));
}

void
callout_dev_sleep_locked(struct iodev *iop, nanosec when)
{
	struct callout *co;

	co = callout_wake_dev_rel(iop, when);
	AN(co);
	AZ(pthread_cond_wait(&iop->sleep_cond, &iop->mtx));
}

/**********************************************************************/

static void
callout_func_dev_is_done(const struct callout *co)
{
	struct iodev *iop;

	iop = co->priv;
	AZ(pthread_mutex_lock(&iop->mtx));
	if (iop->busy) {
		iop->busy = 0;
		iop->done = 1;
		intr_raise(iop);
	}
	AZ(pthread_mutex_unlock(&iop->mtx));
}

static const struct callout_how callout_dev_is_done_how = {
	.name =				"Device is Done",
	.func =				callout_func_dev_is_done,
};

void
callout_dev_is_done_abs(struct iodev *iop, nanosec when)
{
	struct callout *co;

	co = calloc(sizeof *co, 1);
	AN(co);
	co->cs = iop->cs;
	co->when = when;
	co->priv = iop;
	co->how = &callout_dev_is_done_how;
	callout_insert(co);
}

void
callout_dev_is_done(struct iodev *iop, nanosec when)
{
	struct callout *co;

	co = calloc(sizeof *co, 1);
	AN(co);
	co->cs = iop->cs;
	AZ(pthread_mutex_lock(&co->cs->run_mtx));
	co->when = when + co->cs->sim_time;
	AZ(pthread_mutex_unlock(&co->cs->run_mtx));
	co->priv = iop;
	co->how = &callout_dev_is_done_how;
	callout_insert(co);
}

/**********************************************************************/

nanosec
callout_poll(struct rc3600 *cs)
{
	struct callout *co;
	nanosec rv = 0;

	while (1) {
		AZ(pthread_mutex_lock(&cs->callout_mtx));
		co = TAILQ_FIRST(&cs->callouts);
		if (co != NULL && co->when < cs->sim_time) {
			TAILQ_REMOVE(&cs->callouts, co, next);
		} else {
			if (co != NULL)
				rv = co->when;
			co = NULL;
		}
		AZ(pthread_mutex_unlock(&cs->callout_mtx));
		if (co == NULL)
			return (rv);
		co->how->func(co);
		free(co);
	}
}
