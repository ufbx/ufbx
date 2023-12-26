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

typedef struct ufbx_os_thread_pool_task ufbx_os_thread_pool_task;

typedef void ufbx_os_thread_pool_task_fn(void *user, uint32_t index);

ufbx_os_abi uint64_t ufbx_os_thread_pool_run(ufbx_os_thread_pool *pool, ufbx_os_thread_pool_task_fn *fn, void *user, uint32_t count);
ufbx_os_abi bool ufbx_os_thread_pool_try_wait(ufbx_os_thread_pool *pool, uint64_t task_id);
ufbx_os_abi void ufbx_os_thread_pool_wait(ufbx_os_thread_pool *pool, uint64_t task_id);

// #define ufbxos_assert(cond) ufbx_assert(cond)
#define ufbxos_assert(cond) if (!(cond)) __debugbreak()

static void ufbxos_thread_pool_entry(ufbx_os_thread_pool *pool);

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <intrin.h>

typedef volatile long ufbxos_atomic_u32;

static uint32_t ufbxos_atomic_u32_load_relaxed(ufbxos_atomic_u32 *ptr)
{
	return (uint32_t)__iso_volatile_load32((volatile const int*)ptr);
}

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

static uint32_t ufbxos_atomic_u32_add(ufbxos_atomic_u32 *ptr, uint32_t value)
{
	return (uint32_t)_InterlockedExchangeAdd(ptr, (LONG)value);
}

static bool ufbxos_atomic_u32_cas(ufbxos_atomic_u32 *ptr, uint32_t ref, uint32_t value)
{
	return _InterlockedCompareExchange(ptr, value, ref) == ref;
}

static uint32_t ufbxos_atomic_u32_exchange(ufbxos_atomic_u32 *ptr, uint32_t value)
{
	return _InterlockedExchange(ptr, value);
}

typedef volatile LONG64 ufbxos_atomic_u64;

static uint64_t ufbxos_atomic_u64_load(ufbxos_atomic_u64 *ptr)
{
	return (uint64_t)_InterlockedCompareExchange64(ptr, 0, 0);
}

static uint64_t ufbxos_atomic_u64_load_relaxed(ufbxos_atomic_u64 *ptr)
{
	return *((uint64_t*)ptr);
}

static bool ufbxos_atomic_u64_cas(ufbxos_atomic_u64 *ptr, uint64_t *ref, uint64_t value)
{
	uint64_t prev = *ref;
	uint64_t next = (uint64_t)_InterlockedCompareExchange64(ptr, (LONG64)value, (LONG64)prev);
	*ref = next;
	return prev == next;
}

static void ufbxos_atomic_u64_store(ufbxos_atomic_u64 *ptr, uint64_t value)
{
	InterlockedExchange64(ptr, value);
}

static uint64_t ufbxos_atomic_u64_inc(ufbxos_atomic_u64 *ptr)
{
	return _InterlockedIncrement64(ptr) - 1;
}

typedef volatile LONG64 ufbxos_atomic_ptr;

static void *ufbxos_atomic_ptr_load(ufbxos_atomic_ptr *ptr)
{
	// TODO: Load-acquire
	return (void*)*ptr;
}

static bool ufbxos_atomic_ptr_cas_null(ufbxos_atomic_ptr *ptr, void *value)
{
	return _InterlockedCompareExchange64(ptr, (LONG64)NULL, (LONG64)value) == (LONG64)NULL;
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

static uint32_t ufbxos_ctz32(uint32_t mask)
{
	DWORD index;
	_BitScanForward(&index, mask);
	return index;
}

static uint32_t ufbxos_bsr32(uint32_t mask)
{
	DWORD index;
	_BitScanReverse(&index, mask);
	return index;
}

typedef CRITICAL_SECTION ufbxos_os_mutex;
typedef CONDITION_VARIABLE ufbxos_os_cond;

static void ufbxos_os_mutex_init(ufbxos_os_mutex *os_mutex) { InitializeCriticalSection(os_mutex); }
static bool ufbxos_os_mutex_try_lock(ufbxos_os_mutex *os_mutex) { return TryEnterCriticalSection(os_mutex); }
static void ufbxos_os_mutex_lock(ufbxos_os_mutex *os_mutex) { EnterCriticalSection(os_mutex); }
static void ufbxos_os_mutex_unlock(ufbxos_os_mutex *os_mutex) { LeaveCriticalSection(os_mutex); }
static void ufbxos_os_mutex_free(ufbxos_os_mutex *os_mutex) { DeleteCriticalSection(os_mutex); }

static void ufbxos_os_cond_init(ufbxos_os_cond *os_cond) { InitializeConditionVariable(os_cond); }
static void ufbxos_os_cond_wait(ufbxos_os_cond *os_cond, ufbxos_os_mutex *os_mutex) { SleepConditionVariableCS(os_cond, os_mutex, INFINITE); }
static void ufbxos_os_cond_wake_all(ufbxos_os_cond *os_cond) { WakeAllConditionVariable(os_cond); }
static void ufbxos_os_cond_free(ufbxos_os_cond *os_cond) { }

static void ufbxos_os_yield()
{
	Sleep(0);
}

#define ufbxos_os_pause() _mm_pause()

#define UFBXOS_OS_WAIT_NOTIFY 0

#if UFBXOS_OS_WAIT_NOTIFY

#pragma comment(lib, "synchronization.lib")

static void ufbxos_os_atomic_wait32(ufbxos_atomic_u32 *ptr, uint32_t value)
{
	WaitOnAddress(ptr, &value, 4, INFINITE);
}

static void ufbxos_os_atomic_wait64(ufbxos_atomic_u64 *ptr, uint64_t value)
{
	WaitOnAddress(ptr, &value, 8, INFINITE);
}

static void ufbxos_os_atomic_notify32(ufbxos_atomic_u32 *ptr)
{
	WakeByAddressAll((PVOID)ptr);
}

static void ufbxos_os_atomic_notify64(ufbxos_atomic_u64 *ptr)
{
	WakeByAddressAll((PVOID)ptr);
}

#endif

// --

#define UFBXOS_WAIT_SEMA_MAX_WAITERS 0x7fff

typedef struct {
	// [0:15]  waiters
	// [15:30] sleepers
	// [30]    signaled
	// [32:64] revision
	ufbxos_atomic_u64 state;
	ufbxos_os_semaphore os_semaphore[2];
} ufbxos_wait_sema;

typedef struct {
	uint32_t waiters;
	uint32_t sleepers;
	bool signaled;
	uint32_t revision;
} ufbxos_wait_sema_state;

static ufbxos_wait_sema_state ufbxos_wait_sema_decode(uint64_t state)
{
	ufbxos_wait_sema_state s;
	uint32_t state_lo = (uint32_t)state;
	s.waiters = (state_lo >> 0) & 0x7fff;
	s.sleepers = (state_lo >> 15) & 0x7fff;
	s.signaled = ((state_lo >> 30) & 0x1) != 0;
	s.revision = (uint32_t)(state >> 32);
	return s;
}

static uint64_t ufbxos_wait_sema_encode(ufbxos_wait_sema_state s)
{
	return (uint64_t)(s.waiters | s.sleepers << 15 | (s.signaled ? 1 : 0) << 30)
		| (uint64_t)s.revision << 32;
}

typedef struct {
	// [0:8]   sema_index
	// [8:32]  hash
	// [32:64] sema_revision
	ufbxos_atomic_u64 state;
} ufbxos_wait_entry;

typedef struct {
	uint32_t sema_index;
	uint32_t hash;
	uint32_t sema_revision;
} ufbxos_wait_entry_state;

static ufbxos_wait_entry_state ufbxos_wait_entry_decode(uint64_t state)
{
	ufbxos_wait_entry_state s;
	uint32_t state_lo = (uint32_t)state;
	s.sema_index = (state_lo >> 0) & 0xff;
	s.hash = (state_lo >> 8) & 0xffffff;
	s.sema_revision = (uint32_t)(state >> 32);
	return s;
}

static uint64_t ufbxos_wait_entry_encode(ufbxos_wait_entry_state s)
{
	return (uint64_t)(s.sema_index | s.hash << 8) | (uint64_t)s.sema_revision << 32;
}

typedef struct {
	// [0:16] refcount
	// [16:64] cycle
	ufbxos_atomic_u64 state;

	ufbxos_atomic_u64 next;

	ufbx_os_thread_pool_task_fn *fn;
	void *user;
	uint32_t count;
	ufbxos_atomic_u32 counter;

} ufbxos_task;

typedef struct {
	uint32_t refcount;
	uint64_t cycle;
} ufbxos_task_state;

static ufbxos_task_state ufbxos_task_decode(uint64_t state)
{
	ufbxos_task_state s;
	uint32_t state_lo = (uint32_t)state;
	s.refcount = (state_lo >> 0) & 0xffff;
	s.cycle = state >> 16;
	return s;
}

static uint64_t ufbxos_task_encode(ufbxos_task_state s)
{
	return (uint64_t)s.refcount | (s.cycle << 16);
}

#define UFBXOS_WAIT_SEMA_MAX_COUNT 64
#define UFBXOS_WAIT_SEMA_SCAN 16

#define UFBXOS_WAIT_MAP_SIZE 128
#define UFBXOS_WAIT_MAP_SCAN 2

struct ufbx_os_thread_pool {
	ufbxos_atomic_u32 wait_sema_lock;
	ufbxos_atomic_u32 wait_sema_count;
	ufbxos_wait_sema wait_semas[UFBXOS_WAIT_SEMA_MAX_COUNT];

	ufbxos_wait_entry wait_map[UFBXOS_WAIT_MAP_SIZE];

	uint32_t num_tasks;
	uint32_t task_cycle_shift;
	uint32_t task_index_mask;
	ufbxos_task *tasks;

	ufbxos_atomic_u64 task_alloc_head;
	ufbxos_atomic_u64 task_work_tail;
	ufbxos_atomic_u64 task_work_head;
	ufbxos_atomic_u32 task_init_count;

	uint32_t num_threads;
	ufbxos_os_thread *threads;
};

static uint32_t ufbxos_wait_sema_create(ufbx_os_thread_pool *pool)
{
	if (ufbxos_atomic_u32_cas(&pool->wait_sema_lock, 0, 1)) {
		uint32_t index = ufbxos_atomic_u32_load(&pool->wait_sema_count);
		if (index >= UFBXOS_WAIT_SEMA_MAX_COUNT) {
			ufbxos_atomic_u32_store(&pool->wait_sema_lock, 0);
			return 0;
		}

		ufbxos_wait_sema *sema = &pool->wait_semas[index];

		ufbxos_os_semaphore_init(&sema->os_semaphore[0], UFBXOS_WAIT_SEMA_MAX_WAITERS);
		ufbxos_os_semaphore_init(&sema->os_semaphore[1], UFBXOS_WAIT_SEMA_MAX_WAITERS);

		ufbxos_atomic_u32_cas(&pool->wait_sema_count, index, index + 1);
		ufbxos_atomic_u32_store(&pool->wait_sema_lock, 0);
		return index + 1;
	}

	return 0;
}

static uint32_t ufbxos_wait_sema_alloc(ufbx_os_thread_pool *pool, uint32_t hint, uint32_t *p_revision)
{
	uint32_t count = ufbxos_atomic_u32_load(&pool->wait_sema_count);
	uint32_t scan = count < UFBXOS_WAIT_SEMA_SCAN ? count : UFBXOS_WAIT_SEMA_SCAN;
	ufbxos_assert(count > 0);

	for (uint32_t attempt = 0; attempt < 4; attempt++) {
		if (attempt == 1 && ufbxos_atomic_u32_load_relaxed(&pool->wait_sema_count) < UFBXOS_WAIT_SEMA_MAX_COUNT) {
			ufbxos_wait_sema_create(pool);
		}

		uint32_t index = hint % count;
		for (uint32_t i = 0; i < scan; i++) {
			ufbxos_wait_sema *sema = &pool->wait_semas[index];
			uint64_t old_state = ufbxos_atomic_u64_load(&sema->state);
			for (;;) {
				ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

				if (attempt <= 0 && s.waiters > 0) break;
				if (attempt <= 1 && s.sleepers > 0) break;
				if (attempt <= 2 && s.signaled) break;
				if (s.waiters >= UFBXOS_WAIT_SEMA_MAX_WAITERS) break;
				s.waiters += 1;

				if (ufbxos_atomic_u64_cas(&sema->state, &old_state, ufbxos_wait_sema_encode(s))) {
					*p_revision = s.revision;
					return index + 1;
				}
			}

			if (++index >= count) index = 0;
		}
	}

	return 0;
}

static bool ufbxos_wait_sema_outdated(ufbx_os_thread_pool *pool, uint32_t index, uint32_t revision)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[index - 1];
	uint64_t old_state = ufbxos_atomic_u64_load(&sema->state);
	ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);
	return s.revision != revision;
}

