#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
// #include <stdio.h>
// #include <string.h>
// #include <stdint.h>
// #include <unistd.h>

// *************************
//      memory routines
// *************************

void *mem_malloc(size_t size) {
    void *ptr = malloc(size);
    assert(ptr != NULL);
    return ptr;
}

void *mem_calloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    assert(ptr != NULL);
    return ptr;
}

void mem_free(void *ptr) {
    free(ptr);
}

// *************************
//      pthread routines
// *************************

typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;

void thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg) {
    int rc = pthread_create(thread, NULL, start_routine, arg);
    assert(rc == 0);
}

void thread_join(thread_t thread, void **value_ptr) {
    int rc = pthread_join(thread, value_ptr);
    assert(rc == 0);
}

void mutex_init(mutex_t *m) {
    int rc = pthread_mutex_init(m, NULL);
    assert(rc == 0);
}

void mutex_lock(mutex_t *m) {
    int rc = pthread_mutex_lock(m);
    assert(rc == 0);
}
                                                                                
void mutex_unlock(mutex_t *m) {
    int rc = pthread_mutex_unlock(m);
    assert(rc == 0);
}

void mutex_destroy(mutex_t *m) {
    int rc = pthread_mutex_destroy(m);
    assert(rc == 0);
}

void cond_init(cond_t *c) {
    int rc = pthread_cond_init(c, NULL);
    assert(rc == 0);
}

void cond_wait(cond_t *c, mutex_t *m) {
    int rc = pthread_cond_wait(c, m);
    assert(rc == 0);
}
                                                                                
void cond_signal(cond_t *c) {
    int rc = pthread_cond_signal(c);
    assert(rc == 0);
}

void cond_destroy(cond_t *c) {
    int rc = pthread_cond_destroy(c);
    assert(rc == 0);
}

// *************************
//      bounded buffer
// *************************

typedef struct __bounded_buffer_t {
    void **buffer;
    size_t size;
    size_t count;
    size_t head;
    size_t tail;
    mutex_t mutex;
    cond_t empty;
    cond_t fill;
} bounded_buffer_t;

void bounded_buffer_init(bounded_buffer_t *b, size_t size) {
    b->buffer = (void **)mem_calloc(size, sizeof(void *));
    b->size = size;
    b->count = 0;
    b->head = 0;
    b->tail = 0;
    mutex_init(&b->mutex);
    cond_init(&b->empty);
    cond_init(&b->fill);
}

void bounded_buffer_put(bounded_buffer_t *b, void* ptr) {
    mutex_lock(&b->mutex);
    while (b->count == b->size)
        cond_wait(&b->empty, &b->mutex);
    b->buffer[b->tail] = ptr;
    b->tail = (b->tail + 1) % b->size;
    b->count++;
    cond_signal(&b->fill);
    mutex_unlock(&b->mutex);
}

void *bounded_buffer_get(bounded_buffer_t *b) {
    mutex_lock(&b->mutex);
    while (b->count == 0)
        cond_wait(&b->fill, &b->mutex);
    void *ptr = b->buffer[b->head];
    b->buffer[b->head] = NULL;
    b->head = (b->head + 1) % b->size;
    b->count--;
    cond_signal(&b->empty);
    mutex_unlock(&b->mutex);
    return ptr;
}

void bounded_buffer_destroy(bounded_buffer_t *b) {
    mem_free(b->buffer);
    mutex_destroy(&b->mutex);
    cond_destroy(&b->empty);
    cond_destroy(&b->fill);
}

// *************************
//      unbounded buffer
// *************************

typedef struct __node_t {
    void *ptr;
    struct __node_t *next;
} node_t;

typedef struct __unbounded_buffer_t {
    node_t *head;
    node_t *tail;
    size_t count;
    mutex_t head_mutex;
    mutex_t tail_mutex;
    mutex_t count_mutex;
    cond_t fill;
} unbounded_buffer_t;

