# SIM terminal program

This is a C++ written terminal program that uses SIM module with Linux. The program has a permanent connection to the SIM module UART port and serves incoming events, as well as plays predefined commands or direct **AT** commands.

The **RING** event should be hung up after getting an incoming call. This protects against phone scammers that attempt social engineering fraud, making it safe to use the SIM card binded to the bank or other services with 2FA.

The **+CMT** event will print the received SMS message.

The **+CUSD** event will print the USSD message.

The permanent connection to the online SIM card makes 2FA theft that uses fake IMEI, IMSI and phone numbers captured from your mobile device more difficult. These attacks often use to take accounts based on SMS 2FA. Do not use SMS 2FA if possible, and switch to TOTP 2FA instead.

The program was compiled and tested under Armbian Linux for NanoPi Duo2 with NanoPi IoT-2G SIM800C dock but should work under any other Linux system with different Simcom's modules. Pay attention to the PWRKEY pin on the other board, as the IoT-2G board will have the inverted TTL connection of the PG11 pin to the PWRKEY. The GSM antenna included with the IoT-2G dock can interfere with the system, so it's best to use one with an SMA connector cable that can be kept away from the board.

The SIM terminal service will write out the full log to the system journal for the unit **sim-terminal.**

Use the **sim-shell** console script to connect to the terminal. You can use either **AT** commands or predefined console commands.

**Predefined Commands:**

- **kill**: Stops the service.
- **quit**: Disconnects the client.
- **balance**: Retrieves the phone balance.
- **send**: Submits an SMS message.

## How to install

Software requirements:

    g++-10.2+       # apt install build-essential
    nc.traditional  # apt install nc.traditional
    libgpiod-2.0+   # https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git

Run this

	make
	sudo make install
	sudo cp sim-terminal.service /etc/systemd/system
	sudo systemctl daemon-reload
	sudo systemctl enable sim-terminal
	sudo systemctl start sim-terminal
	sim-shell                      # interactivity
	journalctl -fu sim-terminal    # just see events