static bool ufbxos_wait_sema_join(ufbx_os_thread_pool *pool, uint32_t index, uint32_t revision)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[index - 1];
	uint64_t old_state = ufbxos_atomic_u64_load(&sema->state);
	for (;;) {
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		if (s.revision != revision) return false;
		if (s.waiters >= UFBXOS_WAIT_SEMA_MAX_WAITERS) return false;
		s.waiters += 1;

		if (ufbxos_atomic_u64_cas(&sema->state, &old_state, ufbxos_wait_sema_encode(s))) {
			return true;
		}
	}
}

static void ufbxos_wait_sema_wait(ufbx_os_thread_pool *pool, uint32_t index, uint32_t revision)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[index - 1];

	ufbxos_os_semaphore_wait(&sema->os_semaphore[revision & 1]);

	uint64_t old_state = ufbxos_atomic_u64_load(&sema->state);
	for (;;) {
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		bool do_signal = false;
		s.sleepers -= 1;
		if (s.sleepers == 0 && s.signaled) {
			s.sleepers = s.waiters;
			s.waiters = 0;
			s.revision++;
			s.signaled = false;
			do_signal = true;
		}

		if (ufbxos_atomic_u64_cas(&sema->state, &old_state, ufbxos_wait_sema_encode(s))) {
			if (do_signal && s.sleepers > 0) {
				ufbxos_os_semaphore_signal(&sema->os_semaphore[(s.revision - 1) & 1], s.sleepers);
			}
			break;
		}
	}
}

static void ufbxos_wait_sema_signal(ufbx_os_thread_pool *pool, uint32_t index, uint32_t revision)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[index - 1];

	uint64_t old_state = ufbxos_atomic_u64_load(&sema->state);
	for (;;) {
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		if (s.revision != revision) break;
		if (s.signaled) break;

		bool do_signal = false;
		if (s.sleepers == 0) {
			s.sleepers = s.waiters;
			s.waiters = 0;
			s.revision++;
			do_signal = true;
		} else {
			s.signaled = true;
		}

		if (ufbxos_atomic_u64_cas(&sema->state, &old_state, ufbxos_wait_sema_encode(s))) {
			if (do_signal && s.sleepers > 0) {
				ufbxos_os_semaphore_signal(&sema->os_semaphore[(s.revision - 1) & 1], s.sleepers);
			}
			break;
		}
	}
}

static void ufbxos_wait_sema_leave(ufbx_os_thread_pool *pool, uint32_t index, uint32_t revision)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[index - 1];
	uint64_t old_state = ufbxos_atomic_u64_load(&sema->state);
	for (;;) {
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		if (s.revision != revision) {
			ufbxos_wait_sema_wait(pool, index, revision);
			return;
		}
		s.waiters -= 1;

		if (ufbxos_atomic_u64_cas(&sema->state, &old_state, ufbxos_wait_sema_encode(s))) {

			// If we were the last waiter signal the semaphore so it won't stay dangling
			if (s.waiters == 0) {
				ufbxos_wait_sema_signal(pool, index, revision);
			}

			return;
		}
	}
}

static uint32_t ufbxos_hash_ptr(const void *ptr)
{
	uint32_t x = (uint32_t)(((uintptr_t)ptr >> 2) ^ ((uintptr_t)ptr >> 30));
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
	if (x == 0) x = 1;
    return x;
}

static bool ufbxos_hash_is(uint32_t ref_hash, uint32_t hash)
{
	hash = hash & 0x7fffff;
	if (ref_hash & 0x800000) {
		return false;
	} else {
		return ref_hash == hash;
	}
}

static bool ufbxos_hash_contains(uint32_t ref_hash, uint32_t hash)
{
	hash = hash & 0x7fffff;
	if (ref_hash & 0x800000) {
		return (ref_hash & (1u << (hash % 23u))) != 0;
	} else {
		return ref_hash == hash;
	}
}

static uint32_t ufbxos_hash_insert(uint32_t ref_hash, uint32_t hash)
{
	hash = hash & 0x7fffff;
	if (ref_hash == 0 || hash == ref_hash) return hash;
	if ((ref_hash & 0x800000) == 0) {
		ref_hash = 0x800000 | (1u << (ref_hash % 23));
	}
	ref_hash |= 1u << (hash % 23u);
	return ref_hash;
}

static bool ufbxos_atomic_try_wait(ufbx_os_thread_pool *pool, uint32_t *p_sema_index, uint32_t *p_sema_revision, uint32_t hash, uint32_t attempt)
{
	for (uint32_t i = 0; i < UFBXOS_WAIT_MAP_SCAN; i++) {
		uint32_t index = (hash + i) & (UFBXOS_WAIT_MAP_SIZE - 1);
		ufbxos_wait_entry *entry = &pool->wait_map[index];
		uint64_t old_state = ufbxos_atomic_u64_load(&entry->state);

		for (;;) {
			ufbxos_wait_entry_state s = ufbxos_wait_entry_decode(old_state);

			bool ok = false;
			if (ufbxos_hash_is(s.hash, hash)) ok = true;
			if (attempt >= 1 && s.hash == 0) ok = true;
			if (attempt >= 2 && !ufbxos_hash_contains(s.hash, hash)) ok = true;
			if (attempt >= 3) ok = false;
			if (!ok) break;

			if (s.sema_index == 0 || ufbxos_wait_sema_outdated(pool, s.sema_index, s.sema_revision)) {
				uint32_t revision = 0;
				s.hash = ufbxos_hash_insert(s.hash, hash);
				s.sema_index = ufbxos_wait_sema_alloc(pool, index, &revision);
				s.sema_revision = revision;
				if (s.sema_index != 0 && ufbxos_atomic_u64_cas(&entry->state, &old_state, ufbxos_wait_entry_encode(s))) {
					*p_sema_index = s.sema_index;
					*p_sema_revision = s.sema_revision;
					return true;
				} else {
					ufbxos_wait_sema_leave(pool, s.sema_index, s.sema_revision);
				}
			} else {
				uint32_t old_hash = s.hash;
				s.hash = ufbxos_hash_insert(s.hash, hash);
				if (s.hash == old_hash || ufbxos_atomic_u64_cas(&entry->state, &old_state, ufbxos_wait_entry_encode(s))) {
					if (ufbxos_wait_sema_join(pool, s.sema_index, s.sema_revision)) {
						*p_sema_index = s.sema_index;
						*p_sema_revision = s.sema_revision;
						return true;
					} else {
						break;
					}
				}
			}
		}
	}

	// All semas are full, pause for a bit
	if (attempt == 3) {
		ufbxos_os_yield();
	}

	return false;
}

static void ufbxos_atomic_wait32(ufbx_os_thread_pool *pool, ufbxos_atomic_u32 *ptr, uint32_t value)
{
#if UFBXOS_OS_WAIT_NOTIFY
	ufbxos_os_atomic_wait32(ptr, value);
#else
	uint32_t sema_index = 0;
	uint32_t sema_revision = 0;
	uint32_t hash = ufbxos_hash_ptr((const void*)ptr);
	for (uint32_t attempt = 0; attempt < 4; attempt++) {
		if (ufbxos_atomic_u32_load(ptr) != value) return;
		if (ufbxos_atomic_try_wait(pool, &sema_index, &sema_revision, hash, attempt)) {
			if (ufbxos_atomic_u32_load(ptr) != value) {
				ufbxos_wait_sema_leave(pool, sema_index, sema_revision);
			} else {
				ufbxos_wait_sema_wait(pool, sema_index, sema_revision);
			}
			break;
		}
	}
#endif
}