void unbounded_buffer_init(unbounded_buffer_t *b) {
    node_t *node = (node_t *)mem_malloc(sizeof(node_t));
    node->ptr = NULL;
    node->next = NULL;
    b->head = node;
    b->tail = node;
    b->count = 0;
    mutex_init(&b->head_mutex);
    mutex_init(&b->tail_mutex);
    mutex_init(&b->count_mutex);
    cond_init(&b->fill);
}

void unbounded_buffer_put(unbounded_buffer_t *b, void *ptr) {
    node_t *node = (node_t *)mem_malloc(sizeof(node_t));
    node->ptr = ptr;
    node->next = NULL;
    mutex_lock(&b->tail_mutex);
    b->tail->next = node;
    b->tail = node;
    mutex_lock(&b->count_mutex);
    mutex_unlock(&b->tail_mutex);
    b->count++;
    cond_signal(&b->fill);
    mutex_unlock(&b->count_mutex);
}

void *unbounded_buffer_get(unbounded_buffer_t *b) {
    mutex_lock(&b->count_mutex);
    while (b->count == 0)
        cond_wait(&b->fill, &b->count_mutex);
    b->count--;
    mutex_lock(&b->head_mutex);
    mutex_unlock(&b->count_mutex);
    node_t *old_node = b->head;
    node_t *node = old_node->next;
    void *ptr = node->ptr;
    node->ptr = NULL;
    b->head = node;
    mutex_unlock(&b->head_mutex);
    mem_free(old_node);
    return ptr;
}

void unbounded_buffer_destroy(unbounded_buffer_t *b) {
    node_t *node = b->head;
    while (node != NULL) {
        node_t *old_node = node;
        node = old_node->next;
        mem_free(old_node);
    }
    mutex_destroy(&b->head_mutex);
    mutex_destroy(&b->tail_mutex);
    mutex_destroy(&b->count_mutex);
    cond_destroy(&b->fill);
}

// *************************
//      concurrent hash set
// *************************

typedef struct __hashset_t {
    node_t **heads;
    mutex_t *mutexes;
    size_t buckets;
    size_t (*hash)(void *ptr);
} hashset_t;

void hashset_init(hashset_t *h, size_t buckets, size_t (*hash)(void *ptr)) {
    h->heads = mem_calloc(buckets, sizeof(node_t *));
    h->mutexes = mem_calloc(buckets, sizeof(mutex_t));
    h->buckets = buckets;
    h->hash = hash;
    int i = 0;
    for (; i < buckets; ++i)
        mutex_init(&h->mutexes[i]);
}

void hashset_insert(hashset_t *h, void *ptr) {
    size_t bucket = h->hash(ptr) % h->buckets;
    mutex_lock(&h->mutexes[bucket]);
    node_t *node = h->heads[bucket];
    while (node != NULL) {
        if (node->ptr == ptr)
            break;
        node = node->next;
    }
    if (node == NULL) {
        node = (node_t *)mem_malloc(sizeof(node_t));
        node->ptr = ptr;
        node->next = h->heads[bucket];
        h->heads[bucket] = node;
    }
    mutex_unlock(&h->mutexes[bucket]);
}

int hashset_contains(hashset_t *h, void *ptr) {
    size_t bucket = h->hash(ptr) % h->buckets;
    mutex_lock(&h->mutexes[bucket]);
    node_t *node = h->heads[bucket];
    while (node != NULL) {
        if (node->ptr == ptr)
            break;
        node = node->next;
    }
    mutex_unlock(&h->mutexes[bucket]);
    return node != NULL;
}

void hashset_destroy(hashset_t *h) {
    int i = 0;
    for (; i < h->buckets; ++i) {
        node_t *node = h->heads[i];
        while (node != NULL) {
            node_t *old_node = node;
            node = old_node->next;
            mem_free(old_node);
        }
        mutex_destroy(&h->mutexes[i]);
    }
    mem_free(h->heads);
    mem_free(h->mutexes);
}

// *************************
//      main function
// *************************

int crawl(char *start_url, int download_workers, int parse_workers, int queue_size,
    char *(*_fetch_fn)(char *url), void (*_edge_fn)(char *from, char *to)) {
  return -1;
}
