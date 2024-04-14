CXXFLAGS+=-std=c++17
LDLIBS = -lgpiod

sim-terminal: sim-terminal.cc

install:
	strip sim-terminal
	cp sim-terminal /usr/local/sbin
	cp scripts/* /usr/local/bin
	cp sim-terminal.service /etc/systemd/system
	systemctl daemon-reload
	systemctl enable sim-terminal

