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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rc3600.h"
#include "vav.h"

void
trace_state(struct rc3600 *cs)
{
	char buf[BUFSIZ];

	AN(cs);
	if (cs->fd_trace < 0)
		return;
	bprintf(buf,
	    "I %jd %d %04x %04x %06o  %04x %04x %04x %04x %c"
	    // " s%.6f r%.6f d%.6f"
	    " w%8ju %s\n",
	    cs->ins_count,
	    cs->inten[0],
	    cs->pc,
	    core_read(cs, cs->pc, CORE_NULL),
	    core_read(cs, cs->pc, CORE_NULL),
	    cs->acc[0],
	    cs->acc[1],
	    cs->acc[2],
	    cs->acc[3],
	    cs->carry ? 'C' : '.',
	    // cs->sim_time * 1e-9,
	    // cs->real_time * 1e-9,
	    // (cs->sim_time - cs->real_time) * 1e-9,
	    cs->ins_count - cs->last_core,
	    core_disass(cs, cs->pc)
	);
	(void)write(cs->fd_trace, buf, strlen(buf));
}

void
trace(const struct rc3600 *cs, const char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZ];

	AN(cs);
	if (cs->fd_trace < 0)
		return;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	(void)write(cs->fd_trace, buf, strlen(buf));
}

void
dev_trace(const struct iodev *iop, const char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZ];

	AN(iop);
	if (iop->cs->fd_trace < 0 || !iop->trace)
		return;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	(void)write(iop->cs->fd_trace, buf, strlen(buf));
	va_end(ap);
}

/**********************************************************************/

nanosec
now(void)
{
	struct timespec ts;

	AZ(clock_gettime(CLOCK_REALTIME_FAST, &ts));
	return (ts.tv_sec * 1000000000L + ts.tv_nsec);
}

/**********************************************************************/

int
cli_dev_trace(struct iodev *iop, struct cli *cli)
{

	if (cli->ac < 1 || strcasecmp(*cli->av, "trace"))
		return (0);
	if (cli_n_args(cli, 1))
		return (1);
	iop->trace = atoi(cli->av[1]);
	cli->ac -= 2;
	cli->av += 2;
	return (1);
}

void
install_dev(struct iodev *iop, iodev_thr *thr)
{
	struct rc3600 *cs;

	AN(iop);
	cs = iop->cs;
	AN(cs);
	AN(iop->unit);
	AZ(cs->iodevs[iop->unit]);
	AN(iop->imask);
	AZ(pthread_mutex_init(&iop->mtx, NULL));
	AZ(pthread_cond_init(&iop->cond, NULL));
	AZ(pthread_cond_init(&iop->sleep_cond, NULL));
	iop->cs = cs;
	if (thr != NULL)
		AZ(pthread_create(&iop->thread, NULL, thr, iop));
	cs->iodevs[iop->unit] = iop;
}

/**********************************************************************/

static void
cpu_start(struct rc3600 *cs)
{

	if (cs->running)
		return;
	AZ(pthread_mutex_lock(&cs->run_mtx));
	cs->running = 1;
	AZ(pthread_mutex_unlock(&cs->run_mtx));
	AZ(pthread_cond_signal(&cs->run_cond));
}

static void
cpu_stop(struct rc3600 *cs)
{

	if (!cs->running)
		return;
	AZ(pthread_mutex_lock(&cs->run_mtx));
	cs->running = 0;
	AZ(pthread_mutex_lock(&cs->running_mtx));
	AZ(pthread_mutex_unlock(&cs->running_mtx));
	AZ(pthread_mutex_unlock(&cs->run_mtx));
}

static void
cpu_instr(struct rc3600 *cs)
{
	if (cs->do_trace)
		trace_state(cs);
	rc3600_exec(cs);
}

