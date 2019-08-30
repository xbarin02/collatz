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

static volatile sig_atomic_t quit = 0;

void sigint_handler(int i)
{
	(void)i;

	quit = 1;
}

int threads_get_thread_id()
{
#ifdef _OPENMP
	return omp_get_thread_num();
#else
	return 0;
#endif
}

#define REQUEST_LOWEST_INCOMPLETE 0

const char *servername = "pcbarina2.fit.vutbr.cz";
const uint16_t serverport = 5006;

const char *taskpath = "../worker/worker";

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
	ssize_t readen = 0; /* read was already taken */

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

int request_assignment(int fd, unsigned long *n)
{
#if (REQUEST_LOWEST_INCOMPLETE == 1)
	if (write_(fd, "req", 4) < 0) {
		return -1;
	}
#else
	if (write_(fd, "REQ", 4) < 0) {
		return -1;
	}
#endif

	assert( sizeof(uint64_t) == sizeof(unsigned long) );

	if (read_assignment_no(fd, (uint64_t *)n) < 0) {
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

int run_assignment(unsigned long n)
{
	int r;
	char buffer[4096];

	if (sprintf(buffer, "%s %lu", taskpath, n) < 0) {
		return -1;
	}

	/* spawn sub-process */
	r = system(buffer);

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

int open_socket_and_request_assignment(unsigned long *n)
{
	int fd;

	fd = open_socket_to_server();

	if (fd < 0) {
		return -1;
	}

	/* give me the assignment */
	if (request_assignment(fd, n) < 0) {
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

int open_socket_and_return_assignment(unsigned long n)
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

	close(fd);

	return 0;
}

int open_socket_and_revoke_assignment(unsigned long n)
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

	close(fd);

	return 0;
}

int main(int argc, char *argv[])
{
	int threads;
	int opt;
	int one_shot = 0;

	while ((opt = getopt(argc, argv, "1")) != -1) {
		switch (opt) {
			case '1':
				one_shot = 1;
				break;
			default:
				fprintf(stderr, "Usage: %s [-1] num_threads\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	threads = (optind < argc) ? atoi(argv[optind]) : 1;

	assert(threads > 0);

	if (one_shot) {
		printf("one shot mode activated!\n");
	}

	printf("starting %u worker threads...\n", threads);

	fflush(stdout);
	fflush(stderr);

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	#pragma omp parallel num_threads(threads)
	{
		int tid = threads_get_thread_id();

		printf("thread %i: started\n", tid);

		while (!quit) {
			unsigned long n;

			while (open_socket_and_request_assignment(&n) < 0) {
				fprintf(stderr, "thread %i: open_socket_and_request_assignment failed\n", tid);
				fflush(stderr);
				sleep(15);
			}

			printf("thread %i: got assignment %lu\n", tid, n);
			fflush(stdout);

			if (run_assignment(n) < 0) {
				fprintf(stderr, "thread %i: run_assignment failed\n", tid);
				fflush(stderr);

				while (open_socket_and_revoke_assignment(n) < 0) {
					fprintf(stderr, "thread %i: open_socket_and_revoke_assignment failed\n", tid);
					fflush(stderr);
					sleep(15);
				}

				if (one_shot)
					break;
				continue;
			}

			while (open_socket_and_return_assignment(n) < 0) {
				fprintf(stderr, "thread %i: open_socket_and_return_assignment failed\n", tid);
				fflush(stderr);
				sleep(15);
			}

			if (one_shot)
				break;
		}
	}

	printf("client has been halted\n");

	return EXIT_SUCCESS;
}
