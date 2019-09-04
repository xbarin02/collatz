#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
#include <time.h>
#include <stdarg.h>
#include <limits.h>
#include <inttypes.h>
#include <netinet/tcp.h>

const uint16_t serverport = 5006;

static volatile int quit = 0;

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

	assert( nptr != NULL );

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

int write_assignment_no(int fd, uint64_t n)
{
	return write_uint64(fd, n);
}

int read_assignment_no(int fd, uint64_t *n)
{
	return read_uint64(fd, n);
}

#define TASK_SIZE 40

/* 2^32 assignments (tasks) */
#define ASSIGNMENTS_NO (UINT64_C(1) << 32)

#define MAP_SIZE (ASSIGNMENTS_NO >> 3)
#define CHECKSUMS_SIZE (ASSIGNMENTS_NO * 8)

#define IS_ASSIGNED(n) ( ( g_map_assigned[ (n)>>3 ] >> ((n)&7) ) & 1 )
#define IS_COMPLETE(n) ( ( g_map_complete[ (n)>>3 ] >> ((n)&7) ) & 1 )

#define SET_ASSIGNED(n)   ( g_map_assigned[(n)>>3] |= (1<<((n)&7)) )
#define SET_UNASSIGNED(n) ( g_map_assigned[(n)>>3] &= UCHAR_MAX ^ (1<<((n)&7)) )
#define SET_COMPLETE(n)   ( g_map_complete[(n)>>3] |= (1<<((n)&7)) )

int write_task_size(int fd)
{
	return write_uint64(fd, TASK_SIZE);
}

int read_task_size(int fd, uint64_t *task_size)
{
	return read_uint64(fd, task_size);
}

int read_overflow_counter(int fd, uint64_t *overflow_counter)
{
	return read_uint64(fd, overflow_counter);
}

int read_user_time(int fd, uint64_t *user_time)
{
	return read_uint64(fd, user_time);
}

int read_check_sum(int fd, uint64_t *check_sum)
{
	return read_uint64(fd, check_sum);
}

int read_clid(int fd, uint64_t *clid)
{
	return read_uint64(fd, clid);
}

uint64_t g_lowest_unassigned = 0; /* bit index, not byte */
uint64_t g_lowest_incomplete = 0;

unsigned char *g_map_assigned;
unsigned char *g_map_complete;
uint64_t *g_checksums;

void set_complete(uint64_t n)
{
	if (IS_COMPLETE(n)) {
		message(INFO "assignment %" PRIu64 " was already complete (duplicate result)\n", n);
	}

	if (!IS_ASSIGNED(n)) {
		message(WARN "assignment %" PRIu64 " was not assigned, discarting the result!\n", n);
		return;
	}

	SET_COMPLETE(n);

	/* advance g_lowest_incomplete pointer */
	if (n == g_lowest_incomplete) {
		for (; IS_COMPLETE(g_lowest_incomplete); ++g_lowest_incomplete)
			;
	}
}

uint64_t get_assignment()
{
	uint64_t n = g_lowest_unassigned;

	SET_ASSIGNED(n);

	/* advance g_lowest_unassigned */
	for (; IS_ASSIGNED(g_lowest_unassigned); ++g_lowest_unassigned)
		;

	return n;
}

void unset_assignment(uint64_t n)
{
	SET_UNASSIGNED(n);

	if (g_lowest_unassigned > n) {
		g_lowest_unassigned = n;
	}
}

uint64_t get_missed_assignment()
{
	uint64_t n = g_lowest_incomplete;

	SET_ASSIGNED(n);

	/* advance g_lowest_unassigned */
	if (n == g_lowest_unassigned) {
		for (; IS_ASSIGNED(g_lowest_unassigned); ++g_lowest_unassigned)
			;
	}

	return n;
}