static void *
cpu_thread(void *priv)
{
	struct rc3600 *cs = priv;
	struct iodev *iop;
	int time_step;
	nanosec pace;
	nanosec next_tmo;
	struct timespec ts;

	AZ(pthread_mutex_lock(&cs->run_mtx));
	while (1) {
		time_step = !cs->running;
		while (!cs->running)
			AZ(pthread_cond_wait(&cs->run_cond, &cs->run_mtx));
		AZ(pthread_mutex_lock(&cs->running_mtx));
		cs->real_time = now();
		if (time_step)
			cs->sim_time = cs->real_time;

		next_tmo = 0;
		iop = intr_pending(cs);
		AZ(pthread_mutex_unlock(&cs->run_mtx));
		callout_poll(cs);
		if (iop != NULL) {
			dev_trace(iop, "INTERRUPT %s 0x%02x\n",
			    iop->name, iop->unit);
			core_write(cs, 0, cs->pc, CORE_WRITE);
			cs->pc = core_read(cs, 1, CORE_READ | CORE_INDIR);
		}
		if (cs->do_trace)
			trace_state(cs);
		cs->duration = 0;
		cs->ins_count++;
		rc3600_exec(cs);
		cs->sim_time += cs->duration;

		AZ(pthread_mutex_unlock(&cs->running_mtx));
		pace = 0;
if (0) {
		if (cs->pc < 0x100 && core_read(cs, cs->pc, CORE_NULL) == cs->pc) {
			if (cs->ins_count - cs->last_core < 590)
				cs->last_core = cs->ins_count - 590;
		}
		if (cs->ins_count - cs->last_core > 600 && next_tmo > 0)
			pace = next_tmo - cs->real_time;
		else if (cs->ins_count - cs->last_core > 600)
			pace = 1000000;
		else if (cs->sim_time > cs->real_time + 1000000)
			pace = 1000000;
		else
			pace = 0;
		if (pace)
			trace(cs, "Pace %jd %jd\n", pace, next_tmo);
} else {
		if (cs->sim_time > cs->real_time + 1000000)
			pace = 1000000;
}
		AZ(pthread_mutex_lock(&cs->run_mtx));
		if (pace > 0) {
			pace += cs->real_time;
			ts.tv_sec = pace / 1000000000;
			ts.tv_nsec = pace % 1000000000;
			(void)pthread_cond_timedwait(&cs->wait_cond, &cs->run_mtx, &ts);
			// cs->sim_time = now();
			cs->last_core = cs->ins_count;
		}

	}
}

/**********************************************************************/

static uint16_t
to_word(struct cli *cli, const char *src)
{
	char *p;
	unsigned long ul;

	p = NULL;
	ul = strtoul(src, &p, 0);
	if (p == NULL || *p != '\0' || ul > 0xffff)
		return (cli_error(cli, "Bad <word> argument '%s'\n", src));
	return (ul & 0xffff);
}

static void
show_word(const char *pfx, uint16_t val)
{
	printf("%s 0x%04x 0%06o\n", pfx, val, val);
}

/**********************************************************************/

static void v_matchproto_(cli_func_t)
cli_stop(struct cli *cli)
{

	if (!cli_n_args(cli, 0))
		cpu_stop(cli->cs);
}

static void v_matchproto_(cli_func_t)
cli_start(struct cli *cli)
{

	if (!cli_n_args(cli, 0))
		cpu_start(cli->cs);
}

static void v_matchproto_(cli_func_t)
cli_step(struct cli *cli)
{

	if (!cli_n_args(cli, 0)) {
		cpu_stop(cli->cs);
		cpu_instr(cli->cs);
	}
}

static void v_matchproto_(cli_func_t)
cli_autoload(struct cli *cli)
{

	if (!cli_n_args(cli, 0)) {
		cpu_stop(cli->cs);
		AutoRom(cli->cs);
		cli->cs->pc = 0;
		cpu_start(cli->cs);
	}
}

/**********************************************************************/

static void v_matchproto_(cli_func_t)
cli_switches(struct cli *cli)
{
	int i;

	if (cli->ac > 2) {
		cli_printf(cli,
		    "Expected only optional <word> argument after %s\n",
		    cli->av[0]);
		return;
	}
	if (cli->ac == 2) {
		i = to_word(cli, cli->av[1]);
		if (cli->status)
			return;
		cli->cs->switches = i;
	}
	show_word("SWITCHES", cli->cs->switches);
}

