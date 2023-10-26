#ifndef UFBX_OS_H_INCLUDED
#define UFBX_OS_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stddef.h>

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

typedef struct ufbx_os_thread_pool ufbx_os_thread_pool;

ufbx_os_abi ufbx_os_thread_pool *ufbx_os_create_thread_pool(const ufbx_os_thread_pool_opts *user_opts);
ufbx_os_abi void ufbx_os_free_thread_pool(ufbx_os_thread_pool *pool);

ufbx_os_abi void ufbx_os_init_ufbx_thread_pool(ufbx_thread_pool *dst, ufbx_os_thread_pool *pool);

typedef void ufbx_os_thread_pool_task_fn(void *data, uint32_t index);

ufbx_os_abi uint32_t ufbx_os_thread_pool_run(ufbx_os_thread_pool *pool, ufbx_os_thread_pool_task_fn *fn, void *user, uint32_t count);
ufbx_os_abi void ufbx_os_thread_pool_wait(ufbx_os_thread_pool *pool, uint32_t index);

#define ufbxos_assert(cond) ufbx_assert(cond)

static void ufbxos_thread_pool_entry(ufbx_os_thread_pool *pool);

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <intrin.h>

typedef volatile long ufbxos_atomic_u32;

static uint32_t ufbxos_atomic_u32_load(ufbxos_atomic_u32 *ptr)
{
	return _InterlockedOr(ptr, 0);
}

static void ufbxos_atomic_u32_store(ufbxos_atomic_u32 *ptr, uint32_t value)
{
	_InterlockedExchange(ptr, (LONG)value);
}

static uint32_t ufbxos_atomic_u32_inc(ufbxos_atomic_u32 *ptr)
{
	return _InterlockedIncrement(ptr) - 1;
}

static uint32_t ufbxos_atomic_u32_dec(ufbxos_atomic_u32 *ptr)
{
	return _InterlockedDecrement(ptr) + 1;
}

static bool ufbxos_atomic_u32_cas(ufbxos_atomic_u32 *ptr, uint32_t ref, uint32_t value)
{
	return _InterlockedCompareExchange(ptr, value, ref) == ref;
}

static uint32_t ufbxos_atomic_u32_exchange(ufbxos_atomic_u32 *ptr, uint32_t value)
{
	return _InterlockedExchange(ptr, value);
}

typedef HANDLE ufbxos_os_semaphore;

static void ufbxos_os_semaphore_init(ufbxos_os_semaphore *os_sema, uint32_t max_count)
{
	*os_sema = CreateSemaphoreA(NULL, 0, (LONG)max_count, NULL);
}

static void ufbxos_os_semaphore_free(ufbxos_os_semaphore *os_sema)
{
	if (*os_sema == NULL) return;
	CloseHandle(*os_sema);
}

static void ufbxos_os_semaphore_wait(ufbxos_os_semaphore *os_sema)
{
	WaitForSingleObject(*os_sema, INFINITE);
}

static void ufbxos_os_semaphore_signal(ufbxos_os_semaphore *os_sema, uint32_t count)
{
	ReleaseSemaphore(*os_sema, (LONG)count, NULL);
}

typedef HANDLE ufbxos_os_thread;

static DWORD WINAPI ufbxos_os_thread_entry(LPVOID user)
{
	ufbxos_thread_pool_entry((ufbx_os_thread_pool*)user);
	return 0;
}

static bool ufbxos_os_thread_start(ufbxos_os_thread *os_thread, ufbx_os_thread_pool *pool)
{
	*os_thread = CreateThread(NULL, 0, &ufbxos_os_thread_entry, pool, 0, NULL);
	return *os_thread != NULL;
}

static void ufbxos_os_thread_join(ufbxos_os_thread *os_thread)
{
	if (*os_thread == NULL) return;
	WaitForSingleObject(*os_thread, INFINITE);
	CloseHandle(*os_thread);
}

static size_t ufbxos_os_get_logical_cores(void)
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return (size_t)info.dwNumberOfProcessors;
}

// --

#define UFBXOS_TASK_QUEUE_SIZE 256
#define UFBXOS_SEMA_POOL_SIZE (UFBXOS_TASK_QUEUE_SIZE + 1)

typedef struct {
	bool initialized;
	ufbxos_os_semaphore os_semaphore;
	ufbxos_atomic_u32 refcount;
} ufbxos_semaphore;

typedef struct {
	// [0:4] counter
	// [4:24] waiters
	// [24:32] sema_index
	ufbxos_atomic_u32 state;
} ufbxos_event;

