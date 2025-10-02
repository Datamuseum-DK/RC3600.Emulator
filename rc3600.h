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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/types.h>

#define AZ(x) assert((x) == 0);
#define AN(x) assert((x) != 0);

/*
 * In OO-light situations, functions have to match their prototype
 * even if that means not const'ing a const'able argument.
 * The typedef should be specified as argument to the macro.
 */
#define v_matchproto_(xxx)              /*lint --e{818} */

struct rc3600;
struct cli;
struct iodev;
struct core;
struct ins_timing;
struct core_handler;
struct callout;
TAILQ_HEAD(core_handlers, core_handler);

typedef int64_t			nanosec;
typedef void ins_exec_f(struct rc3600 *);
typedef void *iodev_thr(void *);

struct rc3600 {
	const char		*cpu_model;

	pthread_mutex_t		run_mtx;
	pthread_mutex_t		running_mtx;
	pthread_cond_t		run_cond;
	pthread_cond_t		wait_cond;
	pthread_t		cthread;

	int			running;
	uint16_t		acc[4];		/* The accumulators */
	uint16_t		carry;		/* Carry bit */
	uint16_t		pc;		/* Program counter */
	uint16_t		npc;		/* Next program counter */
	uint16_t		ins;		/* Current instruction */

	const struct ins_timing	*timing;
	nanosec			duration;	/* Duration of instruction */

	struct iodev		*iodevs[64];
	struct iodev		*nodev;

	uint64_t		pace_nsec;
	uint64_t		pace_n;
	uint64_t		ins_count;
	ins_exec_f		*ins_exec[1 << 16];

	uint16_t		ident;
	int			ext_core;
	struct core		*core;
	unsigned		core_size;
	uint64_t		last_core;
	int			breakpoint;

	uint16_t		switches;

	uint16_t		imask;
	uint16_t		inten[3];
	TAILQ_HEAD(, iodev)	irq_list;
	TAILQ_HEAD(, iodev)	masked_irq_list;

	nanosec			real_time;
	nanosec			sim_time;

	int			do_trace;
	int			fd_trace;

	pthread_mutex_t		callout_mtx;
	TAILQ_HEAD(, callout)	callouts;
};

/* CPU ****************************************************************/

ins_exec_f rc3600_exec;

struct rc3600 *cpu_new(void);
void cpu_add_dev(struct iodev *iop, iodev_thr *thr);
void cpu_start(struct rc3600 *);
void cpu_stop(struct rc3600 *cs);
void cpu_instr(struct rc3600 *cs);
void cpu_nova(struct rc3600 *cs);
void cpu_extmem(struct rc3600 *cs);
void cpu_720(struct rc3600 *cs);

extern const struct ins_timing nova_timing;
extern const struct ins_timing nova1200_timing;
extern const struct ins_timing nova800_timing;
extern const struct ins_timing nova2_timing;
extern const struct ins_timing rc3608_timing;
extern const struct ins_timing rc3609_timing;

/* CLI ****************************************************************/

struct cli {
	struct rc3600		*cs;
	int			status;
	int			help;
	int			ac;
	char			**av;
};

int cli_exec(struct rc3600 *, const char *);
int cli_from_file(struct rc3600 *cs, FILE *fi, int fatal);


typedef void cli_func_f(struct cli *);

void cli_printf(struct cli *cli, const char *fmt, ...) __printflike(2, 3);
int cli_error(struct cli *cli, const char *fmt, ...) __printflike(2, 3);

void cli_io_help(struct cli *, const char *desc, int trace, int elastic);

int cli_n_args(struct cli *cli, int n);
void cli_unknown(struct cli *cli);

/* Tracing & Debugging ************************************************/

void trace_state(struct rc3600 *cs);
void trace(const struct rc3600 *cs, const char *fmt, ...) __printflike(2, 3);
void dev_trace(const struct iodev *iop, const char *fmt, ...) __printflike(2, 3);

/* CORE memory interface **********************************************/

#define CORE_NULL	(1<<1)
#define CORE_READ	(1<<2)
#define CORE_MODIFY	(1<<3)
#define CORE_WRITE	(1<<4)
#define CORE_DMA	(1<<4)

#define CORE_INS	(1<<5)
#define CORE_INDIR	(1<<6)
#define CORE_DATA	(1<<7)

struct core *core_new(void);

uint16_t core_read(struct rc3600 *, uint16_t addr, int how);
void core_write(struct rc3600 *, uint16_t addr, uint16_t val, int how);

const char *core_disass(const struct rc3600 *, uint16_t addr);

typedef int core_read_f(struct rc3600 *, uint16_t addr, uint16_t *dst, int how);
typedef int core_write_f(struct rc3600 *, uint16_t addr, uint16_t *src, int how);

uint16_t *core_ptr(const struct rc3600 *, uint16_t addr);

struct core_handler {
	TAILQ_ENTRY(core_handler)	next;
	core_read_f			*read_func;
	core_write_f			*write_func;
};


/* Interrupts *********************************************************/

void intr_raise(struct iodev *iop);
void intr_lower(struct iodev *iop);
struct iodev *intr_pending(struct rc3600 *cs);
void intr_msko(struct rc3600 *cs, uint16_t);
uint16_t intr_inta(struct rc3600 *cs);

/* Callout ************************************************************/

