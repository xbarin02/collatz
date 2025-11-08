#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#ifdef _OPENMP
	#include <omp.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
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
const uint16_t serverport = 5007;

const char *taskpath_cpu = "../rs-worker/rs-worker-sc";

static volatile sig_atomic_t quit = 0;

static int g_force_device_index = 0;

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

	buf[strlen(buf) - 1] = 0;

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
			message(ERR "read() failed\n");
			return -1;
		}
		if (t == 0 && (size_t)(readen + t) != count) {
			/* zero indicates end of file */
			message(ERR "end of file\n");
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

int run_assignment(uint64_t target, uint64_t log2_no_procs, uint64_t task_id, uint64_t *p_overflow, uint64_t *p_realtime, uint64_t *p_checksum, uint64_t *p_sieve_logsize, unsigned long alarm_seconds)
{
	int r;
	char buffer[4096];
	char line[4096];
	char ln_part[4][64];
	FILE *output;
	int success = 0, fail = 0;
	const char *path = taskpath_cpu;
	char *dirc = strdup(path);
	char *basec = strdup(path);
	char *dname;

	buffer[0] = 0;

	/* basename */
	if (1) {
		char temp[4096];
		if (sprintf(temp, "./%s", basename(basec)) < 0) {
			return -1;
		}
		strcat(buffer, temp);
	}

	/* alarm */
	if (alarm_seconds) {
		char temp[4096];
		if (sprintf(temp, " -a %lu", alarm_seconds) < 0) {
			return -1;
		}
		strcat(buffer, temp);
	}

	/* target */
	if (1) {
		char temp[4096];
		if (sprintf(temp, " -t %" PRIu64, target) < 0) {
			return -1;
		}
		strcat(buffer, temp);

	}

	/* log2_no_procs */
	if (1) {
		char temp[4096];
		if (sprintf(temp, " -N %" PRIu64, log2_no_procs) < 0) {
			return -1;
		}
		strcat(buffer, temp);

	}

	/* task_id */
	if (1) {
		char temp[4096];
		if (sprintf(temp, " -i %" PRIu64, task_id) < 0) {
			return -1;
		}
		strcat(buffer, temp);
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

		if (c == 3 && strcmp(ln_part[0], "OVERFLOW") == 0) {
			uint64_t size = atou64(ln_part[1]);
			uint64_t overflow = atou64(ln_part[2]);

			if (size == 128) {
				assert(p_overflow != NULL);
				*p_overflow = overflow;
			}
		} else if (c == 3 && strcmp(ln_part[0], "REALTIME") == 0) {
			uint64_t realtime = atou64(ln_part[2]);

			assert(p_realtime != NULL);

			*p_realtime = realtime;
		} else if (c == 3 && strcmp(ln_part[0], "CHECKSUM") == 0) {
			uint64_t checksum_alpha = atou64(ln_part[1]);
			uint64_t checksum_beta = atou64(ln_part[2]);

			assert(p_checksum != NULL);

			*p_checksum = checksum_alpha;
			(void)checksum_beta;
		} else if (c == 1 && strcmp(ln_part[0], "SUCCESS") == 0) {
			/* this was expected */
			success = 1;
		} else if (c == 2 && strcmp(ln_part[0], "ALARM") == 0) {
			unsigned long seconds = atoul(ln_part[1]);

			if (seconds != alarm_seconds) {
				message(WARN "worker misinterpreted the alarm\n");
			}
		} else if (c == 2 && strcmp(ln_part[0], "NO_PROCS") == 0) {
			uint64_t log2_no_procs_ = atou64(ln_part[1]);

			if ((UINT64_C(1) << log2_no_procs) != log2_no_procs_) {
				message(WARN "worker misinterpreted the number of processes (-N argument)\n");
				fail = 1;
			}
		} else if (c == 2 && strcmp(ln_part[0], "TARGET") == 0) {
			uint64_t target_ = atou64(ln_part[1]);

			if (target != target_) {
				message(WARN "worker misinterpreted the target (-t argument)\n");
				fail = 1;
			}
		} else if (c == 2 && strcmp(ln_part[0], "TASK_ID") == 0) {
			uint64_t task_id_ = atou64(ln_part[1]);

			if (task_id != task_id_) {
				message(WARN "worker misinterpreted the task id (-i argument)\n");
				fail = 1;
			}
		} else if (c == 2 && strcmp(ln_part[0], "NUMBER_OF_TESTS") == 0) {
			/* TODO */
		} else if (c == 2 && strcmp(ln_part[0], "SIEVE_LOGSIZE") == 0) {
			unsigned long sieve_logsize = atoul(ln_part[1]);

			message(INFO "worker uses %lu-bit sieve\n", sieve_logsize);

			assert(p_sieve_logsize != NULL);
			*p_sieve_logsize = (uint64_t)sieve_logsize;

			if (sieve_logsize != 0) {
				message(WARN "worker uses wrong sieve\n");
				fail = 1;
			}
		} else if (c == 1 && strcmp(ln_part[0], "ABORTED_DUE_TO_OVERFLOW") == 0) {
			message(ERR "overflow occurred! (try compiling with libgmp)\n");
		} else {
			/* other cases... */
			message(WARN "worker printed unknown message: %s", line);
		}
	}

	r = pclose(output);

	if (!success) {
		message(WARN "worker terminated and did not print SUCCESS\n");
	}

	if (r == -1) {
		message(ERR "pclose failed: if wait4(2) returns an error, or some other error is detected\n");
		return -1;
	}

	if (WIFEXITED(r)) {
		/* the child terminated normally */
		if (success && !fail) {
			/* return success status only if the worker issued HALTED line */
			return 0;
		}

		return -1;

	} else {
		message(ERR "WIFEXITED: the child terminated abnormally\n");

		if (success) {
			message(WARN "internal error\n");
			/* return success status only if the worker issued HALTED line */
			return -1;
		}

		return -1;
	}
}

int multiple_requests(int fd, int threads)
{
	if (write_(fd, "MUL", 4) < 0) {
		message(ERR "write_() failed\n");
		return -1;
	}

	if (write_uint64(fd, (uint64_t)threads) < 0) {
		message(ERR "write_uint64() failed\n");
		return -1;
	}

	return 0;
}

int request_assignment(int fd, int request_lowest_incomplete, uint64_t *target, uint64_t *log2_no_procs, uint64_t *task_id)
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

	if (read_uint64(fd, target) < 0) {
		message(ERR "write_uint64() failed\n");
		return -1;
	}

	if (read_uint64(fd, log2_no_procs) < 0) {
		message(ERR "read_uint64() failed\n");
		return -1;
	}

	if (read_uint64(fd, task_id) < 0) {
		message(ERR "read_uint64() failed\n");
		return -1;
	}

	return 0;
}

