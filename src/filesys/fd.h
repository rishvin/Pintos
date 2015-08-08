/*
 * fd.h
 *
 *  Created on: Aug 7, 2015
 *      Author: rjoshi
 */

#ifndef PINTOS_SRC_FILESYS_FD_H_
#define PINTOS_SRC_FILESYS_FD_H_

#define FD_INVALID      -1
#define FD_MIN          2
#define FD_MAX          PGSIZE

void fd_init(void);
void fd_destroy(void);
int fd_insert(struct file *file);
struct file* fd_remove(int fd);

#endif /* PINTOS_SRC_FILESYS_FD_H_ */
