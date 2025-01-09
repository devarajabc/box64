#ifndef __RECORD_H_ 
#define __RECORD_H_ 

#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>       
#include <fcntl.h>   
typedef struct record_item{
    const char* op_name;
    uint64_t Memory_usage;
    uint64_t op_time;
}record_item;

typedef struct {
    // backing buffer and size
    uint8_t *buffer;
    size_t size;

    // backing buffer's memfd descriptor
    int fd;

    // read / write indices
    size_t head, tail;

    // synchronization primitives
    pthread_cond_t readable, writeable;
    pthread_mutex_t lock;
} queue_t;


/**
 * @brief Get timestamp
 * @return timestamp now
 */
uint64_t get_time()
{
    struct timespec ts;
    clock_gettime(0, &ts); //OS X does not have clock_gettime, use clock_get_time
    return (uint64_t)(ts.tv_sec * 1e6 + ts.tv_nsec / 1e3);
}

/* Convenience wrappers for erroring out */
static inline void queue_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "queue error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();
}

static inline void queue_error_errno(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "queue error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, " (errno %d)\n", errno);
    va_end(args);
    abort();
}

/** Initialize a blocking queue *q* of size *s* */
void queue_init(queue_t *q, size_t s)
{
    /* We mmap two adjacent pages (in virtual memory) that point to the same
     * physical memory. This lets us optimize memory access, so that we don't
     * need to even worry about wrapping our pointers around until we go
     * through the entire buffer.
     */

    // Check that the requested size is a multiple of a page. If it isn't, we're
    // in trouble.
    size_t real_mmap = s;
    size_t buffer_algin = ((s - 1 + getpagesize()) / getpagesize()) * getpagesize();
    s =  real_mmap > buffer_algin ? real_mmap : buffer_algin;
    if (s % getpagesize() != 0) {
        queue_error(
            "Requested size (%lu) is not a multiple of the page size (%d)", s,
            getpagesize());
    }

    // Create an anonymous file backed by memory
    //if ((q->fd = memfd_create("queue_region", 0)) == -1)
        //queue_error_errno("Could not obtain anonymous file");
    q->fd = shm_open("/shm_queue", O_CREAT | O_RDWR, 0666);
    if (q->fd == -1) {
            perror("shm_open failed");
            exit(1);
    }
    // Set buffer size
    if (ftruncate(q->fd, s) != 0)
        perror("Could not set size of shared memory");

    // Ask mmap for a good address
    q->buffer = mmap(NULL, 2 * s, PROT_READ | PROT_WRITE, MAP_SHARED, q->fd, 0);
    if (q->buffer == MAP_FAILED){
        perror("mmap failed"); 
        exit(1);
    } 
        
      // Mmap first region
    if (mmap(q->buffer, s, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
             q->fd, 0) == MAP_FAILED)
        queue_error_errno("Could not map buffer into virtual memory");

    // Mmap second region, with exact address
    if (mmap(q->buffer + s, s, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
             q->fd, 0) == MAP_FAILED)
        queue_error_errno("Could not map buffer into virtual memory");

    // Initialize synchronization primitives
    if (pthread_mutex_init(&q->lock, NULL) != 0)
        queue_error_errno("Could not initialize mutex");
    if (pthread_cond_init(&q->readable, NULL) != 0)
        queue_error_errno("Could not initialize condition variable");
    if (pthread_cond_init(&q->writeable, NULL) != 0)
        queue_error_errno("Could not initialize condition variable");

    // Initialize remaining members
    q->size = s;
    q->head = q->tail = 0;
}
/** Insert into queue *q* a message of *size* bytes from *buffer*
 *
 * Blocks until sufficient space is available in the queue.
 */
void queue_put(queue_t *q, uint8_t *buffer, size_t size)
{
    pthread_mutex_lock(&q->lock);

    // Wait for space to become available
    while ((q->size - (q->tail - q->head)) < (size + sizeof(size_t)))
        pthread_cond_wait(&q->writeable, &q->lock);

    // Write message
    memcpy(&q->buffer[q->tail], buffer, sizeof(size_t));

    // Increment write index
    q->tail += size;

    pthread_cond_signal(&q->readable);
    pthread_mutex_unlock(&q->lock);
}
# endif
