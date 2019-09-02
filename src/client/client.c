#define _XOPEN_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#ifdef _OPENMP
	#include <omp.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>

#define SLEEP_INTERVAL 10

const char *servername = "pcbarina2.fit.vutbr.cz";
const uint16_t serverport = 5006;

const char *taskpath = "../worker/worker";

static volatile sig_atomic_t quit = 0;

void signal_handler(int i)
{
	(void)i;

	quit = 1;
}

#define ERR "ERROR: "
#define WARN "WARNING: "
#define DBG "DEBUG: "
#define INFO "INFO: "

int message(const char *format, ...)
{
	va_list ap;
	time_t now = time(NULL);
	char buf[26];
	int n;

	ctime_r(&now, buf);

	buf[strlen(buf)-1] = 0;

	n = printf("[%s] ", buf);

	va_start(ap, format);
	n += vprintf(format, ap);
	va_end(ap);

	fflush(stdout);

	return n;
}

int threads_get_thread_id()
{
#ifdef _OPENMP
	return omp_get_thread_num();
#else
	return 0;
#endif
}

int init_sockaddr(struct sockaddr_in *name, const char *hostname, uint16_t port)
{
	struct hostent *hostinfo;

	assert(name);

	bzero(name, sizeof(struct sockaddr_in));

	name->sin_family = AF_INET;
	name->sin_port = htons(port);

	hostinfo = gethostbyname(hostname);

	if (hostinfo == NULL) {
		return -1;
	}

	name->sin_addr = *(struct in_addr *) hostinfo->h_addr_list[0];

	return 0;
}

ssize_t write_(int fd, const char *buf, size_t count)
{
	ssize_t written = 0;

	while ((size_t)written < count) {
		ssize_t t = write(fd, buf+written, count - written);

		if (t < 0) {
			/* errno is set appropriately */
			return -1;
		}
		if (t == 0 && (size_t)(written + t) != count) {
			/* zero indicates nothing was written */
			return -1;
		}

		written += t;
	}

	return written;
}

ssize_t read_(int fd, char *buf, size_t count)
{
	/* I known this isn't a word in English, but "read" was already taken. */
	ssize_t readen = 0;

	while ((size_t)readen < count) {
		ssize_t t = read(fd, buf + readen, count - readen);

		if (t < 0) {
			/*  errno is set appropriately */
			return -1;
		}
		if (t == 0 && (size_t)(readen + t) != count) {
			/* zero indicates end of file */
			return -1;
		}

		readen += t;
	}

	return readen;
}

int read_assignment_no(int fd, uint64_t *n)
{
	uint32_t nh, nl;

	if (read_(fd, (void *)&nh, 4) < 0) {
		return -1;
	}

	if (read_(fd, (void *)&nl, 4) < 0) {
		return -1;
	}

	nh = ntohl(nh);
	nl = ntohl(nl);

	assert( n != NULL );

	*n = ((uint64_t)nh << 32) + nl;

	return 0;
}

int write_assignment_no(int fd, uint64_t n)
{
	uint32_t nh, nl;

	nh = (uint32_t)(n >> 32);
	nl = (uint32_t)(n);

	nh = htonl(nh);
	nl = ntohl(nl);

	if (write_(fd, (void *)&nh, 4) < 0) {
		return -1;
	}

	if (write_(fd, (void *)&nl, 4) < 0) {
		return -1;
	}

	return 0;
}

int request_assignment(int fd, unsigned long *n, int request_lowest_incomplete)
{
	if (request_lowest_incomplete) {
		if (write_(fd, "req", 4) < 0) {
			return -1;
		}
	} else {
		if (write_(fd, "REQ", 4) < 0) {
			return -1;
		}
	}

	assert( sizeof(uint64_t) == sizeof(unsigned long) );

	if (read_assignment_no(fd, (uint64_t *)n) < 0) {
		return -1;
	}

	return 0;
}

