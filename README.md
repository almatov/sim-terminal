# sim - SIM800 terminal

This is a C++ written terminal daemon that uses Simcom's SIM800 chip with Linux. The daemon has a permanent connection to the SIM800 UART port and serves incoming events, as well as plays predefined commands.

The **RING** event should be hung up after getting an incoming call. This protects against phone scammers that attempt social engineering fraud, making it safe to use the SIM card binded to the bank or other services with 2FA.

The permanent connection to the online SIM card makes 2FA theft that uses fake IMEI, IMSI and phone numbers captured from your mobile device difficult. These attacks often use to take accounts based on SMS 2FA. Do not use SMS 2FA if possible, and switch to OTP 2FA instead.

The program was compiled and tested under Armbian Linux for NanoPi Duo2 with NanoPi IoT-2G dock but should work under any other Linux system. Pay attention to the PWRKEY pin on the other board, as the IoT-2G board will have the inverted TTL connection of the PG11 pin to the PWRKEY. The GSM antenna included with the IoT-2G dock can interfere with the system, so it's best to use one with an SMA cable that can be kept away from the board.

The daemon will listen on 127.0.0.1:51173. To connect, you can use the **sim-client** script. It is recommended you run the script under tmux.

By itself, the daemon will write out the full log to the system journal for the unit **sim.** The **sim-calls** script will print out the incoming call history from the journal.

The **sim-power** can be used to turn the SIM800 chip on, off and to reset it.

The predefined commands are **kill** to stop the daemon, **quit** to detach client, **list** to print received SMS messages, **delete** to remove read SMS messages, **balance** to get phone balance and **send** to submit SMS message. You also can type out AT commands directly.

## How to install

Software requirements:

    g++-10.2+       (apt install build-essential)
    libgpiod-2.0+   (https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git)

Run this

	make
	sudo make install
	sudo sim-power on
	sudo systemctl start sim
	sudo systemctl enable sim

Edit /etc/rc.local to add

	/usr/local/bin/sim-power on