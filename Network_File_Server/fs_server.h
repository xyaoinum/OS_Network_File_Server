/*
 * fs_server.h
 *
 * Header file for the file server.
 */

#ifndef _FS_SERVER_H_
#define _FS_SERVER_H_

#include <sys/types.h>
#include <pthread.h>
#include <mutex>

#include "fs_param.h"
#include "fs_crypt.h"

/*
 * Size of the disk (in blocks)
 */
#define FS_DISKSIZE     4096

/*
 * Typedefs for on-disk data structures.
 */
typedef struct {
    char name[FS_MAXFILENAME + 1];         // name of this file
    unsigned int inode_block;              // disk block that stores the inode for
                                           // this file
} fs_direntry;

typedef struct {
    char owner[FS_MAXUSERNAME + 1];
    unsigned int size;                     // size of this file or directory
                                           // in bytes
    unsigned int blocks[FS_MAXFILEBLOCKS]; // array of data blocks for this
                                           // file or directory
} fs_inode;

/*
 * Number of direntries that can fit in one block
 */
#define FS_DIRENTRIES  (FS_BLOCKSIZE / sizeof(fs_direntry))

/*
 * Mutexes to prevent garbled output from a multi-threaded file server.
 * Your file server must wrap all calls to cout inside a critical section
 * protected by cout_lock_pthread (if you're using pthreads) or cout_lock_cpp
 * (if you're using C++11 threads).  Use one or the other--don't use both.
 */
extern pthread_mutex_t cout_lock_pthread;  // use this with pthreads
extern class std::mutex cout_lock_cpp;     // use this with C++11 threads

/*
 * Global variable to control debugging output for send and close.
 */
extern bool fs_quiet;

/*
 * Interface to the disk.
 *
 * Disk blocks are numbered from 0 to (FS_DISKSIZE-1).
 * disk_readblock and disk_writeblock are both thread safe, i.e., multiple
 * threads can safely make simultaneous calls to these functions.
 */

/*
 * disk_readblock
 *
 * Copies disk block "block" into buf.  Asserts on failure.
 */
extern void disk_readblock(unsigned int block, void *buf);

/*
 * disk_writeblock
 *
 * Copies buf to disk block "block".  Asserts on failure.
 */
extern void disk_writeblock(unsigned int block, const void *buf);

/*
 * Global variable to control debugging output for disk operations.
 */
extern bool disk_quiet;

#endif /* _FS_SERVER_H_ */
