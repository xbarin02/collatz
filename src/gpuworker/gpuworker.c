/*
 * https://www.eriksmistad.no/getting-started-with-opencl-and-gpu-computing/
 * https://anteru.net/blog/2012/getting-started-with-opencl-part-1/
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdint.h>
#include <inttypes.h>
#include <CL/cl.h>

#include "compat.h"
#include "wideint.h"

/* in log2 */
#define TASK_SIZE 40

static uint64_t g_checksum_alpha = 0;
static uint64_t g_checksum_beta = 0;
static uint64_t g_overflow_counter = 0;

unsigned long atoul(const char *nptr)
{
	return strtoul(nptr, NULL, 10);
}

/* in log2 */
#define TASK_UNITS 16

char *load_source(size_t *size)
{
	FILE *fp;
	char *str;

	assert(size != NULL);

	fp = fopen("kernel.cl", "r");

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
		case CL_SUCCESS: return "CL_SUCCESS";
		case CL_BUILD_PROGRAM_FAILURE: return "CL_BUILD_PROGRAM_FAILURE";
		case CL_COMPILER_NOT_AVAILABLE: return "CL_COMPILER_NOT_AVAILABLE";
		case CL_INVALID_OPERATION: return "CL_INVALID_OPERATION";
		case CL_INVALID_BUILD_OPTIONS: return "CL_INVALID_BUILD_OPTIONS";
		case CL_INVALID_BINARY: return "CL_INVALID_BINARY";
		case CL_INVALID_PROGRAM: return "CL_INVALID_PROGRAM";
		case CL_INVALID_PLATFORM: return "CL_INVALID_PLATFORM";
		case CL_INVALID_VALUE: return "CL_INVALID_VALUE";
		case CL_INVALID_DEVICE: return "CL_INVALID_DEVICE";
		case CL_INVALID_DEVICE_TYPE: return "CL_INVALID_DEVICE_TYPE";
		case CL_DEVICE_NOT_AVAILABLE: return "CL_DEVICE_NOT_AVAILABLE";
		case CL_DEVICE_NOT_FOUND: return "CL_DEVICE_NOT_FOUND";
		case CL_OUT_OF_HOST_MEMORY: return "CL_OUT_OF_HOST_MEMORY";
		default: return "(unknown error code)";
	}
}

int solve(uint64_t task_id, uint64_t task_size)
{
	uint64_t task_units = TASK_UNITS;
	uint128_t n;
	uint128_t n_sup;
	/* arrays */
	uint64_t *overflow_counter;
	uint64_t *checksum_alpha;

	cl_mem mem_obj_overflow_counter;
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

	/* n of the form 4n+3 */
	n     = ( UINT128_C(task_id) << task_size ) + 3;
	n_sup = ( UINT128_C(task_id) << task_size ) + 3 + (UINT64_C(1) << task_size);

	printf("RANGE 0x%016" PRIx64 ":%016" PRIx64 " 0x%016" PRIx64 ":%016" PRIx64 "\n",
		(uint64_t)(n>>64), (uint64_t)n, (uint64_t)(n_sup>>64), (uint64_t)n_sup);

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

#if 1
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

		mem_obj_overflow_counter = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_ulong) << task_units, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

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

		ret = clBuildProgram(program, 1, &device_id[device_index], NULL, NULL, NULL);

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

		ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&mem_obj_overflow_counter);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&mem_obj_checksum_alpha);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		assert(sizeof(cl_ulong) == sizeof(uint64_t));

		ret = clSetKernelArg(kernel, 2, sizeof(cl_ulong), (void *)&task_id);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 3, sizeof(cl_ulong), (void *)&task_size);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 4, sizeof(cl_ulong), (void *)&task_units);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		global_work_size = (size_t)1 << task_units;

		printf("[DEBUG] global_work_size = %lu\n", global_work_size);

		assert(task_units + 2 <= task_size);

		ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		printf("[DEBUG] kernel enqueued\n");

		/* allocate arrays */
		overflow_counter = malloc(sizeof(uint64_t) << task_units);
		checksum_alpha = malloc(sizeof(uint64_t) << task_units);

		if (overflow_counter == NULL || checksum_alpha == NULL) {
			return -1;
		}

		printf("[DEBUG] host buffers allocated\n");

		ret = clEnqueueReadBuffer(command_queue, mem_obj_overflow_counter, CL_TRUE, 0, sizeof(uint64_t) << task_units, overflow_counter, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clEnqueueReadBuffer failed with %s\n", errcode_to_cstr(ret));
			return -1;
		}

		ret = clEnqueueReadBuffer(command_queue, mem_obj_checksum_alpha, CL_TRUE, 0, sizeof(uint64_t) << task_units, checksum_alpha, 0, NULL, NULL);

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
			g_overflow_counter += overflow_counter[i];
			g_checksum_alpha += checksum_alpha[i];
		}

		free(overflow_counter);
		free(checksum_alpha);

		goto done;
	}

	return -1;

done:
	free(device_id);

	if (g_overflow_counter) {
		g_checksum_alpha = 0;
		printf("[ERROR] overflow occured!\n");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	uint64_t task_id = 0;
	uint64_t task_size = TASK_SIZE;
	int opt;
	struct rusage usage;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	while ((opt = getopt(argc, argv, "t:a:")) != -1) {
		switch (opt) {
			unsigned long seconds;
			case 't':
				task_size = atou64(optarg);
				break;
			case 'a':
				alarm(seconds = atoul(optarg));
				printf("ALARM %lu\n", seconds);
				break;
			default:
				fprintf(stderr, "Usage: %s [-t task_size] task_id\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	task_id = (optind < argc) ? atou64(argv[optind]) : 0;

	printf("TASK_SIZE %" PRIu64 "\n", task_size);
	printf("TASK_ID %" PRIu64 "\n", task_id);

	if (solve(task_id, task_size)) {
		printf("ERROR\n");
		abort();
	}

	/* the total amount of time spent executing in user mode, expressed in a timeval structure (seconds plus microseconds) */
	if (getrusage(RUSAGE_SELF, &usage) < 0) {
		/* errno is set appropriately. */
		perror("getrusage");
	} else {
		if ((sizeof(uint64_t) >= sizeof(time_t)) &&
		    (sizeof(uint64_t) >= sizeof(suseconds_t)) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1) <= UINT64_MAX / UINT64_C(1000000)) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1000000) <= UINT64_MAX - usage.ru_utime.tv_usec)) {
			uint64_t secs = (uint64_t)usage.ru_utime.tv_sec;
			uint64_t usecs = usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec;
			if (secs < UINT64_MAX && (uint64_t)usage.ru_utime.tv_usec >= UINT64_C(1000000/2)) {
				/* round up */
				secs++;
			}
			printf("USERTIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);
		} else if (sizeof(uint64_t) >= sizeof(time_t)) {
			uint64_t secs = (uint64_t)usage.ru_utime.tv_sec;
			printf("USERTIME %" PRIu64 "\n", secs);
		}
	}

	printf("OVERFLOW 128 %" PRIu64 "\n", g_overflow_counter);

	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_checksum_alpha, g_checksum_beta);

	printf("HALTED\n");

	return 0;
}