static void ufbxos_atomic_wait64(ufbx_os_thread_pool *pool, ufbxos_atomic_u64 *ptr, uint64_t value)
{
#if UFBXOS_OS_WAIT_NOTIFY
	ufbxos_os_atomic_wait64(ptr, value);
#else
	uint32_t sema_index = 0;
	uint32_t sema_revision = 0;
	uint32_t hash = ufbxos_hash_ptr((const void*)ptr);
	for (uint32_t attempt = 0; attempt < 4; attempt++) {
		if (ufbxos_atomic_u64_load(ptr) != value) return;
		if (ufbxos_atomic_try_wait(pool, &sema_index, &sema_revision, hash, attempt)) {
			if (ufbxos_atomic_u64_load(ptr) != value) {
				ufbxos_wait_sema_leave(pool, sema_index, sema_revision);
			} else {
				ufbxos_wait_sema_wait(pool, sema_index, sema_revision);
			}
			break;
		}
	}
#endif
}

static void ufbxos_atomic_notify(ufbx_os_thread_pool *pool, const void *ptr)
{
	uint32_t hash = ufbxos_hash_ptr(ptr);
	for (uint32_t i = 0; i < UFBXOS_WAIT_MAP_SCAN; i++) {
		uint32_t index = (hash + i) & (UFBXOS_WAIT_MAP_SIZE - 1);
		ufbxos_wait_entry *entry = &pool->wait_map[index];
		uint64_t old_state = ufbxos_atomic_u64_load(&entry->state);
		if (old_state == 0) continue;

		for (;;) {
			ufbxos_wait_entry_state s = ufbxos_wait_entry_decode(old_state);
			if (!ufbxos_hash_contains(s.hash, hash)) break;
			if (ufbxos_atomic_u64_cas(&entry->state, &old_state, 0)) {
				ufbxos_wait_sema_signal(pool, s.sema_index, s.sema_revision);
				break;
			}
		}
	}
}

static void ufbxos_atomic_notify32(ufbx_os_thread_pool *pool, ufbxos_atomic_u32 *ptr)
{
#if UFBXOS_OS_WAIT_NOTIFY
	ufbxos_os_atomic_notify32(ptr);
#else
	ufbxos_atomic_notify(pool, (const void*)ptr);
#endif
}

static void ufbxos_atomic_notify64(ufbx_os_thread_pool *pool, ufbxos_atomic_u64 *ptr)
{
#if UFBXOS_OS_WAIT_NOTIFY
	ufbxos_os_atomic_notify64(ptr);
#else
	ufbxos_atomic_notify(pool, (const void*)ptr);
#endif
}

static uint32_t ufbxos_task_index(ufbx_os_thread_pool *pool, uint64_t task_id)
{
	return (uint32_t)task_id & pool->task_index_mask;
}

static uint64_t ufbxos_task_cycle(ufbx_os_thread_pool *pool, uint64_t task_id)
{
	return task_id >> pool->task_cycle_shift;
}

static uint64_t ufbxos_task_id(ufbx_os_thread_pool *pool, uint32_t index, uint64_t cycle)
{
	ufbxos_assert(index <= pool->task_index_mask);
	return (cycle << pool->task_cycle_shift) | index;
}

static uint64_t ufbxos_push_task(ufbx_os_thread_pool *pool, ufbx_os_thread_pool_task_fn *fn, void *user, uint32_t count)
{
	uint64_t task_id = ufbxos_atomic_u64_load(&pool->task_alloc_head);
	for (;;) {
		if (task_id == 0) {
			if (ufbxos_atomic_u32_load_relaxed(&pool->task_init_count) < pool->num_tasks) {
				uint32_t index = ufbxos_atomic_u32_inc(&pool->task_init_count);
				if (index < pool->num_tasks) {
					task_id = index;
					// TODO: Init
					break;
				}
			}

			ufbxos_atomic_wait64(pool, &pool->task_alloc_head, 0);
			continue;
		}

		ufbxos_task *task = &pool->tasks[ufbxos_task_index(pool, task_id)];
		uint64_t next_id = ufbxos_atomic_u64_load(&task->next);
		if (ufbxos_atomic_u64_cas(&pool->task_alloc_head, &task_id, next_id)) {
			break;
		}
	}

	uint32_t task_index = ufbxos_task_index(pool, task_id);
	uint64_t task_cycle = ufbxos_task_cycle(pool, task_id);
	ufbxos_task *task = &pool->tasks[task_index];

	task->fn = fn;
	task->user = user;
	task->count = count;
	ufbxos_atomic_u32_store(&task->counter, 0);
	ufbxos_atomic_u64_store(&task->next, task_id);

	uint64_t work_head = ufbxos_atomic_u64_load(&pool->task_work_head);
	for (;;) {
		ufbxos_task *prev = &pool->tasks[ufbxos_task_index(pool, work_head)];

		if (ufbxos_atomic_u64_cas(&pool->task_work_head, &work_head, task_id)) {
			ufbxos_atomic_u64_store(&prev->next, task_id);
			ufbxos_atomic_notify64(pool, &prev->next);
			break;
		}
	}

	return task_id;
}

static void ufbxos_thread_pool_entry(ufbx_os_thread_pool *pool)
{
	uint64_t task_id = 0;
	for (;;) {
		uint32_t task_index = ufbxos_task_index(pool, task_id);
		uint64_t task_cycle = ufbxos_task_cycle(pool, task_id);
		ufbxos_task *task = &pool->tasks[task_index];

		bool ok = false;
		uint64_t old_state = ufbxos_atomic_u64_load(&task->state);
		for (;;) {
			ufbxos_task_state s = ufbxos_task_decode(old_state);
			if (s.cycle > task_cycle) break;
			if (s.refcount >= 0xffff) break;

			s.refcount += 1;
			if (ufbxos_atomic_u64_cas(&task->state, &old_state, ufbxos_task_encode(s))) {
				ok = true;
				break;
			}
		}

		uint64_t free_task_id = 0;
		if (ok) {
			ufbx_os_thread_pool_task_fn *fn = task->fn;
			void *user = task->user;
			uint32_t count = task->count;
			for (;;) {
				uint32_t index = ufbxos_atomic_u32_inc(&task->counter);
				if (index >= count) break;
				fn(user, index);
			}

			old_state = ufbxos_atomic_u64_load(&task->state);
			for (;;) {
				ufbxos_task_state s = ufbxos_task_decode(old_state);
				s.refcount -= 1;
				if (s.refcount == 0) {
					s.cycle += 1;
				}
				if (ufbxos_atomic_u64_cas(&task->state, &old_state, ufbxos_task_encode(s))) {
					if (s.refcount == 0) {
						ufbxos_atomic_notify64(pool, &task->state);
						// TODO: if (cycle <= max_cycle)
						free_task_id = ufbxos_task_id(pool, task_index, s.cycle);
					}
					break;
				}
			}
		}

		uint64_t prev_task_id = task_id;
		for (;;) {
			task_id = ufbxos_atomic_u64_load(&pool->task_work_tail);
			if (task_id != prev_task_id) break;

			task_id = ufbxos_atomic_u64_load(&task->next);
			if (task_id == prev_task_id) {
				ufbxos_atomic_wait64(pool, &task->next, prev_task_id);
				continue;
			}

			ufbxos_atomic_u64_cas(&pool->task_work_tail, &prev_task_id, task_id);
			break;
		}

		if (free_task_id != 0) {
			uint64_t prev_head = ufbxos_atomic_u64_load(&pool->task_alloc_head);
			for (;;) {
				ufbxos_atomic_u64_store(&task->next, prev_head);
				if (ufbxos_atomic_u64_cas(&pool->task_alloc_head, &prev_head, free_task_id)) {
					break;
				}
			}
		}
	}
}

// -- API

ufbx_os_abi ufbx_os_thread_pool *ufbx_os_create_thread_pool(const ufbx_os_thread_pool_opts *user_opts)
{
	ufbx_os_thread_pool *pool = (ufbx_os_thread_pool*)calloc(1, sizeof(ufbx_os_thread_pool));
	for (uint32_t i = 0; i < 2; i++) {
		ufbxos_wait_sema_create(pool);
	}

	pool->num_tasks = 256;
	pool->task_index_mask = pool->num_tasks - 1;
	pool->task_cycle_shift = 8;
	pool->tasks = (ufbxos_task*)calloc(pool->num_tasks, sizeof(ufbxos_task));

	pool->num_threads = 4;
	pool->threads = (ufbxos_os_thread*)calloc(pool->num_threads, sizeof(ufbxos_os_thread));
	for (uint32_t i = 0; i < pool->num_threads; i++) {
		ufbxos_os_thread_start(&pool->threads[i], pool);
	}

	ufbxos_atomic_u32_store(&pool->task_init_count, 1);

	return pool;
}

ufbx_os_abi void ufbx_os_free_thread_pool(ufbx_os_thread_pool *pool)
{
}

ufbx_os_abi uint64_t ufbx_os_thread_pool_run(ufbx_os_thread_pool *pool, ufbx_os_thread_pool_task_fn *fn, void *user, uint32_t count)
{
	return ufbxos_push_task(pool, fn, user, count);
}

ufbx_os_abi bool ufbx_os_thread_pool_try_wait(ufbx_os_thread_pool *pool, uint64_t task_id)
{
	uint32_t task_index = ufbxos_task_index(pool, task_id);
	uint64_t task_cycle = ufbxos_task_cycle(pool, task_id);
	ufbxos_task *task = &pool->tasks[task_index];
	ufbxos_task_state s = ufbxos_task_decode(ufbxos_atomic_u64_load(&task->state));
	return s.cycle > task_cycle;
}

ufbx_os_abi void ufbx_os_thread_pool_wait(ufbx_os_thread_pool *pool, uint64_t task_id)
{
	uint32_t task_index = ufbxos_task_index(pool, task_id);
	uint64_t task_cycle = ufbxos_task_cycle(pool, task_id);
	ufbxos_task *task = &pool->tasks[task_index];
	for (;;) {
		uint64_t state = ufbxos_atomic_u64_load(&task->state);
		ufbxos_task_state s = ufbxos_task_decode(state);
		if (s.cycle > task_cycle) return;
		ufbxos_atomic_wait64(pool, &task->state, state);
	}
}

