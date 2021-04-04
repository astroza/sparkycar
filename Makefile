%: %.c
	$(CC) $? -I./include -o $@
all: joystick_feed vehicle_controller
install: all
	cp joystick_feed vehicle_controller /usr/local/bin/
clean:
	rm -f joystick_feed vehicle_controller
