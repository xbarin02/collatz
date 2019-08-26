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

void init_sockaddr(struct sockaddr_in *name, const char *hostname, uint16_t port)
{
	struct hostent *hostinfo;

	assert(name);

	bzero(name, sizeof(struct sockaddr_in));

	name->sin_family = AF_INET;
	name->sin_port = htons(port);

	hostinfo = gethostbyname(hostname);

	if (hostinfo == NULL) {
		fprintf(stderr, "gethostbyname failed: %s\n", servername);
		abort();
	}

	name->sin_addr = *(struct in_addr *) hostinfo->h_addr_list[0];
}

ssize_t write_(int fd, const char *buf, size_t count)
{
	ssize_t written = 0;

	while ((size_t)written < count) {
		ssize_t t = write(fd, buf+written, count - written);

		if (t < 0) {
			/* errno is set appropriately */
			perror("write");
			abort();
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
			abort();
		}

		readen += t;
	}

	return readen;
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
			int fd = socket(AF_INET, SOCK_STREAM, 0);
			struct sockaddr_in server_addr;
			uint32_t nh, nl; /* high and low part of the uint64_t assignment */
			uint64_t n;
			char buffer[4096];
			int r;

			if (fd < 0) {
				/* errno is set appropriately */
				fprintf(stderr, "thread %i: socket: %s\n", tid, strerror(errno));
				abort();
			}

			init_sockaddr(&server_addr, servername, serverport);

			if (connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
				/* errno is set appropriately */
				fprintf(stderr, "thread %i: connect: %s\n", tid, strerror(errno));
				abort();
			}

			/* give me the assignment */
			write_(fd, "REQ", 4);

			/* get assignment from server */
			read_(fd, (void *)&nh, 4);
			nh = ntohl(nh);
			read_(fd, (void *)&nl, 4);
			nl = ntohl(nl);
			n = ((uint64_t)nh << 32) + nl;

			assert( sizeof(unsigned long) == sizeof(uint64_t) );

			printf("thread %i: got assignment %lu\n", tid, (unsigned long)n);

			/* TODO spawn sub-process */
			sprintf(buffer, "%s %lu", taskpath, (unsigned long)n);

			r = system(buffer);

			if (r == -1) {
				fprintf(stderr, "thread %i: system: %s\n", tid, taskpath);
				abort();
			}

			printf("thread %i: task result: %i\n", tid, WEXITSTATUS(r));

			/* TODO send the result back to server */

			close(fd);
		} while (1);
	}

	return 0;
}
