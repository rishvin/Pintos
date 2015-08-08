/*
 * fd.c
 *
 *  Created on: Aug 7, 2015
 *      Author: rjoshi
 */

#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <bitmap.h>
#include <hash.h>
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "fd.h"

#define IDX_TO_FD(I) ((I) + FD_MIN)
#define FD_TO_IDX(F) ((F) - FD_MIN)

struct fd_node
{
    struct hash_elem elem;
    struct file *file;
    int fd;
};

static struct bitmap *fd_bm;
static struct  hash fd_hash;

static unsigned fd_hash_func(const struct hash_elem *e, void *aux UNUSED);
static bool fd_less_func(const struct hash_elem *a,
                         const struct hash_elem *b,
                         void *aux UNUSED);

static unsigned fd_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
    const struct fd_node *node = hash_entry(e, const struct fd_node, elem);
    return hash_int(node->fd);
}

static bool fd_less_func (const struct hash_elem *a,
                          const struct hash_elem *b,
                          void *aux UNUSED)
{
    const struct fd_node *node_a = hash_entry(a, const struct fd_node, elem);
    const struct fd_node *node_b = hash_entry(b, const struct fd_node, elem);
    return node_a->fd < node_b->fd;
}

void fd_init()
{
    fd_bm = bitmap_create(FD_MAX);
    if(!fd_bm)
    {
        ASSERT(0);
    }
    else hash_init(&fd_hash, fd_hash_func, fd_less_func, NULL);
}

void fd_destroy()
{
    bitmap_destroy(fd_bm);
    hash_destroy(&fd_hash, NULL);
}

int fd_insert(struct file *file)
{
    struct fd_node *node;
    size_t idx;

    idx = bitmap_scan(fd_bm, 0, 1, false);
    if(idx == BITMAP_ERROR)
        return FD_INVALID;

    node = malloc(sizeof (struct fd_node));
    if(!node)
        return FD_INVALID;

    node->fd = IDX_TO_FD(idx);
    node->file = file;

    if(hash_insert(&fd_hash, &node->elem) != NULL)
    {
        free(node);
        return FD_INVALID;
    }
    bitmap_set(fd_bm, idx, true);
    return node->fd;
}

struct file* fd_remove(int fd)
{
    struct fd_node node;
    struct fd_node *ret;
    struct file *file;

    if(fd < FD_MIN || fd > FD_MAX)
        return false;

    if(!bitmap_test(fd_bm, FD_TO_IDX(fd)))
        return NULL;

    node.fd = fd;
    bitmap_set(fd_bm, FD_TO_IDX(fd), false);
    ret = hash_delete(&fd_hash, &node.elem);
    if(ret == NULL)
        return NULL;
    file = ret->file;
    free(ret);
    return file;
}

struct file* fd_search(int fd)
{
    struct fd_node node;
    struct fd_node *ret;

    if(fd < FD_MIN || fd > FD_MAX)
        return false;

    if(!bitmap_test(fd_bm, FD_TO_IDX(fd)))
        return NULL;

    node.fd = fd;
    ret = hash_find(&fd_hash, &node.elem);
    if(ret == NULL)
        return NULL;
    return ret->file;
}