int open_socket_and_request_multiple_assignments(int threads, int request_lowest_incomplete, uint64_t target[], uint64_t log2_no_procs[], uint64_t task_id[])
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
		if (request_assignment(fd, request_lowest_incomplete, target+tid, log2_no_procs+tid, task_id+tid) < 0) {
			message(ERR "request_assignment failed (%i/%i)\n", tid, threads);
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}

int open_socket_and_request_multiple_assignments_batch(int threads, int request_lowest_incomplete, uint64_t target[], uint64_t log2_no_procs[], uint64_t task_id[])
{
	int fd;
	int tid;

	assert(request_lowest_incomplete == 0);

	fd = open_socket_to_server();

	if (fd < 0) {
		message(ERR "open_socket_to_server() failed\n");
		return -1;
	}

	/* client to server */

	if (write_(fd, "MRQ", 4) < 0) {
		message(ERR "write_() failed\n");
		return -1;
	}

	if (write_uint64(fd, (uint64_t)threads) < 0) {
		message(ERR "write_uint64() failed\n");
		return -1;
	}
#if 0
	/* n times: write clid */
	for (tid = 0; tid < threads; ++tid) {
		if (write_uint64(fd, clientid[tid]) < 0) {
			message(ERR "write_uint64() failed\n");
			return -1;
		}
	}
#endif
	/* server to client */

	/* n times: read task_id, ... */
	for (tid = 0; tid < threads; ++tid) {
		if (read_uint64(fd, target + tid) < 0) {
			message(ERR "read_uint64() failed\n");
			return -1;
		}
		if (read_uint64(fd, log2_no_procs + tid) < 0) {
			message(ERR "read_uint64() failed\n");
			return -1;
		}
		if (read_uint64(fd, task_id + tid) < 0) {
			message(ERR "read_uint64() failed\n");
			return -1;
		}
	}

	close(fd);

	return 0;
}

int open_socket_and_request_multiple_assignments_wrapper(int batch_mode, int threads, int request_lowest_incomplete, uint64_t target[], uint64_t log2_no_procs[], uint64_t task_id[])
{
	if (batch_mode) {
		return open_socket_and_request_multiple_assignments_batch(threads, request_lowest_incomplete, target, log2_no_procs, task_id);
	} else {
		return open_socket_and_request_multiple_assignments(threads, request_lowest_incomplete, target, log2_no_procs, task_id);
	}
}

int revoke_assignment(int fd, uint64_t target, uint64_t log2_no_procs, uint64_t task_id)
{
	if (write_(fd, "INT", 4) < 0) {
		return -1;
	}

	if (write_uint64(fd, target) < 0) {
		return -1;
	}

	if (write_uint64(fd, log2_no_procs) < 0) {
		return -1;
	}

	if (write_uint64(fd, task_id) < 0) {
		return -1;
	}

	return 0;
}

int open_socket_and_revoke_multiple_assignments(int threads, uint64_t target[], uint64_t log2_no_procs[], uint64_t task_id[])
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
		if (revoke_assignment(fd, target[tid], log2_no_procs[tid], task_id[tid]) < 0) {
			message(ERR "revoke_assignment failed (%i/%i)\n", tid, threads);
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}

