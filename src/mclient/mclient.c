#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#ifdef _OPENMP
	#include <omp.h>
#endif
#include <sys/types.h>
#ifndef __WIN32__
#	include <sys/socket.h>
#	include <netdb.h>
#	include <arpa/inet.h>
#	include <netinet/tcp.h>
#else
#	include <winsock2.h>
#	include <windows.h>
#	define WIFEXITED(r) (1)
#	define bzero(s, n) ZeroMemory((s), (n))
#	define close(fd) closesocket(fd)
#endif
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>

#include "compat.h"

#define SLEEP_INTERVAL 10

const char *servername = "localhost";
const uint16_t serverport = 5006;

const char *taskpath_cpu = "../worker/worker";
const char *taskpath_gpu = "../gpuworker/gpuworker";

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
#ifdef __WIN32__
		int t = send(fd, buf + written, count - written, 0);

		if (t == SOCKET_ERROR) {
			message(ERR, "send() failed\n");
			return -1;
		}

		written += t;
#else
		ssize_t t = write(fd, buf + written, count - written);

		if (t < 0) {
			/* errno is set appropriately */
			message(ERR "write() failed\n");
			return -1;
		}
		if (t == 0 && (size_t)(written + t) != count) {
			/* zero indicates nothing was written */
			message(ERR "nothing was written\n");
			return -1;
		}

		written += t;
#endif
	}

	return written;
}

ssize_t read_(int fd, char *buf, size_t count)
{
	/* I known this isn't a word in English, but "read" was already taken. */
	ssize_t readen = 0;

	while ((size_t)readen < count) {
#ifdef __WIN32__
		int t = recv(fd, buf + readen, count - readen, 0);

		if (t == SOCKET_ERROR) {
			message(ERR "recv() failed\n");
			return -1;
		}

		readen += t;
#else
		ssize_t t = read(fd, buf + readen, count - readen);

		if (t < 0) {
			/*  errno is set appropriately */
			message(ERR "read() failed\n");
			return -1;
		}
		if (t == 0 && (size_t)(readen + t) != count) {
			/* zero indicates end of file */
			message(ERR "end of file\n");
			return -1;
		}

		readen += t;
#endif
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

int open_socket_to_server()
{
	int fd;
	struct sockaddr_in server_addr;
	int i = 1;

	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0) {
		/* errno is set appropriately */
		message(ERR "socket() failed\n");
		return -1;
	}

	/* TCP_NODELAY have to be either enabled or disabled on both sides (server as well as client), not just on one side! */
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof i) < 0) {
		message(ERR "setsockopt() failed\n");
		close(fd);
		return -1;
	}

	if (init_sockaddr(&server_addr, servername, serverport) < 0 ) {
		message(ERR "init_sockaddr() failed\n");
		close(fd);
		return -1;
	}

	if (connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		/* errno is set appropriately */
		message(ERR "connect() failed\n");
		close(fd);
		return -1;
	}

	return fd;
}

unsigned long atoul(const char *nptr)
{
	return strtoul(nptr, NULL, 10);
}

const char *get_task_path(int gpu_mode)
{
	return gpu_mode ? taskpath_gpu : taskpath_cpu;
}

