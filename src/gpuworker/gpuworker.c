#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <CL/cl.h>

/* used by mmap */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "wideint.h"
#include "compat.h"

/* in log2 */
#define TASK_SIZE 40

/* in log2 */
#ifndef TASK_UNITS
#	define TASK_UNITS 16
#endif

/* in log2 */
#ifndef SIEVE_LOGSIZE
#	define SIEVE_LOGSIZE 16
#endif

#define SIEVE_MASK ((1UL << SIEVE_LOGSIZE) - 1)
#define SIEVE_SIZE ((1UL << SIEVE_LOGSIZE) / 8) /* in bytes */
#define IS_LIVE(n) ( ( g_map_sieve[ (n)>>3 ] >> ((n)&7) ) & 1 )

const unsigned char *g_map_sieve;

static uint64_t g_checksum_alpha = 0;
static uint64_t g_checksum_beta = 0;
static uint128_t g_max_n = 0;
static uint128_t g_max_n0;
static uint64_t g_max_cycle = 0;
static uint128_t g_max_cycle_n0;

const void *open_map(const char *path, size_t map_size)
{
	int fd = open(path, O_RDONLY, 0600);
	void *ptr;

	if (map_size == 0) {
		map_size = 1;
	}

	if (fd < 0) {
		perror("open");
		abort();
	}

	ptr = mmap(NULL, (size_t)map_size, PROT_READ, MAP_SHARED, fd, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		abort();
	}

	close(fd);

	return ptr;
}

#define LUT_SIZE64 41

uint64_t g_lut64[LUT_SIZE64];

void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE64; ++a) {
		g_lut64[a] = pow3u64((uint64_t)a);
	}
}

uint128_t get_max(uint128_t n0)
{
	int alpha, beta;
	uint128_t max_n = 0;
	uint128_t n = n0;

	assert(n0 != UINT128_MAX);

	do {
		n++;

		do {
			alpha = ctzu64(n);
			if (alpha >= LUT_SIZE64) {
				alpha = LUT_SIZE64 - 1;
			}
			n >>= alpha;
			if (n > UINT128_MAX / g_lut64[alpha]) {
				printf("ABORTED_DUE_TO_OVERFLOW\n");
			}
			assert(n <= UINT128_MAX / g_lut64[alpha]);
			n *= g_lut64[alpha];
		} while (!(n & 1));

		n--;

		if (n > max_n) {
			max_n = n;
		}

		do {
			beta = ctzu64(n);
			n >>= beta;
		} while (!(n & 1));

		if (n < n0) {
			return max_n;
		}
	} while (1);
}

uint64_t get_cycles(uint128_t n0)
{
	int alpha, beta;
	uint64_t cycles = 0;
	uint128_t n = n0;

	assert(n0 != UINT128_MAX);

	do {
		n++;

		cycles++;

		do {
			alpha = ctzu64(n);
			if (alpha >= LUT_SIZE64) {
				alpha = LUT_SIZE64 - 1;
			}
			n >>= alpha;
			if (n > UINT128_MAX / g_lut64[alpha]){
				printf("ABORTED_DUE_TO_OVERFLOW\n");
			}
			assert(n <= UINT128_MAX / g_lut64[alpha]);
			n *= g_lut64[alpha];
		} while (!(n & 1));

		n--;

		do {
			beta = ctzu64(n);
			n >>= beta;
		} while (!(n & 1));

		if (n < n0) {
			return cycles;
		}
	} while (1);
}

static const char *default_kernel = "kernel.cl";
static const char *kernel = NULL;

char *load_source(size_t *size)
{
	FILE *fp;
	char *str;

	assert(size != NULL);

	printf("KERNEL %s\n", kernel);

	fp = fopen(kernel, "r");

	if (fp == NULL) {
		return NULL;
	}

	str = malloc(1<<20);

	if (str == NULL) {
		return NULL;
	}

	*size = fread(str, 1, 1<<20, fp);

	fclose(fp);

	return str;
}