/**********************************************************************/

static void
exam_deposit_what(struct cli *cli,
    uint16_t **dst, const char **fld, const char *what)
{
	int i;

	if (!strcasecmp(what, "ac0")) {
		*fld = "AC0";
		*dst = &cli->cs->acc[0];
	} else if (!strcasecmp(what, "ac1")) {
		*fld = "AC1";
		*dst = &cli->cs->acc[0];
	} else if (!strcasecmp(what, "ac2")) {
		*fld = "AC2";
		*dst = &cli->cs->acc[0];
	} else if (!strcasecmp(what, "ac3")) {
		*fld = "AC3";
		*dst = &cli->cs->acc[0];
	} else if (!strcasecmp(what, "pc")) {
		*fld = "PC";
		*dst = &cli->cs->ins;
	} else if (!strcasecmp(what, "carry")) {
		*fld = "CARRY";
		*dst = &cli->cs->carry;
	} else {
		i = to_word(cli, what);
		if (cli->status)
			return;
		*fld = "MEM";
		*dst = core_ptr(cli->cs, i);
	}
}

static void v_matchproto_(cli_func_t)
cli_examine(struct cli *cli)
{
	const char *fld;
	uint16_t *dst;

	if (cli->ac != 2) {
		cli_printf(cli,
		    "USAGE %s {ac0|ac1|ac2|ac3|pc|carry|<word>}\n", cli->av[0]);
		return;
	}
	exam_deposit_what(cli, &dst, &fld, cli->av[1]);
	if (!cli->status)
		show_word(fld, *dst);
}

static void v_matchproto_(cli_func_t)
cli_deposit(struct cli *cli)
{
	int j;
	const char *fld;
	uint16_t *dst;

	if (cli->ac != 3) {
		cli_printf(cli,
		    "USAGE %s {ac0|ac1|ac2|ac3|pc|carry|<word>} <word>\n",
		    cli->av[0]);
		return;
	}
	exam_deposit_what(cli, &dst, &fld, cli->av[1]);
	if (cli->status)
		return;
	j = to_word(cli, cli->av[2]);
	if (cli->status)
		return;
	*dst = j;
	show_word(fld, *dst);
}

static void v_matchproto_(cli_func_t)
cli_help(struct cli *cli)
{
	cli_printf(cli, "... I need somebody's...\n");
}

/**********************************************************************/

void
cli_printf(struct cli *cli, const char *fmt, ...)
{
	va_list ap;

	(void)cli;

	va_start(ap, fmt);
	(void)vprintf(fmt, ap);
	va_end(ap);
}

int
cli_error(struct cli *cli, const char *fmt, ...)
{
	va_list ap;

	cli->status = 1;

	va_start(ap, fmt);
	(void)vprintf(fmt, ap);
	va_end(ap);
	return (1);
}

void
cli_unknown(struct cli *cli)
{

	cli_printf(cli, "Unknown argument '%s'\n", cli->av[0]);
	cli->status = 1;
}

int
cli_n_args(struct cli *cli, int n)
{
	if (cli->ac == n + 1)
		return (0);
	if (n == 0)
		return (cli_error(cli, "Expected no arguments after '%s'\n",
		    cli->av[0]));
	return(cli_error(cli, "Expected %d arguments after '%s'\n",
	    n, cli->av[0]));
}

static const struct cli_cmds {
	const char	*cmd;
	cli_func_f	*func;
} cli_cmds[] = {
	{ "help",	cli_help },
	{ "switches",	cli_switches },
	{ "switch",	cli_switches },
	{ "examine",	cli_examine },
	{ "x",		cli_examine },
	{ "deposit",	cli_deposit },
	{ "d",		cli_deposit },
	{ "stop",	cli_stop },
	{ "start",	cli_start },
	{ "step",	cli_step },
	{ "s",		cli_step },
	{ "autoload",	cli_autoload },
	// reset
	// continue (differs from start how ?)

	{ "tty",	cli_tty },
	{ "dkp",	cli_dkp },
	{ "rtc",	cli_rtc },
	{ "ptp",	cli_ptp },
	{ "ptr",	cli_ptr },
	{ "fdd",	cli_fdd },
	{ "amx",	cli_amx },
	{ NULL,		NULL },
};