typedef struct {
	uint64_t task_id;
	uint32_t start_index; 
	ufbx_thread_pool_context ctx;
} ufbxos_pool_group;

typedef struct {
	union {
		ufbxos_pool_group group;
		char padding[64];
	} groups[UFBX_THREAD_GROUP_COUNT];
	void *allocation;
} ufbxos_pool_ctx;

static void ufbxos_ufbx_task(void *user, uint32_t index)
{
	ufbxos_pool_group *group = (ufbxos_pool_group*)user;
	ufbx_thread_pool_run_task(group->ctx, group->start_index + index);
}

static bool ufbxos_ufbx_thread_pool_init(void *user, ufbx_thread_pool_context ctx, const ufbx_thread_pool_info *info)
{
	void *allocation = calloc(1, sizeof(ufbxos_pool_ctx) + 128);
	if (!allocation) return false;

	char *data = (char*)allocation + (((uintptr_t)-(intptr_t)allocation) & 63);
	ufbxos_pool_ctx *up = (ufbxos_pool_ctx*)data;
	up->allocation = allocation;

	ufbx_thread_pool_set_user_ptr(ctx, up);

	return true;
}

static bool ufbxos_ufbx_thread_pool_run(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t start_index, uint32_t count)
{
	ufbx_os_thread_pool *pool = (ufbx_os_thread_pool*)user;
	ufbxos_pool_ctx *up = (ufbxos_pool_ctx*)ufbx_thread_pool_get_user_ptr(ctx);
	ufbxos_pool_group *ug = &up->groups[group].group;

	ug->start_index = start_index;
	ug->ctx = ctx;
	ug->task_id = ufbx_os_thread_pool_run(pool, &ufbxos_ufbx_task, ug, count);

	return true;
}

static bool ufbxos_ufbx_thread_pool_wait(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t max_index, bool speculative)
{
	ufbx_os_thread_pool *pool = (ufbx_os_thread_pool*)user;
	ufbxos_pool_ctx *up = (ufbxos_pool_ctx*)ufbx_thread_pool_get_user_ptr(ctx);
	ufbxos_pool_group *ug = &up->groups[group].group;

	ufbx_os_thread_pool_wait(pool, ug->task_id);

	return true;
}

static void ufbxos_ufbx_thread_pool_free(void *user, ufbx_thread_pool_context ctx)
{
	ufbx_os_thread_pool *pool = (ufbx_os_thread_pool*)user;
	ufbxos_pool_ctx *up = (ufbxos_pool_ctx*)ufbx_thread_pool_get_user_ptr(ctx);

	free(up->allocation);
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

#if 0

typedef struct {
	// [0:15] waiters
	// [15] parity
	// [16:31] sleepers
	// [31] signaled
	ufbxos_atomic_u32 state;
	// 0: uninitialized
	// 1: initializing
	// 2: initialized
	ufbxos_atomic_u32 initialized;
	ufbxos_os_semaphore os_semaphore[2];
} ufbxos_wait_sema;

typedef struct {
	uint32_t waiters;
	uint32_t sleepers;
	uint8_t parity;
	bool signaled;
} ufbxos_wait_sema_state;

static ufbxos_wait_sema_state ufbxos_wait_sema_decode(uint32_t state)
{
	ufbxos_wait_sema_state s;
	s.waiters = (state >> 0) & 0x7fff;
	s.sleepers = (state >> 16) & 0x7fff;
	s.parity = (uint8_t)((state >> 15) & 0x1);
	s.signaled = ((state >> 31) & 0x1) != 0;
	return s;
}

static uint32_t ufbxos_wait_sema_encode(ufbxos_wait_sema_state s)
{
	return s.waiters
		| (uint32_t)s.parity << 15
		| s.sleepers << 16
		| (s.signaled ? 1 : 0) << 31;
}

#define UFBXOS_WAIT_SEMA_COUNT 256

struct ufbx_os_thread_pool {
	ufbxos_wait_sema wait_semas[UFBXOS_WAIT_SEMA_COUNT];

	uint32_t num_threads;
};

static uint32_t ufbxos_wait_sema_alloc(ufbx_os_thread_pool *pool, uintptr_t hint)
{
	// Attempt 1: Try to allocate a unused wait sema
	for (uint32_t i = 0; i < UFBXOS_WAIT_SEMA_COUNT; i++) {
		ufbxos_wait_sema *sema = &pool->wait_semas[i];

		// Lazily initialize semaphores as we see new ones. Skip semaphores
		// being actively initialized by other threads (initialized=1)
		uint32_t init = ufbxos_atomic_u32_load(&sema->initialized);
		if (init == 0) {
			if (ufbxos_atomic_u32_cas(&sema->initialized, 0, 1)) {
				ufbxos_os_semaphore_init(&sema->os_semaphore[0], 256);
				ufbxos_os_semaphore_init(&sema->os_semaphore[1], 256);
				ufbxos_atomic_u32_store(&sema->initialized, 2);
			} else {
				continue;
			}
		} else if (init == 1) {
			continue;
		}

		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		if (s.signaled) continue;
		if (s.waiters > 0) continue;

		s.waiters += 1;

		if (ufbxos_atomic_u32_cas(&sema->state, old_state, ufbxos_wait_sema_encode(s))) {
			return i + 1;
		}
	}

	// Attempt 2: Try to alias into an existing wait sema that is not signaled.
	uint32_t hint_hash = (uint32_t)(hint ^ (hint >> 8));
	for (uint32_t i = 0; i < UFBXOS_WAIT_SEMA_COUNT; i++) {
		uint32_t index = (i + hint_hash) % UFBXOS_WAIT_SEMA_COUNT;
		ufbxos_wait_sema *sema = &pool->wait_semas[index];
		if (ufbxos_atomic_u32_load(&sema->initialized) < 2) continue;

		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		if (s.signaled) continue;

		s.waiters += 1;

		if (ufbxos_atomic_u32_cas(&sema->state, old_state, ufbxos_wait_sema_encode(s))) {
			return index + 1;
		}
	}

	// Attempt 3: Join any initialized semaphore, even if signaled.
	for (uint32_t i = 0; i < UFBXOS_WAIT_SEMA_COUNT; i++) {
		uint32_t index = (i + hint_hash) % UFBXOS_WAIT_SEMA_COUNT;
		ufbxos_wait_sema *sema = &pool->wait_semas[index];
		if (ufbxos_atomic_u32_load(&sema->initialized) < 2) continue;

		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		s.waiters += 1;

		if (ufbxos_atomic_u32_cas(&sema->state, old_state, ufbxos_wait_sema_encode(s))) {
			return index + 1;
		}
	}

	ufbxos_assert(0 && "Should always find an initialized semaphore");
	return 0;
}

static uint32_t ufbxos_wait_sema_join(ufbx_os_thread_pool *pool, uint32_t index)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[index - 1];

	for (;;) {
		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		s.waiters += 1;

		if (ufbxos_atomic_u32_cas(&sema->state, old_state, ufbxos_wait_sema_encode(s))) {
			return old_state;
		}
	}
}

static void ufbxos_wait_sema_wait(ufbx_os_thread_pool *pool, uint32_t index, uint32_t original_state)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[index - 1];
	ufbxos_wait_sema_state os = ufbxos_wait_sema_decode(original_state);

	ufbxos_os_semaphore_wait(sema->os_semaphore[os.parity]);

	for (;;) {
		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		ufbxos_assert(s.sleepers > 0);
		s.sleepers -= 1;

		// If we're the last sleeper to wake up and the sema has been re-signaled,
		// signal the currently waiting threads.
		bool do_signal = false;
		if (s.signaled && s.sleepers == 0) {
			s.sleepers = s.waiters;
			s.waiters = 0;
			s.parity ^= 1;
			s.signaled = false;
			do_signal = true;
		}

		if (ufbxos_atomic_u32_cas(&sema->state, old_state, ufbxos_wait_sema_encode(s))) {
			if (do_signal) {
				ufbxos_os_semaphore_signal(sema->os_semaphore[s.parity ^ 1], s.sleepers);
			}
			break;
		}
	}
}

static void ufbxos_wait_sema_leave(ufbx_os_thread_pool *pool, uint32_t index, uint32_t original_state)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[index - 1];
	ufbxos_wait_sema_state os = ufbxos_wait_sema_decode(original_state);

	for (;;) {
		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		// If semaphore we joined has been signaled, we are counted into the
		// OS semaphore release count, so we need to wait on it to keep it in sync.
		if (s.parity != os.parity) {
			ufbxos_wait_sema_wait(pool, index, original_state);
			return;
		}

		// Has not been signaled, so we can remove ourselves from the sema.
		ufbxos_assert(s.waiters > 0);
		s.waiters -= 1;

		if (ufbxos_atomic_u32_cas(&sema->state, old_state, ufbxos_wait_sema_encode(s))) {
			return;
		}
	}
}

static void ufbxos_wait_sema_signal(ufbx_os_thread_pool *pool, uint32_t index, uint32_t original_state)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[index - 1];
	ufbxos_wait_sema_state os = ufbxos_wait_sema_decode(original_state);

	for (;;) {
		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		ufbxos_wait_sema_state s = ufbxos_wait_sema_decode(old_state);

		// Already re-signaled
		if (s.signaled) break;

		// Signal the semaphroe if all existing sleepers have returned from the
		// semaphore, otherwise delay re-signaling by setting the `signaled` bit.
		bool do_signal = false;
		if (s.sleepers == 0) {
			s.sleepers = s.waiters;
			s.waiters = 0;
			s.parity ^= 1;
			s.signaled = false;
			do_signal = true;
		} else {
			s.signaled = true;
		}

		if (ufbxos_atomic_u32_cas(&sema->state, old_state, ufbxos_wait_sema_encode(s))) {
			if (do_signal) {
				ufbxos_os_semaphore_signal(sema->os_semaphore[s.parity ^ 1], s.sleepers);
			}
			return;
		}
	}
}

#endif

#if 0

typedef struct {
	ufbxos_atomic_u32 count;
	ufbxos_os_semaphore os_semaphore;
} ufbxos_semaphore;

static void ufbxos_semaphore_wait(ufbxos_semaphore *sema)
{
	int32_t count = (int32_t)ufbxos_atomic_u32_dec(&sema->count);
	if (count <= 0) {
		ufbxos_os_semaphore_wait(&sema->os_semaphore);
	}
}

