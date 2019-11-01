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
#ifdef _USE_GMP
#	include <gmp.h>
#endif

#include "compat.h"
#include "wideint.h"

/* in log2 */
#define TASK_SIZE 40

/* in log2 */
#ifndef TASK_UNITS
#define TASK_UNITS 16
#endif

static uint64_t g_checksum_alpha = 0;
static uint64_t g_checksum_beta = 0;
static uint64_t g_overflow_counter = 0;

static uint64_t g_private_overflow_counter;
static uint64_t g_private_checksum_alpha;

unsigned long atoul(const char *nptr)
{
	return strtoul(nptr, NULL, 10);
}

uint128_t pow3x(uint128_t n)
{
	uint128_t r = 1;

	for (; n > 0; --n) {
		assert(r <= UINT128_MAX / 3);

		r *= 3;
	}

	return r;
}

#ifdef _USE_GMP
static void mpz_pow3(mpz_t r, unsigned long n)
{
	mpz_ui_pow_ui(r, 3UL, n);
}
#endif

#define LUT_SIZE128 81
#ifdef _USE_GMP
#	define LUT_SIZEMPZ 512
#endif

uint128_t g_lut[LUT_SIZE128];
#ifdef _USE_GMP
mpz_t g_mpz_lut[LUT_SIZEMPZ];
#endif

void init_lut()
{
	int a;

	for (a = 0; a < LUT_SIZE128; ++a) {
		g_lut[a] = pow3x((uint128_t)a);
	}

#ifdef _USE_GMP
	for (a = 0; a < LUT_SIZEMPZ; ++a) {
		mpz_init(g_mpz_lut[a]);
		mpz_pow3(g_mpz_lut[a], (unsigned long)a);
	}
#endif
}

#ifdef _USE_GMP
static mp_bitcnt_t mpz_ctz(const mpz_t n)
{
	return mpz_scan1(n, 0);
}
#endif

#ifdef _USE_GMP
static void mpz_init_set_u128(mpz_t rop, uint128_t op)
{
	uint64_t nh = (uint64_t)(op>>64);
	uint64_t nl = (uint64_t)(op);

	assert(sizeof(unsigned long) == sizeof(uint64_t));

	mpz_init_set_ui(rop, (unsigned long)nh);
	mpz_mul_2exp(rop, rop, (mp_bitcnt_t)64);
	mpz_add_ui(rop, rop, (unsigned long)nl);
}
#endif

#ifdef _USE_GMP
static void mpz_check2(uint128_t n0_, uint128_t n_, int alpha_)
{
	mp_bitcnt_t alpha, beta;
	mpz_t n;
	mpz_t n0;

	assert(alpha_ >= 0);
	alpha = (mp_bitcnt_t)alpha_;

	mpz_init_set_u128(n, n_);
	mpz_init_set_u128(n0, n0_);

	do {
		if (alpha >= LUT_SIZEMPZ) {
			abort();
		}

		/* n *= lut[alpha] */
		mpz_mul(n, n, g_mpz_lut[alpha]);

		/* n-- */
		mpz_sub_ui(n, n, 1UL);

		beta = mpz_ctz(n);

		/* n >>= ctz(n) */
		mpz_fdiv_q_2exp(n, n, beta);

		if (mpz_cmp(n, n0) < 0) {
			break;
		}

		/* n++ */
		mpz_add_ui(n, n, 1UL);

		alpha = mpz_ctz(n);

		g_private_checksum_alpha += alpha;

		/* n >>= alpha */
		mpz_fdiv_q_2exp(n, n, alpha);
	} while (1);

	mpz_clear(n);
	mpz_clear(n0);
}
#endif

static void check2(uint128_t n0, uint128_t n, int alpha)
{
	int beta;

	g_private_overflow_counter++;

	do {
		if (alpha >= LUT_SIZE128 || n > UINT128_MAX / g_lut[alpha]) {
#ifdef _USE_GMP
			mpz_check2(n0, n, alpha);
#else
			abort();
#endif
			return;
		}

		n *= g_lut[alpha];

		n--;

		beta = __builtin_ctzu128(n);

		n >>= beta;

		if (n < n0) {
			return;
		}

		n++;

		alpha = __builtin_ctzu128(n);

		g_private_checksum_alpha += alpha;

		n >>= alpha;
	} while (1);
}

