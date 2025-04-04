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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rc3600.h"
#include "elastic.h"
#include "vav.h"

static int cli_alias_help(struct cli *cli, const char *canonical);

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
	if (cli->help) {
		cli_printf(cli, "%s\n\t\tStop the CPU\n", cli->av[0]);
		return;
	}

	if (!cli_n_args(cli, 0))
		cpu_stop(cli->cs);
}

static void v_matchproto_(cli_func_t)
cli_start(struct cli *cli)
{

	if (cli->help) {
		cli_printf(cli,
		    "%s\n\t\tStart the CPU at the current PC\n",
		    cli->av[0]);
		return;
	}
	if (!cli_n_args(cli, 0))
		cpu_start(cli->cs);
}

static void v_matchproto_(cli_func_t)
cli_wait_halt(struct cli *cli)
{
	if (cli->help) {
		cli_printf(cli,
		    "%s\n\t\tWait for CPU to HALT\n",
		    cli->av[0]);
		return;
	}
	while (cli->cs->running)
		usleep(250000);
}


static void v_matchproto_(cli_func_t)
cli_step(struct cli *cli)
{

	if (cli->help) {
		if (cli_alias_help(cli, "step"))
			return;
		cli_printf(cli, "%s\n\t\tSingle step the CPU\n", cli->av[0]);
		return;
	}

	if (!cli_n_args(cli, 0)) {
		cpu_stop(cli->cs);
		cpu_instr(cli->cs);
	}
}

static void v_matchproto_(cli_func_t)
cli_autoload(struct cli *cli)
{
	if (cli->help) {
		cli_printf(cli, "%s\n\t\tAutoload\n", cli->av[0]);
		return;
	}

	if (!cli_n_args(cli, 0)) {
		cpu_stop(cli->cs);
		AutoRom(cli->cs);
		cli->cs->pc = 0;
		cpu_start(cli->cs);
	}
}

/**********************************************************************/

static void v_matchproto_(cli_func_t)
cli_trace(struct cli *cli)
{
	int i;

	if (cli->help) {
		cli_printf(cli, "%s [<word>]\n", cli->av[0]);
		cli_printf(cli, "\t\tSet or read trace flag\n");
		return;
	}

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
		cli->cs->do_trace = i;
	}
	show_word("TRACE", cli->cs->do_trace);
}


/**********************************************************************/

static void v_matchproto_(cli_func_t)
cli_break(struct cli *cli)
{
	int i;

	if (cli->help) {
		if (cli_alias_help(cli, "break"))
			return;
		cli_printf(cli, "%s [<word>]\n", cli->av[0]);
		cli_printf(cli, "\t\tSet breakpoint\n");
		return;
	}

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
		cli->cs->breakpoint = i;
	}
}

/**********************************************************************/