#define UFBXOS_EVENT_COUNTER_MASK 0xf
#define UFBXOS_EVENT_WAITER_MASK 0xfffff

#define ufbxos_event_counter(state) ((state) & UFBXOS_EVENT_COUNTER_MASK)
#define ufbxos_event_waiters(state) (((state) >> 4) & UFBXOS_EVENT_WAITER_MASK)
#define ufbxos_event_sema_index(state) ((state) >> 24)
#define ufbxos_event_pack(counter, waiters, sema_index) ((counter) | (waiters) << 4 | (sema_index) << 24)

typedef union {
	struct {
		ufbx_thread_pool_context ctx;
		uint32_t start_index;
	} ufbx;
	struct {
		ufbx_os_thread_pool_task_fn *fn;
		void *user;
	} user;
} ufbxos_task_data;

typedef enum {
	UFBXOS_TASK_UFBX,
	UFBXOS_TASK_USER,
	UFBXOS_TASK_QUIT,
} ufbxos_task_type;

typedef struct {
	ufbxos_task_type type;
	uint32_t instance_count;
	ufbxos_task_data data;
	ufbxos_atomic_u32 counter;
	ufbxos_event start_event;
	ufbxos_event done_event;
} ufbxos_task;

struct ufbx_os_thread_pool {
	ufbxos_atomic_u32 start_index;
	ufbxos_task tasks[UFBXOS_TASK_QUEUE_SIZE];
	ufbxos_semaphore sema_pool[UFBXOS_SEMA_POOL_SIZE+1];
	uint32_t num_threads;
	ufbxos_os_thread threads[];
};

static uint32_t ufbxos_sema_alloc(ufbx_os_thread_pool *pool)
{
	for (uint32_t i = 1; i < UFBXOS_SEMA_POOL_SIZE; i++) {
		ufbxos_semaphore *sema = &pool->sema_pool[i];
		if (ufbxos_atomic_u32_cas(&sema->refcount, 0, 1)) {
			if (!sema->initialized) {
				sema->initialized = true;
				ufbxos_os_semaphore_init(&sema->os_semaphore, pool->num_threads * 2);
			}
			return i;
		}
	}
	return 0;
}

static void ufbxos_sema_acquire(ufbx_os_thread_pool *pool, uint32_t index)
{
	if (!index) return;
	ufbxos_semaphore *sema = &pool->sema_pool[index];
	ufbxos_atomic_u32_inc(&sema->refcount);
}

static void ufbxos_sema_release(ufbx_os_thread_pool *pool, uint32_t index)
{
	if (!index) return;
	ufbxos_semaphore *sema = &pool->sema_pool[index];
	ufbxos_atomic_u32_dec(&sema->refcount);
}

static void ufbxos_sema_wait(ufbx_os_thread_pool *pool, uint32_t index)
{
	ufbxos_os_semaphore_wait(&pool->sema_pool[index].os_semaphore);
}

static void ufbxos_sema_signal(ufbx_os_thread_pool *pool, uint32_t index, uint32_t count)
{
	ufbxos_os_semaphore_signal(&pool->sema_pool[index].os_semaphore, count);
}

static void ufbxos_event_wait(ufbx_os_thread_pool *pool, ufbxos_event *event, uint32_t ref_counter)
{
	for (;;) {
		uint32_t old_state = ufbxos_atomic_u32_load(&event->state);
		uint32_t counter = ufbxos_event_counter(old_state);
		uint32_t waiters = ufbxos_event_waiters(old_state);
		uint32_t sema_index = ufbxos_event_sema_index(old_state);

		if (counter == (ref_counter & UFBXOS_EVENT_COUNTER_MASK)) return;

		waiters += 1;
		ufbxos_assert(waiters <= UFBXOS_EVENT_WAITER_MASK);
		// TODO: Sleep?
		if (waiters > UFBXOS_EVENT_WAITER_MASK) continue;

		if (sema_index == 0) {
			sema_index = ufbxos_sema_alloc(pool);
			if (!sema_index) continue;
		} else {
			ufbxos_sema_acquire(pool, sema_index);
		}

		uint32_t new_state = ufbxos_event_pack(counter, waiters, sema_index);
		if (ufbxos_atomic_u32_cas(&event->state, old_state, new_state)) {
			ufbxos_sema_wait(pool, sema_index);
		}

		ufbxos_sema_release(pool, sema_index);
	}
}