const char *errcode_to_cstr(cl_int errcode)
{
	switch (errcode) {
		case CL_BUILD_PROGRAM_FAILURE: return "CL_BUILD_PROGRAM_FAILURE";
		case CL_COMPILER_NOT_AVAILABLE: return "CL_COMPILER_NOT_AVAILABLE";
		case CL_DEVICE_NOT_AVAILABLE: return "CL_DEVICE_NOT_AVAILABLE";
		case CL_DEVICE_NOT_FOUND: return "CL_DEVICE_NOT_FOUND";
		case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
		case CL_INVALID_BINARY: return "CL_INVALID_BINARY";
		case CL_INVALID_BUILD_OPTIONS: return "CL_INVALID_BUILD_OPTIONS";
		case CL_INVALID_COMMAND_QUEUE: return "CL_INVALID_COMMAND_QUEUE";
		case CL_INVALID_CONTEXT: return "CL_INVALID_CONTEXT";
		case CL_INVALID_DEVICE: return "CL_INVALID_DEVICE";
		case CL_INVALID_DEVICE_TYPE: return "CL_INVALID_DEVICE_TYPE";
		case CL_INVALID_EVENT_WAIT_LIST: return "CL_INVALID_EVENT_WAIT_LIST";
		case CL_INVALID_MEM_OBJECT: return "CL_INVALID_MEM_OBJECT";
		case CL_INVALID_OPERATION: return "CL_INVALID_OPERATION";
		case CL_INVALID_PLATFORM: return "CL_INVALID_PLATFORM";
		case CL_INVALID_PROGRAM: return "CL_INVALID_PROGRAM";
		case CL_INVALID_VALUE: return "CL_INVALID_VALUE";
		case CL_MEM_OBJECT_ALLOCATION_FAILURE: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
		case CL_MISALIGNED_SUB_BUFFER_OFFSET: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
		case CL_OUT_OF_HOST_MEMORY: return "CL_OUT_OF_HOST_MEMORY";
		case CL_OUT_OF_RESOURCES: return "CL_OUT_OF_RESOURCES";
		case CL_SUCCESS: return "CL_SUCCESS";
		default: return "(unknown error code)";
	}
}

static int g_ocl_ver1 = 0;

int solve(uint64_t task_id, uint64_t task_size)
{
	uint64_t task_units = TASK_UNITS;
	/* arrays */
	uint64_t *checksum_alpha;

	cl_mem mem_obj_checksum_alpha;

	cl_int ret;
	cl_platform_id platform_id[64];
	cl_uint num_platforms;

	cl_device_id *device_id = NULL;
	cl_uint num_devices;

	cl_context context;

	cl_command_queue command_queue;

	char *program_string;
	size_t program_length;

	cl_program program;

	cl_kernel kernel;

	size_t global_work_size;

	size_t i;

	int platform_index = 0;
	int device_index = 0;

	/* used by sieve */
	char path[4096];
	size_t k = SIEVE_LOGSIZE;
	size_t map_size = SIEVE_SIZE;
	cl_mem mem_obj_sieve;

	uint64_t *mxoffset;
	cl_mem mem_obj_mxoffset;

	uint64_t *cycleoff;
	cl_mem mem_obj_cycleoff;

	assert((uint128_t)task_id <= (UINT128_MAX >> task_size));

	/* informative */
	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " 0x%016" PRIx64 ":%016" PRIx64 "\n",
		(uint64_t)(((uint128_t)(task_id + 0) << task_size)>>64),
		(uint64_t)(((uint128_t)(task_id + 0) << task_size)    ),
		(uint64_t)(((uint128_t)(task_id + 1) << task_size)>>64),
		(uint64_t)(((uint128_t)(task_id + 1) << task_size)    )
	);

	ret = clGetPlatformIDs(0, NULL, &num_platforms);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clGetPlarformIDs failed with = %s\n", errcode_to_cstr(ret));
		return -1;
	}

	printf("[DEBUG] num_platforms = %u\n", (unsigned)num_platforms);

	if (num_platforms == 0) {
		printf("[ERROR] no platform\n");
		return -1;
	}

	ret = clGetPlatformIDs(num_platforms, &platform_id[0], NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clGetPlarformIDs failed\n");
		return -1;
	}