static void ufbxos_semaphore_signal(ufbxos_semaphore *sema)
{
	int32_t count = (int32_t)ufbxos_atomic_u32_inc(&sema->count);
	if (count < 0) {
		ufbxos_os_semaphore_signal(&sema->os_semaphore, 1);
	}
}

typedef struct {
	ufbxos_atomic_ptr pointers[28];
	size_t element_size;
} ufbxos_array;

static void ufbxos_array_init(ufbxos_array *arr, size_t element_size)
{
	arr->element_size = element_size;
}

static void *ufbxos_array_get(ufbxos_array *arr, uint32_t index)
{
	uint32_t bucket = index < 16 ? 0 : ufbxos_bsr32(index) - 3;
	void *ptr = ufbxos_atomic_ptr_load(&arr->pointers[bucket]);
	uint32_t prefix_size = ((1 << bucket) - 1) * 16;

	if (ptr == NULL) {
		uint32_t bucket_size = 16 << bucket;
		void *new_ptr = calloc(bucket_size, arr->element_size);
		ufbxos_assert(new_ptr);
		if (ufbxos_atomic_ptr_cas_null(&arr->pointers[bucket], new_ptr)) {
			ptr = new_ptr;
		} else {
			free(new_ptr);
			ptr = ufbxos_atomic_ptr_load(&arr->pointers[bucket]);
		}
	}

	return (char*)ptr + arr->element_size * (index - prefix_size);
}

typedef struct {
	ufbxos_array data;
	ufbxos_array free_list;
	ufbxos_atomic_u32 init_count;
	ufbxos_atomic_u64 free_count;
} ufbxos_pool;

static void ufbxos_pool_init(ufbxos_pool *pool, size_t data_size)
{
	ufbxos_array_init(&pool->data, data_size);
	ufbxos_array_init(&pool->free_list, sizeof(uint32_t));
	ufbxos_atomic_u32_store(&pool->init_count, 0);
	ufbxos_atomic_u64_store(&pool->free_count, 0);
}

static bool ufbxos_pool_alloc(ufbxos_pool *pool, uint32_t *p_index)
{
	uint64_t free_count = ufbxos_atomic_u64_load(&pool->free_count);
	for (;;) {
		uint32_t free_index = (uint32_t)free_count;
		if (free_index == 0) break;
		uint32_t index = *(uint32_t*)ufbxos_array_get(&pool->free_list, free_index - 1);

		uint64_t new_free_count = free_count + ((uint64_t)1 << 32) - 1;
		if (ufbxos_atomic_u64_cas(&pool->free_count, &free_count, new_free_count)) {
			*p_index = index;
			return true;
		}
	}

	*p_index = ufbxos_atomic_u32_inc(&pool->init_count);
	return false;
}

static void ufbxos_pool_free(ufbxos_pool *pool, uint32_t index)
{
	uint64_t free_count = ufbxos_atomic_u64_load(&pool->free_count);
	for (;;) {
		uint32_t free_index = (uint32_t)free_count;
		*(uint32_t*)ufbxos_array_get(&pool->free_list, free_index) = index;

		uint64_t new_free_count = free_count + ((uint64_t)1 << 32) - 1;
		if (ufbxos_atomic_u64_cas(&pool->free_count, &free_count, new_free_count)) {
			*p_index = index;
			return true;
		}
	}

	*p_index = ufbxos_atomic_u32_inc(&pool->init_count);
	return false;
}

typedef struct {
	ufbxos_os_semaphore os_semaphore;
	ufbxos_atomic_u32 waiters;
} ufbxos_pooled_sema;

typedef struct {
	// [0:16] waiters
	// [16:32] index
	ufbxos_atomic_u32 state;
} ufbxos_wait_sema;

#define ufbxos_wait_sema_waiters(state) (uint32_t)((state) & 0xffff)
#define ufbxos_wait_sema_index(state) (uint32_t)((((state) >> 16) & 0xffff)

#define ufbxos_wait_sema_pack(waiters, signaled) ((waiters) | (signaled) << 31u)

#define UFBXOS_WAIT_SEMA_COUNT 256

typedef struct {
	// [0:8] sema_index
	// [8:16] workers
	// [16:64] cycle
	ufbxos_atomic_u64 state;

	ufbxos_atomic_u32 instance_index;
	uint32_t instance_count;

	// TODO: Linked list for free/used tasks
	uint64_t next;

	ufbx_os_thread_pool_task_fn *fn;
	void *user;

} ufbxos_queue_task;

struct ufbx_os_thread_pool {
	ufbxos_atomic_u64 start_index;

	ufbxos_os_semaphore wait_sema_init;
	ufbxos_wait_sema wait_semas[UFBXOS_WAIT_SEMA_COUNT];

	uint32_t task_queue_shift;
	uint32_t task_queue_mask;
	ufbxos_queue_task *task_queue;

	ufbxos_atomic_u64 free_tasks;

	uint32_t num_threads;
};

static uint32_t ufbxos_wait_sema_alloc(ufbx_os_thread_pool *pool, uintptr_t hint)
{
	// Attempt 1: Allocate a free wait sema
	for (uint32_t i = 0; i < UFBXOS_WAIT_SEMA_COUNT; i++) {
		ufbxos_wait_sema *sema = &pool->wait_semas[i];

		uint32_t old_state = ufbxos_atomic_u32_load_relaxed(&sema->state);
		if (old_state != 0) continue;

		uint32_t new_state = ufbxos_wait_sema_pack(1, 0);
		if (ufbxos_atomic_u32_cas(&sema->state, 0, new_state)) {
			if (!sema->initialized) {
				ufbxos_os_semaphore_init(&sema->os_semaphore, 256); // TODO: Count
				sema->initialized = true;
			}
			return i + 1;
		}
	}

	// Attempt 2: All semaphores should be initialized now, try to allocate the
	// a semaphore near our 
	for (;;) {
		uint32_t prev_waiters = UINT32_MAX;
		uint32_t best_index = 0;
		uint32_t hint_hash = (uint32_t)hint ^ ((uint32_t)hint >> 8);
		for (uint32_t i = 0; i < UFBXOS_WAIT_SEMA_COUNT; i++) {
			uint32_t index = (i + hint_hash) % UFBXOS_WAIT_SEMA_COUNT;

			ufbxos_wait_sema *sema = &pool->wait_semas[i];

			uint32_t old_state = ufbxos_atomic_u32_load_relaxed(&sema->state);
			if (ufbxos_wait_sema_signaled(old_state)) continue;

			uint32_t waiters = ufbxos_wait_sema_waiters(sema->state);
			if (waiters < prev_waiters) {
				best_index = index;
			}

		}

		{
			ufbxos_wait_sema *sema = &pool->wait_semas[i];
			uint32_t new_state = ufbxos_wait_sema_pack(1, 0);
			if (ufbxos_atomic_u32_cas(&sema->state, 0, new_state)) {
				if (!ufbxos_atomic_u32_load_relaxed(&sema->initialized)) {
					ufbxos_os_semaphore_init(&sema->os_semaphore, 256); // TODO: Count
					ufbxos_os_semaphore_init(&sema->os_semaphore_next, 256); // TODO: Count
					ufbxos_atomic_u32_store(&sema->initialized, 1);
				}
				return i;
			}
		}
	}

#endif

#if 0
	for (uint32_t attempt = 0; attempt < 3; attempt++) {

		for (uint32_t i = 0; i < UFBXOS_WAIT_SEMA_COUNT; i++) {
			uint32_t index = i;

			// Randomize start position on further attempts (all semaphores in use)
			if (attempt > 0) {
				uint32_t hash = (uint32_t)hint ^ ((uint32_t)hint >> 8);
				index = (index + hash) % UFBXOS_WAIT_SEMA_COUNT;
			}

			ufbxos_wait_sema *sema = &pool->wait_semas[index];

			uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
			uint32_t waiters = ufbxos_wait_sema_waiters(old_state);
			uint32_t signaled = ufbxos_wait_sema_signaled(old_state);

			if (signaled) continue;
			if (attempt <= 0 && waiters > 0) continue;
			if (attempt > 0 && !ufbxos_atomic_u32_load(&sema->initialized)) continue;

			if (signaled) {
				next_waiters += 1;
			} else {
				waiters += 1;
			}

			uint32_t new_state = ufbxos_wait_sema_pack(waiters, signaled, next_waiters, next_signaled);
			if (ufbxos_atomic_u32_cas(&sema->state, old_state, new_state)) {
				if (!ufbxos_atomic_u32_load_relaxed(&sema->initialized)) {
					ufbxos_os_semaphore_init(&sema->os_semaphore, 256); // TODO: Count
					ufbxos_os_semaphore_init(&sema->os_semaphore_next, 256); // TODO: Count
					ufbxos_atomic_u32_store(&sema->initialized, 1);
				}

				if (signaled) {
					ufbxos_os_semaphore_wait(&sema->os_semaphore_next);
					for (;;) {
						ufbxos_atomic_u32_cas(&sema->state, old_state, new_state)
					}
				}

				return index;
			}
		}
	}
}
#endif

#if 0

typedef struct {
	// [0:30] waiters
	// [30] signaled
	// [31] reserved
	// [32:64] task_index
	ufbxos_atomic_u64 state;
	bool initialized;
	ufbxos_os_semaphore os_semaphore;
} ufbxos_wait_sema;

#define UFBXOS_WAIT_SEMA_COUNT 64

typedef struct {
	// [0:16] sema_index
	// [16:32] workers
	// [32:64] cycle
	ufbxos_atomic_u64 state;

	ufbxos_atomic_u32 instance_index;
	uint32_t instance_count;

	// TODO: Linked list for free/used tasks
	uint64_t next;

	ufbx_os_thread_pool_task_fn *fn;
	void *user;

} ufbxos_queue_task;

#define ufbxos_queue_task_sema_index(state) (uint32_t)((state) & 0xffff)
#define ufbxos_queue_task_cycle(state) ((state) >> 16)
#define ufbxos_queue_task_pack(sema_index, cycle) ((uint64_t)(sema_index) | (cycle) << 16u)

struct ufbx_os_thread_pool {
	ufbxos_atomic_u64 start_index;

