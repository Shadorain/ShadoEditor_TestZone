#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))


/* Reference Counter Structure */
struct ref {
    void (*free)(const struct ref *);
    uint8_t count;
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
    char id[64];
    float value;
    struct node *next;
    struct ref refcount; /* extra 16 bytes */
};

static void node_free (const struct ref *ref) {
    struct node *node = container_of(ref, struct node, refcount);
    struct node *child = node->next;
    free(node);
    if (child) ref_dec_ts(&child->refcount);
}

struct node *node_create (char *id, float value) {
    struct node *node = malloc(sizeof(*node));
    snprintf(node->id, sizeof(node->id), "%s", id);
    node->value = value;
    node->next = NULL;
    node->refcount = (struct ref){node_free, 1};
    return node;
}

void node_push (struct node **nodes, char *id, float value) {
    struct node *node = node_create(id, value);
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
        printf("%s = %f\n", node->id, node->value);
}

int main (void) {
    struct node *nodes = NULL;
    char id[64];
    float value;
    while (scanf(" %63s %f", id, &value) == 2)
        node_push(&nodes, id, value);
    if (nodes != NULL) {
        node_print(nodes);
        struct node *old = node_pop(&nodes);
        node_push(&nodes, "foobar", 0.0f);
        node_print(nodes);
        ref_dec_ts(&old->refcount);
        ref_dec(&nodes->refcount);
    }
    return 0;
}
/* }}} */
