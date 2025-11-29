#ifndef SGL_OSAL_H_
#define SGL_OSAL_H_

#include <pthread.h>

#define NULL_THREAD     (sgl_osal_thread_t)(0)
#define EXIT_ROUTINE    return NULL;

typedef pthread_t       sgl_osal_thread_t;
typedef void*           sgl_osal_thread_return_t;
typedef void*           sgl_osal_thread_arg_t;
typedef sgl_osal_thread_return_t(*sgl_osal_thread_entry_t)(sgl_osal_thread_arg_t arg);
typedef pthread_mutex_t sgl_osal_mutex_t;
typedef pthread_cond_t  sgl_osal_cond_t;

/* Thread */
static inline sgl_osal_thread_t sgl_thread_create(sgl_osal_thread_entry_t start_routine, sgl_osal_thread_arg_t arg)
{
    pthread_t thread = NULL_THREAD;
    pthread_create(&thread, NULL, start_routine, arg);
    return thread;
}

static inline void sgl_osal_thread_join(sgl_osal_thread_t thread)
{
    pthread_join(thread, NULL);
}

static inline void sgl_osal_thread_exit(sgl_osal_thread_return_t retval)
{
    pthread_exit(retval);
}

/* Mutex */
static inline void sgl_osal_mutex_init(sgl_osal_mutex_t *mutex)
{
    pthread_mutex_init(mutex, NULL);
}

static inline void sgl_osal_mutex_lock(sgl_osal_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
}

static inline void sgl_osal_mutex_unlock(sgl_osal_mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}

static inline void sgl_osal_mutex_destroy(sgl_osal_mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

/* Conditional Variable */
static inline void sgl_osal_cond_init(sgl_osal_cond_t *cond)
{
    pthread_cond_init(cond, NULL);
}

static inline void sgl_osal_cond_wait(sgl_osal_cond_t *cond, sgl_osal_mutex_t *mutex)
{
    pthread_cond_wait(cond, mutex);
}

static inline void sgl_osal_cond_signal(sgl_osal_cond_t *cond)
{
    pthread_cond_signal(cond);
}

static inline void sgl_osal_cond_broadcast(sgl_osal_cond_t *cond)
{
    pthread_cond_broadcast(cond);
}

static inline void sgl_osal_cond_destroy(sgl_osal_cond_t *cond)
{
    pthread_cond_destroy(cond);
}

#endif  /* !SGL_OSAL_H_ */
