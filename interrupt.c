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

void
intr_raise(struct iodev *iop)
{
	if (!iop->ipen)
		dev_trace(iop, "Raise %s %jd\n",
		    iop->name, iop->cs->ins_count - iop->cs->last_core);
	AZ(pthread_mutex_lock(&iop->cs->run_mtx));
	iop->cs->last_core = iop->cs->ins_count;
	if (!iop->ipen) {
		TAILQ_INSERT_TAIL(&iop->cs->irq_list, iop, irq_list);
		iop->ipen = 1;
	}
	AZ(pthread_cond_signal(&iop->cs->wait_cond));
	AZ(pthread_mutex_unlock(&iop->cs->run_mtx));
}

void
intr_lower(struct iodev *iop)
{

	if (iop->ipen)
		dev_trace(iop, "Lower %s %jd\n",
		    iop->name, iop->cs->ins_count - iop->cs->last_core);
	AZ(pthread_mutex_lock(&iop->cs->run_mtx));
	if (iop->ipen == 1) {
		TAILQ_REMOVE(&iop->cs->irq_list, iop, irq_list);
		iop->ipen = 0;
	} else if (iop->ipen == 2) {
		TAILQ_REMOVE(&iop->cs->masked_irq_list, iop, irq_list);
		iop->ipen = 0;
	}
	AZ(iop->ipen);
	AZ(pthread_mutex_unlock(&iop->cs->run_mtx));
}

struct iodev *
intr_pending(struct rc3600 *cs)
{
	struct iodev *iop;

	// Assumes cs->run_mtx held
	if (!cs->inten[0])
		return (NULL);
	while (!TAILQ_EMPTY(&cs->irq_list)) {
		iop = TAILQ_FIRST(&cs->irq_list);
		if (!((0x8000 >> iop->imask) & cs->imask)) {
			memset(cs->inten, 0, sizeof cs->inten);
			return (iop);
		}
		iop->ipen = 2;
		TAILQ_REMOVE(&cs->irq_list, iop, irq_list);
		TAILQ_INSERT_TAIL(&cs->masked_irq_list, iop, irq_list);
	}
	return (NULL);
}

void
intr_msko(struct rc3600 *cs, uint16_t m)
{
	struct iodev *iop;

	AZ(pthread_mutex_lock(&cs->run_mtx));
	cs->imask = m;
	while (!TAILQ_EMPTY(&cs->masked_irq_list)) {
		iop = TAILQ_FIRST(&cs->masked_irq_list);
		iop->ipen = 1;
		TAILQ_REMOVE(&cs->masked_irq_list, iop, irq_list);
		TAILQ_INSERT_TAIL(&cs->irq_list, iop, irq_list);
	}
	AZ(pthread_mutex_unlock(&cs->run_mtx));
}

uint16_t
intr_inta(struct rc3600 *cs)
{
	struct iodev *iop;
	uint16_t rv = 0;

	AZ(pthread_mutex_lock(&cs->run_mtx));
	iop = TAILQ_FIRST(&cs->irq_list);
	if (iop != NULL)
		rv = iop->devno;
	AZ(pthread_mutex_unlock(&cs->run_mtx));
	return (rv);
}