int return_assignment(int fd, uint64_t target, uint64_t log2_no_procs, uint64_t task_id, uint64_t overflow, uint64_t realtime, uint64_t checksum)
{
	char msg[4];

	strcpy(msg, "RET");

	msg[3] = 0;

	if (write_(fd, msg, 4) < 0) {
		return -1;
	}

	if (write_uint64(fd, target) < 0) {
		return -1;
	}

	if (write_uint64(fd, log2_no_procs) < 0) {
		return -1;
	}

	if (write_uint64(fd, task_id) < 0) {
		return -1;
	}

	if (write_uint64(fd, overflow) < 0) {
		return -1;
	}

	if (write_uint64(fd, realtime) < 0) {
		return -1;
	}

	if (write_uint64(fd, checksum) < 0) {
		return -1;
	}

	return 0;
}

int open_socket_and_return_multiple_assignments(int threads, uint64_t target[], uint64_t log2_no_procs[], uint64_t task_id[], uint64_t overflow[], uint64_t realtime[], uint64_t checksum[])
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
		if (return_assignment(fd, target[tid], log2_no_procs[tid], task_id[tid], overflow[tid], realtime[tid], checksum[tid]) < 0) {
			message(ERR "return_assignment failed (%i/%i)\n", tid, threads);
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}

int run_assignments_in_parallel(int threads, const uint64_t target[], const uint64_t log2_no_procs[], const uint64_t task_id[], uint64_t overflow[], uint64_t realtime[], uint64_t checksum[], uint64_t sieve_logsize[], unsigned long alarm_seconds)
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
		realtime[tid] = 0;
		checksum[tid] = 0;
		sieve_logsize[tid] = 0;

		success[tid] = run_assignment(target[tid], log2_no_procs[tid], task_id[tid], overflow+tid, realtime+tid, checksum+tid, sieve_logsize+tid, alarm_seconds);

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
	uint64_t *target;
	uint64_t *log2_no_procs;
	uint64_t *task_id;
	uint64_t *overflow;
	uint64_t *realtime;
	uint64_t *checksum;
	uint64_t *sieve_logsize;
	unsigned long alarm_seconds = 0;
	int batch_mode = 0;

	if (getenv("SERVER_NAME")) {
		servername = getenv("SERVER_NAME");
	}

	message(INFO "server to be used: %s\n", servername);

	while ((opt = getopt(argc, argv, "1la:b:gdB")) != -1) {
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
			case 'b':
				alarm(seconds = atoul(optarg));
				message(DBG "MCLIENT ALARM %lu\n", seconds);
				break;
			case 'd':
				g_force_device_index = 1;
				message(INFO "force device index!\n");
				break;
			case 'B':
				batch_mode = 1;
				message(INFO "batch mode activated!\n");
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

	target = malloc(sizeof(uint64_t) * threads);
	log2_no_procs = malloc(sizeof(uint64_t) * threads);
	task_id = malloc(sizeof(uint64_t) * threads);
	overflow = malloc(sizeof(uint64_t) * threads);
	realtime = malloc(sizeof(uint64_t) * threads);
	checksum = malloc(sizeof(uint64_t) * threads);
	sieve_logsize = malloc(sizeof(uint64_t) * threads); 

	if (target == NULL || log2_no_procs == NULL || task_id == NULL || overflow == NULL || realtime == NULL || checksum == NULL || sieve_logsize == NULL) {
		message(ERR "memory allocation failed!\n");
		return EXIT_FAILURE;
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGALRM, signal_handler);
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);

	while (!quit) {
		while (open_socket_and_request_multiple_assignments_wrapper(batch_mode, threads, request_lowest_incomplete, target, log2_no_procs, task_id) < 0) {
			message(ERR "open_socket_and_request_multiple_assignments_wrapper failed\n");
			if (quit)
				goto end;
			sleep(SLEEP_INTERVAL);
		}

		if (run_assignments_in_parallel(threads, target, log2_no_procs, task_id, overflow, realtime, checksum, sieve_logsize, alarm_seconds) < 0) {
			while (open_socket_and_revoke_multiple_assignments(threads, target, log2_no_procs, task_id) < 0) {
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

		while (open_socket_and_return_multiple_assignments(threads, target, log2_no_procs, task_id, overflow, realtime, checksum) < 0) {
			message(ERR "open_socket_and_return_multiple_assignments failed\n");
			sleep(SLEEP_INTERVAL);
		}

		message(INFO "all assignments returned\n");

		if (one_shot)
			break;

		end:
			;
	}

	free(target);
	free(log2_no_procs);
	free(task_id);
	free(overflow);
	free(realtime);
	free(checksum);
	free(sieve_logsize);

	message(INFO "client has been halted\n");

	return EXIT_SUCCESS;
}