next_platform:
	printf("[DEBUG] platform = %i\n", platform_index);

	assert((cl_uint)platform_index < num_platforms);

	num_devices = 0;

	ret = clGetDeviceIDs(platform_id[platform_index], CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices);

	if (ret == CL_DEVICE_NOT_FOUND) {
		if ((cl_uint)platform_index + 1 < num_platforms) {
			platform_index++;
			goto next_platform;
		}
	}

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clGetDeviceIDs failed with %s\n", errcode_to_cstr(ret));
		return -1;
	}

	printf("[DEBUG] num_devices = %u\n", num_devices);

	device_id = malloc(sizeof(cl_device_id) * num_devices);

	if (device_id == NULL) {
		return -1;
	}

	ret = clGetDeviceIDs(platform_id[platform_index], CL_DEVICE_TYPE_GPU, num_devices, &device_id[0], NULL);

	if (ret != CL_SUCCESS) {
		return -1;
	}

	for (; (cl_uint)device_index < num_devices; ++device_index) {
		printf("[DEBUG] device_index = %i...\n", device_index);

		context = clCreateContext(NULL, 1, &device_id[device_index], NULL, NULL, &ret);

		if (ret == CL_INVALID_DEVICE) {
			continue;
		}

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateContext failed with %s\n", errcode_to_cstr(ret));
			return -1;
		}

		printf("[DEBUG] context created @ device_index = %i\n", device_index);

#ifdef DEBUG
		{
			size_t size;
			cl_uint uint;
			cl_ulong ulong;

			ret = clGetDeviceInfo(device_id[device_index], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &size, NULL);

			if (ret != CL_SUCCESS) {
				return -1;
			}

			printf("[DEBUG] CL_DEVICE_MAX_WORK_GROUP_SIZE = %lu\n", (unsigned long)size);

			ret = clGetDeviceInfo(device_id[device_index], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &uint, NULL);

			if (ret != CL_SUCCESS) {
				return -1;
			}

			printf("[DEBUG] CL_DEVICE_MAX_COMPUTE_UNITS = %u\n", (unsigned)uint);

			ret = clGetDeviceInfo(device_id[device_index], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &ulong, NULL);

			if (ret != CL_SUCCESS) {
				return -1;
			}

			printf("[DEBUG] CL_DEVICE_LOCAL_MEM_SIZE = %lu B\n", (unsigned long)ulong);
		}
#endif

		command_queue = clCreateCommandQueue(context, device_id[device_index], 0, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateCommandQueue failed\n");
			return -1;
		}

		assert(sizeof(cl_ulong) == sizeof(uint64_t));

		mem_obj_checksum_alpha = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_ulong) << task_units, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		program_string = load_source(&program_length);

		if (program_string == NULL) {
			printf("[ERROR] load_source failed\n");
			return -1;
		}

		program = clCreateProgramWithSource(context, 1, (const char **)&program_string, (const size_t *)&program_length, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateProgramWithSource failed\n");
			return -1;
		}

		ret = clBuildProgram(program, 1, &device_id[device_index], g_ocl_ver1 ? NULL : "-cl-std=CL2.0", NULL, NULL);

		if (ret == CL_BUILD_PROGRAM_FAILURE) {
			size_t log_size;
			char *log;

			clGetProgramBuildInfo(program, device_id[device_index], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

			log = malloc(log_size);

			clGetProgramBuildInfo(program, device_id[device_index], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);

			printf("%s\n", log);
		}

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clBuildProgram failed with %s\n", errcode_to_cstr(ret));
			return -1;
		}

		kernel = clCreateKernel(program, "worker", &ret);

		if (ret != CL_SUCCESS) {
			return -1;
		}

