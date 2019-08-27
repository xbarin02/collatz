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
		fprintf(stderr, "gethostbyname failed: %s\n", servername);
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
			perror("write");
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
			perror("read");
			return -1;
		}

		readen += t;
	}

	return readen;
}

int read_assignment_no(int fd, unsigned long *assignment)
{
	uint32_t nh, nl; /* high and low part of the uint64_t assignment */
	uint64_t n;

	if (read_(fd, (void *)&nh, 4) < 0) {
		return -1;
	}
	nh = ntohl(nh);
	if (read_(fd, (void *)&nl, 4) < 0) {
		return -1;
	}
	nl = ntohl(nl);

	n = ((uint64_t)nh << 32) + nl;

	assert( sizeof(unsigned long) == sizeof(uint64_t) );

	assert( assignment != NULL );

	*assignment = (unsigned long)n;

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
			int fd;
			struct sockaddr_in server_addr;
			unsigned long n;
			char buffer[4096];
			int r;

			fd = socket(AF_INET, SOCK_STREAM, 0);

			if (fd < 0) {
				/* errno is set appropriately */
				fprintf(stderr, "thread %i: socket: %s\n", tid, strerror(errno));
				abort();
			}

			if (init_sockaddr(&server_addr, servername, serverport) < 0 ) {
				fprintf(stderr, "thread %i: init_sockaddr failed\n", tid);
				abort();
			}

			if (connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
				/* errno is set appropriately */
				fprintf(stderr, "thread %i: connect: %s\n", tid, strerror(errno));
				abort();
			}

			/* give me the assignment */
			if (write_(fd, "REQ", 4) < 0) {
				fprintf(stderr, "thread %i: write_ failed\n", tid);
				abort();
			}

			/* get assignment from server */
			if (read_assignment_no(fd, &n) < 0) {
				fprintf(stderr, "thread %i: read_assignment_no failed\n", tid);
				abort();
			}

			printf("thread %i: got assignment %lu\n", tid, n);

			if (sprintf(buffer, "%s %lu", taskpath, n) < 0) {
				fprintf(stderr, "thread %i: sprintf failed\n", tid);
				abort();
			}

			/* spawn sub-process */
			r = system(buffer);

			if (r == -1) {
				fprintf(stderr, "thread %i: system: %s\n", tid, taskpath);
				abort();
			}

			if (WIFEXITED(r)) {
				/* the child terminated normally */
				printf("thread %i: task result: %i\n", tid, WEXITSTATUS(r));
				/* TODO send the result back to server */

			} else {
				printf("thread %i: task failed\n", tid);
			}

			close(fd);
		} while (1);
	}

	return 0;
}