int run_assignment(uint64_t task_id, uint64_t task_size, uint64_t *p_overflow, uint64_t *p_usertime, uint64_t *p_checksum, uint64_t *p_mxoffset, uint64_t *p_cycleoff, unsigned long alarm_seconds, int gpu_mode)
{
	int r;
	char buffer[4096];
	char line[4096];
	char ln_part[4][64];
	FILE *output;
	int success = 0;
	const char *path = get_task_path(gpu_mode);
	char *dirc = strdup(path);
	char *basec = strdup(path);
	char *dname;

	if (alarm_seconds) {
		if (sprintf(buffer, "./%s -a %lu %" PRIu64, basename(basec), alarm_seconds, task_id) < 0) {
			return -1;
		}
	} else {
		if (sprintf(buffer, "./%s %" PRIu64, basename(basec), task_id) < 0) {
			return -1;
		}
	}

	dname = dirname(dirc);

	if (chdir(dname) < 0) {
		message(ERR "chdir failed\n");
		return -1;
	}

	free(basec);
	free(dirc);

	output = popen(buffer, "r");

	if (output == NULL) {
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

			if (task_id != worker_task_id) {
				message(ERR "client <---> worker communication problem!\n");
				return -1;
			}
		} else if (c == 3 && strcmp(ln_part[0], "OVERFLOW") == 0) {
			uint64_t size = atou64(ln_part[1]);
			uint64_t overflow = atou64(ln_part[2]);

			if (size == 128) {
				assert(p_overflow != NULL);
				*p_overflow = overflow;
			}
		} else if (c == 2 && strcmp(ln_part[0], "USERTIME") == 0) {
			uint64_t usertime = atou64(ln_part[1]);

			assert(p_usertime != NULL);

			*p_usertime = usertime;
		} else if (c == 3 && strcmp(ln_part[0], "USERTIME") == 0) {
			uint64_t usertime = atou64(ln_part[1]);

			assert(p_usertime != NULL);

			*p_usertime = usertime;
		} else if (c == 2 && strcmp(ln_part[0], "TIME") == 0) {
			uint64_t usertime = atou64(ln_part[1]);

			assert(p_usertime != NULL);

			*p_usertime = usertime;
		} else if (c == 3 && strcmp(ln_part[0], "TIME") == 0) {
			uint64_t usertime = atou64(ln_part[1]);

			assert(p_usertime != NULL);

			*p_usertime = usertime;
		} else if (c == 3 && strcmp(ln_part[0], "CHECKSUM") == 0) {
			uint64_t checksum_alpha = atou64(ln_part[1]);
			uint64_t checksum_beta = atou64(ln_part[2]);

			assert(p_checksum != NULL);

			*p_checksum = checksum_alpha;
			(void)checksum_beta;
		} else if (c == 2 && strcmp(ln_part[0], "CHECKSUM") == 0) {
			uint64_t checksum = atou64(ln_part[1]);

			assert(p_checksum != NULL);

			*p_checksum = checksum;
		} else if (c == 1 && strcmp(ln_part[0], "HALTED") == 0) {
			/* this was expected */
			success = 1;
		} else if (c == 2 && strcmp(ln_part[0], "ALARM") == 0) {
			unsigned long seconds = atoul(ln_part[1]);

			if (seconds != alarm_seconds) {
				message(WARN "worker misinterpreted the alarm\n");
			}
		} else if (c == 3 && strcmp(ln_part[0], "RANGE") == 0) {
			/* range */
		} else if (c > 1 && strcmp(ln_part[0], "KERNEL") == 0) {
			message(INFO "worker implementation: %s", line+7); /* incl. the newline character */
		} else if (c == 2 && strcmp(ln_part[0], "SIEVE_LOGSIZE") == 0) {
			unsigned long sieve_logsize = atoul(ln_part[1]);

			message(INFO "worker uses %lu-bit sieve\n", sieve_logsize);
		} else if (c == 2 && strcmp(ln_part[0], "SIEVE_SIZE") == 0) {
			unsigned long sieve_logsize = atoul(ln_part[1]);

			message(INFO "worker uses %lu-bit sieve\n", sieve_logsize);
		} else if (c == 2 && strcmp(ln_part[0], "MAXIMUM_OFFSET") == 0) {
			uint64_t maximum_offset = atou64(ln_part[1]);

			assert(p_mxoffset != NULL);

			*p_mxoffset = maximum_offset;

			message(INFO "found maximum at offset +%" PRIu64 "\n", maximum_offset);
		} else if (c == 2 && strcmp(ln_part[0], "MAXIMUM_CYCLE_OFFSET") == 0) {
			uint64_t maximum_cycle_offset = atou64(ln_part[1]);

			assert(p_cycleoff != NULL);

			*p_cycleoff = maximum_cycle_offset;

			message(INFO "found maximum cycle at offset +%" PRIu64 "\n", maximum_cycle_offset);
		} else if (c == 2 && strcmp(ln_part[0], "MAXIMUM_CYCLE") == 0) {
			uint64_t maximum_cycle = atou64(ln_part[1]);

			message(INFO "maximum cycle of the length %" PRIu64 "\n", maximum_cycle);
		} else if (c > 1 && strcmp(ln_part[0], "MAXIMUM") == 0) {
			message(INFO "maximum value found: %s", line+8); /* incl. the newline character */
		} else if (c == 1 && strcmp(ln_part[0], "ABORTED_DUE_TO_OVERFLOW") == 0) {
			message(ERR "overflow occurred!\n");
			if (gpu_mode == 1) {
				message(WARN "trying to run on the CPU...\n");
				if (run_assignment(task_id, task_size, p_overflow, p_usertime, p_checksum, p_mxoffset, p_cycleoff, alarm_seconds, 0) < 0) {
					message(ERR "even the CPU worker failed!\n");
					return -1;
				}
				success = 1;
			}
		} else if (c == 1 && strcmp(ln_part[0], "NO_GPU_FOUND") == 0) {
			message(ERR "(gpu)worker reported that no GPU was found, exiting...\n");
			quit = 1;
			success = 0;
		} else {
			/* other cases... */
			message(WARN "worker printed unknown message: %s", line);
		}
	}

	r = pclose(output);

	if (!success) {
		message(WARN "worker terminated and did not print HALTED\n");
	}

	if (r == -1) {
		message(ERR "pclose failed: if wait4(2) returns an error, or some other error is detected\n");
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
		message(ERR "WIFEXITED: the child terminated abnormally\n");

		if (gpu_mode && success) {
			message(WARN "maybe gpuworker --> overflow --> worker chain\n");
			/* return success status only if the worker issued HALTED line */
			return 0;
		}

		return -1;
	}
}

