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

struct elastic_match;

typedef void elastic_deliver_f(void *priv, const void *, size_t);

struct chunk {
	TAILQ_ENTRY(chunk)		next;
	uint8_t				*ptr;
	ssize_t				len;
	ssize_t				read;
};

struct elastic_subscriber {
	TAILQ_ENTRY(elastic_subscriber)	next;
	struct elastic			*ep;
	elastic_deliver_f		*func;
	void				*priv;
	pthread_t			thread;
	int				die;
	pthread_cond_t			cond;
	TAILQ_HEAD(,chunk)		chunks;
};

struct elastic {
	struct rc3600			*cs;
	TAILQ_HEAD(,elastic_subscriber)	subscribers;
	TAILQ_HEAD(,chunk)		chunks_out;
	TAILQ_HEAD(,chunk)		chunks_in;
	int				mode;
	pthread_mutex_t			mtx;
	pthread_cond_t			cond_in;
	pthread_cond_t			cond_out;

	struct elastic_match		*em;
};

struct elastic *elastic_new(struct rc3600 *, int mode);

struct elastic_subscriber *elastic_subscribe(struct elastic *ep, elastic_deliver_f *, void *);
void elastic_unsubscribe(struct elastic *ep, struct elastic_subscriber *);

void elastic_inject(struct elastic *ep, const void *ptr, ssize_t len);

void elastic_put(struct elastic *ep, const void *ptr, ssize_t len);
ssize_t elastic_get(struct elastic *ep, void *ptr, ssize_t len);
int elastic_empty(struct elastic *ep);

typedef int cli_elastic_f(struct elastic *ep, struct cli *);

cli_elastic_f cli_elastic;
cli_elastic_f cli_elastic_tcp;
cli_elastic_f cli_elastic_fd;
cli_elastic_f cli_elastic_match;

void elastic_fd_use(struct elastic *ep, int fd, int mode);