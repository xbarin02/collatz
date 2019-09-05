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
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/tcp.h>

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

/* DEPRECATED */
int read_ul(int fd, unsigned long *nptr)
{
	assert( sizeof(unsigned long) == sizeof(uint64_t) );

	return read_uint64(fd, (uint64_t *)nptr);
}

/* DEPRECATED */
int write_ul(int fd, unsigned long n)
{
	assert( sizeof(unsigned long) == sizeof(uint64_t) );

	return write_uint64(fd, (uint64_t)n);
}

int write_clid(int fd, uint64_t clid)
{
	return write_uint64(fd, clid);
}

int read_assignment_no(int fd, uint64_t *n)
{
	return read_uint64(fd, n);
}

int write_assignment_no(int fd, uint64_t n)
{
	return write_uint64(fd, n);
}

int read_task_size(int fd, uint64_t *task_size)
{
	return read_uint64(fd, task_size);
}

int write_task_size(int fd, uint64_t task_size)
{
	return write_uint64(fd, task_size);
}

int write_overflow_counter(int fd, uint64_t overflow_counter)
{
	return write_uint64(fd, overflow_counter);
}

int write_user_time(int fd, uint64_t user_time)
{
	return write_uint64(fd, user_time);
}

int write_checksum(int fd, uint64_t checksum)
{
	return write_uint64(fd, checksum);
}

