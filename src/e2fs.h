/*
 *------------
 * This code is provided solely for the personal and private use of
 * students taking the CSC369H5 course at the University of Toronto.
 * Copying for purposes other than this use is expressly prohibited.
 * All forms of distribution of this code, whether as given or with
 * any changes, are expressly prohibited.
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 MCS @ UTM
 * -------------
 */

#ifndef CSC369_E2FS_H
#define CSC369_E2FS_H

#include "ext2.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define PATH_MAX 4096

/**
 * TODO: add in here prototypes for any helpers you might need.
 * Implement the helpers in e2fs.c
 */

extern unsigned char *disk;
extern pthread_mutex_t operation_lock;

// return the inode corresponding to the path.
// If path doesn't exist, ...


char * extract_dir_name(char * path);
struct ext2_super_block *get_super_block();
struct ext2_group_desc *get_group_desc();
//struct ext2_inode *get_inode_table();
struct ext2_inode *get_inode_table();
unsigned char *get_inode_bitmap();
unsigned char *get_block_bitmap();
struct ext2_inode *get_inode(int inode_num);
unsigned char *get_block (int block_num);
struct ext2_dir_entry *get_entry(int block_num, int block_pos);

struct ext2_inode *get_inode_from_path (char *path);
unsigned int get_inode_number_from_path (char *path);

void write_inode(struct ext2_inode *inode, char *data, size_t size);
int assign_block_to_inode(struct ext2_inode *inode, int block_num);

unsigned int find_avail_inode();
unsigned int allocate_inode(unsigned char file_type);
unsigned int find_avail_block();
unsigned int allocate_block();


struct ext2_dir_entry *find_entry(struct ext2_inode *parent_inode, char *last_entry);
int get_aligned_bytes(int dir_size);
struct ext2_dir_entry *add_dir_entry(struct ext2_inode *parent_inode, char *dir_name, int file_type, int dir_inode);
void rm_dir_entry(struct ext2_inode *parent_inode, char *dir_name);

void get_parent_path(char *dest, char *path);
char *get_last_entry(char *path);

#endif