// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef MAC_BASE_H
#define MAC_BASE_H

////////////////////////////////
//~ rjf: Includes

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <copyfile.h>
// rad: base_core.h defines `internal` as `static`, but <mach/mach.h> has struct
// members named `internal`; undefine it around the include, then restore it.
#undef internal
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <dispatch/dispatch.h>
#define internal static

extern char **environ;

typedef struct tm tm;
typedef struct timespec timespec;

////////////////////////////////
//~ rad: Semaphores (macOS has no unnamed sem_init and no sem_timedwait)

typedef struct MAC_Semaphore MAC_Semaphore;
struct MAC_Semaphore
{
  dispatch_semaphore_t dispatch; // unnamed semaphores
  sem_t *posix;                  // named semaphores
};

////////////////////////////////
//~ rjf: macOS Call Interruption Retry Helper

#define MAC_RETRY_ON_EINTR(expr)             \
(__extension__({                           \
__typeof__(expr) __ret;                    \
do {                                       \
__ret = (expr);                          \
} while ((__ret == -1) && errno == EINTR); \
__ret;                                     \
}))


////////////////////////////////
//~ rjf: File Iterator

typedef struct MAC_FileIter MAC_FileIter;
struct MAC_FileIter
{
  DIR *dir;
  struct dirent *dp;
  String8 path;
};
StaticAssert(sizeof(Member(FileIter, memory)) >= sizeof(MAC_FileIter), mac_file_iter_size_check);

////////////////////////////////
//~ rjf: Safe Call Handler Chain

typedef struct MAC_SafeCallChain MAC_SafeCallChain;
struct MAC_SafeCallChain
{
  MAC_SafeCallChain *next;
  ThreadEntryPointFunctionType *fail_handler;
  void *ptr;
};

////////////////////////////////
//~ rjf: Custom Barrier (macOS lacks pthread_barrier_t)

typedef struct MAC_Barrier MAC_Barrier;
struct MAC_Barrier
{
  pthread_mutex_t mutex;
  pthread_cond_t  cond;
  U32             count;
  U32             waiting;
  U32             generation;
};

////////////////////////////////
//~ rjf: Entities

typedef enum MAC_EntityKind
{
  MAC_EntityKind_Thread,
  MAC_EntityKind_Mutex,
  MAC_EntityKind_RWMutex,
  MAC_EntityKind_ConditionVariable,
  MAC_EntityKind_Barrier,
}
MAC_EntityKind;

typedef struct MAC_Entity MAC_Entity;
struct MAC_Entity
{
  MAC_Entity *next;
  MAC_EntityKind kind;
  union
  {
    struct
    {
      pthread_t handle;
      ThreadEntryPointFunctionType *func;
      void *ptr;
    } thread;
    pthread_mutex_t mutex_handle;
    pthread_rwlock_t rwmutex_handle;
    struct
    {
      pthread_cond_t cond_handle;
      pthread_mutex_t rwlock_mutex_handle;
    } cv;
    MAC_Barrier barrier;
  };
};

////////////////////////////////
//~ On Demand Memory

#define MAC_MEMORY_FAULT_WORKER_LIMIT 32

typedef struct
{
  void *address;
  U32 result;
} MAC_MemoryFaultRequest;

typedef struct
{
  MemoryReadFaultFunction *fault;
  void *user_data;
  Thread workers[MAC_MEMORY_FAULT_WORKER_LIMIT];
  U32 worker_count;
  U32 stops_sent;
  U32 workers_joined;
  int requests[2];
  U32 active;
  pid_t owner_pid;
} MAC_DemandMemory;

////////////////////////////////
//~ rjf: State

typedef struct MAC_State MAC_State;
struct MAC_State
{
  Arena *arena;
  SystemInfo system_info;
  ProcessInfo process_info;
  pthread_mutex_t entity_mutex;
  Arena *entity_arena;
  MAC_Entity *entity_free;
  U64 default_env_count;
  char **default_env;
  MAC_DemandMemory demand_memory;
};

////////////////////////////////
//~ rjf: Globals

global MAC_State mac_state = {0};
thread_static MAC_SafeCallChain *mac_safe_call_chain = 0;
thread_static B32 mac_in_memory_fault_callback;

////////////////////////////////
//~ rjf: Helpers

internal DateTime mac_date_time_from_tm(tm in, U32 msec);
internal tm mac_tm_from_date_time(DateTime dt);
internal timespec mac_timespec_from_date_time(DateTime dt);
internal DenseTime mac_dense_time_from_timespec(timespec in);
internal FileProperties mac_file_properties_from_stat(struct stat *s);
internal timespec mac_rel_timespec_from_endt_us(U64 endt_us);
internal B32 mac_dispatch_memory_read_fault(int sig, siginfo_t *info, void *context);
internal void mac_safe_call_sig_handler(int sig, siginfo_t *info, void *context);

////////////////////////////////
//~ rjf: Entities

internal MAC_Entity *mac_entity_alloc(MAC_EntityKind kind);
internal void mac_entity_release(MAC_Entity *entity);

////////////////////////////////
//~ rjf: Thread Entry Point

internal void *mac_thread_entry_point(void *ptr);

#endif // MAC_BASE_H
