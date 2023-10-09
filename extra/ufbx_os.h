#ifndef UFBX_OS_H_INCLUDED
#define UFBX_OS_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if !defined(UFBX_VERSION)
	#error "ufbx.h" must be included before "ufbx_os.h"
#endif

#ifndef ufbx_os_abi
#define ufbx_os_abi
#endif

#ifndef UFBX_OS_DEFAULT_MAX_THREADS
#define UFBX_OS_DEFAULT_MAX_THREADS 8
#endif

typedef struct ufbx_os_thread_pool_opts {
	uint32_t _begin_zero;

	size_t max_threads;

	uint32_t _end_zero;
} ufbx_os_thread_pool_opts;

ufbx_os_abi bool ufbx_os_init_thread_pool(ufbx_thread_pool *pool, const ufbx_os_thread_pool_opts *opts);

#endif

#if defined(UFBX_OS_IMPLEMENTATION)
#ifndef UFBX_OS_H_IMPLEMENTED
#define UFBX_OS_H_IMPLEMENTED

#if defined(_WIN32)
	#define UFBX_OS_WINDOWS
#endif

#if defined(UFBX_OS_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <intrin.h>
#include <stdlib.h>
#include <string.h>

#if defined(__cplusplus)
	#define ufbx_os_extern_c extern "C"
#else
	#define ufbx_os_extern_c
#endif

typedef struct {
	HANDLE handle;
} ufbx_win32_thread;

typedef struct {
	volatile long count;
	HANDLE handle;
} ufbx_win32_sema;

typedef struct {
	ufbx_win32_sema work_sema;
	HANDLE done_event;

	volatile long tasks_done;

	ufbx_thread_pool_context ctx;
	bool quit_flag;

	size_t max_threads;
	size_t max_threads_to_spawn;
	size_t num_threads;
	ufbx_win32_thread threads[];
} ufbx_win32_thread_pool;

static void ufbx_win32_close_handle(HANDLE h)
{
	if (h != NULL && h != INVALID_HANDLE_VALUE) {
		CloseHandle(h);
	}
}

static bool ufbx_win32_sema_init(ufbx_win32_sema *sema, size_t max_count)
{
	sema->count = 0;
	sema->handle = CreateSemaphoreA(NULL, 0, (LONG)max_count, NULL);
	return sema->handle != NULL;
}

static void ufbx_win32_sema_free(ufbx_win32_sema *sema)
{
	ufbx_win32_close_handle(sema->handle);
}

static void ufbx_win32_sema_wait(ufbx_win32_sema *sema)
{
	long count = _InterlockedDecrement(&sema->count);
	if (count <= -1) WaitForSingleObject(sema->handle, INFINITE);
}

static void ufbx_win32_sema_post(ufbx_win32_sema *sema, uint32_t count)
{
	long value = _InterlockedExchangeAdd(&sema->count, (LONG)count);
	if (value < 0) {
		long release = (long)count < -value ? (long)count : -value;
		ReleaseSemaphore(sema->handle, release, NULL);
	}
}

static size_t ufbx_os_get_logical_cores()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return (size_t)info.dwNumberOfProcessors;
}

static DWORD WINAPI ufbx_win32_thread_entry(LPVOID user)
{
	ufbx_win32_thread_pool *imp = (ufbx_win32_thread_pool*)user;
	for (;;) {
		ufbx_win32_sema_wait(&imp->work_sema);
		if (imp->quit_flag) break;
		ufbx_thread_pool_run_task(imp->ctx);
		_InterlockedIncrement(&imp->tasks_done);
		SetEvent(imp->done_event);
	}
}

static bool ufbx_win32_thread_pool_init(void *user, ufbx_thread_pool_context ctx, const ufbx_thread_pool_info *info)
{
	ufbx_win32_thread_pool *imp = (ufbx_win32_thread_pool*)user;
	imp->ctx = ctx;
	size_t max_count = info->max_concurrent_tasks + imp->max_threads;
	if (!ufbx_win32_sema_init(&imp->work_sema, max_count)) return false;
	return true;
}

