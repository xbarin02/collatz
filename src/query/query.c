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
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/tcp.h>

const char *servername = "localhost";
const uint16_t serverport = 5006;

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

	#pragma omp critical
	{
		n = printf("[%s] ", buf);

		va_start(ap, format);
		n += vprintf(format, ap);
		va_end(ap);

		fflush(stdout);
	}

	return n;
}

int init_sockaddr(struct sockaddr_in *name, const char *hostname, uint16_t port)
{
	struct hostent *hostinfo;

	assert(name != NULL);

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

int read_uint64(int fd, uint64_t *nptr)
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

	assert(nptr != NULL);

	*nptr = n;

	return 0;
}

int write_uint64(int fd, uint64_t n)
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

/* DEPRECATED */
int read_ul(int fd, unsigned long *nptr)
{
	assert(sizeof(unsigned long) == sizeof(uint64_t));

	return read_uint64(fd, (uint64_t *)nptr);
}

/* DEPRECATED */
int write_ul(int fd, unsigned long n)
{
	assert(sizeof(unsigned long) == sizeof(uint64_t));

	return write_uint64(fd, (uint64_t)n);
}

int read_assignment_no(int fd, uint64_t *n)
{
	return read_uint64(fd, n);
}

int read_task_size(int fd, uint64_t *task_size)
{
	return read_uint64(fd, task_size);
}

int query_lowest_incomplete(int fd)
{
	if (write_(fd, "LOI", 4) < 0) {
		return -1;
	}

	return 0;
}

int query_highest_requested(int fd)
{
	if (write_(fd, "HIR", 4) < 0) {
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

int open_socket_and_query_lowest_incomplete(uint64_t *n, uint64_t *task_size)
{
	int fd;

	fd = open_socket_to_server();

	if (fd < 0) {
		return -1;
	}

	if (query_lowest_incomplete(fd) < 0) {
		close(fd);
		return -1;
	}

	if (read_assignment_no(fd, n) < 0) {
		close(fd);
		return -1;
	}

	if (read_task_size(fd, task_size) < 0) {
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

int open_socket_and_query_highest_requested(uint64_t *n, uint64_t *task_size)
{
	int fd;

	fd = open_socket_to_server();

	if (fd < 0) {
		return -1;
	}

	if (query_highest_requested(fd) < 0) {
		close(fd);
		return -1;
	}

	if (read_assignment_no(fd, n) < 0) {
		close(fd);
		return -1;
	}

	if (read_task_size(fd, task_size) < 0) {
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

int main(int argc, char *argv[])
{
	int opt;
	int query = 0;

	if (getenv("SERVER_NAME")) {
		servername = getenv("SERVER_NAME");
	}

	message(INFO "server to be used: %s\n", servername);

	while ((opt = getopt(argc, argv, "lh")) != -1) {
		switch (opt) {
			case 'l':
				query = opt;
				break;
			case 'h':
				query = opt;
				break;
			default:
				message(ERR "Usage: %s [-l|-h]\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	switch (query) {
			uint64_t n;
			uint64_t task_size;
		case 'l':
			if (open_socket_and_query_lowest_incomplete(&n, &task_size) < 0) {
				message(ERR "open_socket_and_query_lowest_incomplete failed\n");
			} else {
				printf("%" PRIu64 " %" PRIu64 "\n", n, task_size);
			}
			break;
		case 'h':
			if (open_socket_and_query_highest_requested(&n, &task_size) < 0) {
				message(ERR "open_socket_and_query_highest_requested failed\n");
			} else {
				printf("%" PRIu64 " %" PRIu64 "\n", n, task_size);
			}
			break;
	}

	return EXIT_SUCCESS;
}
