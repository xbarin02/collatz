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
#include <sys/time.h>
#include <sys/resource.h>

const uint16_t serverport = 5006;

static volatile sig_atomic_t quit = 0;

static int fd;

void signal_handler(int i)
{
	(void)i;

	quit = 1;

	shutdown(fd, SHUT_RDWR);
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
#define USERTIMES_SIZE (ASSIGNMENTS_NO * 8)
#define OVERFLOWS_SIZE (ASSIGNMENTS_NO * 8)
#define CLIENTIDS_SIZE (ASSIGNMENTS_NO * 8)

#define IS_ASSIGNED(n) ( ( g_map_assigned[ (n)>>3 ] >> ((n)&7) ) & 1 )
#define IS_COMPLETE(n) ( ( g_map_complete[ (n)>>3 ] >> ((n)&7) ) & 1 )

#define SET_ASSIGNED(n)   ( g_map_assigned[(n)>>3] |= (1<<((n)&7)) )
#define SET_UNASSIGNED(n) ( g_map_assigned[(n)>>3] &= UCHAR_MAX ^ (1<<((n)&7)) )
#define SET_COMPLETE(n)   ( g_map_complete[(n)>>3] |= (1<<((n)&7)) )
#define SET_INCOMPLETE(n) ( g_map_complete[(n)>>3] &= UCHAR_MAX ^ (1<<((n)&7)) )

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
uint64_t *g_usertimes;
uint64_t *g_overflows;
uint64_t *g_clientids;

int set_complete(uint64_t n)
{
	if (IS_COMPLETE(n)) {
		message(INFO "assignment %" PRIu64 " was already complete (duplicate result)\n", n);
	}

	if (!IS_ASSIGNED(n)) {
		message(ERR "assignment %" PRIu64 " was not assigned, discarting the result!\n", n);
		return -1;
	}

	SET_COMPLETE(n);

	/* advance g_lowest_incomplete pointer */
	if (n == g_lowest_incomplete) {
		for (; IS_COMPLETE(g_lowest_incomplete); ++g_lowest_incomplete)
			;
	}

	return 0;
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
	if (IS_COMPLETE(n)) {
		message(WARN "assignment %" PRIu64 " is already complete, invalid interrupt request!\n", n);
	}

	SET_UNASSIGNED(n);

	if (g_lowest_unassigned > n) {
		g_lowest_unassigned = n;
	}
}

uint64_t get_missed_assignment(int thread_id)
{
	uint64_t n = g_lowest_incomplete;
	int t;

	for (t = 0; t < thread_id; ++t) {
		n++;
		while (IS_COMPLETE(n)) {
			n++;
		}
	}

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

uint64_t *open_usertimes()
{
	const char *path = "usertimes.dat";
	int fd = open(path, O_RDWR | O_CREAT, 0600);
	void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	if (ftruncate(fd, (off_t)USERTIMES_SIZE) < 0) {
		perror("ftruncate");
		abort();
	}

	ptr = mmap(NULL, (size_t)USERTIMES_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return (uint64_t *)ptr;
}

uint64_t *open_overflows()
{
	const char *path = "overflows.dat";
	int fd = open(path, O_RDWR | O_CREAT, 0600);
	void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	if (ftruncate(fd, (off_t)OVERFLOWS_SIZE) < 0) {
		perror("ftruncate");
		abort();
	}

	ptr = mmap(NULL, (size_t)OVERFLOWS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return (uint64_t *)ptr;
}

uint64_t *open_clientids()
{
	const char *path = "clientids.dat";
	int fd = open(path, O_RDWR | O_CREAT, 0600);
	void *ptr;

	if (fd < 0) {
		perror("open");
		abort();
	}

	if (ftruncate(fd, (off_t)CLIENTIDS_SIZE) < 0) {
		perror("ftruncate");
		abort();
	}

	ptr = mmap(NULL, (size_t)CLIENTIDS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return (uint64_t *)ptr;
}

int read_message(int fd, int thread_id, const char *ipv4)
{
	char msg[4];

	if (read_(fd, msg, 4) < 0) {
		return -1;
	}

	/* unsupported protocol */
	if (msg[3] != 0) {
		return -1;
	}

	if (strcmp(msg, "MUL") == 0) {
		uint64_t threads;
		int tid;

		read_uint64(fd, &threads);

		message(INFO "received multiple requests for %" PRIu64 " threads from address %s\n", threads, ipv4);

		assert(threads < INT_MAX);

		for (tid = 0; tid < (int)threads; ++tid) {
			if (read_message(fd, tid, ipv4) < 0) {
				message(ERR "cannot completely process the MUL request\n");
				return -1;
			}
		}
	} else if (strcmp(msg, "REQ") == 0) {
		/* requested assignment */
		uint64_t n;
		uint64_t clid = 0;

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
			message(ERR "client does not send client ID\n");
			unset_assignment(n);
			return -1;
		}

		if (g_clientids[n] != 0) {
			message(WARN "assignment %" PRIu64 " was already assigned to another client, re-assigning\n", n);
		}

		g_clientids[n] = clid;
	} else if (strcmp(msg, "RET") == 0) {
		/* returning assignment */
		uint64_t n;
		uint64_t task_size = 0;
		uint64_t overflow_counter = 0;
		uint64_t user_time = 0;
		uint64_t checksum = 0;
		uint64_t clid = 0;

		if (read_assignment_no(fd, &n) < 0) {
			return -1;
		}

		if (read_task_size(fd, &task_size) < 0) {
			message(ERR "unable to read task size, update the client!\n");
			return -1;
		}

		if (task_size != TASK_SIZE) {
			message(ERR "TASK_SIZE mismatch! (client sent %" PRIu64 ")\n", task_size);
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

		if (read_check_sum(fd, &checksum) < 0) {
			message(ERR "client does not send the check sum!\n");
			return -1;
		}

		if (read_clid(fd, &clid) < 0) {
			message(ERR "client does not send client ID\n");
			return -1;
		}

		if (g_clientids[n] != clid) {
			message(WARN "assignment %" PRIu64 " was assigned to another client, ignoring the result!\n", n);
			/* this can however be part of MUL request, so do not return -1 */
			return 0;
		}

		if (clid == 42) {
			message(WARN "client ID is 42 :(\n");
		}

		if (user_time == 0 && checksum == 0) {
			message(ERR "broken client, discarting the result!\n");
			return -1;
		}

		if (checksum == 0) {
			message(ERR "zero checksum is invalid! (assignment %" PRIu64 ")\n", n);
			return -1;
		}

		if ((checksum>>23) != 196126 && (checksum>>28) != 0xff5) {
			message(ERR "suspicious checksum (%" PRIu64 ", 0x%" PRIx64 "), done in %" PRIu64 " secs, rejecting the result! (assignment %" PRIu64 ")\n",
				checksum, checksum, user_time, n);
			return -1;
		}

		if ((checksum>>28) == 0xff5 && user_time < 30*60) {
			message(ERR "new checksum is not allowed for GPU (%" PRIu64 ", 0x%" PRIx64 "), done in %" PRIu64 " secs, rejecting the result! (assignment %" PRIu64 ")\n",
				checksum, checksum, user_time, n);
			return 0;
		}

		message(INFO "assignment returned: %" PRIu64 " (%" PRIu64 " overflows, time %" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ", checksum 0x%016" PRIx64 ")\n",
			n, overflow_counter, user_time/60/60, user_time/60%60, user_time%60, checksum);

		if (set_complete(n) < 0) {
			message(ERR "result rejected!\n");
			/* this can however be part of MUL request, so do not return -1 */
			return 0;
		}

		if (g_checksums[n] != 0 && g_checksums[n] != checksum) {
			message(ERR "checksums do not match! (the other checksum was %" PRIu64 ", 0x%016" PRIx64 ")\n", g_checksums[n], g_checksums[n]);
		}

		g_checksums[n] = checksum;

		g_usertimes[n] = user_time;

		g_overflows[n] = overflow_counter;

		g_clientids[n] = 0;
	} else if (strcmp(msg, "req") == 0) {
		/* requested lowest incomplete assignment */
		uint64_t n;
		uint64_t clid = 0;

		n = get_missed_assignment(thread_id);

		message(INFO "assignment requested: %" PRIu64 " (lowest incomplete +%i)\n", n, thread_id);

		if (write_assignment_no(fd, n) < 0) {
			return -1;
		}

		if (write_task_size(fd) < 0) {
			message(ERR "unable to write task size, update the client!\n");
			return -1;
		}

		if (read_clid(fd, &clid) < 0) {
			message(ERR "client does not send client ID\n");
			return -1;
		}

		if (g_clientids[n] != 0) {
			message(WARN "re-assigning the assignment\n");
		}

		g_clientids[n] = clid;
	} else if (strcmp(msg, "INT") == 0) {
		/* interrupted or unable to solve, unreserve the assignment */
		uint64_t n;
		uint64_t task_size = 0;
		uint64_t clid = 0;

		if (read_assignment_no(fd, &n) < 0) {
			return -1;
		}

		if (read_task_size(fd, &task_size) < 0) {
			message(ERR "unable to read task size, update the client!\n");
			return -1;
		}

		if (read_clid(fd, &clid) < 0) {
			message(ERR "client does not send client ID\n");
			return -1;
		}

		if (g_clientids[n] != clid) {
			message(WARN "invalid request, assignment was assigned to another client, ignoring the request!\n");
			/* this can be part of MUL request, so do not return -1 */
			return 0;
		}

		if (task_size != TASK_SIZE) {
			message(ERR "TASK_SIZE mismatch!\n");
			return -1;
		}

		message(INFO "assignment interrupted: %" PRIu64 "\n", n);

		unset_assignment(n);

		g_clientids[n] = 0;
	} else if (strcmp(msg, "LOI") == 0) {
		if (write_assignment_no(fd, g_lowest_incomplete) < 0) {
			return -1;
		}

		if (write_task_size(fd) < 0) {
			return -1;
		}
	} else if (strcmp(msg, "HIR") == 0) {
		if (write_assignment_no(fd, g_lowest_unassigned) < 0) {
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

int main(int argc, char *argv[])
{
	struct sockaddr_in server_addr;
	int reuse = 1;
	struct rlimit rlim;
	int opt;
	int clear_incomplete_assigned = 0;
	int fix_records = 0;
	int invalidate_overflows = 0;
	int invalidate_new = 0;

	fd = socket(AF_INET, SOCK_STREAM, 0);

	while ((opt = getopt(argc, argv, "cfiz")) != -1) {
		switch (opt) {
			case 'c':
				clear_incomplete_assigned = 1;
				break;
			case 'f':
				fix_records = 1;
				break;
			case 'i':
				invalidate_overflows = 1;
				break;
			case 'z':
				invalidate_new = 1;
				break;
			default:
				message(ERR "Usage: %s [-c]\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	if (getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
		/* errno is set appropriately. */
		perror("getrlimit");
	} else {
		assert( sizeof(uint64_t) >= sizeof(rlim_t) );

		message(INFO "limit file descriptors: %" PRIu64 " (hard %" PRIu64 ")\n", rlim.rlim_cur, rlim.rlim_max);
	}

	message(INFO "starting server...\n");

	g_map_assigned = open_map("assigned.map");
	g_map_complete = open_map("complete.map");
	g_checksums = open_checksums();
	g_usertimes = open_usertimes();
	g_overflows = open_overflows();
	g_clientids = open_clientids();

	if (!IS_COMPLETE(0)) {
		message(INFO "initializing new search...\n");
		set_complete_range_from_hercher();
	}

	if (invalidate_overflows) {
		uint64_t n;

		message(WARN "Invalidating overflows...\n");

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t overflow = g_overflows[n];

			if (overflow != 0) {
				printf("- resetting the assignment %" PRIu64 " due to overflow\n", n);

				SET_UNASSIGNED(n);
				SET_INCOMPLETE(n);
			}
		}
	}

	if (invalidate_new) {
		uint64_t n;

		message(WARN "Invalidating new checksums...\n");

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			uint64_t checksum = g_checksums[n];

			if ((checksum>>28) == 0xff5) {
				printf("- resetting the assignment %" PRIu64 " due to new checksum\n", n);

				SET_UNASSIGNED(n);
				SET_INCOMPLETE(n);
			}
		}
	}

	/* fix records the *.map and *.dat */
	if (fix_records) {
		uint64_t n;
		uint64_t c0 = 0, c1 = 0, c2 = 0;

		message(WARN "Processing the records...\n");

		for (n = 0; n < ASSIGNMENTS_NO; ++n) {
			/* complete ==> assigned */
			if (IS_COMPLETE(n) && !IS_ASSIGNED(n)) {
				SET_ASSIGNED(n);
				c0++;
			}

			/* complete ==> zero clid */
			if (IS_COMPLETE(n) && g_clientids[n] != 0) {
				g_clientids[n] = 0;
				c1++;
			}

			/* not assigned ==> no clid */
			if (!IS_ASSIGNED(n) && g_clientids[n] != 0) {
				g_clientids[n] = 0;
				c2++;
			}
		}

		message(WARN "These corrections have been made: %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", c0, c1, c2);
	}

	for (g_lowest_unassigned = 0; IS_ASSIGNED(g_lowest_unassigned); ++g_lowest_unassigned)
		;

	for (g_lowest_incomplete = 0; IS_COMPLETE(g_lowest_incomplete); ++g_lowest_incomplete)
		;

	if (clear_incomplete_assigned) {
		message(WARN "incomplete assignments will be cleared...\n");

		while (g_lowest_unassigned > g_lowest_incomplete) {
			g_lowest_unassigned--;
			if (IS_ASSIGNED(g_lowest_unassigned) && !IS_COMPLETE(g_lowest_unassigned)) {
				SET_UNASSIGNED(g_lowest_unassigned);
			}
		}

		message(WARN "incomplete assignments have been cleared!\n");
	}

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

	/* TCP_NODELAY have to be either enabled or disabled on both sides (server as well as client), not just on one side! */
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
		const char *ipv4 = "(unknown)";

		if (-1 == cl_fd) {
			if (quit)
				break;

			message(ERR "cannot accept a connection on a socket!\n");
		}

		if (sockaddr_len >= sizeof sockaddr_in && sockaddr_in.sin_family == AF_INET) {
			ipv4 = inet_ntoa(sockaddr_in.sin_addr);
		}

		if (read_message(cl_fd, 0, ipv4) < 0) {
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
	msync(g_usertimes, USERTIMES_SIZE, MS_SYNC);
	msync(g_overflows, OVERFLOWS_SIZE, MS_SYNC);
	msync(g_clientids, CLIENTIDS_SIZE, MS_SYNC);

	munmap(g_map_assigned, MAP_SIZE);
	munmap(g_map_complete, MAP_SIZE);
	munmap(g_checksums, CHECKSUMS_SIZE);
	munmap(g_usertimes, USERTIMES_SIZE);
	munmap(g_overflows, OVERFLOWS_SIZE);
	munmap(g_clientids, CLIENTIDS_SIZE);

	return 0;
}