static int
cli_exec(struct rc3600 *cs, const char *s)
{
	int ac;
	char **av;
	const struct cli_cmds *cc;
	struct cli cli;

	av = VAV_Parse(s, &ac, ARGV_COMMENT);
	AN(av);
	if (av[0] != NULL) {
		printf("CLI error: %s\n", av[0]);
		VAV_Free(av);
		return (1);
	}
	if (av[1] == NULL) {
		VAV_Free(av);
		return (0);
	}
	for (cc = cli_cmds; cc->cmd != NULL; cc++)
		if (!strcasecmp(cc->cmd, av[1]))
			break;
	if (cc->cmd == NULL) {
		printf("CLI error: no command '%s'\n", av[1]);
		VAV_Free(av);
		return (1);
	}

	memset(&cli, 0, sizeof cli);
	cli.cs = cs;
	cli.ac = ac - 1;
	cli.av = av + 1;
	AN(cc->func);
	cc->func(&cli);
	VAV_Free(av);
	return (cli.status);
}

static struct rc3600 *
cpu_new(void)
{
	struct rc3600 *cs;

	cs = calloc(1, sizeof *cs);
	AN(cs);

	AZ(pthread_mutex_init(&cs->run_mtx, NULL));
	AZ(pthread_mutex_init(&cs->running_mtx, NULL));
	AZ(pthread_cond_init(&cs->run_cond, NULL));
	AZ(pthread_cond_init(&cs->wait_cond, NULL));
	AZ(pthread_mutex_init(&cs->callout_mtx, NULL));

	cs->timing = ins_timings[3];
	cs->core_size = 0x8000;
	cs->core = core_new(cs->core_size);

	iodev_cpu.cs = cs;
	cs->iodevs[0x3f] = &iodev_cpu;

	iodev_cpu721.cs = cs;
	//cs->iodevs[0x01] = &iodev_cpu721;
	//cs->iodevs[0x02] = &iodev_cpu721;

	iodev_cpu.init_func(&iodev_cpu);

	TAILQ_INIT(&cs->irq_list);
	TAILQ_INIT(&cs->masked_irq_list);
	TAILQ_INIT(&cs->units_list);
	TAILQ_INIT(&cs->callouts);
	cs->fd_trace = -1;

	AZ(pthread_create(&cs->cthread, NULL, cpu_thread, cs));

	return (cs);
}

int
main(int argc, char **argv)
{
	int ch, i;
	char buf[BUFSIZ];
	int bare = 0;
	struct rc3600 *cs;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	cs = cpu_new();
	AN(cs);

	AZ(ins_timing_check());


	while ((ch = getopt(argc, argv, "b:htT:")) != -1) {
		switch (ch) {
		case 'b':
			bare = 1;
			break;
		case 't':
			cs->do_trace++;
			break;
		case 'T':
			cs->fd_trace =
			    open(optarg, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (cs->fd_trace < 0) {
				fprintf(stderr, "Cannot open: %s: %s\n",
				    optarg, strerror(errno));
				exit(2);
			}
			break;
		default:
			fprintf(stderr, "Usage...\n");
			exit(0);
		}
	}
	argc -= optind;
	argv += optind;

	if (!bare) {
		AZ(cli_exec(cs, "tty 0"));
		AZ(cli_exec(cs, "rtc 0"));
	}
	for (i = 0; i < argc; i++) {
		printf("CLI <%s>\n", argv[i]);
		if (cli_exec(cs, argv[i]))
			exit(2);
	}

	printf("CLI open\n");

	while (1) {
		if (fgets(buf, sizeof buf, stdin) != buf)
			break;
		if (cli_exec(cs, buf) < 0)
			break;
	}
	return (0);
}
