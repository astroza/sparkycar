/* felipe@astroza.cl - 2021
 * under MIT license
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <fcntl.h>
#include "vehicle.h"

#define PWM_COUNT 2
#define PWM_MAX_VALUE 2000000
#define PWM_NEUTRAL_VALUE 1500000
#define PWM_MIN_VALUE 1000000

#define WHEELS_INDEX 0
#define STEERING_INDEX 1

int write_number(int fd, int number)
{
	char str[32];

	snprintf(str, sizeof(str), "%d\n", number);
	if (write(fd, str, strlen(str)) <= 0)
		return -1;

	return 0;
}

int write_number_to_file(char *path, int number)
{
	int fd;

	fd = open(path, O_WRONLY);
	if (fd == -1)
		goto error;
	if (write_number(fd, number) == -1)
	{
		close(fd);
		goto error;
	}
	close(fd);
	return 0;
error:
	fprintf(stderr, "Can't write on %s\n", path);
	return -1;
}

int export_pwm(int number)
{
	return write_number_to_file("/sys/class/pwm/pwmchip0/export", number);
}

int set_pwm_enable(int number, int value)
{
	char path[128];

	snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip0/pwm%d/enable", number);
	return write_number_to_file(path, value);
}

int set_pwm_period(int number, int period)
{
	char path[128];

	snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip0/pwm%d/period", number);
	return write_number_to_file(path, period);
}

int set_pwm_duty_cycle(int fd, int duty_cycle)
{
	return write_number(fd, duty_cycle);
}

int init_pwm(int *pwm_fds, int count)
{
	int i = 0;
	char path[128];
	struct stat s;

	for (i = 0; i < count; i++)
	{
		snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip0/pwm%d", i);
		if (stat(path, &s) == -1)
		{
			if (export_pwm(i) == -1)
				return -1;
		}
		// 20hz
		set_pwm_period(i, 50000000);
		snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", i);
		pwm_fds[i] = open(path, O_WRONLY);
		if (pwm_fds[i] == -1)
		{
			// Should close the others pwm_fds? after this error the process should exit so, no
			return -1;
		}
		set_pwm_duty_cycle(pwm_fds[i], PWM_NEUTRAL_VALUE);
		if (set_pwm_enable(i, 1) == -1)
		{
			return -1;
		}
	}
}

int init_server()
{
	struct sockaddr_in so_addr;
	int fd;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
	{
		perror("socket");
		return -1;
	}

	so_addr.sin_family = AF_INET;
	so_addr.sin_port = htons(4000);
	so_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(fd, (struct sockaddr *)&so_addr, sizeof(so_addr)) == -1)
	{
		perror("bind");
		close(fd);
		return -1;
	}
	return fd;
}

void stop_vehicle(int *pwm_fds)
{
	set_pwm_duty_cycle(pwm_fds[STEERING_INDEX], PWM_NEUTRAL_VALUE);
	set_pwm_duty_cycle(pwm_fds[WHEELS_INDEX], PWM_NEUTRAL_VALUE);
}

void sync_vehicle_state(int *pwm_fds, struct vehicle_state *state)
{
	int pwm_steering_value;
	int pwm_wheels_value;

	pwm_steering_value = PWM_NEUTRAL_VALUE + (PWM_MAX_VALUE - PWM_MIN_VALUE) / 2 * state->steering / 127;
	pwm_wheels_value = PWM_NEUTRAL_VALUE + (PWM_MAX_VALUE - PWM_MIN_VALUE) / 2 * state->wheels / 127;
	set_pwm_duty_cycle(pwm_fds[STEERING_INDEX], pwm_steering_value);
	set_pwm_duty_cycle(pwm_fds[WHEELS_INDEX], pwm_wheels_value);
}

int server_loop(int *pwm_fds, int server_fd)
{
	struct sockaddr_in client_so_addr;
	socklen_t client_so_addr_size;
	struct vehicle_state state;
	fd_set rfds;
	struct timeval tv;
	int ret;

	while (1)
	{
		FD_ZERO(&rfds);
		FD_SET(server_fd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		ret = select(server_fd + 1, &rfds, NULL, NULL, &tv);
		if (ret == -1)
		{
			perror("select");
			break;
		}
		if (ret == 0)
		{
			/* In timeout case we stop the vehicle */
			stop_vehicle(pwm_fds);
		}
		else
		{
			client_so_addr_size = sizeof(client_so_addr);
			if (recvfrom(server_fd, &state, sizeof(state), 0, (struct sockaddr *)&client_so_addr, &client_so_addr_size) == -1)
			{
				perror("recvfrom");
				break;
			}
			sync_vehicle_state(pwm_fds, &state);
		}
	}
	stop_vehicle(pwm_fds);
	return EXIT_FAILURE;
}

int main()
{
	int pwm_fds[PWM_COUNT];
	int server_fd;

	if (init_pwm(pwm_fds, PWM_COUNT) == -1)
	{
		fprintf(stderr, "Can't init PWM\n");
		return EXIT_FAILURE;
	}

	server_fd = init_server();
	if (server_fd == -1)
	{
		fprintf(stderr, "Can't init server\n");
		return EXIT_FAILURE;
	}
	return server_loop(pwm_fds, server_fd);
}
