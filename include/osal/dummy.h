#ifndef SGL_OSAL_DUMMY_H_
#define SGL_OSAL_DUMMY_H_

#define NULL_THREAD         (sgl_osal_thread_t)(0)
#define EXIT_ROUTINE        return NULL;

typedef uintptr_t           sgl_osal_thread_t;
typedef void*               sgl_osal_thread_return_t;
typedef void*               sgl_osal_thread_arg_t;
typedef sgl_osal_thread_return_t(*sgl_osal_thread_entry_t)(sgl_osal_thread_arg_t arg);
typedef uintptr_t           sgl_osal_spinlock_t;
typedef uintptr_t           sgl_osal_mutex_t;
typedef uintptr_t           sgl_osal_cond_t;

/* Thread */
static SGL_ALWAYS_INLINE sgl_osal_thread_t sgl_thread_create(sgl_osal_thread_entry_t start_routine, sgl_osal_thread_arg_t arg)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_thread_join(sgl_osal_thread_t thread)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_thread_exit(sgl_osal_thread_return_t retval)
{
    /* NOP */
}

/* Spinlock */
static SGL_ALWAYS_INLINE void sgl_osal_spinlock_init(sgl_osal_spinlock_t *spinlock)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_spinlock_lock(sgl_osal_spinlock_t *spinlock)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_spinlock_unlock(sgl_osal_spinlock_t *spinlock)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_spinlock_destroy(sgl_osal_spinlock_t *spinlock)
{
    /* NOP */
}

/* Mutex */
static SGL_ALWAYS_INLINE void sgl_osal_mutex_init(sgl_osal_mutex_t *mutex)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_mutex_lock(sgl_osal_mutex_t *mutex)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_mutex_unlock(sgl_osal_mutex_t *mutex)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_mutex_destroy(sgl_osal_mutex_t *mutex)
{
    /* NOP */
}

/* Conditional Variable */
static SGL_ALWAYS_INLINE void sgl_osal_cond_init(sgl_osal_cond_t *cond)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_cond_wait(sgl_osal_cond_t *cond, sgl_osal_mutex_t *mutex)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_cond_signal(sgl_osal_cond_t *cond)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_cond_broadcast(sgl_osal_cond_t *cond)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_cond_destroy(sgl_osal_cond_t *cond)
{
    /* NOP */
}

static SGL_ALWAYS_INLINE void sgl_osal_yield_thread(void)
{
    /* NOP */
}

#endif  /* !SGL_OSAL_DUMMY_H_ */
