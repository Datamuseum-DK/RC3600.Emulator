
OBJS	= main.o cli.o core.o autorom.o
OBJS	+= cpu.o cpu_nova.o cpu_extmem.o cpu_720.o cpu_timing.o
OBJS	+= cpu_exec.o interrupt.o device.o
OBJS	+= elastic.o elastic_fd.o elastic_tcp.o elastic_match.o
OBJS	+= callout.o
OBJS	+= disass.o
OBJS	+= domus.o
OBJS	+= vav.o

OBJS	+= io_tty.o
OBJS	+= io_dkp.o
OBJS	+= io_rtc.o
OBJS	+= io_ptp.o
OBJS	+= io_ptr.o
OBJS	+= io_fdd.o
OBJS	+= io_amx.o

CFLAGS	+= -Wall -Werror -pthread -g -O0
LDFLAGS	+= -lm

FDDIR = /critter/DDHF/20190815_8inch_floppies

DKPDIR = DKP/
PTRDIR = PTR/

default:	nakskov

help:	rc3600
	./rc3600 \
		"help" "exit"

dkp:	rc3600
	./rc3600 \
		-T /critter/_36 \
		-t \
		'cpu model rc3803' \
		'cpu core 128' \
		'cpu ident 2' \
		rtc \
		ptr \
		ptp \
		'tty > _tty' \
		'ptr < /tmp/RCSL44RT1662.bin' \
		'dkp' \
		'switch 10' \
		'autoload'

test:	rc3600
	./rc3600 \
		-T /critter/_36 \
		-t \
		'cpu model rc3803' \
		'cpu core 128' \
		'cpu ident 2' \
		'domus' \
		'rtc 0' \
		'rtc trace 1' \
		'tty telnet :2100' \
		'tty serial /dev/nmdm3A' \
		'tty trace 1' \
		'ptp > _ptp' \
		'ptr 0' \
		'dkp 0 load 0 /home/phk/DDHF/DDHF/Rc3600/DKP/004/__' \
		'dkp 0 load 1 T/DKP027/_out' \
		'switch 0000073' \
		'tty baud 9600' \
		'tty match arm "SYSTEM:"' \
		'autoload' \
		'tty match wait' \
		'tty << ""' \
		'tty match xon' \
		#'tty << "LIST/CORE"' \
		#'tty match xon' \
		#'tty << "CAP8"' \
		#'tty match xon' \
		#'tty << "CATLI MU$$$$$$"' \
		#'tty match expect "FINIS CATLI"' \
		#'exit' \
		2>&1 | tee /critter/_3

nakskov: rc3600
	./rc3600 \
		-T /critter/_36 \
		'tty trace 1' \
		'tty baud 9600' \
		'tty telnet :2000' \
		'ptp 0' \
		'ptr 0' \
		"dkp 0 load 0 ${DKPDIR}/dkp_011.bin" \
		'amx port 0 telnet :2100' \
		'amx port 0 serial /dev/nmdm7A' \
		'amx port 1 telnet :2101' \
		'amx port 2 telnet :2102' \
		'amx port 3 telnet :2103' \
		'amx port 4 telnet :2104' \
		'amx port 5 telnet :2105' \
		'amx port 6 telnet :2106' \
		'amx port 7 telnet :2107' \
		'switch 0000073' \
		'tty match arm "SYSTEM:"' \
		'autoload' \
		'tty match wait' \
		'tty << ""' \
		'tty match xon ' \
		'tty << "CAP8"' \
		'tty match xon ' \
		'tty << "CMB00"' \
		'tty match xon ' \
		'tty << "AMX"' \
		'tty match xon ' \
		'tty << "PTR"' \
		'tty match xon ' \
		'tty << "PTP"' \
		'tty match xon ' \
		'tty << "AMXIN AMXCO"' \
		'tty match expect "FINIS AMXIN"' \
		'tty << "BPAR"' \
		'tty match xon ' \
		'tty << "COPS"' \
		'tty match expect "DATE (YY.MM.DD)="' \
		'tty match xon ' \
		"tty << `date +%y.%m.%d`" \
		'tty match expect "TIME (HH.MM.SS)="' \
		'tty match xon ' \
		"tty << `date +%H.%M.%S`" \
		2>&1 | tee /critter/_3

