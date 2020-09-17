# RC3600.Emulator

Datamuseum.dk's RC3600 Emulator

Currently only tested on FreeBSD.

CLI commands:

	help [<command>]
			Show command syntax
	exit [<word>]
			Exit emulator with optional return code
	switches [<word>]
			Set or read front panel switches
	examine {ac0|ac1|ac2|ac3|pc|carry|<word>}
			Examine value of register or memory
	deposit {ac0|ac1|ac2|ac3|pc|carry|<word>} <word>
			Deposit value in register or memory
	stop
			Stop the CPU
	start
			Start the CPU at the current PC
	step
			Single step the CPU
	autoload
			Autoload
	tty [<unit>] [arguments]
			TTI+TTO device pair
		trace <word>
			I/O trace level.
		<elastic>
			Elastic buffer arguments
	dkp [<unit>] [arguments]
			RC3652 "Diablo" disk controller
		load <0…3> <filename>
			Load disk-image from file
		save <0…3> <filename>
			Save disk-image to file
	rtc [<unit>] [arguments]
			Real Time Clock
		trace <word>
			I/O trace level.
	ptp [<unit>] [arguments]
			Paper Tape Punch
		<elastic>
			Elastic buffer arguments
	ptr [<unit>] [arguments]
			Paper Tape Reader
		<elastic>
			Elastic buffer arguments
	fdd [<unit>] [arguments]
			RC3650 Floppy disk controller
		load <filename>
			Load floppy-image from file
		save <filename>
			Save floppy-image to file
	amx [<unit>] [arguments]
			Asynchronous multiplexor
		trace <word>
			I/O trace level.
		port <0…7> <elastic>
			Per port elastic buffer arguments
	switch		Alias for switches
	x		Alias for examine
	d		Alias for deposit
	?		Alias for help
	<elastic> [arguments]
			Elastic buffer arguments
		<< <string>
			Input <string> + CR into buffer
		< <filename>
			Read input from file
		> <filename>
			Write output to file
		>> <filename>
			Append output to file
		serial <tty-device>
			Connect to (UNIX) tty-device
		tcp <host>:<port
			Connect to raw TCP socket
		telnet [<host>]:<port>
			Start TELNET server
		match arm <string>
			Start looking for string
		match wait
			Wait for 'arm' to match
		match expect <string>
			Shortcut for 'arm' + 'wait'
		match xon
			Wait for XON character
