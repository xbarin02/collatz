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

int threads_get_thread_id()
{
#ifdef _OPENMP
	return omp_get_thread_num();
#else
	return 0;
#endif
}

const char *servername = "pcbarina2.fit.vutbr.cz";
const uint16_t serverport = 5006;

const char *taskpath = "../task/task";

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
		if (t == 0 && written + t != count) {
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
		if (t == 0 && readen + t != count) {
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
	if (write_(fd, "REQ", 4) < 0) {
		return -1;
	}

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

int main(int argc, char *argv[])
{
	int threads = (argc > 1) ? atoi(argv[1]) : 1;

	assert(threads > 0);

	printf("starting %u worker threads...\n", threads);

	fflush(stdout);
	fflush(stderr);

	#pragma omp parallel num_threads(threads)
	{
		int tid = threads_get_thread_id();

		printf("thread %i: started\n", tid);

		do {
			unsigned long n;

			while (open_socket_and_request_assignment(&n) < 0) {
				fprintf(stderr, "thread %i: open_socket_and_request_assignment failed\n", tid);
				sleep(60);
			}

			printf("thread %i: got assignment %lu\n", tid, n);

			if (run_assignment(n) < 0) {
				fprintf(stderr, "thread %i: run_assignment failed\n", tid);
				continue;
			}

			while (open_socket_and_return_assignment(n) < 0) {
				fprintf(stderr, "thread %i: open_socket_and_return_assignment failed\n", tid);
				sleep(60);
			}
		} while (1);
	}

	return 0;
}
