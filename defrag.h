#ifndef DEFRAG_H_
#define DEFRAG_H_

#define N_DBLOCKS 10
#define N_IBLOCKS 4
#define MAX_BUFFER_SIZE (10 * 1024 * 1024)
#define OUTPUT_FILE_NAME "disk_defrag"

struct inode {
  int next_inode; /* index of next free inode */
  int protect; /* protection field */
  int nlink; /* number of links to this file */
  int size; /* numer of bytes in file */
  int uid; /* owner’s user ID */
  int gid; /* owner’s group ID */
  int ctime; /* change time */
  int mtime; /* modification time */
  int atime; /* access time */
  int dblocks[N_DBLOCKS]; /* pointers to data blocks */
  int iblocks[N_IBLOCKS]; /* pointers to indirect blocks */
  int i2block; /* pointer to doubly indirect block */
  int i3block; /* pointer to triply indirect block */
};


struct superblock {
  int blocksize; /* size of blocks in bytes */
  int inode_offset; /* offset of inode region in blocks */
  int data_offset; /* data region offset in blocks */
  int swap_offset; /* swap region offset in blocks */
  int free_inode; /* head of free inode list, index */
  int free_block; /* head of free block list, index */
};


int FILLEDBLOCK = 0;
int DATABLKSIZE= 0;
int DATAOFFSET = 0;
int defragDiskImage();

#endif