/*
 * fd.h
 *
 *  Created on: Aug 7, 2015
 *      Author: rjoshi
 */

#ifndef PINTOS_SRC_FILESYS_FD_H_
#define PINTOS_SRC_FILESYS_FD_H_

#include <bitmap.h>
#include <hash.h>

#define FD_INVALID      -1
#define FD_MIN          2
#define FD_MAX          128

typedef void fd_destructor(struct file *file);

struct fd_node
{
    struct bitmap *fd_bm;
    struct hash fd_hash;
};

bool fd_init(struct fd_node *fd_node);
void fd_destroy(struct fd_node *fd_node, fd_destructor *destruct);
int fd_insert(struct fd_node *fd_node, struct file *file);
struct file* fd_remove(struct fd_node *fd_node, int fd);
struct file* fd_search(struct fd_node *fd_node, int fd);

#endif /* PINTOS_SRC_FILESYS_FD_H_ */