static bool ufbx_win32_thread_pool_run(void *user, ufbx_thread_pool_context ctx, uint32_t start_index, uint32_t count)
{
	ufbx_win32_thread_pool *imp = (ufbx_win32_thread_pool*)user;

	uint32_t tasks_to_run = (uint32_t)(start_index - imp->tasks_done) + count;
	if (tasks_to_run > imp->num_threads && imp->num_threads < imp->max_threads_to_spawn) {
		ufbx_win32_thread *thread = &imp->threads[imp->num_threads];
		thread->handle = CreateThread(NULL, 0, &ufbx_win32_thread_entry, imp, 0, NULL);
		if (thread->handle != NULL) {
			imp->num_threads++;
		} else {
			imp->max_threads_to_spawn = 0;
		}
	}

	ufbx_win32_sema_post(&imp->work_sema, count);
	return true;
}

static bool ufbx_win32_thread_pool_wait(void *user, ufbx_thread_pool_context ctx, uint32_t index)
{
	ufbx_win32_thread_pool *imp = (ufbx_win32_thread_pool*)user;
	WaitForSingleObject(imp->done_event, INFINITE);
	return true;
}

static void ufbx_win32_thread_pool_free(void *user, ufbx_thread_pool_context ctx)
{
	ufbx_win32_thread_pool *imp = (ufbx_win32_thread_pool*)user;

	if (imp->num_threads > 0) {
		imp->quit_flag = true;
		ReleaseSemaphore(imp->work_sema.handle, (LONG)imp->num_threads, NULL);
		for (size_t i = 0; i < imp->num_threads; i++) {
			WaitForSingleObject(imp->threads[i].handle, INFINITE);
			CloseHandle(imp->threads[i].handle);
		}
	}

	ufbx_win32_sema_free(&imp->work_sema);
	ufbx_win32_close_handle(imp->done_event);
}

static bool ufbx_os_init_thread_pool_imp(ufbx_thread_pool *pool, const ufbx_os_thread_pool_opts *opts)
{
	size_t max_threads = opts->max_threads;
	ufbx_win32_thread_pool *imp = malloc(sizeof(ufbx_win32_thread_pool) + sizeof(ufbx_win32_thread) * max_threads);
	if (!pool) return false;

	memset(imp, 0, sizeof(ufbx_win32_thread_pool));

	pool->user = imp;
	pool->init_fn = &ufbx_win32_thread_pool_init;
	pool->run_fn = &ufbx_win32_thread_pool_run;
	pool->wait_fn = &ufbx_win32_thread_pool_wait;
	pool->free_fn = &ufbx_win32_thread_pool_free;

	imp->max_threads = max_threads;
	imp->max_threads_to_spawn = max_threads;

	imp->done_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (imp->done_event == NULL) return false;

	return true;
}

#endif

ufbx_os_abi bool ufbx_os_init_thread_pool(ufbx_thread_pool *pool, const ufbx_os_thread_pool_opts *user_opts)
{
	ufbx_os_thread_pool_opts opts;
	if (user_opts) {
		memcpy(&opts, user_opts, sizeof(ufbx_os_thread_pool_opts));
	} else {
		memset(&opts, 0, sizeof(ufbx_os_thread_pool_opts));
	}
	ufbx_assert(opts._begin_zero == 0 && opts._end_zero == 0);

	if (opts.max_threads == 0) {
		opts.max_threads = ufbx_os_get_logical_cores();
		if (opts.max_threads == 0) {
			opts.max_threads = 1;
		}
		if (opts.max_threads > UFBX_OS_DEFAULT_MAX_THREADS) {
			opts.max_threads = UFBX_OS_DEFAULT_MAX_THREADS;
		}
	}

	memset(pool, 0, sizeof(ufbx_thread_pool));
	bool ok = ufbx_os_init_thread_pool_imp(pool, &opts);
	if (!ok) {
		if (pool->free_fn) {
			pool->free_fn(pool->user, 0);
		}
		memset(pool, 0, sizeof(ufbx_thread_pool));
	}
	return ok;
}

#endif
#endif