int write_group_size(int fd, int threads)
{
	assert(threads > 0);

	return write_uint64(fd, (uint64_t)threads);
}

int multiple_requests(int fd, int threads)
{
	if (write_(fd, "MUL", 4) < 0) {
		message(ERR "write_() failed\n");
		return -1;
	}

	if (write_group_size(fd, threads) < 0) {
		message(ERR "write_group_size() failed\n");
		return -1;
	}

	return 0;
}

int request_assignment(int fd, int request_lowest_incomplete, uint64_t *task_id, uint64_t *task_size, uint64_t clientid)
{
	if (request_lowest_incomplete) {
		if (write_(fd, "req", 4) < 0) {
			message(ERR "write_() failed\n");
			return -1;
		}
	} else {
		if (write_(fd, "REQ", 4) < 0) {
			message(ERR "write_() failed\n");
			return -1;
		}
	}

	if (write_uint64(fd, clientid) < 0) {
		message(ERR "write_uint64() failed\n");
		return -1;
	}

	if (read_uint64(fd, task_id) < 0) {
		message(ERR "read_uint64() failed\n");
		return -1;
	}

	if (read_uint64(fd, task_size) < 0) {
		return -1;
	}

	if (task_size == 0) {
		return -1;
	}

	return 0;
}

int open_socket_and_request_multiple_assignments(int threads, int request_lowest_incomplete, uint64_t task_id[], uint64_t task_size[], const uint64_t clientid[])
{
	int fd;
	int tid;

	fd = open_socket_to_server();

	if (fd < 0) {
		message(ERR "open_socket_to_server() failed\n");
		return -1;
	}

	if (multiple_requests(fd, threads) < 0) {
		message(ERR "server does not implement the MUL command\n");
		close(fd);
		return -1;
	}

	for (tid = 0; tid < threads; ++tid) {
		if (request_assignment(fd, request_lowest_incomplete, task_id+tid, task_size+tid, clientid[tid]) < 0) {
			message(ERR "request_assignment failed (%i/%i)\n", tid, threads);
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}

int revoke_assignment(int fd, uint64_t n, uint64_t task_size, uint64_t clientid)
{
	if (write_(fd, "INT", 4) < 0) {
		return -1;
	}

	if (write_uint64(fd, n) < 0) {
		return -1;
	}

	if (write_uint64(fd, task_size) < 0) {
		return -1;
	}

	if (write_uint64(fd, clientid) < 0) {
		return -1;
	}

	return 0;
}

int open_socket_and_revoke_multiple_assignments(int threads, uint64_t n[], uint64_t task_size[], uint64_t clientid[])
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
		if (revoke_assignment(fd, n[tid], task_size[tid], clientid[tid]) < 0) {
			message(ERR "revoke_assignment failed (%i/%i)\n", tid, threads);
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}

int return_assignment(int fd, uint64_t n, uint64_t task_size, uint64_t overflow, uint64_t usertime, uint64_t checksum, uint64_t mxoffset, uint64_t cycleoff, uint64_t clientid)
{
	char protocol_version = 0;
	char msg[4];

	strcpy(msg, "RET");

	if (mxoffset != 0) {
		protocol_version = 1;
	}

	if (cycleoff != 0) {
		protocol_version = 2;
	}

	msg[3] = protocol_version;

	if (write_(fd, msg, 4) < 0) {
		return -1;
	}

	if (write_uint64(fd, n) < 0) {
		return -1;
	}

	if (write_uint64(fd, task_size) < 0) {
		return -1;
	}

	if (write_uint64(fd, overflow) < 0) {
		return -1;
	}

	if (write_uint64(fd, usertime) < 0) {
		return -1;
	}

	if (write_uint64(fd, checksum) < 0) {
		return -1;
	}

	if (write_uint64(fd, clientid) < 0) {
		return -1;
	}

	if (protocol_version > 0) {
		if (write_uint64(fd, mxoffset) < 0) {
			return -1;
		}
	}

	if (protocol_version > 1) {
		if (write_uint64(fd, cycleoff) < 0) {
			return -1;
		}
	}

	return 0;
}

int open_socket_and_return_multiple_assignments(int threads, uint64_t n[], uint64_t task_size[], uint64_t overflow[], uint64_t usertime[], uint64_t checksum[], uint64_t mxoffset[], uint64_t cycleoff[], uint64_t clientid[])
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
		if (return_assignment(fd, n[tid], task_size[tid], overflow[tid], usertime[tid], checksum[tid], mxoffset[tid], cycleoff[tid], clientid[tid]) < 0) {
			message(ERR "return_assignment failed (%i/%i)\n", tid, threads);
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}

int open_urandom_and_read_clientid(uint64_t *clientid)
{
	int fd = open("/dev/urandom", O_RDONLY);

	if (fd < 0) {
		return -1;
	}

	assert(clientid != NULL);

	if (read_(fd, (char *)clientid, sizeof(uint64_t)) < 0) {
		return -1;
	}

	close(fd);

	return 0;
}

int run_assignments_in_parallel(int threads, const uint64_t task_id[], const uint64_t task_size[], uint64_t overflow[], uint64_t usertime[], uint64_t checksum[], uint64_t mxoffset[], uint64_t cycleoff[], unsigned long alarm_seconds, int gpu_mode)
{
	int *success;
	int tid;

	assert(threads > 0);

	success = malloc(sizeof(int) * threads);

	if (success == NULL) {
		return -1;
	}

	#pragma omp parallel num_threads(threads)
	{
		int tid = threads_get_thread_id();

		message(INFO "thread %i: got assignment %" PRIu64 "\n", tid, task_id[tid]);

		/* initialization, since the worker is not mandated to fill these */
		overflow[tid] = 0;
		usertime[tid] = 0;
		checksum[tid] = 0;
		mxoffset[tid] = 0;
		cycleoff[tid] = 0;

		success[tid] = run_assignment(task_id[tid], task_size[tid], overflow+tid, usertime+tid, checksum+tid, mxoffset+tid, cycleoff+tid, alarm_seconds, gpu_mode);

		if (success[tid] < 0) {
			message(ERR "thread %i: run_assignment failed\n", tid);
		}
	}

	for (tid = 0; tid < threads; ++tid) {
		if (success[tid] < 0) {
			free(success);
			return -1;
		}
	}

	free(success);

	return 0;
}

int main(int argc, char *argv[])
{
	int threads;
	int opt;
	int one_shot = 0;
	int request_lowest_incomplete = 0;
	int tid;
	uint64_t *task_id;
	uint64_t *task_size;
	uint64_t *clientid;
	uint64_t *overflow;
	uint64_t *usertime;
	uint64_t *checksum;
	uint64_t *mxoffset; /* offset of the starting value n0 leading to the maximum value */
	uint64_t *cycleoff; /* offset of the starting value n0 leading to the highest number of spins */
	unsigned long alarm_seconds = 0;
	int gpu_mode = 0;

#ifdef __WIN32__
	WORD versionWanted = MAKEWORD(1, 1);
	WSADATA wsaData;
	if (WSAStartup(versionWanted, &wsaData)) {
		message(ERR "WSAStartup failed\n");
		return -1;
	}
#endif

	if (getenv("SERVER_NAME")) {
		servername = getenv("SERVER_NAME");
	}

	message(INFO "server to be used: %s\n", servername);

	while ((opt = getopt(argc, argv, "1la:b:g")) != -1) {
		switch (opt) {
			unsigned long seconds;
			case '1':
				one_shot = 1;
				break;
			case 'l':
				request_lowest_incomplete = 1;
				break;
			case 'a':
				alarm_seconds = atoul(optarg);
				break;
#ifndef __WIN32__
			case 'b':
				alarm(seconds = atoul(optarg));
				message(DBG "MCLIENT ALARM %lu\n", seconds);
				break;
#endif
			case 'g':
				gpu_mode = 1;
				message(INFO "gpu mode activated!\n");
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

	if (alarm_seconds) {
		message(INFO "a signal to be delivered to workers in %lu seconds!\n", alarm_seconds);
	}

	task_id = malloc(sizeof(uint64_t) * threads);
	task_size = malloc(sizeof(uint64_t) * threads);
	clientid = malloc(sizeof(uint64_t) * threads);
	overflow = malloc(sizeof(uint64_t) * threads);
	usertime = malloc(sizeof(uint64_t) * threads);
	checksum = malloc(sizeof(uint64_t) * threads);
	mxoffset = malloc(sizeof(uint64_t) * threads);
	cycleoff = malloc(sizeof(uint64_t) * threads);

	if (clientid == NULL || task_id == NULL || task_size == NULL || overflow == NULL || usertime == NULL || checksum == NULL || mxoffset == NULL || cycleoff == NULL) {
		message(ERR "memory allocation failed!\n");
		return EXIT_FAILURE;
	}

	for (tid = 0; tid < threads; ++tid) {
		clientid[tid] = 0;
#ifndef __WIN32__
		if (open_urandom_and_read_clientid(clientid+tid) < 0) {
			message(WARN "unable to generate random client ID");
		}
#else
		clientid[tid] = 42; /* chosen randomly */
#endif
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
#ifndef __WIN32__
	signal(SIGALRM, signal_handler);
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);
#endif

	while (!quit) {
		while (open_socket_and_request_multiple_assignments(threads, request_lowest_incomplete, task_id, task_size, clientid) < 0) {
			message(ERR "open_socket_and_request_multiple_assignments failed\n");
			if (quit)
				goto end;
			sleep(SLEEP_INTERVAL);
		}

		if (run_assignments_in_parallel(threads, task_id, task_size, overflow, usertime, checksum, mxoffset, cycleoff, alarm_seconds, gpu_mode) < 0) {
			while (open_socket_and_revoke_multiple_assignments(threads, task_id, task_size, clientid) < 0) {
				message(ERR "open_socket_and_revoke_multiple_assignments failed\n");
				sleep(SLEEP_INTERVAL);
			}

			if (one_shot)
				break;

			if (!quit) {
				/* even if revoking assignments was successful, wait few seconds to give others a chance to pick up the just revoked assignments */
				sleep(SLEEP_INTERVAL);
			}
			continue;
		}

		while (open_socket_and_return_multiple_assignments(threads, task_id, task_size, overflow, usertime, checksum, mxoffset, cycleoff, clientid) < 0) {
			message(ERR "open_socket_and_return_multiple_assignments failed\n");
			sleep(SLEEP_INTERVAL);
		}

		message(INFO "all assignments returned\n");

		if (one_shot)
			break;

		end:
			;
	}

	free(task_id);
	free(task_size);
	free(clientid);
	free(overflow);
	free(usertime);
	free(checksum);
	free(mxoffset);
	free(cycleoff);

	message(INFO "client has been halted\n");

#ifdef __WIN32__
	WSACleanup();
#endif

	return EXIT_SUCCESS;
}