void *open_map(const char *path)
{
	int fd = open(path, O_RDWR | O_CREAT, 0600);
	void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	if (ftruncate(fd, (off_t)MAP_SIZE) < 0) {
		perror("ftruncate");
		abort();
	}

	ptr = mmap(NULL, (size_t)MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return ptr;
}

uint64_t *open_checksums()
{
	const char *path = "checksums.dat";
	int fd = open(path, O_RDWR | O_CREAT, 0600);
	void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	if (ftruncate(fd, (off_t)CHECKSUMS_SIZE) < 0) {
		perror("ftruncate");
		abort();
	}

	ptr = mmap(NULL, (size_t)CHECKSUMS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return (uint64_t *)ptr;
}

int read_message(int fd)
{
	char msg[4];
	uint64_t clid;

	if (read_(fd, msg, 4) < 0) {
		return -1;
	}

	/* unsupported protocol */
	if (msg[3] != 0) {
		return -1;
	}

	if (strcmp(msg, "REQ") == 0) {
		/* requested assignment */
		uint64_t n;

		n = get_assignment();

		message(INFO "assignment requested: %" PRIu64 "\n", n);

		if (write_assignment_no(fd, n) < 0) {
			return -1;
		}

		if (write_task_size(fd) < 0) {
			message(ERR "unable to write task size, update the client!\n");
			return -1;
		}

		if (read_clid(fd, &clid) < 0) {
			message(WARN "client does not send client ID\n");
		}
	} else if (strcmp(msg, "RET") == 0) {
		/* returning assignment */
		uint64_t n;
		uint64_t task_size = 0;
		uint64_t overflow_counter = 0;
		uint64_t user_time = 0;
		uint64_t check_sum = 0;

		if (read_assignment_no(fd, &n) < 0) {
			return -1;
		}

		if (read_task_size(fd, &task_size) < 0) {
			message(ERR "unable to read task size, update the client!\n");
			return -1;
		}

		if (task_size != TASK_SIZE) {
			message(ERR "TASK_SIZE mismatch!\n");
			return -1;
		}

		if (read_overflow_counter(fd, &overflow_counter) < 0) {
			message(ERR "client does not send the overflow counter!\n");
			return -1;
		}

		if (read_user_time(fd, &user_time) < 0) {
			message(ERR "client does not send the user time!\n");
			return -1;
		}

		if (read_check_sum(fd, &check_sum) < 0) {
			message(ERR "client does not send the check sum!\n");
			return -1;
		}

		if (read_clid(fd, &clid) < 0) {
			message(WARN "client does not send client ID\n");
		}

		if (user_time == 0 && check_sum == 0) {
			message(ERR "broken client, discarting the result!\n");
			return -1;
		}

		if (check_sum == 0) {
			message(ERR "zero checksum is invalid!\n");
			return -1;
		}

		message(INFO "assignment returned: %" PRIu64 " (%" PRIu64 " overflows, time %" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ", checksum %" PRIu64 ")\n",
			n, overflow_counter, user_time/60/60, user_time/60%60, user_time%60, check_sum);

		set_complete(n);

		if (g_checksums[n] != 0 && g_checksums[n] != check_sum) {
			message(ERR "checksums do not match! (the other checksum is %" PRIu64 ")\n", g_checksums[n]);
		}

		g_checksums[n] = check_sum;
	} else if (strcmp(msg, "req") == 0) {
		/* requested lowest incomplete assignment */
		uint64_t n;

		n = get_missed_assignment();

		message(INFO "assignment requested: %" PRIu64 " (lowest incomplete)\n", n);

		if (write_assignment_no(fd, n) < 0) {
			return -1;
		}

		if (write_task_size(fd) < 0) {
			message(ERR "unable to write task size, update the client!\n");
			return -1;
		}

		if (read_clid(fd, &clid) < 0) {
			message(WARN "client does not send client ID\n");
		}
	} else if (strcmp(msg, "INT") == 0) {
		/* interrupted or unable to solve, unreserve the assignment */
		uint64_t n;
		uint64_t task_size = 0;

		if (read_assignment_no(fd, &n) < 0) {
			return -1;
		}

		if (read_task_size(fd, &task_size) < 0) {
			message(ERR "unable to read task size, update the client!\n");
			return -1;
		}

		if (read_clid(fd, &clid) < 0) {
			message(WARN "client does not send client ID\n");
		}

		if (task_size != TASK_SIZE) {
			message(ERR "TASK_SIZE mismatch!\n");
			return -1;
		}

		message(INFO "assignment interrupted: %" PRIu64 "\n", n);

		unset_assignment(n);
	} else if (strcmp(msg, "LOI") == 0) {
		if (write_assignment_no(fd, g_lowest_incomplete) < 0) {
			return -1;
		}

		if (write_task_size(fd) < 0) {
			return -1;
		}
	} else if (strcmp(msg, "HIR") == 0) {
		if (write_assignment_no(fd, g_lowest_unassigned-1) < 0) {
			return -1;
		}

		if (write_task_size(fd) < 0) {
			return -1;
		}
	} else {
		message(ERR "%s: unknown client message!\n", msg);
		return -1;
	}

	return 0;
}

void set_complete_range_from_hercher()
{
	uint64_t n;
	uint64_t n_max = 91226112; /* = ( 87 * 2^60 ) / 2^TASK_SIZE */

	for (n = 0; n < ASSIGNMENTS_NO; ++n) {
		if (n < n_max) {
			SET_ASSIGNED(n);
			SET_COMPLETE(n);
		} else
			break;
	}

}

int main(/*int argc, char *argv[]*/)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	int reuse = 1;

	message(INFO "starting server...\n");

	g_map_assigned = open_map("assigned.map");
	g_map_complete = open_map("complete.map");
	g_checksums = open_checksums();

	if (!IS_COMPLETE(0)) {
		message(INFO "initializing new search...\n");
		set_complete_range_from_hercher();
	}

	for (g_lowest_unassigned = 0; IS_ASSIGNED(g_lowest_unassigned); ++g_lowest_unassigned)
		;

	for (g_lowest_incomplete = 0; IS_COMPLETE(g_lowest_incomplete); ++g_lowest_incomplete)
		;

	message(INFO "lowest unassigned = %" PRIu64 "\n", g_lowest_unassigned);
	message(INFO "lowest incomplete = %" PRIu64 "\n", g_lowest_incomplete);

	message(INFO "*** all numbers below %" PRIu64 " * 2^%" PRIu64 " are convergent (blocks) ***\n", g_lowest_incomplete, TASK_SIZE);
	message(INFO "*** all numbers below %" PRIu64 " * 2^%" PRIu64 " are convergent (superblocks) ***\n", g_lowest_incomplete >> 20, TASK_SIZE + 20);

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	if (fd < 0) {
		perror("socket");
		abort();
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse, (socklen_t)sizeof(reuse)) < 0) {
		perror("setsockopt");
		abort();
	}

	/* This probably doesn't do anything at all. */
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const void *)&reuse, (socklen_t)sizeof(reuse)) < 0) {
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

	message(INFO "listening...\n");

	while (1) {
		struct sockaddr_in sockaddr_in;
		socklen_t sockaddr_len = sizeof sockaddr_in;
		int cl_fd = accept(fd, &sockaddr_in, &sockaddr_len);

		if (-1 == cl_fd) {
			if (errno == EINTR) {
				if (quit)
					break;
			}
		}
#if 0
		if (sockaddr_len >= sizeof sockaddr_in && sockaddr_in.sin_family == AF_INET) {
			message(INFO "client IPv4 %s\n", inet_ntoa(sockaddr_in.sin_addr));
		}
#endif

		if (read_message(cl_fd) < 0) {
			message(ERR "client <--> server communication failure!\n");
		}

		close(cl_fd);

		if (quit)
			break;
	}

	message(INFO "closing server socket...\n");

	close(fd);

	msync(g_map_assigned, MAP_SIZE, MS_SYNC);
	msync(g_map_complete, MAP_SIZE, MS_SYNC);
	msync(g_checksums, CHECKSUMS_SIZE, MS_SYNC);

	munmap(g_map_assigned, MAP_SIZE);
	munmap(g_map_complete, MAP_SIZE);
	munmap(g_checksums, CHECKSUMS_SIZE);

	return 0;
}
