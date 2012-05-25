#define MEMORY_PREFIX react
#include "memory.h"
#undef MEMORY_PREFIX
#include "map.h"
#include "hash.h"
#include "reactive.h"
#include "lio/lio.h"
#include "lio/protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RENGINE_ENTRY_INITSIZE 16

#ifdef __cplusplus
extern "C" {
#endif

DEF_ARRAY_OP(react_watcher_t);
DEF_ARRAY_OP(reaction_entry_t);

#define mark_free(e) (e)->h.flag = 1
#define is_marked(e) (e)->h.flag
struct LogEntry {
    struct LogHead {
        struct LogEntry *next;
        uint64_t time;
        uint32_t size:30;
        uint32_t flag:2;
    } h;
    struct Message data;
};

static struct LogEntry *LogEntry_new(uint32_t logsize, uint64_t time)
{
    uint32_t size = 1U << SizeToKlass(sizeof(struct LogHead) + logsize);
    struct LogEntry *e = react_do_malloc(size);
    e->h.next = NULL;
    e->h.size = size;
    e->h.time = time;
    return e;
}

static void LogList_init(struct LogList *list)
{
    struct LogEntry *head = LogEntry_new(0, UINT64_MAX);
    list->head = head;
    list->tail = head;
}

static void LogList_check_timer(struct LogList *list, uint64_t current, uint64_t interval)
{
    struct LogEntry *next, *head;
    head = list->head->h.next;
    while (head) {
        next = head->h.next;
        if (current - head->h.time < interval)
            break;
        /* Logical remove */
        mark_free(head);
        head = next;
    }
}

static struct LogList *LogList_check_interval(struct LogList *list)
{
    struct LogEntry *e, *next, *prev;
    e    = list->head->h.next;
    prev = list->head;
    if (is_marked(list->tail)) {
#ifdef DEBUG
        struct LogEntry *head = e;
        while (head) {
            assert(is_marked(head));
            head = head->h.next;
        }
#endif
        list->tail = list->head;
    }
    while (e) {
        if (!is_marked(e))
            break;
        next = e->h.next;
        react_do_free(e, e->h.size);
        e = next;
    }
    prev->h.next = e;
    return list;
}

static void LogList_append(struct LogList *list, struct Log *log, uint32_t logsize, uint64_t interval)
{
    struct LogEntry *e;
    uint64_t current = TimeMilliSecond();
    LogList_check_timer(list, current, interval);
    LogList_check_interval(list);
    e = LogEntry_new(logsize, current);
    memcpy(&e->data, log, logsize);
    list->tail->h.next = e;
    list->tail = e;
}

static void LogList_dispose(struct LogList *list)
{
    struct LogEntry *e = list->head, *tmp;
    while (e) {
        tmp = e->h.next;
#if 0
        struct Log *log = (struct Log *) &e->data;
        uint16_t traceLen  = log_get_length(log, 1);
        char    *traceName = log_get_data(log) + log_get_length(log, 0);
        char buf[10] = {};
        memcpy(buf, traceName, traceLen);
#endif
        react_do_free(e, e->h.size);
        e = tmp;
    }
}

static void react_entry_append_log(react_engine_t *re, reaction_entry_t *e, struct Log *logbuf, uint32_t logsize)
{
    react_watcher_t *w, *we;
    LogList_append(&e->logs, logbuf, logsize, e->expire_time);
    FOR_EACH_ARRAY(e->watcher, w, we) {
        w->watch(w->data);
    }
}

void react_engine_append_log(react_engine_t *re, struct Log *logbuf, uint32_t logsize)
{
    uint16_t traceLen  = log_get_length(logbuf, 1);
    char    *traceName = log_get_data(logbuf) + log_get_length(logbuf, 0);

    pmap_record_t *rec;
    if ((rec = poolmap_get(re->react_entries, traceName, traceLen))) {
        struct reaction_entry *e = (struct reaction_entry *) rec->v;
        react_entry_append_log(re, e, logbuf, logsize);
    }
}

#define MAX(x, y) ({\
        typeof(x) __max1 = (x);\
        typeof(x) __max2 = (y);\
        __max1 > __max2 ? __max1: __max2; })

void react_engine_append_watcher(react_engine_t *re, char *key, uint32_t klen, react_watcher_t *watcher)
{
    pmap_record_t *rec;
    if ((rec = poolmap_get(re->react_entries, key, klen))) {
        struct reaction_entry *e = (struct reaction_entry *) rec->v;
        ARRAY_add(react_watcher_t, &e->watcher, watcher);
        e->expire_time = MAX(e->expire_time, watcher->expire_time);
    }
}

static void reaction_entry_reset(struct reaction_entry *entry)
{
    react_watcher_t *w, *we;
    FOR_EACH_ARRAY(entry->watcher, w, we) {
        w->remove(w->data);
    }
    ARRAY_dispose(react_watcher_t, &entry->watcher);
    LogList_dispose(&entry->logs);
}

void react_engine_append(react_engine_t *re, char *key, uint32_t klen, reaction_entry_t *entry)
{
    reaction_entry_t *e = cast(reaction_entry_t *, react_do_malloc(sizeof(*e)));
    memcpy(e, entry, sizeof(*e));
    poolmap_set(re->react_entries, key, klen, e);
    ARRAY_init(react_watcher_t, &e->watcher, 2);
    LogList_init(&e->logs);
}

static int entry_key_cmp(uintptr_t k0, uintptr_t k1)
{
    return k0 == k1;
}

static uintptr_t entry_keygen(char *key, uint32_t klen)
{
#if 0
    uint32_t ukey = *(uint32_t *) key;
    uint32_t mask = ~(((uint32_t)-1) << (klen * 8));
    uint32_t hash = ukey & mask;
#else
    uint32_t hash = 38873;
#endif
    return hash0(hash, key, klen);
}

static void entry_free(pmap_record_t *r)
{
    reaction_entry_t *e = cast(reaction_entry_t *, r->v);
    reaction_entry_reset(e);
    react_do_free(e, sizeof(reaction_entry_t));
}

react_engine_t *react_engine_new(unsigned int entry_size)
{
    react_engine_t *re = cast(react_engine_t *, react_do_malloc(sizeof(*re)));
    if (entry_size < RENGINE_ENTRY_INITSIZE)
        entry_size = RENGINE_ENTRY_INITSIZE;
    re->react_entries = poolmap_new(entry_size, entry_keygen, entry_key_cmp, entry_free);
    return re;
}

void react_engine_delete(react_engine_t *re)
{
    poolmap_delete(re->react_entries);
    react_do_free(re, sizeof(react_engine_t));
    CHECK_MALLOCED_SIZE(react);
}

#ifdef __cplusplus
}
#endif
