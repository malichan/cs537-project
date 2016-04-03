#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

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

char *str_duplicate(char *str) {
    char *str_new = strdup(str);
    assert(str_new != NULL);
    return str_new;
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

void cond_broadcast(cond_t *c) {
    int rc = pthread_cond_broadcast(c);
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
    int done;
    size_t workers;
    mutex_t worker_mutex;
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
    b->done = 0;
    b->workers = 0;
    mutex_init(&b->worker_mutex);
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
    while (b->count == 0) {
        if (b->done != 0) {
            mutex_unlock(&b->mutex);
            return NULL;
        }
        cond_wait(&b->fill, &b->mutex);
    }
    mutex_lock(&b->worker_mutex);
    b->workers++;
    mutex_unlock(&b->worker_mutex);
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

void bounded_buffer_done(bounded_buffer_t *b) {
    mutex_lock(&b->worker_mutex);
    b->workers--;
    mutex_unlock(&b->worker_mutex);
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
    mutex_t mutex;
    mutex_t head_mutex;
    mutex_t tail_mutex;
    cond_t fill;
    int done;
    size_t workers;
    mutex_t worker_mutex;
} unbounded_buffer_t;

void unbounded_buffer_init(unbounded_buffer_t *b) {
    node_t *node = (node_t *)mem_malloc(sizeof(node_t));
    node->ptr = NULL;
    node->next = NULL;
    b->head = node;
    b->tail = node;
    b->count = 0;
    mutex_init(&b->mutex);
    mutex_init(&b->head_mutex);
    mutex_init(&b->tail_mutex);
    cond_init(&b->fill);
    b->done = 0;
    b->workers = 0;
    mutex_init(&b->worker_mutex);
}

void unbounded_buffer_put(unbounded_buffer_t *b, void *ptr) {
    node_t *node = (node_t *)mem_malloc(sizeof(node_t));
    node->ptr = ptr;
    node->next = NULL;
    mutex_lock(&b->tail_mutex);
    b->tail->next = node;
    b->tail = node;
    mutex_lock(&b->mutex);
    mutex_unlock(&b->tail_mutex);
    b->count++;
    cond_signal(&b->fill);
    mutex_unlock(&b->mutex);
}

void *unbounded_buffer_get(unbounded_buffer_t *b) {
    mutex_lock(&b->mutex);
    while (b->count == 0) {
        if (b->done != 0) {
            mutex_unlock(&b->mutex);
            return 0;
        }
        cond_wait(&b->fill, &b->mutex);
    }
    b->count--;
    mutex_lock(&b->worker_mutex);
    mutex_unlock(&b->mutex);
    b->workers++;
    mutex_unlock(&b->worker_mutex);
    mutex_lock(&b->head_mutex);
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
    mutex_destroy(&b->mutex);
    mutex_destroy(&b->head_mutex);
    mutex_destroy(&b->tail_mutex);
    cond_destroy(&b->fill);
}

void unbounded_buffer_done(unbounded_buffer_t *b) {
    mutex_lock(&b->worker_mutex);
    b->workers--;
    mutex_unlock(&b->worker_mutex);
}

// *************************
//      string hash set
// *************************

typedef struct __hashset_t {
    node_t **heads;
    mutex_t *mutexes;
    size_t buckets;
} hashset_t;

size_t hashset_hash(char *str) {
    size_t hash = 0;
    char *c = str;
    while (*c != '\0') {
        hash = (size_t)*c + 31 * hash;
        c++;
    }
    return hash;
}

void hashset_init(hashset_t *h, size_t buckets) {
    h->heads = mem_calloc(buckets, sizeof(node_t *));
    h->mutexes = mem_calloc(buckets, sizeof(mutex_t));
    h->buckets = buckets;
    int i = 0;
    for (; i < buckets; i++)
        mutex_init(&h->mutexes[i]);
}

void hashset_insert(hashset_t *h, char *str) {
    size_t bucket = hashset_hash(str) % h->buckets;
    mutex_lock(&h->mutexes[bucket]);
    node_t *node = h->heads[bucket];
    while (node != NULL) {
        if (strcmp((char *)node->ptr, str) == 0)
            break;
        node = node->next;
    }
    if (node == NULL) {
        node = (node_t *)mem_malloc(sizeof(node_t));
        node->ptr = (void *)str_duplicate(str);
        node->next = h->heads[bucket];
        h->heads[bucket] = node;
    }
    mutex_unlock(&h->mutexes[bucket]);
}

int hashset_contains(hashset_t *h, char *str) {
    size_t bucket = hashset_hash(str) % h->buckets;
    mutex_lock(&h->mutexes[bucket]);
    node_t *node = h->heads[bucket];
    while (node != NULL) {
        if (strcmp((char *)node->ptr, str) == 0)
            break;
        node = node->next;
    }
    mutex_unlock(&h->mutexes[bucket]);
    return node != NULL;
}

void hashset_destroy(hashset_t *h) {
    int i = 0;
    for (; i < h->buckets; i++) {
        node_t *node = h->heads[i];
        while (node != NULL) {
            node_t *old_node = node;
            node = old_node->next;
            mem_free(old_node->ptr);
            mem_free(old_node);
        }
        mutex_destroy(&h->mutexes[i]);
    }
    mem_free(h->heads);
    mem_free(h->mutexes);
}

// *************************
//      main functions
// *************************

const size_t HASHSET_BUCKETS = 97;

struct input_args {
    bounded_buffer_t *url_queue;
    unbounded_buffer_t *page_queue;
    hashset_t *url_set;
    char *(*fetch)(char *url);
    void (*edge)(char *from, char *to);
    mutex_t *done_mutex;
    cond_t *done_cond;
};

struct page {
    char *url;
    char *content;
};

void *downloader(void *arg) {
    struct input_args *in_args = (struct input_args *)arg;
    while (1) {
        char *url = (char *)bounded_buffer_get(in_args->url_queue);
        if (url == NULL)
            break;
        if (hashset_contains(in_args->url_set, url) == 0) {
            hashset_insert(in_args->url_set, url);
            char *content = in_args->fetch(url);
            assert(content != NULL);
            struct page *page = (struct page *)mem_malloc(sizeof(struct page));
            page->url = url;
            page->content = content;
            unbounded_buffer_put(in_args->page_queue, (void *)page);
        } else {
            mem_free(url);
        }
        bounded_buffer_done(in_args->url_queue);
        mutex_lock(in_args->done_mutex);
        cond_signal(in_args->done_cond);
        mutex_unlock(in_args->done_mutex);
    }
    return NULL;
}

void *parser(void *arg) {
    struct input_args *in_args = (struct input_args *)arg;
    while (1) {
        struct page *page = (struct page *)unbounded_buffer_get(in_args->page_queue);
        if (page == NULL)
            break;
        char *start = page->content;
        while ((start = strstr(start, "link:")) != NULL) {
            char *end = start + 5;
            while (*end != ' ' && *end != '\n' && *end != '\0')
                end++;
            if (*end == '\0') {
                char *url = str_duplicate(start + 5);
                in_args->edge(page->url, url);
                bounded_buffer_put(in_args->url_queue, (void *)url);
                break;
            } else {
                char tmp = *end;
                *end = '\0';
                char *url = str_duplicate(start + 5);
                in_args->edge(page->url, url);
                bounded_buffer_put(in_args->url_queue, (void *)url);
                *end = tmp;
                start = end + 1;
            }
        }
        mem_free(page->url);
        mem_free(page->content);
        mem_free(page);
        unbounded_buffer_done(in_args->page_queue);
        mutex_lock(in_args->done_mutex);
        cond_signal(in_args->done_cond);
        mutex_unlock(in_args->done_mutex);
    }
    return NULL;
}

int crawl(char *start_url, int download_workers, int parse_workers, int queue_size,
    char *(*_fetch_fn)(char *url), void (*_edge_fn)(char *from, char *to)) {
    int i;

    bounded_buffer_t url_queue;
    unbounded_buffer_t page_queue;
    hashset_t url_set;
    bounded_buffer_init(&url_queue, queue_size);
    unbounded_buffer_init(&page_queue);
    hashset_init(&url_set, HASHSET_BUCKETS);

    bounded_buffer_put(&url_queue, (void *)str_duplicate(start_url));

    mutex_t done_mutex;
    cond_t done_cond;

    mutex_init(&done_mutex);
    cond_init(&done_cond);

    struct input_args in_args;
    in_args.url_queue = &url_queue;
    in_args.page_queue = &page_queue;
    in_args.url_set = &url_set;
    in_args.fetch = _fetch_fn;
    in_args.edge = _edge_fn;
    in_args.done_mutex = &done_mutex;
    in_args.done_cond = &done_cond;

    thread_t downloaders[download_workers];
    thread_t parsers[parse_workers];
    for (i = 0; i < download_workers; i++)
        thread_create(&downloaders[i], downloader, (void *)&in_args);
    for (i = 0; i < parse_workers; i++)
        thread_create(&parsers[i], parser, (void *)&in_args);

    while (1) {
        mutex_lock(&done_mutex);
        mutex_lock(&url_queue.mutex);
        mutex_lock(&url_queue.worker_mutex);
        mutex_lock(&page_queue.mutex);
        mutex_lock(&page_queue.worker_mutex);
        if (url_queue.count == 0 && url_queue.workers == 0 &&
            page_queue.count == 0 && page_queue.workers == 0) {
            url_queue.done = 1;
            page_queue.done = 1;
            cond_broadcast(&url_queue.empty);
            cond_broadcast(&url_queue.fill);
            cond_broadcast(&page_queue.fill);
            mutex_unlock(&url_queue.mutex);
            mutex_unlock(&url_queue.worker_mutex);
            mutex_unlock(&page_queue.mutex);
            mutex_unlock(&page_queue.worker_mutex);
            mutex_unlock(&done_mutex);
            break;
        } else {
            mutex_unlock(&url_queue.mutex);
            mutex_unlock(&url_queue.worker_mutex);
            mutex_unlock(&page_queue.mutex);
            mutex_unlock(&page_queue.worker_mutex);
            cond_wait(&done_cond, &done_mutex);
            mutex_unlock(&done_mutex);
        }
    }

    for (i = 0; i < download_workers; i++)
        thread_join(downloaders[i], NULL);
    for (i = 0; i < parse_workers; i++)
        thread_join(parsers[i], NULL);

    bounded_buffer_destroy(&url_queue);
    unbounded_buffer_destroy(&page_queue);
    hashset_destroy(&url_set);

    return 0;
}