uint128_t ceil_mod12(uint128_t n)
{
	return (n + 11) / 12 * 12;
}

uint64_t cpu_worker(
	uint64_t *checksum_alpha,
	unsigned long task_id,
	unsigned long task_size /* in log2 */,
	unsigned long task_units /* in log2 */,
	size_t id)
{
	uint128_t l     = (((uint128_t)(task_id + 0) << task_size) + ((uint128_t)(id + 0) << (task_size - task_units))) + 3;
	uint128_t n_sup = (((uint128_t)(task_id + 0) << task_size) + ((uint128_t)(id + 1) << (task_size - task_units))) + 3;

	g_private_overflow_counter = 0;
	g_private_checksum_alpha = 0;

	printf("[DEBUG] verifying block (thread id %lu / %lu) on CPU...\n", (unsigned long)id, (size_t)1 << task_units);

	for (; l < n_sup; l += 4) {
		if (1) {
			uint128_t n0 = l;
			uint128_t n = n0;

			do {
				int alpha;

				if (n == UINT128_MAX) {
					g_private_checksum_alpha += 128;
					check2(n0, UINT128_C(1), 128);
					break; /* next n0 */
				}

				n++;
				alpha = __builtin_ctzu128(n);
				g_private_checksum_alpha += alpha;
				n >>= alpha;

				if (n > UINT128_MAX >> 2*alpha || alpha >= LUT_SIZE128) {
					check2(n0, n, alpha);
					break; /* next n0 */
				}

				n *= g_lut[alpha];

				n--;
				n >>= __builtin_ctzu128(n);
			} while (n >= n0);
		}
	}

	assert(checksum_alpha != NULL);

	checksum_alpha[id] = g_private_checksum_alpha;

	return g_private_overflow_counter;
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
	uint128_t n;
	uint128_t n_sup;
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

	uint64_t *lbegin, *hbegin, *lsup, *hsup;
	cl_mem mem_obj_lbegin, mem_obj_hbegin, mem_obj_lsup, mem_obj_hsup;

	assert((uint128_t)task_id <= (UINT128_MAX >> task_size));

	/* n of the form 12n+3 */
	n     = ((uint128_t)(task_id + 0) << task_size) + 3;
	n_sup = ((uint128_t)(task_id + 1) << task_size) + 3;

	/* informative */
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

		global_work_size = (size_t)1 << task_units;

		printf("[DEBUG] global_work_size = %lu\n", global_work_size);

		assert(task_units + 2 <= task_size);

		/* allocate hbegin, lbegin, hsup, lsup */
		lbegin = malloc(sizeof(uint64_t) * global_work_size);
		hbegin = malloc(sizeof(uint64_t) * global_work_size);
		lsup = malloc(sizeof(uint64_t) * global_work_size);
		hsup = malloc(sizeof(uint64_t) * global_work_size);

		if (hbegin == NULL || lbegin == NULL || hsup == NULL || lsup == NULL) {
			return -1;
		}

		/* fill begin and sup */
		for (i = 0; i < global_work_size; ++i) {
			uint128_t begin = (((uint128_t)(task_id + 0) << task_size) + ((uint128_t)(i + 0) << (task_size - task_units))) + 3;
			uint128_t sup   = (((uint128_t)(task_id + 0) << task_size) + ((uint128_t)(i + 1) << (task_size - task_units))) + 3;

			lbegin[i] = (uint64_t)begin;
			hbegin[i] = (uint64_t)(begin >> 64);
			lsup[i] = (uint64_t)sup;
			hsup[i] = (uint64_t)(sup >> 64);
		}

		/* create mem objects */
		mem_obj_lbegin = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_ulong) << task_units, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		mem_obj_hbegin = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_ulong) << task_units, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		mem_obj_lsup = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_ulong) << task_units, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		mem_obj_hsup = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_ulong) << task_units, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		/* transfer the begin and sup arrays to GPU global memory */
		ret = clEnqueueWriteBuffer(command_queue, mem_obj_lbegin, CL_TRUE, 0, sizeof(uint64_t) << task_units, lbegin, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clEnqueueWriteBuffer(command_queue, mem_obj_hbegin, CL_TRUE, 0, sizeof(uint64_t) << task_units, hbegin, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clEnqueueWriteBuffer(command_queue, mem_obj_lsup, CL_TRUE, 0, sizeof(uint64_t) << task_units, lsup, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clEnqueueWriteBuffer(command_queue, mem_obj_hsup, CL_TRUE, 0, sizeof(uint64_t) << task_units, hsup, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		/* set kernel args */
		ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&mem_obj_lbegin);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&mem_obj_hbegin);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&mem_obj_lsup);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void *)&mem_obj_hsup);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
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
			uint64_t overflow_counter = 0;

			if (checksum_alpha[i] == 0) {
				overflow_counter = cpu_worker(checksum_alpha, task_id, task_size, task_units, i);
			}

			g_overflow_counter += overflow_counter;
			g_checksum_alpha += checksum_alpha[i];
		}

		free(lbegin);
		free(hbegin);
		free(lsup);
		free(hsup);

		free(checksum_alpha);

		free(program_string);

		goto done;
	}

	return -1;

