#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))


/* Reference Counter Structure */
struct ref {
    void (*free)(const struct ref *);
    int count;
};

static inline void ref_inc (const struct ref *ref) {
    ((struct ref *)ref)->count++;
}
static inline void ref_dec (const struct ref *ref) {
    if (--((struct ref *)ref)->count == 0)
        ref->free(ref);
}
static inline void ref_inc_ts (const struct ref *ref) {
    __sync_add_and_fetch((int *)&ref->count, 1);
}
static inline void ref_dec_ts (const struct ref *ref) {
    if (__sync_sub_and_fetch((int *)&ref->count, 1) == 0)
        ref->free(ref);
}

/* Example List {{{ */
struct node {
    struct ref refcount; /* extra 16 bytes */

    char *word;
    struct node *next;
};

static void node_free (const struct ref *ref) {
    struct node *node = container_of(ref, struct node, refcount);
    struct node *child = node->next;
    free(node);
    if (child) ref_dec_ts(&child->refcount);
}

struct node *node_create (char *w) {
    struct node *node = malloc(sizeof(*node));
    node->word = w;
    node->next = NULL;
    node->refcount = (struct ref){node_free, 1};
    return node;
}

void node_push (struct node **nodes, char *w) {
    struct node *node = node_create(w);
    node->next = *nodes;
    *nodes = node;
}

struct node *node_pop (struct node **nodes) {
    struct node *node = *nodes;
    *nodes = (*nodes)->next;
    if (*nodes) ref_inc_ts(&(*nodes)->refcount);
    return node;
}

void node_print (struct node *node) {
    for (; node; node = node->next)
        printf("%s\n", node->word);
}

int main (void) {
    struct node *nodes = NULL;
    char *w = "HIIII";

    node_push(&nodes, w);
    node_push(&nodes, w);
    node_pop(&nodes);
    printf("ref_count: %d\n", nodes->refcount.count);
    if (nodes != NULL) {
        node_print(nodes);
        struct node *old = node_pop(&nodes);
        node_push(&nodes, "foobar");
        node_print(nodes);
        printf("ref_count: %d\n", nodes->refcount.count);
        ref_dec_ts(&old->refcount);
        ref_dec(&nodes->refcount);
    }
    printf("ref_count: %d\n", nodes->refcount.count);
    return 0;
}
/* }}} */