static void ufbxos_event_signal(ufbx_os_thread_pool *pool, ufbxos_event *event, uint32_t counter)
{
	uint32_t new_state = ufbxos_event_pack(counter & UFBXOS_EVENT_COUNTER_MASK, 0, 0);
	uint32_t old_state = ufbxos_atomic_u32_exchange(&event->state, new_state);
	uint32_t waiters = ufbxos_event_waiters(old_state);
	uint32_t sema_index = ufbxos_event_sema_index(old_state);
	if (sema_index != 0) {
		ufbxos_sema_signal(pool, sema_index, waiters);
	}
}

static void ufbxos_thread_pool_entry(ufbx_os_thread_pool *pool)
{
	uint32_t task_index = 0;
	for (;;) {
		uint32_t cycle = task_index / UFBXOS_TASK_QUEUE_SIZE;
		ufbxos_task *task = &pool->tasks[task_index % UFBXOS_TASK_QUEUE_SIZE];

		// Wait for the start event to be signalled for the cycle.
		ufbxos_event_wait(pool, &task->start_event, cycle + 1);

		// Cache non-atomic task fields.
		ufbxos_task_type type = task->type;
		ufbxos_task_data data = task->data;
		uint32_t instance_count = task->instance_count;
		if (type == UFBXOS_TASK_QUIT) return;

		// Run tasks.
		uint32_t index = 0;
		for (;;) {
			index = ufbxos_atomic_u32_inc(&task->counter);
			if (index >= instance_count) break;

			if (type == UFBXOS_TASK_UFBX) {
				ufbx_thread_pool_run_task(data.ufbx.ctx, data.ufbx.start_index + index);
			} else if (type == UFBXOS_TASK_USER) {
				data.user.fn(data.user.user, index);
			}
		}

		// We will always overflow by exactly `num_threads`, so signal done
		// when the last thread is done.
		if (index == instance_count + pool->num_threads - 1) {
			ufbxos_event_signal(pool, &task->done_event, cycle * 2 + 1);
		}

		task_index++;
	}
}

static uint32_t ufbxos_thread_pool_run(ufbx_os_thread_pool *pool, ufbxos_task_type type, uint32_t count, const ufbxos_task_data *data)
{
	uint32_t index = ufbxos_atomic_u32_inc(&pool->start_index);
	uint32_t cycle = index / UFBXOS_TASK_QUEUE_SIZE;
	ufbxos_task *task = &pool->tasks[index % UFBXOS_TASK_QUEUE_SIZE];

	// Wait for `done_event`, the counter of `done_event` is bumped twice per cycle:
	// first once all threads have finished the tasks, second time when it's been waited on.
	ufbxos_event_wait(pool, &task->done_event, cycle * 2);

	// `task` is now mutually exclusive to this thread
	ufbxos_atomic_u32_store(&task->counter, 0);
	task->type = type;
	if (data) {
		task->data = *data;
	}
	task->instance_count = count;

	// Publish `task` to workers.
	ufbxos_event_signal(pool, &task->start_event, cycle + 1);
	return index;
}

static void ufbxos_thread_pool_wait(ufbx_os_thread_pool *pool, uint32_t task_index)
{
	uint32_t cycle = task_index / UFBXOS_TASK_QUEUE_SIZE;
	ufbxos_task *task = &pool->tasks[task_index % UFBXOS_TASK_QUEUE_SIZE];

	// Wait for `done_event` to be bumped once, then bump it a second time.
	ufbxos_event_wait(pool, &task->done_event, cycle * 2 + 1);
	ufbxos_event_signal(pool, &task->done_event, cycle * 2 + 2);
}

typedef struct {
	uint32_t task_index;
} ufbxos_thread_group;

typedef struct {
	ufbxos_thread_group groups[UFBX_THREAD_GROUP_COUNT];
} ufbxos_thread_pool_ctx;

static bool ufbxos_ufbx_thread_pool_init(void *user, ufbx_thread_pool_context ctx, const ufbx_thread_pool_info *info)
{
	ufbx_os_thread_pool *pool = (ufbx_os_thread_pool*)user;
	ufbxos_thread_pool_ctx *pool_ctx = (ufbxos_thread_pool_ctx*)calloc(1, sizeof(ufbxos_thread_pool_ctx));
	if (!pool_ctx) return false;
	ufbx_thread_pool_set_user_ptr(ctx, pool_ctx);
	return true;
}