static void v_matchproto_(cli_func_t)
cli_switches(struct cli *cli)
{
	int i;

	if (cli->help) {
		if (cli_alias_help(cli, "switches"))
			return;
		cli_printf(cli, "%s [<word>]\n", cli->av[0]);
		cli_printf(cli, "\t\tSet or read front panel switches\n");
		return;
	}

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
		*dst = &cli->cs->pc;
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

	if (cli->help) {
		if (cli_alias_help(cli, "examine"))
			return;
		cli_printf(cli,
		    "%s {ac0|ac1|ac2|ac3|pc|carry|<word>}\n", cli->av[0]);
		cli_printf(cli, "\t\tExamine value of register or memory\n");
		return;
	}
	if (cli_n_args(cli, 1))
		return;

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

	if (cli->help) {
		if (cli_alias_help(cli, "deposit"))
			return;
		cli_printf(cli,
		    "%s {ac0|ac1|ac2|ac3|pc|carry|<word>} <word>\n",
		    cli->av[0]);
		cli_printf(cli, "\t\tDeposit value in register or memory\n");
		return;
	}
	if (cli_n_args(cli, 2))
		return;
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

static void
cli_exit(struct cli *cli)
{
	uint16_t w;

	if (cli->help) {
		cli_printf(cli, "%s [<word>]\n", cli->av[0]);
		cli_printf(cli,
		    "\t\tExit emulator with optional return code\n");
		return;
	}
	printf("%ju instructions, %ju paces, %ju pace nsecs\n",
	    cli->cs->ins_count,
	    cli->cs->pace_n,
	    cli->cs->pace_nsec
	);
	if (cli->ac == 1)
		exit(0);
	if (cli_n_args(cli, 1))
		return;
	w = to_word(cli, cli->av[1]);
	if (cli->status)
		exit(-1);
	exit(w);
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

static int
cli_alias_help(struct cli *cli, const char *canonical)
{
	if (strcmp(cli->av[0], canonical)) {
		cli_printf(cli, "%s\t\tAlias for %s\n", cli->av[0], canonical);
		return (1);
	}
	return (0);
}

void
cli_io_help(struct cli *cli, const char *desc, int has_trace, int has_elastic)
{
	cli_printf(cli, "%s [<unit>] [arguments]\n", cli->av[0]);
	cli_printf(cli, "\t\t%s\n", desc);
	if (has_trace) {
		cli_printf(cli, "\ttrace <word>\n");
		cli_printf(cli, "\t\tI/O trace level.\n");
	}
	if (has_elastic) {
		cli_printf(cli, "\t<elastic>\n");
		cli_printf(cli, "\t\tElastic buffer arguments\n");
	}
}

void
cli_unknown(struct cli *cli)
{

	cli_printf(cli, "Unknown argument '%s'\n", cli->av[0]);
	cli->status = 1;
}

void
cli_background(struct cli *cli)
{

	cli_printf(cli, "CLI processing stops\n.");
	while (1) {
		sleep(120);
	}
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

static cli_func_f cli_help;

static const struct cli_cmds {
	const char	*cmd;
	cli_func_f	*func;
} cli_cmds[] = {
	{ "help",	cli_help },
	{ "exit",	cli_exit },
	{ "cpu",	cli_cpu },
	{ "switches",	cli_switches },
	{ "examine",	cli_examine },
	{ "deposit",	cli_deposit },
	{ "stop",	cli_stop },
	{ "start",	cli_start },
	{ "step",	cli_step },
	{ "autoload",	cli_autoload },
	{ "trace",	cli_trace },
	{ "wait_halt",	cli_wait_halt },
	{ "break",	cli_break },
	// reset
	// continue (differs from start how ?)

	{ "tty",	cli_tty },
	{ "dkp",	cli_dkp },
	{ "rtc",	cli_rtc },
	{ "ptp",	cli_ptp },
	{ "ptr",	cli_ptr },
	{ "fdd",	cli_fdd },
	{ "amx",	cli_amx },

	{ "domus",	cli_domus },

	{ "switch",	cli_switches },
	{ "background",	cli_background },
	{ "x",		cli_examine },
	{ "d",		cli_deposit },
	{ "b",		cli_break },
	{ "?",		cli_help },
	{ NULL,		NULL },
};

static void v_matchproto_(cli_func_t)
cli_help(struct cli *cli)
{
	const struct cli_cmds *cc;
	char *save;
	int bug;

	if (cli->help) {
		if (cli_alias_help(cli, "help"))
			return;
		cli_printf(cli, "%s [<command>]\n", cli->av[0]);
		cli_printf(cli, "\t\tShow command syntax\n");
		return;
	}
	cli->help = 1;
	bug = cli->ac;
	for (cc = cli_cmds; cc->cmd != NULL; cc++) {
		if (cli->ac > 1 && strcmp(cli->av[1], cc->cmd))
			continue;
		save = cli->av[0];
		cli->av[0] = strdup(cc->cmd);
		AN(cli->av[0]);
		cc->func(cli);
		free(cli->av[0]);
		cli->av[0] = save;
		assert(bug == cli->ac);
		cli_printf(cli, "\n");
	}
	if (cli->ac == 1 || (cli->ac > 1 && !strcmp(cli->av[1], "elastic")))
		(void)cli_elastic(NULL, cli);
}

int
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

int
cli_from_file(struct rc3600 *cs, FILE *fi, int fatal)
{
	char buf[BUFSIZ];
	char *p;
	int rv = 0;

	while (1) {
		if (fgets(buf, sizeof buf, fi) != buf)
			break;
		p = strchr(buf, '\n');
		if (p != NULL)
			*p = '\0';
		printf("cli <%s>\n", buf);
		rv = cli_exec(cs, buf);
		if (rv < 0 || (rv && fatal))
			break;
	}
	return (rv);
}
