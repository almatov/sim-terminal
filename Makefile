CXXFLAGS+=-std=c++17

sim:	sim.cc

install:
	strip sim
	cp sim /usr/local/sbin
	cp scripts/* /usr/local/bin
	cp sim.service /etc/systemd/system
	systemctl daemon-reload
	systemctl enable sim