	ufbxos_wait_sema wait_semas[UFBXOS_WAIT_SEMA_COUNT];

	uint32_t task_queue_shift;
	uint32_t task_queue_mask;
	ufbxos_queue_task *task_queue;

	ufbxos_atomic_u64 free_tasks;

	uint32_t num_threads;
};

#define ufbxos_wait_sema_waiters(state) (uint32_t)((state) & 0x3fffffff)
#define ufbxos_wait_sema_signaled(state) (uint32_t)(((state) >> 30) & 0x1)
#define ufbxos_wait_sema_reserved(state) (uint32_t)(((state) >> 31) & 0x1)
#define ufbxos_wait_sema_task_index(state) (uint32_t)((state) >> 32)
#define ufbxos_wait_sema_pack(waiters, task_index, signaled, reserved) ((uint64_t)((waiters) | (signaled) << 30u | (reserved) << 31u) | (uint64_t)(task_index) << 32u)

static uint64_t ufbxos_task_token(ufbx_os_thread_pool *pool, uint32_t task_index, uint64_t cycle)
{
	return task_index | cycle << pool->task_queue_shift;
}

static uint32_t ufbxos_task_index(ufbx_os_thread_pool *pool, uint64_t token)
{
	return (uint32_t)token & pool->task_queue_mask;
}

static uint64_t ufbxos_task_cycle(ufbx_os_thread_pool *pool, uint64_t token)
{
	return token >> pool->task_queue_mask;
}

static uint32_t ufbxos_wait_sema_alloc(ufbx_os_thread_pool *pool, uint32_t task_index)
{
	uint64_t new_state = ufbxos_wait_sema_pack(1, task_index, 0, 1);
	for (uint32_t i = 1; i < UFBXOS_WAIT_SEMA_COUNT; i++) {
		ufbxos_wait_sema *sema = &pool->wait_semas[i];
		uint64_t ref = 0;
		if (ufbxos_atomic_u64_cas(&sema->state, &ref, new_state)) {
			if (!sema->initialized) {
				ufbxos_os_semaphore_init(&sema->os_semaphore, 256); // TODO: Count
				sema->initialized = true;
			}
			return i;
		}
	}
	return 0;
}

static uint32_t ufbxos_wait_sema_cleanup(uint32_t state)
{
	uint32_t waiters = ufbxos_wait_sema_waiters(state);
	uint32_t signaled = ufbxos_wait_sema_signaled(state);
	if (waiters == 0 && signaled) return 0;
	return state;
}

static void ufbxos_wait_sema_free(ufbx_os_thread_pool *pool, uint32_t sema_index, uint32_t task_index)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[sema_index];
	uint64_t old_state = ufbxos_atomic_u64_load(&sema->state);
	for (;;) {
		uint32_t waiters = ufbxos_wait_sema_waiters(old_state);

		ufbxos_assert(waiters == 1);
		ufbxos_assert(ufbxos_wait_sema_reserved(old_state));
		ufbxos_assert(!ufbxos_wait_sema_signaled(old_state));
		ufbxos_assert(ufbxos_wait_sema_task_index(old_state) == task_index);

		if (ufbxos_atomic_u32_cas(&sema->state, &old_state, 0)) {
			break;
		}
	}
}

static bool ufbxos_wait_sema_join(ufbx_os_thread_pool *pool, uint32_t sema_index, uint32_t task_index)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[sema_index];
	uint64_t old_state = ufbxos_atomic_u64_load(&sema->state);
	for (;;) {
		uint32_t waiters = ufbxos_wait_sema_waiters(old_state);

		if (!ufbxos_wait_sema_reserved(old_state)) return false;
		if (ufbxos_wait_sema_signaled(old_state)) return false;
		if (ufbxos_wait_sema_task_index(old_state) != task_index) return false;

		uint64_t new_state = ufbxos_wait_sema_pack(waiters + 1, task_index, 0, 1);
		if (ufbxos_atomic_u64_cas(&sema->state, &old_state, new_state)) {
			return true;
		}
	}
}

static void ufbxos_wait_sema_wait(ufbx_os_thread_pool *pool, uint32_t sema_index, uint32_t task_index)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[sema_index];

	ufbxos_os_semaphore_wait(&sema->os_semaphore);

	uint64_t old_state = ufbxos_atomic_u64_load(&sema->state);
	for (;;) {
		uint32_t waiters = ufbxos_wait_sema_waiters(old_state);

		// The wait sema cannot be reused before all waiters are cleared.
		ufbxos_assert(waiters > 0);
		ufbxos_assert(ufbxos_wait_sema_reserved(old_state));
		ufbxos_assert(ufbxos_wait_sema_signaled(old_state));
		ufbxos_assert(ufbxos_wait_sema_task_index(old_state) == task_index);

		uint64_t new_state = ufbxos_wait_sema_pack(waiters - 1, task_index, 1, 1);
		new_state = ufbxos_wait_sema_cleanup(new_state);
		if (ufbxos_atomic_u64_cas(&sema->state, &old_state, new_state)) {
			break;
		}
	}
}

static void ufbxos_wait_sema_signal(ufbx_os_thread_pool *pool, uint32_t sema_index, uint32_t task_index)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[sema_index];
	uint64_t old_state = ufbxos_atomic_u64_load(&sema->state);
	for (;;) {
		uint32_t waiters = ufbxos_wait_sema_waiters(old_state);

		// The wait sema cannot be reused before all waiters are cleared.
		ufbxos_assert(ufbxos_wait_sema_reserved(old_state));
		ufbxos_assert(!ufbxos_wait_sema_signaled(old_state));
		ufbxos_assert(ufbxos_wait_sema_task_index(old_state) == task_index);

		uint64_t new_state = ufbxos_wait_sema_pack(waiters, task_index, 1, 1);
		new_state = ufbxos_wait_sema_cleanup(new_state);
		if (ufbxos_atomic_u64_cas(&sema->state, &old_state, new_state)) {
			ufbxos_os_semaphore_signal(&sema->os_semaphore, waiters);
			break;
		}
	}
}

static uint64_t ufbxos_queue_task_wait(ufbx_os_thread_pool *pool, uint32_t task_index, uint64_t cycle)
{
	ufbxos_queue_task *task = &pool->task_queue[task_index];
	uint64_t old_state = ufbxos_atomic_u64_load(&task->state);
	for (;;) {
		uint64_t ref_cycle = ufbxos_queue_task_cycle(old_state);
		if (ref_cycle >= cycle) break;

		bool owns_sema = false;
		uint32_t sema_index = ufbxos_queue_task_sema_index(old_state);
		if (sema_index == 0) {
			sema_index = ufbxos_wait_sema_alloc(pool, task_index);
			owns_sema = true;
		} else {
			ufbxos_wait_sema_join(pool, sema_index, task_index);
		}

		// TODO: Sleep
		if (sema_index == 0) {
			continue;
		}

		uint64_t new_state = ufbxos_queue_task_pack(sema_index, ref_cycle);
		if (ufbxos_atomic_u64_cas(&task->state, &old_state, new_state)) {
			ufbxos_wait_sema_wait(pool, sema_index, task_index);
		} else {
			ufbxos_wait_sema_free(pool, sema_index, task_index);
		}
	}
}

static uint64_t ufbxos_queue_task_signal(ufbx_os_thread_pool *pool, uint32_t task_index, uint64_t cycle)
{
	ufbxos_queue_task *task = &pool->task_queue[task_index];
	uint64_t old_state = ufbxos_atomic_u64_load(&task->state);
	for (;;) {
		uint32_t sema_index = ufbxos_queue_task_sema_index(old_state);
		ufbxos_assert(ufbxos_queue_task_cycle(old_state) == cycle);

		uint64_t new_state = ufbxos_queue_task_pack(sema_index, cycle);
		if (ufbxos_atomic_u64_cas(&task->state, &old_state, new_state)) {
			if (sema_index != 0) {
				ufbxos_wait_sema_signal(pool, sema_index, task_index);
			}
			break;
		}
	}
}

static void ufbxos_pool_worker(ufbx_os_thread_pool *pool)
{
	uint32_t task_index = 0;
	for (;;) {
		uint32_t task_index = (uint32_t)(index & pool->task_queue_mask);
		uint64_t cycle = (index >> pool->task_queue_shift) * 2;

		ufbxos_queue_task_wait(pool, task_index, cycle + 1);

		// Run tasks.
		ufbxos_queue_task *task = &pool->task_queue[task_index];
		ufbx_os_thread_pool_task_fn *fn = task->fn;
		void *user = task->user;

		uint32_t num_done = 0;
		uint32_t instance_count = task->instance_count;
		uint32_t instance_index = 0;
		for (;;) {
			instance_index = ufbxos_atomic_u32_inc(&task->instance_index);
			if (instance_index >= instance_count) break;

			fn(user, instance_index);
			num_done++;
		}

		uint32_t done_count = ufbxos_atomic_u32_add(&task->done_count, num_done);
		if (done_count < instance_count && done_count + num_done >= instance_count) {
			if (index == instance_count + pool->num_threads - 1) {
				ufbxos_queue_task_signal(pool, task_index, cycle + 2);
			}
		}

		index++;
	}
}

ufbx_os_abi uint64_t ufbx_os_thread_pool_run(ufbx_os_thread_pool *pool, ufbx_os_thread_pool_task_fn *fn, void *user, uint32_t count)
{
	uint64_t index = ufbxos_atomic_u64_inc(&pool->start_index);
	uint32_t task_index = (uint32_t)(index & pool->task_queue_mask);
	uint64_t cycle = (index >> pool->task_queue_shift) * 2;

	ufbxos_queue_task_wait(pool, task_index, cycle);

	ufbxos_queue_task *task = &pool->task_queue[task_index];
	ufbxos_atomic_u32_store(&task->instance_index, 0);
	ufbxos_atomic_u32_store(&task->done_count, 0);
	task->fn = fn;
	task->user = user;
	task->instance_count = count;

	ufbxos_queue_task_signal(pool, task_index, cycle + 1);
}