int open_socket_to_server()
{
	int fd;
	struct sockaddr_in server_addr;
	int i = 1;

	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0) {
		/* errno is set appropriately */
		return -1;
	}

	/* This probably doesn't do anything at all. */
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof i) < 0) {
		close(fd);
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

/* DEPRECATED */
static unsigned long atoul(const char *nptr)
{
	return strtoul(nptr, NULL, 10);
}

static uint64_t atou64(const char *nptr)
{
	assert( sizeof(uint64_t) == sizeof(unsigned long) );

	return (uint64_t)atoul(nptr);
}

int run_assignment(uint64_t n, uint64_t task_size, uint64_t *p_overflow_counter, uint64_t *p_user_time, uint64_t *p_checksum)
{
	int r;
	char buffer[4096];
	char line[4096];
	char ln_part[4][64];
	FILE *output;
	int success = 0;

	if (sprintf(buffer, "%s %" PRIu64, taskpath, n) < 0) {
		return -1;
	}

	output = popen(buffer, "r");

	if (!output) {
		return -1;
	}

	while (fgets(line, 4096, output)) {
		int c;
#if 0
		printf("%s", line);
#endif
		c = sscanf(line, "%63s %63s %63s %63s", ln_part[0], ln_part[1], ln_part[2], ln_part[3]);

		ln_part[0][63] = 0;
		ln_part[1][63] = 0;
		ln_part[2][63] = 0;
		ln_part[3][63] = 0;

		if (c == 2 && strcmp(ln_part[0], "TASK_SIZE") == 0) {
			uint64_t worker_task_size = atou64(ln_part[1]);

			if (worker_task_size != task_size) {
				message(ERR "worker uses different TASK_SIZE (%" PRIu64 "), whereas %" PRIu64 " is required\n", worker_task_size, task_size);
				return -1;
			}
		} else if (c == 2 && strcmp(ln_part[0], "TASK_ID") == 0) {
			uint64_t worker_task_id = atou64(ln_part[1]);

			if (n != worker_task_id) {
				message(ERR "client <---> worker communication problem!\n");
				return -1;
			}
		} else if (c == 3 && strcmp(ln_part[0], "OVERFLOW") == 0) {
			uint64_t size = atou64(ln_part[1]);
			uint64_t overflow_counter = atou64(ln_part[2]);

			if (size == 128) {
				assert(p_overflow_counter != NULL);
				*p_overflow_counter = overflow_counter;
			}
		} else if (c == 2 && strcmp(ln_part[0], "USERTIME") == 0) {
			uint64_t user_time = atou64(ln_part[1]);

			assert(p_user_time != NULL);

			*p_user_time = user_time;
		} else if (c == 3 && strcmp(ln_part[0], "USERTIME") == 0) {
			uint64_t user_time = atou64(ln_part[1]);

			assert(p_user_time != NULL);

			*p_user_time = user_time;
		} else if (c == 2 && strcmp(ln_part[0], "CHECKSUM") == 0) {
			uint64_t checksum = atou64(ln_part[1]);

			assert(p_checksum != NULL);

			*p_checksum = checksum;
		} else if (c == 1 && strcmp(ln_part[0], "HALTED") == 0) {
			/* this was expected */
			success = 1;
		} /* other cases... */
	}

	r = pclose(output);

	if (r == -1) {
		return -1;
	}

	if (WIFEXITED(r)) {
		/* the child terminated normally */
		if (success) {
			/* return success status only if the worker issued HALTED line */
			return 0;
		}

		return -1;

	} else {
		return -1;
	}
}

int write_group_size(int fd, int threads)
{
	assert( threads > 0 );

	return write_uint64(fd, (uint64_t)threads);
}

int multiple_requests(int fd, int threads)
{
	if (write_(fd, "MUL", 4) < 0) {
		return -1;
	}

	if (write_group_size(fd, threads) < 0) {
		return -1;
	}

	return 0;
}

int request_assignment(int fd, uint64_t *n, uint64_t *task_size, uint64_t clid)
{
	if (write_(fd, "REQ", 4) < 0) {
		return -1;
	}

	if (write_clid(fd, clid) < 0) {
		return -1;
	}

	if (read_assignment_no(fd, n) < 0) {
		return -1;
	}

	if (read_task_size(fd, task_size) < 0) {
		return -1;
	}

	if (task_size == 0) {
		return -1;
	}

	return 0;
}

int open_socket_and_request_multiple_assignments(int threads, uint64_t n[], uint64_t task_size[], uint64_t clid[])
{
	int fd;
	int tid;

	fd = open_socket_to_server();

	if (fd < 0) {
		return -1;
	}

	if (multiple_requests(fd, threads) < 0) {
		message(ERR "server does not implement the MUL command\n");
		close(fd);
		return -1;
	}

	for (tid = 0; tid < threads; ++tid) {
		if (request_assignment(fd, n+tid, task_size+tid, clid[tid]) < 0) {
			message(ERR "request_assignment failed (%i/%i)\n", tid, threads);
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}

int revoke_assignment(int fd, uint64_t n, uint64_t task_size, uint64_t clid)
{
	if (write_(fd, "INT", 4) < 0) {
		return -1;
	}

	if (write_assignment_no(fd, n) < 0) {
		return -1;
	}

	if (write_task_size(fd, task_size) < 0) {
		return -1;
	}

	if (write_clid(fd, clid) < 0) {
		return -1;
	}

	return 0;
}

int open_socket_and_revoke_multiple_assignments(int threads, uint64_t n[], uint64_t task_size[], uint64_t clid[])
{
	int fd;
	int tid;

	fd = open_socket_to_server();

	if (fd < 0) {
		return -1;
	}

	if (multiple_requests(fd, threads) < 0) {
		message(ERR "server does not implement the MUL command\n");
		close(fd);
		return -1;
	}

	for (tid = 0; tid < threads; ++tid) {
		if (revoke_assignment(fd, n[tid], task_size[tid], clid[tid]) < 0) {
			message(ERR "revoke_assignment failed (%i/%i)\n", tid, threads);
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}

int return_assignment(int fd, uint64_t n, uint64_t task_size, uint64_t overflow_counter, uint64_t user_time, uint64_t checksum, uint64_t clid)
{
	if (write_(fd, "RET", 4) < 0) {
		return -1;
	}

	if (write_assignment_no(fd, n) < 0) {
		return -1;
	}

	if (write_task_size(fd, task_size) < 0) {
		return -1;
	}

	if (write_overflow_counter(fd, overflow_counter) < 0) {
		return -1;
	}

	if (write_user_time(fd, user_time) < 0) {
		return -1;
	}

	if (write_checksum(fd, checksum) < 0) {
		return -1;
	}

	if (write_clid(fd, clid) < 0) {
		return -1;
	}

	return 0;
}

int open_socket_and_return_multiple_assignments(int threads, uint64_t n[], uint64_t task_size[], uint64_t overflow_counter[], uint64_t user_time[], uint64_t checksum[], uint64_t clid[])
{
	int fd;
	int tid;

	fd = open_socket_to_server();

	if (fd < 0) {
		return -1;
	}

	if (multiple_requests(fd, threads) < 0) {
		message(ERR "server does not implement the MUL command\n");
		close(fd);
		return -1;
	}

	for (tid = 0; tid < threads; ++tid) {
		if (return_assignment(fd, n[tid], task_size[tid], overflow_counter[tid], user_time[tid], checksum[tid], clid[tid]) < 0) {
			message(ERR "return_assignment failed (%i/%i)\n", tid, threads);
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}

int open_urandom_and_read_clid(uint64_t *clid)
{
	int fd = open("/dev/urandom", O_RDONLY);

	if (fd < 0) {
		return -1;
	}

	assert(clid != NULL);

	if (read_(fd, (char *)clid, sizeof(uint64_t)) < 0) {
		return -1;
	}

	close(fd);

	return 0;
}

int all_succeeded(int threads, int *success)
{
	int tid;

	for (tid = 0; tid < threads; ++tid) {
		if (success[tid] < 0) {
			return -1;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int threads;
	int opt;
	int one_shot = 0;
	int tid;
	uint64_t *clid;
	uint64_t *task_id;
	uint64_t *task_size;
	uint64_t *overflow_counter;
	uint64_t *user_time;
	uint64_t *checksum;
	int *success;

	if (getenv("SERVER_NAME")) {
		servername = getenv("SERVER_NAME");
	}

	message(INFO "server to be used: %s\n", servername);

	while ((opt = getopt(argc, argv, "1")) != -1) {
		switch (opt) {
			case '1':
				one_shot = 1;
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

	task_id = malloc(sizeof(uint64_t) * threads);
	task_size = malloc(sizeof(uint64_t) * threads);
	overflow_counter = malloc(sizeof(uint64_t) * threads);
	user_time = malloc(sizeof(uint64_t) * threads);
	checksum = malloc(sizeof(uint64_t) * threads);
	clid = malloc(sizeof(uint64_t) * threads);
	success = malloc(sizeof(int) * threads);

	if (task_id == NULL || task_size == NULL || overflow_counter == NULL || user_time == NULL || checksum == NULL || clid == NULL) {
		message(ERR "memory allocation failed!\n");
		return EXIT_FAILURE;
	}

	for (tid = 0; tid < threads; ++tid) {
		clid[tid] = 0;
		if (open_urandom_and_read_clid(clid+tid) < 0) {
			message(WARN "unable to generate random client ID");
		}
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGALRM, signal_handler);
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);

	while (!quit) {
		while (open_socket_and_request_multiple_assignments(threads, task_id, task_size, clid) < 0) {
			message(ERR "open_socket_and_request_multiple_assignments failed\n");
			sleep(SLEEP_INTERVAL);
		}

		#pragma omp parallel num_threads(threads)
		{
			int tid = threads_get_thread_id();

			message(INFO "thread %i: got assignment %" PRIu64 "\n", tid, task_id[tid]);

			success[tid] = run_assignment(task_id[tid], task_size[tid], overflow_counter+tid, user_time+tid, checksum+tid);

			if (success[tid] < 0) {
				message(ERR "thread %i: run_assignment failed\n", tid);
			}
		}

		if (all_succeeded(threads, success) < 0) {
			while (open_socket_and_revoke_multiple_assignments(threads, task_id, task_size, clid) < 0) {
				message(ERR "open_socket_and_revoke_multiple_assignments failed\n");
				sleep(SLEEP_INTERVAL);
			}

			if (one_shot)
				break;

			continue;
		}

		while (open_socket_and_return_multiple_assignments(threads, task_id, task_size, overflow_counter, user_time, checksum, clid) < 0) {
			message(ERR "open_socket_and_return_multiple_assignments failed\n");
			sleep(SLEEP_INTERVAL);
		}

		message(INFO "all assignments returned\n");

		if (one_shot)
			break;
	}

	free(task_id);
	free(task_size);
	free(overflow_counter);
	free(user_time);
	free(checksum);
	free(clid);

	message(INFO "client has been halted\n");

	return EXIT_SUCCESS;
}
