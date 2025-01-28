#ifndef __RING_H_ 
#define __RING_H_ 

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <semaphore.h>
typedef struct record_item{
    const char op_name[32];
    uint64_t Memory_usage;
    uint64_t op_time;
}record_item;

typedef struct {
    uint32_t size, gap;
} ringbuf_element_t;

typedef struct {
    size_t size, mask, rsvd /* reserved */, gapd;
    memory_order acquire, release;
    atomic_size_t head, tail;
    uint8_t buf[] __attribute__((aligned(sizeof(ringbuf_element_t))));
} ringbuf_t;

#define ARRAY_LENGTH 9000

typedef struct _ringbuf_shm_t ringbuf_shm_t;

struct _ringbuf_shm_t {
    char *name;
    int fd;
    ringbuf_t *ringbuf;
};

typedef struct {
    record_item * shared_array;
    _Atomic int index;
    pthread_mutex_t lock;
    bool init_le_ma;
    const char name[32];
    int fd;
}s_array_t;

ringbuf_shm_t ringbuf_shm;
s_array_t s_array;

int ringbuf_shm_init(ringbuf_shm_t *ringbuf_shm,
                            const char *name,
                            size_t minimum,
                            bool release_and_acquire);

void ringbuf_shm_deinit(ringbuf_shm_t *ringbuf_shm);

inline void *ringbuf_write_request_max(ringbuf_t *ringbuf,
                                              size_t minimum,
                                              size_t *maximum);


inline void ringbuf_write_advance(ringbuf_t *ringbuf, size_t written);
inline const void *ringbuf_read_request(ringbuf_t *ringbuf, size_t *toread);
inline void ringbuf_read_advance(ringbuf_t *ringbuf);
void Saving(ringbuf_t *ringbuf, char *name, uint64_t Size, s_array_t *s_array);
void Reading(ringbuf_t *ringbuf, s_array_t *s_array);
# endif