ufbx_os_abi bool ufbx_os_thread_pool_try_wait(ufbx_os_thread_pool *pool, uint64_t index)
{
	uint64_t index = ufbxos_atomic_u64_inc(&pool->start_index);
	uint32_t task_index = (uint32_t)(index & pool->task_queue_mask);
	ufbxos_queue_task *task = &pool->task_queue[task_index];
	uint64_t cycle = (index >> pool->task_queue_shift) * 2;

	uint64_t old_state = ufbxos_atomic_u64_load(&task->state);
	uint64_t ref_cycle = ufbxos_queue_task_cycle(old_state);
	return ref_cycle >= cycle + 2;
}

ufbx_os_abi void ufbx_os_thread_pool_wait(ufbx_os_thread_pool *pool, uint64_t index)
{
	uint64_t index = ufbxos_atomic_u64_inc(&pool->start_index);
	uint32_t task_index = (uint32_t)(index & pool->task_queue_mask);
	uint64_t cycle = (index >> pool->task_queue_shift) * 2;

	ufbxos_queue_task_wait(pool, task_index, cycle + 2);
}

#endif

#if 0

typedef struct {
	// [0:16] waiters
	// [16:30] task_index
	// [30] signaled
	// [31] reserved
	ufbxos_atomic_u32 state;
	bool initialized;
	ufbxos_os_semaphore os_semaphore;
} ufbxos_wait_sema;

#define ufbxos_wait_sema_waiters(state) ((state) & 0xffff)
#define ufbxos_wait_sema_task_index(state) (((state) >> 16) & 0x3fff)
#define ufbxos_wait_sema_signaled(state) (((state) >> 31) & 0x1)
#define ufbxos_wait_sema_reserved(state) (((state) >> 31) & 0x1)
#define ufbxos_wait_sema_pack(waiters, task_index, signaled, reserved) ((uint32_t)(waiters) | (uint32_t)(task_index) << 16 | (uint32_t)(signaled) << 30u | (uint32_t)(reserved) << 31u)

#define UFBXOS_WAIT_SEMA_COUNT 64

typedef struct {
	ufbxos_atomic_u32 hi[2];

	// [0:31] lo
	// [31] hi_index
	ufbxos_atomic_u32 state;
} ufbxos_atomic_u64;

static uint64_t ufbxos_atomic_u64_inc(ufbxos_atomic_u64 *a)
{
	uint32_t state = ufbxos_atomic_u32_inc(&a->state);
	uint32_t hi_index = state >> 31;
	uint32_t hi = ufbxos_atomic_u32_load_relaxed(&a->hi[hi_index]);

	if ((state & 0x7fffffff) == 0x40000000) {
		ufbxos_atomic_u32_store(a->hi[hi_index ^ 1], hi + 1);
	}

	return (uint64_t)state | (uint64_t)hi << 30;
}

typedef struct {
	// [0:16] cycle
	// [16:32] sema_index
	ufbxos_atomic_u32 state;

	ufbxos_atomic_u32 count;

	ufbx_os_thread_pool_task *task;

} ufbxos_queue_task;

#define ufbxos_queue_task_cycle(state) ((state) & 0xffff)
#define ufbxos_queue_task_sema_index(state) (((state) >> 16) & 0xffff)
#define ufbxos_queue_task_pack(cycle, sema_index) ((cycle) | (sema_index) << 16)

struct ufbx_os_thread_pool {

	ufbxos_atomic_u64 start_index;

	ufbxos_atomic_u32 cycle;
	ufbxos_wait_sema wait_semas[UFBXOS_WAIT_SEMA_COUNT];

	uint32_t task_queue_shift;
	uint32_t task_queue_mask;
	ufbxos_queue_task *task_queue;

};

static uint32_t ufbxos_wait_sema_alloc(ufbx_os_thread_pool *pool, uint32_t task_index)
{
	uint32_t new_state = ufbxos_wait_sema_pack(1, task_index, 0, 1);
	for (uint32_t i = 0; i < UFBXOS_WAIT_SEMA_COUNT; i++) {
		ufbxos_wait_sema *sema = &pool->wait_semas[i];
		if (ufbxos_atomic_u32_cas(&sema->state, 0, new_state)) {
			if (!sema->initialized) {
				ufbxos_os_semaphore_init(&sema->os_semaphore, 256); // TODO: Count
				sema->initialized = true;
			}
			return i;
		}
	}
	return ~0u;
}

static uint32_t ufbxos_wait_sema_cleanup(uint32_t state)
{
	uint32_t waiters = ufbxos_wait_sema_waiters(state);
	uint32_t signaled = ufbxos_wait_sema_waiters(state);
	if (waiters == 0 && signaled) return 0;
	return state;
}

static void ufbxos_wait_sema_leave(ufbx_os_thread_pool *pool, uint32_t sema_index, uint32_t task_index)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[sema_index];
	bool waited = false;
	for (;;) {
		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		uint32_t waiters = ufbxos_wait_sema_waiters(old_state);

		// The wait sema cannot be reused before all waiters are cleared.
		ufbxos_assert(waiters > 0);
		ufbxos_assert(ufbxos_wait_sema_reserved(old_state));
		ufbxos_assert(ufbxos_wait_sema_task_index(old_state) == task_index);

		// If the sema becomes signaled at some point, we must wait once at the
		// OS semaphore to keep it in sync.
		uint32_t signaled = ufbxos_wait_sema_signaled(old_state);
		if (signaled && !waited) {
			ufbxos_os_semaphore_wait(&sema->os_semaphore);
		}

		uint32_t new_state = ufbxos_wait_sema_pack(waiters - 1, task_index, signaled, 1);
		new_state = ufbxos_wait_sema_cleanup(new_state);
		if (ufbxos_atomic_u32_cas(&sema->state, old_state, new_state)) {
			break;
		}
	}
}

static bool ufbxos_wait_sema_join(ufbx_os_thread_pool *pool, uint32_t cycle, uint32_t sema_index, uint32_t task_index)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[sema_index];
	for (;;) {
		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		uint32_t waiters = ufbxos_wait_sema_waiters(old_state);

		if (!ufbxos_wait_sema_reserved(old_state)) return false;
		if (ufbxos_wait_sema_signaled(old_state)) return false;
		if (ufbxos_wait_sema_task_index(old_state) != task_index) return false;

		uint32_t new_state = ufbxos_wait_sema_pack(waiters + 1, task_index, 0, 1);
		if (ufbxos_atomic_u32_cas(&sema->state, old_state, new_state)) {

			// If succeeded, check that we are still in a relevant cycle,
			// if not, leave the sema and return failure.
			if (ufbxos_atomic_u32_load(&pool->cycle) != cycle) {
				ufbxos_wait_sema_leave(pool, sema_index, task_index);
				return false;
			}

			return true;
		}
	}
}

static void ufbxos_wait_sema_wait(ufbx_os_thread_pool *pool, uint32_t sema_index, uint32_t task_index)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[sema_index];

	ufbxos_os_semaphore_wait(&sema->os_semaphore);

	for (;;) {
		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		uint32_t waiters = ufbxos_wait_sema_waiters(old_state);

		// The wait sema cannot be reused before all waiters are cleared.
		ufbxos_assert(waiters > 0);
		ufbxos_assert(ufbxos_wait_sema_reserved(old_state));
		ufbxos_assert(ufbxos_wait_sema_signaled(old_state));
		ufbxos_assert(ufbxos_wait_sema_task_index(old_state) == task_index);

		uint32_t new_state = ufbxos_wait_sema_pack(waiters - 1, task_index, 1, 1);
		new_state = ufbxos_wait_sema_cleanup(new_state);
		if (ufbxos_atomic_u32_cas(&sema->state, old_state, new_state)) {
			break;
		}
	}
}

static void ufbxos_wait_sema_signal(ufbx_os_thread_pool *pool, uint32_t sema_index, uint32_t task_index)
{
	ufbxos_wait_sema *sema = &pool->wait_semas[sema_index];
	for (;;) {
		uint32_t old_state = ufbxos_atomic_u32_load(&sema->state);
		uint32_t waiters = ufbxos_wait_sema_waiters(old_state);

		// The wait sema cannot be reused before all waiters are cleared.
		ufbxos_assert(ufbxos_wait_sema_reserved(old_state));
		ufbxos_assert(!ufbxos_wait_sema_signaled(old_state));
		ufbxos_assert(ufbxos_wait_sema_task_index(old_state) == task_index);

		uint32_t new_state = ufbxos_wait_sema_pack(waiters, task_index, 1, 1);
		new_state = ufbxos_wait_sema_cleanup(new_state);
		if (ufbxos_atomic_u32_cas(&sema->state, old_state, new_state)) {
			if (waiters > 0) {
				ufbxos_os_semaphore_signal(&sema->os_semaphore, waiters);
			}
			break;
		}
	}
}

static uint32_t ufbxos_queue_wait(ufbx_os_thread_pool *pool, uint32_t task_index, uint64_t cycle)
{
	ufbxos_queue_task *task = &pool->task_queue[task_index];

	for (;;) {
		uint32_t cycle_hi = ufbxos_atomic_u32_load(&pool->cycle);
		uint32_t old_state = ufbxos_atomic_u32_load(&task->state);

		uint32_t ref_cycle = ufbxos_queue_task_cycle(old_state);
		uint32_t sema_index = ufbxos_queue_task_sema_index(old_state);



	}
}

ufbx_os_abi uint64_t ufbx_os_thread_pool_run(ufbx_os_thread_pool *pool, ufbx_os_thread_pool_task *task_ptr)
{
	for (;;) {
		uint64_t index = ufbxos_atomic_u64_inc(&pool->start_index);

		uint32_t task_index = index & pool->task_queue_mask;
		ufbxos_queue_task *task = &pool->task_queue[task_index];
		uint64_t cycle = index >> pool->task_queue_shift;

		uint32_t cycle_lo = cycle & 0xffff;

	}

}

#endif

#if 0

#define UFBXOS_WAIT_QUEUE_SLOTS 16
#define UFBXOS_WAIT_QUEUE_HASHES 4
#define UFBXOS_WAIT_SPIN_COUNT 64

typedef struct {
	ufbxos_os_mutex mutex;
	ufbxos_os_cond conditions[UFBXOS_WAIT_QUEUE_HASHES];
	uint32_t hashes[UFBXOS_WAIT_QUEUE_HASHES];
} ufbxos_wait_queue_entry;

typedef struct {
	ufbxos_atomic_u32 cycle;
	ufbxos_atomic_u32 count_left;
	ufbx_os_thread_pool_task *task_ptr;
} ufbxos_pool_queue_task;

