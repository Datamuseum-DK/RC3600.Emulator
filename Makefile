
OBJS	= main.o core.o autorom.o
OBJS	+= ins_exec.o ins_timing.o interrupt.o device.o
OBJS	+= elastic.o elastic_fd.o elastic_tcp.o elastic_match.o
OBJS	+= callout.o
OBJS	+= disass.o
OBJS	+= vav.o

OBJS	+= io_cpu.o
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

test:	rc3600
	./rc3600 \
		-T /critter/_36 \
		'tty telnet :2100' \
		'tty trace 1' \
		'ptp > _ptp' \
		'ptr 0' \
		'dkp 0 load 0 /home/phk/DDHF/DDHF/Rc3600/DKP/011/__' \
		'switch 0000073' \
		'tty speed 9600' \
		'tty match arm "SYSTEM:"' \
		'autoload' \
		'tty match wait' \
		'tty << ""' \
		2>&1 | tee /critter/_3

nakskov: rc3600
	./rc3600 \
		-T /critter/_36 \
		'tty trace 1' \
		'tty speed 110' \
		'tty telnet :2000' \
		'ptp 0' \
		'ptr 0' \
		"dkp 0 load 0 ${DKPDIR}/dkp_011.bin" \
		'amx port 0 telnet :2100' \
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
		-T /critter/_36 \
		'ptr 0' \
		'ptp 0' \
		'tty telnet :2000' \
		'amx port 1 telnet :2101' \
		'amx port 2 telnet :2102' \
		'amx port 3 telnet :2103' \
		"fdd 0 load ${FDDIR}/_sg0113.flp" \
		'switch 0000061' \
		'tty speed 110' \
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
		'tty speed 9600' \
		'autoload' \
		2>&1 | tee /critter/_3

test2:	rc3600
	./rc3600 \
		'tty telnet :2100' \
		'ptp > _ptp' \
		'ptr 0' \
		'dkp 0' \
		'switch 0000073' \
		'tty speed 9600' \
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

timer:	rc3600
	./rc3600 \
		'ptr 0' \
		'tty telnet :2100' \
		'tty speed 9600' \
		'switch 0000012' \
		'ptr 0 < ${PTRDIR}/RCSL_44_RT_1558_RC3600_INSTRUCTION_TIMER_TEST.bin' \
		'tty match arm "STARTADDR   400 ?  "' \
		'autoload' \
		'tty match wait' \
		'tty << "401"' \
		'tty match expect "TTY SPEED  1200 ?  "' \
		'tty << "9600"' \
		'tty match expect "INITIALIZED TO    11    ?  "' \
		'tty << ""' \
		'tty match expect "INITIALIZED TO    16    ?  "' \
		'tty << "22"' \
		'tty match expect "STARTADDR   400 ?  "' \
		'stop' \
		2>&1 | tee /critter/_3


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

main.o:			rc3600.h vav.h main.c
core.o:			rc3600.h core.c
autorom.o:		rc3600.h autorom.c
device.o:		rc3600.h device.c
ins_exec.o:		rc3600.h ins_exec.c
ins_timing.o:		rc3600.h ins_timing.c
interrupt.o:		rc3600.h interrupt.c
callout.o:		rc3600.h callout.c
elastic.o:		rc3600.h elastic.h elastic.c
elastic_fd.o:		rc3600.h elastic.h elastic_fd.c
elastic_match.o:	rc3600.h elastic.h elastic_match.c
elastic_tcp.o:		rc3600.h elastic.h elastic_tcp.c
disass.o:		rc3600.h disass.c
vav.o:			rc3600.h vav.h
io_cpu.o:		rc3600.h io_cpu.c
io_tty.o:		rc3600.h elastic.h io_tty.c
io_dkp.o:		rc3600.h io_dkp.c
io_rtc.o:		rc3600.h io_rtc.c
io_ptp.o:		rc3600.h elastic.h io_ptp.c
io_ptr.o:		rc3600.h elastic.h io_ptr.c
io_fdd.o:		rc3600.h io_fdd.c
io_amx.o:		rc3600.h elastic.h io_amx.c