static bool ufbxos_ufbx_thread_pool_run(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t start_index, uint32_t count)
{
	ufbx_os_thread_pool *pool = (ufbx_os_thread_pool*)user;
	ufbxos_thread_pool_ctx *pool_ctx = ufbx_thread_pool_get_user_ptr(ctx);

	ufbxos_task_data data;
	data.ufbx.ctx = ctx;
	data.ufbx.start_index = start_index;
	pool_ctx->groups[group].task_index = ufbxos_thread_pool_run(pool, UFBXOS_TASK_UFBX, count, &data);

	return true;
}

static bool ufbxos_ufbx_thread_pool_wait(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t max_index, bool speculative)
{
	ufbx_os_thread_pool *pool = (ufbx_os_thread_pool*)user;
	ufbxos_thread_pool_ctx *pool_ctx = ufbx_thread_pool_get_user_ptr(ctx);
	ufbxos_thread_pool_wait(pool, pool_ctx->groups[group].task_index);
	return true;
}

static void ufbxos_ufbx_thread_pool_free(void *user, ufbx_thread_pool_context ctx)
{
	ufbxos_thread_pool_ctx *pool_ctx = ufbx_thread_pool_get_user_ptr(ctx);
	free(pool_ctx);
}

// -- API

ufbx_os_abi ufbx_os_thread_pool *ufbx_os_create_thread_pool(const ufbx_os_thread_pool_opts *user_opts)
{
	ufbx_os_thread_pool_opts opts;
	if (user_opts) {
		memcpy(&opts, user_opts, sizeof(ufbx_os_thread_pool_opts));
	} else {
		memset(&opts, 0, sizeof(ufbx_os_thread_pool_opts));
	}

	ufbxos_assert(opts._begin_zero == 0 && opts._end_zero == 0);

	size_t num_threads = opts.max_threads;
	if (num_threads == 0) {
		num_threads = ufbxos_os_get_logical_cores();
		if (num_threads > UFBX_OS_DEFAULT_MAX_THREADS) {
			num_threads = UFBX_OS_DEFAULT_MAX_THREADS;
		}
	}
	if (num_threads > UINT32_MAX) {
		num_threads = UINT32_MAX;
	}

	ufbx_os_thread_pool *pool = (ufbx_os_thread_pool*)calloc(1, sizeof(ufbx_os_thread_pool) + sizeof(ufbxos_os_thread) * num_threads);
	if (!pool) return NULL;

	for (size_t i = 0; i < num_threads; i++) {
		if (!ufbxos_os_thread_start(&pool->threads[i], pool)) {
			ufbx_os_free_thread_pool(pool);
			return NULL;
		}
		pool->num_threads = (uint32_t)(i + 1);
	}

	return pool;
}

ufbx_os_abi void ufbx_os_free_thread_pool(ufbx_os_thread_pool *pool)
{
	if (!pool) return;

	// Signal end task
	ufbxos_thread_pool_run(pool, UFBXOS_TASK_QUIT, 0, NULL);

	for (size_t i = 0; i < pool->num_threads; i++) {
		ufbxos_os_thread_join(&pool->threads[i]);
	}

	for (size_t i = 0; i < UFBXOS_SEMA_POOL_SIZE; i++) {
		if (!pool->sema_pool[i].initialized) break;
		ufbxos_os_semaphore_free(&pool->sema_pool[i].os_semaphore);
	}

	free(pool);
}

ufbx_os_abi void ufbx_os_init_ufbx_thread_pool(ufbx_thread_pool *dst, ufbx_os_thread_pool *pool)
{
	if (!dst || !pool) return;

	memset(dst, 0, sizeof(ufbx_thread_pool));
	dst->user = pool;
	dst->init_fn = &ufbxos_ufbx_thread_pool_init;
	dst->run_fn = &ufbxos_ufbx_thread_pool_run;
	dst->wait_fn = &ufbxos_ufbx_thread_pool_wait;
	dst->free_fn = &ufbxos_ufbx_thread_pool_free;
}

ufbx_os_abi uint32_t ufbx_os_thread_pool_run(ufbx_os_thread_pool *pool, ufbx_os_thread_pool_task_fn *fn, void *user, uint32_t count)
{
	ufbxos_task_data data;
	data.user.fn = fn;
	data.user.user = user;
	return ufbxos_thread_pool_run(pool, UFBXOS_TASK_USER, count, &data);
}

ufbx_os_abi void ufbx_os_thread_pool_wait(ufbx_os_thread_pool *pool, uint32_t index)
{
	ufbxos_thread_pool_wait(pool, index);
}

