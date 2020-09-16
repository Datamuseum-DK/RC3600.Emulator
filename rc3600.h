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
struct iodev;
struct core;
struct ins_timing;
struct core_handler;
struct callout;
TAILQ_HEAD(core_handlers, core_handler);

typedef int64_t			nanosec;

struct rc3600 {
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
	nanosec			duration;	/* Duration of instruction */
	struct iodev		*iodevs[64];
	const struct ins_timing	*timing;

	struct core		*core;
	unsigned		core_size;

	uint16_t		switches;

	uint16_t		imask;
	uint16_t		inten[3];
	TAILQ_HEAD(, iodev)	units_list;	/* get_dev_unit */
	TAILQ_HEAD(, iodev)	irq_list;
	TAILQ_HEAD(, iodev)	masked_irq_list;

	nanosec			real_time;
	nanosec			sim_time;

	uint64_t		last_core;
	uint64_t		ins_count;

	int			do_trace;
	int			fd_trace;

	pthread_mutex_t		callout_mtx;
	TAILQ_HEAD(, callout)	callouts;
};

void rc3600_exec(struct rc3600 *);

/* CLI ****************************************************************/

struct cli {
	struct rc3600		*cs;
	int			status;
	int			ac;
	char			**av;
};

typedef void cli_func_f(struct cli *);

void cli_printf(struct cli *cli, const char *fmt, ...) __printflike(2, 3);
int cli_error(struct cli *cli, const char *fmt, ...) __printflike(2, 3);

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

struct core *core_new(unsigned size);
void core_setsize(struct core *cp, unsigned siz);

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

struct callout *callout_wake_dev_abs(struct iodev *, nanosec);
struct callout *callout_wake_dev_rel(struct iodev *, nanosec);
void callout_dev_sleep(struct iodev *, nanosec);
void callout_dev_is_done(struct iodev *iop, nanosec when);
void callout_dev_is_done_abs(struct iodev *iop, nanosec when);

void callout_poll(struct rc3600 *cs);

/* IO device interface ************************************************/

typedef void iodev_init_f(struct iodev *);
typedef void iodev_ins_f(struct iodev *, uint16_t ioi, uint16_t *reg);

iodev_ins_f std_io_ins;

typedef void *iodev_thr(void *);

typedef void *new_dev_f(struct iodev *, struct iodev *);
void *cli_dev_get_unit(struct cli *, const char *, const char *, new_dev_f *);

int cli_dev_trace(struct iodev *iop, struct cli *cli);

void install_dev(struct iodev *iop, iodev_thr *thr);

struct iodev {
	char			name[6];
	struct rc3600		*cs;
	iodev_init_f		*init_func;
	iodev_ins_f		*ins_func;
	void			*priv;		/* private (instance) data */
	uint8_t			busy;		/* I/O bit */
	uint8_t			done;		/* I/O bit */
	uint8_t			pulse;		/* I/O bit */
	uint8_t			unit;		/* Device number [0...63] */

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


	nanosec			sleep_until;
	pthread_cond_t		sleep_cond;
	TAILQ_ENTRY(iodev)	sleep_list;
};

#define	NIO		0x6000
#define	NIOS		0x6040
#define	NIOC		0x6080
#define	NIOP		0x60c0
#define	DIA		0x6100
#define	DIAS		0x6140
#define	DIAC		0x6180
#define	DIAP		0x61c0
#define	DOA		0x6200
#define	DOAS		0x6240
#define	DOAC		0x6280
#define	DOAP		0x62c0
#define	DIB		0x6300
#define	DIBS		0x6340
#define	DIBC		0x6380
#define	DIBP		0x63c0
#define	DOB		0x6400
#define	DOBS		0x6440
#define	DOBC		0x6480
#define	DOBP		0x64c0
#define	DIC		0x6500
#define	DICS		0x6540
#define	DICC		0x6580
#define	DICP		0x65c0
#define	DOC		0x6600
#define	DOCS		0x6640
#define	DOCC		0x6680
#define	DOCP		0x66c0
#define	SKPBN		0x6700 /* Not dispatched to driver */
#define	SKPBZ		0x6740 /* Not dispatched to driver */
#define	SKPDN		0x6780 /* Not dispatched to driver */
#define	SKPDZ		0x67c0 /* Not dispatched to driver */

#define IO_ACTION(x)	(IO_OPER(x) >= SKPBN ? 0 : (x) & 0x00c0)
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
	const char		*model;
#define TIMING_MACRO(fld, x)	nanosec (fld);
	TIMINGS
#undef	TIMING_MACRO
};

extern const struct ins_timing * const ins_timings[];
int ins_timing_check(void);

nanosec now(void);

/* AUTOROM ************************************************************/

void AutoRom(struct rc3600 *cs);

/* I/O DRIVERS ********************************************************/

extern struct iodev iodev_cpu;
extern struct iodev iodev_cpu721;
cli_func_f cli_tty;
cli_func_f cli_dkp;
cli_func_f cli_rtc;
cli_func_f cli_ptp;
cli_func_f cli_ptr;
cli_func_f cli_fdd;
cli_func_f cli_amx;

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