co040:	rc3600
	./rc3600 \
		- /critter/_36 \
		'ptr 0' \
		'ptp 0' \
		'tty telnet :2000' \
		'amx port 1 telnet :2101' \
		'amx port 2 telnet :2102' \
		'amx port 3 telnet :2103' \
		"fdd 0 load ${FDDIR}/_sg0113.flp" \
		'switch 0000061' \
		'tty baud 110' \
		'autoload' \
		'tty match xon ' \
		'tty << "BOOT CO040"' \
		'tty match expect "TERM 32: "' \
		'tty match xon ' \
		'tty << "2000"' \
		'tty match expect "TERM 01: "' \
		'tty match xon ' \
		'tty << "2000"' \
		'tty match expect "TERM 02: "' \
		'tty match xon ' \
		'tty << "2000"' \
		'tty match expect "DATE (YY.MM.DD)="' \
		'tty match xon ' \
		"tty << `date +%y.%m.%d`" \
		'tty match expect "TIME (HH.MM.SS)="' \
		'tty match xon ' \
		"tty << `date +%H.%M.%S`" \
		2>&1 | tee /critter/_3


rc6000:	rc3600
	./rc3600 \
		'ptp > _ptp' \
		'ptr 0' \
		'tty telnet localhost:2100' \
		'dkp 0 load 0 /home/phk/DDHF/DDHF/Rc3600/DKP/000/__' \
		'switch 0000073' \
		'tty baud 9600' \
		'autoload' \
		2>&1 | tee /critter/_3

test2:	rc3600
	./rc3600 \
		'tty telnet :2100' \
		'ptp > _ptp' \
		'ptr 0' \
		'dkp 0' \
		'switch 0000073' \
		'tty baud 9600' \
		'tty match arm "SYSTEM:"' \
		'autoload' \
		'tty match wait' \
		'tty << ""' \
		'tty match xon ' \
		'tty << " LIST/PROGRAM"' \
		'tty match xon ' \
		'tty << "  LIST"' \
		'tty match xon ' \
		'tty << "  CATLI Y$$$$$$$$"' \
		'tty match expect "FINIS CATLI"' \
		2>&1 | tee /critter/_3

regression:	rc3600
	./rc3600 -f Tests/rcsl_44_rt_1715_rc3600_cpu_logic_text.cli
	./rc3600 -f Tests/rcsl_44_rt_1648_rc3600_extended_memory_test.cli
	./rc3600 -f Tests/rcsl_44_rt_1595_rc3600_extended_memory_test.cli
	./rc3600 -f Tests/rcsl_44_rt_1558_rc3600_instruction_timer_test.cli
	./rc3600 -f Tests/rcsl_52_aa_900_rc3600_cpu_720_ext_test.cli

expect:	rc3600
	./rc3600 \
		'ptp > _ptp' \
		'ptr 0' \
		'dkp 0' \
		'switch 0000073' \
		'tty expect match "SYSTEM: "' \
		'autoload' \
		'tty expect wait' \
		'tty << ""' \
		2>&1 | tee /critter/_3

rc3600:	${OBJS}
	${CC} -o rc3600 ${CFLAGS} ${LDFLAGS} ${OBJS}
	rm -f *.tmp

clean:
	rm -f *.o *.tmp rc3600

autorom.o:		rc3600.h autorom.c
callout.o:		rc3600.h callout.c
cli.o:			rc3600.h vav.h cli.c
core.o:			rc3600.h core.c
cpu.o:			rc3600.h cpu.c
cpu_720.o:		rc3600.h cpu_720.c
cpu_exec.o:		rc3600.h cpu_exec.c
cpu_extmem.o:		rc3600.h cpu_extmem.c
cpu_nova.o:		rc3600.h cpu_nova.c
cpu_timing.o:		rc3600.h cpu_timing.c
device.o:		rc3600.h device.c
disass.o:		rc3600.h disass.c
domus.o:		rc3600.h domus.c
elastic.o:		rc3600.h elastic.h elastic.c
elastic_fd.o:		rc3600.h elastic.h elastic_fd.c
elastic_match.o:	rc3600.h elastic.h elastic_match.c
elastic_tcp.o:		rc3600.h elastic.h elastic_tcp.c
interrupt.o:		rc3600.h interrupt.c
io_amx.o:		rc3600.h elastic.h io_amx.c
io_dkp.o:		rc3600.h io_dkp.c
io_fdd.o:		rc3600.h io_fdd.c
io_ptp.o:		rc3600.h elastic.h io_ptp.c
io_ptr.o:		rc3600.h elastic.h io_ptr.c
io_rtc.o:		rc3600.h io_rtc.c
io_tty.o:		rc3600.h elastic.h io_tty.c
main.o:			rc3600.h main.c
vav.o:			rc3600.h vav.h