#if 0

typedef struct {
	bool used;
	HANDLE handle;
} ufbxos_semaphore;

#define UFBXOS_MAX_THREADS 8

typedef struct {
	CRITICAL_SECTION sema_lock;
	ufbxos_semaphore sema_pool[UFBXOS_MAX_THREADS*2];
} ufbxos_thread_pool;

static uint32_t ufbxos_alloc_sema(ufbxos_thread_pool *pool)
{
	EnterCriticalSection(&pool->sema_lock);
	for (size_t i = 0; i < UFBXOS_MAX_THREADS*2; i++) {
		if (!pool->sema_pool[i].used) {
			pool->sema_pool[i].used = true;

			// Initialize handle if missing
			if (pool->sema_pool[i].handle == NULL) {
				pool->sema_pool[i].handle = CreateSemaphoreA(NULL, 0, UFBXOS_MAX_THREADS*2, NULL);
			}

			LeaveCriticalSection(&pool->sema_lock);
			return (uint32_t)i;
		}
	}
	LeaveCriticalSection(&pool->sema_lock);
	return (uint32_t)-1;
}

static void ufbxos_free_sema(ufbxos_thread_pool *pool, uint32_t index)
{
	EnterCriticalSection(&pool->sema_lock);
	ufbxos_semaphore *sema = &pool->sema_pool[index];
	sema->used = false;
	LeaveCriticalSection(&pool->sema_lock);
}

static void ufbxos_sema_signal(ufbxos_thread_pool *pool, uint32_t index, uint32_t count)
{
	ufbxos_semaphore *sema = &pool->sema_pool[index];
	ReleaseSemaphore(sema->handle, (LONG)count, NULL);
}

static void ufbxos_sema_wait(ufbxos_thread_pool *pool, uint32_t index)
{
	ufbxos_semaphore *sema = &pool->sema_pool[index];
	WaitForSingleObject(sema->handle, INFINITE);
}

typedef struct {
	// [0:16] count
	// [16:24] waiters
	// [24:32] sema index
	ufbxos_atomic_u32 state;
} ufbxos_barrier;

#define ufbxos_barrier_count(state) ((state) & 0xffff)
#define ufbxos_barrier_waiters(state) (((state) >> 16) & 0xff)
#define ufbxos_barrier_sema(state) ((state) >> 24)

#define ufbxos_barrier_pack(count, waiters, sema) ((count) | (waiters) << 16 | (sema) << 24)

static void ufbxos_barrier_wait(ufbxos_thread_pool *pool, ufbxos_barrier *barrier)
{
	for (;;) {
		uint32_t state = ufbxos_atomic_u32_load(&barrier->state);
		uint32_t count = ufbxos_barrier_count(state);
		if (count == 0) return;

		uint32_t waiters = ufbxos_barrier_waiters(state);
		uint32_t sema_index = ufbxos_barrier_sema(state);
		bool owns_sema = false;

		if (sema_index == 0) {
			sema_index = ufbxos_alloc_sema(pool);
			owns_sema = true;
		}

		uint32_t new_state = ufbxos_barrier_pack(count, waiters + 1, sema_index);
		if (ufbxos_atomic_u32_cas(&barrier->state, state, new_state)) {
			ufbxos_sema_wait(pool, sema_index);
			if (owns_sema) {
				ufbxos_free_sema(pool, sema_index);
			}
			return;
		} else {
			if (owns_sema) {
				ufbxos_free_sema(pool, sema_index);
			}
		}
	}
}

static void ufbxos_barrier_signal(ufbxos_thread_pool *pool, ufbxos_barrier *barrier)
{
	for (;;) {
		uint32_t state = ufbxos_atomic_u32_load(&barrier->state);
		uint32_t count = ufbxos_barrier_count(state);
		uint32_t waiters = ufbxos_barrier_waiters(state);
		uint32_t sema_index = ufbxos_barrier_sema(state);

		uint32_t new_state = ufbxos_barrier_pack(count - 1, waiters, sema_index);
		if (ufbxos_atomic_u32_cas(&barrier->state, state, new_state)) {
			ufbxos_sema_signal(pool, sema_index, 1);
			return;
		}
	}
}

#endif

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
#if 0

typedef uint32_t ufbxos_atomic_u32;

typedef struct {
	ufbxos_atomic_u32 count;
	ufbxos_atomic_u32 done_count;
	uint32_t offset;
	uintptr_t context;
} ufbxos_task;


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

#if 0

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

#endif
#endif