#ifdef DEBUG
		{
			size_t size;
			cl_ulong ulong;

			ret = clGetKernelWorkGroupInfo(kernel, device_id[device_index], CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &size, NULL);

			if (ret != CL_SUCCESS) {
				printf("[ERROR] clGetKernelWorkGroupInfo failed\n");
				return -1;
			}

			printf("[DEBUG] CL_KERNEL_WORK_GROUP_SIZE = %lu\n", (unsigned long)size);

			ret = clGetKernelWorkGroupInfo(kernel, device_id[device_index], CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(size_t), &size, NULL);

			if (ret != CL_SUCCESS) {
				printf("[ERROR] clGetKernelWorkGroupInfo failed\n");
				return -1;
			}

			printf("[DEBUG] CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE = %lu\n", (unsigned long)size);

			ret = clGetKernelWorkGroupInfo(kernel, device_id[device_index], CL_KERNEL_PRIVATE_MEM_SIZE, sizeof(cl_ulong), &ulong, NULL);

			if (ret != CL_SUCCESS) {
				printf("[ERROR] clGetKernelWorkGroupInfo failed\n");
				return -1;
			}

			printf("[DEBUG] CL_KERNEL_PRIVATE_MEM_SIZE = %lu\n", (unsigned long)ulong);

			ret = clGetKernelWorkGroupInfo(kernel, device_id[device_index], CL_KERNEL_LOCAL_MEM_SIZE, sizeof(cl_ulong), &ulong, NULL);

			if (ret != CL_SUCCESS) {
				printf("[ERROR] clGetKernelWorkGroupInfo failed\n");
				return -1;
			}

			printf("[DEBUG] CL_KERNEL_LOCAL_MEM_SIZE = %lu\n", (unsigned long)ulong);
		}
#endif

		ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&mem_obj_checksum_alpha);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		assert(sizeof(cl_ulong) == sizeof(uint64_t));

		ret = clSetKernelArg(kernel, 1, sizeof(cl_ulong), (void *)&task_id);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 2, sizeof(cl_ulong), (void *)&task_size);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 3, sizeof(cl_ulong), (void *)&task_units);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		global_work_size = (size_t)1 << task_units;

		printf("[DEBUG] global_work_size = %lu\n", global_work_size);

		assert(task_units + 2 <= task_size);

#ifdef USE_ESIEVE
		sprintf(path, "esieve-%lu.map", (unsigned long)k);
#else
		sprintf(path, "sieve-%lu.map", (unsigned long)k);
#endif

		/* allocate memory for sieve & load sieve */
		g_map_sieve = open_map(path, map_size);

		printf("SIEVE_LOGSIZE %lu\n", (unsigned long)SIEVE_LOGSIZE);

		/* create memobj */
		mem_obj_sieve = clCreateBuffer(context, CL_MEM_READ_ONLY, SIEVE_SIZE, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		/* transfer sieve to gpu */
		ret = clEnqueueWriteBuffer(command_queue, mem_obj_sieve, CL_TRUE, 0, SIEVE_SIZE, g_map_sieve, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clEnqueueWriteBuffer() failed\n");
			return -1;
		}

		/* set kernel arg */
		ret = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *)&mem_obj_sieve);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clSetKernelArg() failed\n");
			return -1;
		}

		mem_obj_mxoffset = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_ulong) << task_units, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		ret = clSetKernelArg(kernel, 5, sizeof(cl_mem), (void *)&mem_obj_mxoffset);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clSetKernelArg() failed\n");
			return -1;
		}

		mem_obj_cycleoff = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_ulong) << task_units, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		ret = clSetKernelArg(kernel, 6, sizeof(cl_mem), (void *)&mem_obj_cycleoff);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clSetKernelArg() failed\n");
			return -1;
		}

		ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clEnqueueNDRangeKernel() failed\n");
			return -1;
		}

		printf("[DEBUG] kernel enqueued\n");

		/* allocate arrays */
		checksum_alpha = malloc(sizeof(uint64_t) << task_units);

		if (checksum_alpha == NULL) {
			return -1;
		}

		printf("[DEBUG] host buffers allocated\n");

		ret = clEnqueueReadBuffer(command_queue, mem_obj_checksum_alpha, CL_TRUE, 0, sizeof(uint64_t) << task_units, checksum_alpha, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clEnqueueReadBuffer failed with %s\n", errcode_to_cstr(ret));
			return -1;
		}

		mxoffset = malloc(sizeof(uint64_t) * global_work_size);

		if (mxoffset == NULL) {
			return -1;
		}

		ret = clEnqueueReadBuffer(command_queue, mem_obj_mxoffset, CL_TRUE, 0, sizeof(uint64_t) << task_units, mxoffset, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clEnqueueReadBuffer failed with %s\n", errcode_to_cstr(ret));
			return -1;
		}

		cycleoff = malloc(sizeof(uint64_t) * global_work_size);

		if (cycleoff == NULL) {
			return -1;
		}

		ret = clEnqueueReadBuffer(command_queue, mem_obj_cycleoff, CL_TRUE, 0, sizeof(uint64_t) << task_units, cycleoff, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clEnqueueReadBuffer failed with %s\n", errcode_to_cstr(ret));
			return -1;
		}

		printf("[DEBUG] buffers transferred\n");

		ret = clFlush(command_queue);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clFlush failed with %s\n", errcode_to_cstr(ret));
			return -1;
		}

		printf("[DEBUG] flushed\n");

		ret = clFinish(command_queue);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clFinish failed with %s\n", errcode_to_cstr(ret));
			return -1;
		}

		ret = clReleaseKernel(kernel);
		ret = clReleaseProgram(program);
		ret = clReleaseCommandQueue(command_queue);
		ret = clReleaseContext(context);

		for (i = 0; i < global_work_size; ++i) {
			if (checksum_alpha[i] == 0) {
				printf("ABORTED_DUE_TO_OVERFLOW\n");
				abort();
			}
		}

		for (i = 0; i < global_work_size; ++i) {
			uint128_t max_n0 = mxoffset[i] + ((uint128_t)(task_id + 0) << task_size);
			uint128_t max_cycle_n0 = cycleoff[i] + ((uint128_t)(task_id + 0) << task_size);

			uint128_t max_n;
			uint64_t max_cycle;

			max_n = get_max(max_n0);
			max_cycle = get_cycles(max_cycle_n0);

			g_checksum_alpha += checksum_alpha[i];

			if (max_n > g_max_n) {
				g_max_n = max_n;
				g_max_n0 = max_n0;
			}

			if (max_cycle > g_max_cycle) {
				g_max_cycle = max_cycle;
				g_max_cycle_n0 = max_cycle_n0;
			}
		}

		free(checksum_alpha);
		free(mxoffset);
		free(cycleoff);

		free(program_string);

		goto done;
	}

	printf("NO_GPU_FOUND\n");

	return -1;