int read_task_size(int fd, unsigned long *task_size)
{
	uint32_t nh, nl;
	uint64_t n;

	if (read_(fd, (void *)&nh, 4) < 0) {
		return -1;
	}

	if (read_(fd, (void *)&nl, 4) < 0) {
		return -1;
	}

	nh = ntohl(nh);
	nl = ntohl(nl);

	n = ((uint64_t)nh << 32) + nl;

	assert( task_size != NULL );
	assert( sizeof(uint64_t) == sizeof(unsigned long) );

	*task_size = (unsigned long)n;

	return 0;
}

int write_task_size(int fd, unsigned long task_size)
{
	uint64_t n;
	uint32_t nh, nl;

	assert( sizeof(unsigned long) == sizeof(uint64_t) );

	n = (uint64_t)task_size;

	nh = (uint32_t)(n >> 32);
	nl = (uint32_t)(n);

	nh = htonl(nh);
	nl = ntohl(nl);

	if (write_(fd, (void *)&nh, 4) < 0) {
		return -1;
	}

	if (write_(fd, (void *)&nl, 4) < 0) {
		return -1;
	}

	return 0;
}

int return_assignment(int fd, unsigned long n)
{
	if (write_(fd, "RET", 4) < 0) {
		return -1;
	}

	assert( sizeof(uint64_t) == sizeof(unsigned long) );

	if (write_assignment_no(fd, (uint64_t)n) < 0) {
		return -1;
	}

	return 0;
}

int revoke_assignment(int fd, unsigned long n)
{
	if (write_(fd, "INT", 4) < 0) {
		return -1;
	}

	assert( sizeof(uint64_t) == sizeof(unsigned long) );

	if (write_assignment_no(fd, (uint64_t)n) < 0) {
		return -1;
	}

	return 0;
}

int open_socket_to_server()
{
	int fd;
	struct sockaddr_in server_addr;

	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0) {
		/* errno is set appropriately */
		return -1;
	}

	if (init_sockaddr(&server_addr, servername, serverport) < 0 ) {
		close(fd);
		return -1;
	}

	if (connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		/* errno is set appropriately */
		close(fd);
		return -1;
	}

	return fd;
}

int run_assignment(unsigned long n, unsigned long task_size)
{
	int r;
	char buffer[4096];
	char line[4096];
	char ln_part[4][64];
	FILE *output;

	if (sprintf(buffer, "%s %lu", taskpath, n) < 0) {
		return -1;
	}

	/* spawn sub-process */
	/* https://www.gnu.org/software/libc/manual/html_mono/libc.html#Pipe-to-a-Subprocess */

	output = popen(buffer, "r");

	if (!output) {
		return -1;
	}

	while (fgets(line, 4096, output)) {
		int c;

		printf("%s", line);

		c = sscanf(line, "%63s %63s %63s %63s", ln_part[0], ln_part[1], ln_part[2], ln_part[3]);

		ln_part[0][63] = 0;
		ln_part[1][63] = 0;
		ln_part[2][63] = 0;
		ln_part[3][63] = 0;

		if (c == 2 && strcmp(ln_part[0], "TASK_SIZE") == 0) {
			unsigned long worker_task_size = (unsigned long)atol(ln_part[1]);

			if (worker_task_size != task_size) {
				message(ERR "worker uses different TASK_SIZE (%lu), whereas %lu is required\n", worker_task_size, task_size);
				return -1;
			}
		} else if (c == 2 && strcmp(ln_part[0], "TASK_ID") == 0) {
			unsigned long task_id = (unsigned long)atol(ln_part[1]);

			if (n != task_id) {
				message(ERR "client <---> worker communication problem!\n");
				return -1;
			}
		} else if (c == 3 && strcmp(ln_part[0], "OVERFLOW") == 0) {
			unsigned long size = (unsigned long)atol(ln_part[1]);
			unsigned long overflow_counter = (unsigned long)atol(ln_part[2]);

			if (size == 128) {
				/* TODO overflow_counter must be returned to server */
			}
		} else if (c == 1 && strcmp(ln_part[0], "HALTED") == 0) {
			/* this was expected */
			(void)0;
		} /* other cases... */
	}

	r = pclose(output);

	if (r == -1) {
		return -1;
	}

	if (WIFEXITED(r)) {
		/* the child terminated normally */
		return 0;

	} else {
		return -1;
	}
}

