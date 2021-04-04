/* felipe@astroza.cl - 2021
 * under MIT license
*/
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/input.h>
#include "vehicle.h"

double get_time()
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int joystick_loop(int fd, int endpoint_fd)
{
	struct input_event ev;
	struct vehicle_state state;
	double last_time = 0, current_time;
	unsigned int size;

	state.steering = 0;
	state.wheels = 0;
	while (1)
	{
		size = read(fd, &ev, sizeof(struct input_event));
		if (size < sizeof(ev))
		{
			return EXIT_FAILURE;
		}
		if (ev.type == 3)
		{
			switch (ev.code)
			{
			// Steering
			case 0:
				// from joystick
				// 0 (L), 127 (C), 255 (R)
				// for vehicle -127 (L), 0 (C), 127 (R)
				state.steering = ev.value - 127;
				break;
			// Reverse
			case 2:
				// from joystick 0 - 255
				// for vehicle -127, 0
				state.wheels = -127 * ev.value / 255;
				break;
			// Forward
			case 5:
				// from joystick 0 - 255
				// for vehicle 0, 127
				state.wheels = 127 * ev.value / 255;
				break;
			}
		}
		// Joystick sends EV_SYNC events periodically, so
		// this loop is always iterating unless a joystick disconnection
		current_time = get_time();
		if (current_time - last_time >= 1.0 / 30)
		{
			if (send(endpoint_fd, &state, sizeof(state), 0) <= 0)
			{
				perror("send");
				return EXIT_FAILURE;
			}
			last_time = current_time;
		}
	}
}

int main(int c, char **v)
{
	int ev_fd, endpoint_fd;
	struct sockaddr_in so_addr;

	ev_fd = open(v[1], O_RDONLY);
	if (ev_fd == -1)
	{
		fprintf(stderr, "Can't open %s\n", v[1]);
		return EXIT_FAILURE;
	}

	endpoint_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (endpoint_fd == -1)
	{
		perror("socket");
		return EXIT_FAILURE;
	}
	so_addr.sin_family = AF_INET;
	so_addr.sin_port = htons(CONTROL_PORT);
	so_addr.sin_addr.s_addr = inet_addr(CONTROL_ADDR);
	if (connect(endpoint_fd, (struct sockaddr *)&so_addr, sizeof(so_addr)) != 0)
	{
		perror("connect");
		return EXIT_FAILURE;
	}
	return joystick_loop(ev_fd, endpoint_fd);
}