nanosec now(void);
void callout_dev_sleep(struct iodev *, nanosec);
void callout_dev_is_done(struct iodev *iop, nanosec when);
void callout_dev_is_done_abs(struct iodev *iop, nanosec when);

nanosec callout_poll(struct rc3600 *cs);

/* IO device interface ************************************************/

void iodev_init(struct rc3600 *);

typedef void iodev_io_f(struct iodev *, uint16_t ioi, uint16_t *reg);
typedef void iodev_skp_f(struct iodev *, uint16_t ioi);

iodev_io_f std_io_ins;
iodev_skp_f std_skp_ins;


typedef void *new_dev_f(struct iodev *, struct iodev *);
void *cli_dev_get_unit(struct cli *, const char *, const char *, new_dev_f *);

int cli_dev_trace(struct iodev *iop, struct cli *cli);

struct iodev {
	char			name[6];
	struct rc3600		*cs;
	iodev_io_f		*io_func;
	iodev_skp_f		*skp_func;
	void			*priv;		/* private (instance) data */
	uint8_t			busy;		/* I/O bit */
	uint8_t			done;		/* I/O bit */
	uint8_t			pulse;		/* I/O bit */
	unsigned		devno;		/* Device number [0...63] */

	uint8_t			imask;		/* Bit in interrupt mask */
	uint8_t			ipen;		/* Interrupt Pending */
	TAILQ_ENTRY(iodev)	irq_list;

	int			trace;

	pthread_t		thread;
	pthread_mutex_t		mtx;
	pthread_cond_t		cond;
	uint16_t		ireg_a;
	uint16_t		ireg_b;
	uint16_t		ireg_c;
	uint16_t		oreg_a;
	uint16_t		oreg_b;
	uint16_t		oreg_c;


	pthread_cond_t		sleep_cond;
};

#define IO_CPUDEV	0x3f
#define IO_MAXDEV	0x3f

#define	IO_NIO		0x6000
#define	IO_DIA		0x6100
#define	IO_DOA		0x6200
#define	IO_DIB		0x6300
#define	IO_DOB		0x6400
#define	IO_DIC		0x6500
#define	IO_DOC		0x6600
#define	IO_SKP		0x6700
#define	IO_SKPBN	0x6700
#define	IO_SKPBZ	0x6740
#define	IO_SKPDN	0x6780
#define	IO_SKPDZ	0x67c0

#define IO_ACTION(x)	(IO_OPER(x) >= IO_SKP ? 0 : (x) & 0x00c0)
#define IO_START	0x0040
#define IO_CLEAR	0x0080
#define IO_PULSE	0x00c0
#define IO_OPER(x)	((x) & ~0x18c0)

/* (INSTRUCTION) TIMING ***********************************************/

#define TIMINGS \
	TIMING_MACRO(time_lda, 1) \
	TIMING_MACRO(time_sta, 1) \
	TIMING_MACRO(time_isz, 1) \
	TIMING_MACRO(time_isz_skp, 0) \
	TIMING_MACRO(time_jmp, 1) \
	TIMING_MACRO(time_jsr, 1) \
	TIMING_MACRO(time_indir_adr, 1) \
	TIMING_MACRO(time_base_reg, 0) \
	TIMING_MACRO(time_auto_idx, 0) \
	TIMING_MACRO(time_alu_1, 1) \
	TIMING_MACRO(time_alu_2, 1) \
	TIMING_MACRO(time_alu_skip, 0) \
	TIMING_MACRO(time_alu_shift, 0) \
	TIMING_MACRO(time_alu_swap, 0) \
	TIMING_MACRO(time_io_input, 1) \
	TIMING_MACRO(time_io_nio, 1) \
	TIMING_MACRO(time_io_output, 1) \
	TIMING_MACRO(time_io_scp, 0) \
	TIMING_MACRO(time_io_skp, 1) \
	TIMING_MACRO(time_io_skp_skip, 0) \
	TIMING_MACRO(time_io_inta, 1)

struct ins_timing {
#define TIMING_MACRO(fld, x)	nanosec (fld);
	TIMINGS
#undef	TIMING_MACRO
};

/* AUTOROM ************************************************************/

void AutoRom(struct rc3600 *cs);

/* I/O DRIVERS ********************************************************/

cli_func_f cli_cpu;
cli_func_f cli_tty;
cli_func_f cli_dkp;
cli_func_f cli_rtc;
cli_func_f cli_ptp;
cli_func_f cli_ptr;
cli_func_f cli_fdd;
cli_func_f cli_amx;
cli_func_f cli_cdr;
cli_func_f cli_domus;

/* DISASSEMBLER *******************************************************/

#define DISASS_BUF	20

char *disass(uint16_t u, const char * const *pz,
    const struct rc3600 *cs, char *buf, int *offset);

void disass_magic(uint16_t, const char *fmt, ...) __printflike(2, 3);

#define Rc3600Disass_NO_OFFSET  -9999

/* UTILITIES  *********************************************************/

/* Safe printf into a fixed-size buffer */
#define bprintf(buf, fmt, ...)						\
	do {								\
		int ibprintf;						\
		ibprintf = snprintf(buf, sizeof buf, fmt, __VA_ARGS__);	\
		assert(ibprintf >= 0 && ibprintf < (int)sizeof buf);	\
	} while (0)