int open_socket_and_request_assignment(unsigned long *n, int request_lowest_incomplete, unsigned long *server_task_size)
{
	int fd;
	unsigned long task_size = 0;

	fd = open_socket_to_server();

	if (fd < 0) {
		return -1;
	}

	/* give me the assignment */
	if (request_assignment(fd, n, request_lowest_incomplete) < 0) {
		close(fd);
		return -1;
	}

	/* try to read TASK_SIZE */
	if (read_task_size(fd, &task_size) < 0) {
		message(ERR "server does not send the TASK_SIZE\n");
		close(fd);
		return -1;
	}

	if (task_size == 0) {
		message(ERR "server sent zero TASK_SIZE\n");
		close(fd);
		return -1;
	}

	close(fd);

	assert( server_task_size != NULL );

	*server_task_size = task_size;

	return 0;
}

int open_socket_and_return_assignment(unsigned long n, unsigned long task_size)
{
	int fd;

	fd = open_socket_to_server();

	if (fd < 0) {
		return -1;
	}

	if (return_assignment(fd, n) < 0) {
		close(fd);
		return -1;
	}

	/* write TASK_SIZE */
	if (write_task_size(fd, task_size) < 0) {
		message(ERR "server does not receive the TASK_SIZE\n");
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

int open_socket_and_revoke_assignment(unsigned long n, unsigned long task_size)
{
	int fd;

	fd = open_socket_to_server();

	if (fd < 0) {
		return -1;
	}

	if (revoke_assignment(fd, n) < 0) {
		close(fd);
		return -1;
	}

	/* write TASK_SIZE */
	if (write_task_size(fd, task_size) < 0) {
		message(ERR "server does not receive the TASK_SIZE\n");
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

int main(int argc, char *argv[])
{
	int threads;
	int opt;
	int one_shot = 0;
	int request_lowest_incomplete = 0;

	while ((opt = getopt(argc, argv, "1l")) != -1) {
		switch (opt) {
			case '1':
				one_shot = 1;
				break;
			case 'l':
				request_lowest_incomplete = 1;
				break;
			default:
				message(ERR "Usage: %s [-1] num_threads\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	threads = (optind < argc) ? atoi(argv[optind]) : 1;

	assert(threads > 0);

	if (one_shot) {
		message(INFO "one shot mode activated!\n");
	}

	message(INFO "starting %u worker threads...\n", threads);

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGALRM, signal_handler);
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);

	#pragma omp parallel num_threads(threads)
	{
		int tid = threads_get_thread_id();

		message(INFO "thread %i: started\n", tid);

		while (!quit) {
			unsigned long n;
			unsigned long task_size = 0;

			while (open_socket_and_request_assignment(&n, request_lowest_incomplete, &task_size) < 0) {
				message(ERR "thread %i: open_socket_and_request_assignment failed\n", tid);
				sleep(SLEEP_INTERVAL);
			}

			message(INFO "thread %i: got assignment %lu\n", tid, n);

			if (run_assignment(n, task_size) < 0) {
				message(ERR "thread %i: run_assignment failed\n", tid);

				while (open_socket_and_revoke_assignment(n, task_size) < 0) {
					message(ERR "thread %i: open_socket_and_revoke_assignment failed\n", tid);
					sleep(SLEEP_INTERVAL);
				}

				if (one_shot)
					break;

				continue;
			}

			while (open_socket_and_return_assignment(n, task_size) < 0) {
				message(ERR "thread %i: open_socket_and_return_assignment failed\n", tid);
				sleep(SLEEP_INTERVAL);
			}

			if (one_shot)
				break;
		}
	}

	message(INFO "client has been halted\n");

	return EXIT_SUCCESS;
}