struct ufbx_os_thread_pool {
	ufbxos_atomic_u32 task_queue_head;

	uint32_t task_queue_bits;
	uint32_t task_queue_mask;
	ufbxos_pool_queue_task *task_queue;

	ufbxos_wait_queue_entry wait_queue[UFBXOS_WAIT_QUEUE_SLOTS];

	uint32_t num_threads;
};

static uint32_t ufbxos_hash_addr(ufbxos_atomic_u32 *ptr)
{
	uintptr_t uptr = (uintptr_t)ptr;
	uintptr_t ptr_hash = (uintptr_t)(uptr >> 2);
	ptr_hash ^= ptr_hash >> 24;

	uint32_t hash = (uint32_t)ptr_hash;
    hash *= 0x45d9f3bu;
	hash ^= hash >> 16;
	if (hash == 0) hash = 1;
	return hash;
}

static void ufbxos_mutex_lock_spin(ufbxos_os_mutex *os_mutex)
{
	for (size_t i = 0; i < UFBXOS_WAIT_SPIN_COUNT; i++) {
		if (ufbxos_os_mutex_try_lock(os_mutex)) return;
		ufbxos_os_pause();
	}
	ufbxos_os_mutex_lock(os_mutex);
}

static void ufbxos_atomic_signal_all(ufbx_os_thread_pool *pool, ufbxos_atomic_u32 *ptr)
{
	uintptr_t addr_hash = ufbxos_hash_addr(ptr);
	ufbxos_wait_queue_entry *wait_entry = &pool->wait_queue[addr_hash % UFBXOS_WAIT_QUEUE_SLOTS];
	uint32_t hash = (uint32_t)(addr_hash / UFBXOS_WAIT_QUEUE_SLOTS);

	ufbxos_mutex_lock_spin(&wait_entry->mutex);

	uint32_t signal_mask = 0;
	signal_mask |= wait_entry->hashes[0] == hash ? 1 : 0;
	signal_mask |= wait_entry->hashes[1] == hash ? 2 : 0;
	signal_mask |= wait_entry->hashes[2] == hash ? 4 : 0;
	signal_mask |= (wait_entry->hashes[3] & (1u << (hash % 32u))) != 0 ? 8 : 0;
	for (uint32_t mask = signal_mask; mask != 0; mask &= mask - 1) {
		uint32_t index = ufbxos_ctz32(signal_mask);
		wait_entry->hashes[index] = 0;
	}

	ufbxos_os_mutex_unlock(&wait_entry->mutex);

	for (uint32_t mask = signal_mask; mask != 0; mask &= mask - 1) {
		uint32_t index = ufbxos_ctz32(signal_mask);
		ufbxos_os_cond_wake_all(&wait_entry->conditions[index]);
	}
}

static void ufbxos_atomic_wait(ufbx_os_thread_pool *pool, ufbxos_atomic_u32 *ptr, uint32_t value)
{
	if (ufbxos_atomic_u32_load_relaxed(ptr) != value) return;

	uintptr_t addr_hash = ufbxos_hash_addr(ptr);
	ufbxos_wait_queue_entry *wait_entry = &pool->wait_queue[addr_hash % UFBXOS_WAIT_QUEUE_SLOTS];
	uint32_t hash = (uint32_t)(addr_hash / UFBXOS_WAIT_QUEUE_SLOTS);

	ufbxos_mutex_lock_spin(&wait_entry->mutex);

	uint32_t old_hash = 0;
	uint32_t index = UFBXOS_WAIT_QUEUE_HASHES - 1;

	for (uint32_t i = 0; i < UFBXOS_WAIT_QUEUE_HASHES - 1; i++) {
		uint32_t ref = wait_entry->hashes[i];
		if (ref == hash) {
			index = i;
			break;
		} else if (ref == 0) {
			index = i;
		}
	}

	// Last slot hashes are shared and encoded via a bitmap
	if (index == UFBXOS_WAIT_QUEUE_HASHES - 1) {
		hash = wait_entry->hashes[index] | (1u << (hash % 32u));
	}

	if (ufbxos_atomic_u32_load(ptr) != value) {
		ufbxos_os_mutex_unlock(&wait_entry->mutex);
		return;
	}

	wait_entry->hashes[index] = hash;

	ufbxos_os_cond_wait(&wait_entry->conditions[index], &wait_entry->mutex);
	ufbxos_os_mutex_unlock(&wait_entry->mutex);
}

static bool ufbxos_pool_wait_imp(ufbx_os_thread_pool *pool, uint32_t index, uint32_t ref_cycle, bool exact, bool sleep)
{
	ufbxos_pool_queue_task *task = &pool->task_queue[index];
	for (;;) {
		uint32_t cycle = ufbxos_atomic_u32_load(&task->cycle);
		if (cycle == ref_cycle) break;
		if (!exact && (int32_t)(cycle - ref_cycle) >= 0) break;
		if (!sleep) return false;
	}
	return true;
}

static bool ufbxos_pool_bump_cycle(ufbx_os_thread_pool *pool, uint32_t index, uint32_t ref_cycle)
{
	ufbxos_pool_queue_task *task = &pool->task_queue[index];
	ufbxos_atomic_u32_store(task->cycle, ref_cycle);
	ufbxos_atomic_signal_all(pool, &task->cycle);
}

static void ufbxos_pool_worker_entry(ufbx_os_thread_pool *pool)
{
	uint32_t task_index = 0;
	for (;;) {
		ufbxos_pool_queue_task *task = &pool->task_queue[task_index & pool->task_queue_mask];
		uint32_t cycle = (task_index >> pool->task_queue_bits) * 2;

		for (;;) {
			uint32_t ref_cycle = ufbxos_atomic_u32_load(&task->cycle);
			if (ref_cycle == cycle + 1) break;
			ufbxos_atomic_wait(pool, &task->cycle, ref_cycle);
		}

		ufbx_os_thread_pool_task *task_ptr = task->task_ptr;
		ufbx_os_thread_pool_task task_data = *task_ptr;

		int32_t count;
		for (;;) {
			count = (int32_t)ufbxos_atomic_u32_dec(&task->count_left) - 1;
			if (count < 0) break;

			uint32_t index = task_data.count - 1 - count;
			task_data.fn(task, index);
		}

		if (count == pool->num_threads - 1) {
			ufbxos_pool_bump_cycle(pool, task_index, cycle + 2);
		}
	}
}

static void ufbxos_pool_enqueue(ufbx_os_thread_pool *pool, ufbx_os_thread_pool_task *task_ptr)
{
	uint32_t index = ufbxos_atomic_u32_inc(pool->task_queue_head);
	ufbxos_pool_queue_task *task = &pool->task_queue[index & pool->task_queue_mask];
	uint32_t cycle = (index >> pool->task_queue_bits) * 2;

	// Wait for exact cycle
	for (;;) {
		uint32_t ref_cycle = ufbxos_atomic_u32_load(&task->cycle);
		if (ref_cycle == cycle) break;
		ufbxos_atomic_wait(pool, &task->cycle, ref_cycle);
	}

	task->task_ptr = task->task_ptr;
	ufbxos_atomic_u32_store(&task->count_left, task_ptr->count);
	ufbxos_pool_bump_cycle(pool, index, cycle + 1);
}

ufbx_os_abi bool ufbx_os_thread_pool_try_wait(ufbx_os_thread_pool *pool, uint32_t index)
{
	ufbxos_pool_queue_task *task = &pool->task_queue[index & pool->task_queue_mask];
	uint32_t cycle = (index >> pool->task_queue_bits) * 2;
	return (int32_t)(ufbxos_atomic_u32_load(task->cycle) - cycle) >= 2;
}

ufbx_os_abi uint32_t ufbx_os_thread_pool_run(ufbx_os_thread_pool *pool, ufbx_os_thread_pool_task *task_ptr)
{
	ufbxos_pool_enqueue(pool, task_ptr);
}

ufbx_os_abi void ufbx_os_thread_pool_wait(ufbx_os_thread_pool *pool, uint32_t index)
{
	ufbxos_pool_queue_task *task = &pool->task_queue[index & pool->task_queue_mask];
	uint32_t cycle = (index >> pool->task_queue_bits) * 2;
	for (;;) {
		uint32_t ref_cycle = ufbxos_atomic_u32_load(task->cycle);
		if ((int32_t)(ref_cycle - cycle) >= 2) return;
		ufbxos_atomic_wait(pool, &task->cycle, ref_cycle);
	}
}

// -- ufbx tasks

typedef struct {
	ufbx_os_thread_pool_task task;
	ufbx_thread_pool_context ctx;
	uint32_t start_index;
} ufbxos_thread_group_task;

typedef struct {
	ufbxos_thread_group_task group_task[UFBX_THREAD_GROUP_COUNT];
	uint32_t group_index[UFBX_THREAD_GROUP_COUNT];
} ufbxos_thread_pool_ctx;

static void ufbxos_ufbx_task(ufbx_os_thread_pool_task *task, uint32_t index)
{
	ufbxos_thread_group_task *t = (ufbxos_thread_group_task*)task;
	ufbx_thread_pool_run_task(t->ctx, t->start_index + index);
}

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
	ufbxos_thread_group_task *task = &pool_ctx->group_task[group];

	task->ctx = ctx;
	task->start_index = start_index;
	task->task.count = count;
	task->task.fn = &ufbxos_ufbx_task;
	pool_ctx->group_index[group] = ufbx_os_thread_pool_run(pool, task);

	return true;
}

static bool ufbxos_ufbx_thread_pool_wait(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t max_index, bool speculative)
{
	ufbx_os_thread_pool *pool = (ufbx_os_thread_pool*)user;
	ufbxos_thread_pool_ctx *pool_ctx = ufbx_thread_pool_get_user_ptr(ctx);
	ufbx_os_thread_pool_wait(pool, pool_ctx->group_index[group]);
	return true;
}

static void ufbxos_ufbx_thread_pool_free(void *user, ufbx_thread_pool_context ctx)
{
	ufbxos_thread_pool_ctx *pool_ctx = ufbx_thread_pool_get_user_ptr(ctx);
	free(pool_ctx);
}

#endif

#if 0

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

#endif

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