done:
	free(device_id);

	return 0;
}

int main(int argc, char *argv[])
{
	uint64_t task_id = 0;
	uint64_t task_size = TASK_SIZE;
	int opt;
	struct rusage usage;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	while ((opt = getopt(argc, argv, "t:a:k:1")) != -1) {
		switch (opt) {
			unsigned long seconds;
			case 't':
				task_size = atou64(optarg);
				break;
			case 'a':
				alarm(seconds = atoul(optarg));
				printf("ALARM %lu\n", seconds);
				break;
			case 'k':
				kernel = strdup(optarg);
				break;
			case '1':
				g_ocl_ver1 = 1;
				break;
			default:
				fprintf(stderr, "Usage: %s [-t task_size] task_id\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	if (kernel == NULL) {
		kernel = default_kernel;
	}

	task_id = (optind < argc) ? atou64(argv[optind]) : 0;

	printf("TASK_SIZE %" PRIu64 "\n", task_size);
	printf("TASK_ID %" PRIu64 "\n", task_id);

	init_lut();

	if (solve(task_id, task_size)) {
		printf("ERROR\n");
		abort();
	}

	if (kernel != default_kernel) {
		free((void *)kernel);
	}

	assert(sizeof(uint64_t) >= sizeof(time_t));
	assert(sizeof(uint64_t) >= sizeof(suseconds_t));
	assert(sizeof(uint64_t) >= sizeof(time_t));

	/* user + system time */
	if (getrusage(RUSAGE_SELF, &usage) < 0) {
		/* errno is set appropriately. */
		perror("getrusage");
	} else {
		/* may wrap around */
		uint64_t usecs = usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec
		               + usage.ru_stime.tv_sec * UINT64_C(1000000) + usage.ru_stime.tv_usec;
		uint64_t secs = (usecs + 500000) / 1000000;

		printf("TIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);
	}

	printf("OVERFLOW 128 %" PRIu64 "\n", UINT64_C(0));

	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_checksum_alpha, g_checksum_beta);

	printf("MAXIMUM_OFFSET %" PRIu64 "\n", (uint64_t)(g_max_n0 - ((uint128_t)(task_id + 0) << task_size)));

	printf("MAXIMUM_CYCLE_OFFSET %" PRIu64 "\n", (uint64_t)(g_max_cycle_n0 - ((uint128_t)(task_id + 0) << task_size)));

	printf("HALTED\n");

	return 0;
}
