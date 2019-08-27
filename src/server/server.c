#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

const uint16_t serverport = 5006;

static volatile int quit = 0;

void sigint_handler(int i)
{
	(void)i;

	quit = 1;
}

int init_sockaddr(struct sockaddr_in *name, uint16_t port)
{
	assert(name);

	bzero(name, sizeof(struct sockaddr_in));

	name->sin_family = AF_INET;
	name->sin_addr.s_addr = INADDR_ANY;
	name->sin_port = htons(port);

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

/* 2^32 assignments */
#define ASSIGNMENTS_NO (1UL<<32)

void *g_map_assigned;

void *open_map_assigned()
{
	int fd = open("assigned.map", O_RDWR | O_CREAT, 0600);
	void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	if (ftruncate(fd, (ASSIGNMENTS_NO>>3)) < 0) {
		perror("ftruncate");
		abort();
	}

	ptr = mmap(NULL, (ASSIGNMENTS_NO>>3), PROT_READ | PROT_WRITE, MAP_SHARED /*| MAP_POPULATE*/, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return ptr;
}

int read_message(int fd)
{
	char msg[4];

	if (read_(fd, msg, 4) < 0) {
		return -1;
	}

	/* unsupported protocol */
	if (msg[3] != 0) {
		return -1;
	}

	if (strcmp(msg, "REQ") == 0) {
		/* requested assignment */
		unsigned long n = rand()%256;

		printf("assignment requested: %lu\n", n);

		assert( sizeof(uint64_t) == sizeof(unsigned long) );

		if (write_assignment_no(fd, (uint64_t)n) < 0) {
			return -1;
		}
	} else if (strcmp(msg, "RET") == 0) {
		unsigned long n;
		/* returning assignment */

		assert( sizeof(uint64_t) == sizeof(unsigned long) );

		if (read_assignment_no(fd, (uint64_t *)&n) < 0) {
			return -1;
		}

		printf("assignment returned: %lu\n", n);
	} else {
		printf("%s: unknown client message!\n", msg);
		return -1;
	}

	return 0;
}

int main(/*int argc, char *argv[]*/)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	int reuse = 1;

	printf("starting server...\n");

	g_map_assigned = open_map_assigned();

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	if (fd < 0) {
		perror("socket");
		abort();
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse, (socklen_t)sizeof(reuse)) < 0) {
		perror("setsockopt");
		abort();
	}

	init_sockaddr(&server_addr, serverport);

	if (bind(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		perror("bind");
		abort();
	}

	if (listen(fd, 10) < 0) {
		perror("listen");
		abort();
	}

	printf("listening...\n");

	while (1) {
		int cl_fd = accept(fd, NULL, NULL);

		if (-1 == cl_fd) {
			if (errno == EINTR) {
				if (quit)
					break;
			}
		}

		printf("client connected\n");

		if (read_message(cl_fd) < 0) {
			fprintf(stderr, "client <--> server communication failure!\n");
		}

		close(cl_fd);

		if (quit)
			break;
	}

	printf("closing server socket...\n");

	close(fd);

	munmap(g_map_assigned, (ASSIGNMENTS_NO>>3));

	return 0;
}