done:
	free(device_id);

	if (g_overflow_counter) {
		printf("[DEBUG] overflow occured!\n");
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

	/* user + system time */
	if (getrusage(RUSAGE_SELF, &usage) < 0) {
		/* errno is set appropriately. */
		perror("getrusage");
	} else {
		if ((sizeof(uint64_t) >= sizeof(time_t)) &&
		    (sizeof(uint64_t) >= sizeof(suseconds_t)) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1) <= UINT64_MAX / UINT64_C(1000000)) &&
		    (usage.ru_stime.tv_sec * UINT64_C(1) <= UINT64_MAX / UINT64_C(1000000)) &&
		    ((uint64_t)usage.ru_utime.tv_sec <= UINT64_MAX - usage.ru_stime.tv_sec) &&
		    (usage.ru_utime.tv_sec * UINT64_C(1000000) <= UINT64_MAX - usage.ru_utime.tv_usec) &&
		    (usage.ru_stime.tv_sec * UINT64_C(1000000) <= UINT64_MAX - usage.ru_stime.tv_usec) &&
		    ((usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec) <= UINT64_MAX - (usage.ru_stime.tv_sec * UINT64_C(1000000) + usage.ru_stime.tv_usec))) {
			uint64_t secs = (uint64_t)usage.ru_utime.tv_sec + usage.ru_stime.tv_sec;
			uint64_t usecs = usage.ru_utime.tv_sec * UINT64_C(1000000) + usage.ru_utime.tv_usec
			               + usage.ru_stime.tv_sec * UINT64_C(1000000) + usage.ru_stime.tv_usec;
			if (secs < UINT64_MAX && (uint64_t)usage.ru_utime.tv_usec + usage.ru_stime.tv_usec >= UINT64_C(1000000/2)) {
				/* round up */
				secs++;
			}
			printf("TIME %" PRIu64 " %" PRIu64 "\n", secs, usecs);
		} else if (sizeof(uint64_t) >= sizeof(time_t)) {
			uint64_t secs = (uint64_t)usage.ru_utime.tv_sec + usage.ru_stime.tv_sec; /* may overflow */
			printf("TIME %" PRIu64 "\n", secs);
		}
	}

	printf("OVERFLOW 128 %" PRIu64 "\n", g_overflow_counter);

	printf("CHECKSUM %" PRIu64 " %" PRIu64 "\n", g_checksum_alpha, g_checksum_beta);

	printf("HALTED\n");

	return 0;
}
