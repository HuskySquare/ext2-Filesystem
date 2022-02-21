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

#include "ext2fsal.h"
#include "e2fs.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>



extern unsigned char *disk;
extern pthread_mutex_t operation_lock;

int32_t ext2_fsal_ln_sl(const char *src,
                        const char *dst)
{
    /**
     * TODO: implement the ext2_ln_sl command here ...
     * src and dst are the ln command arguments described in the handout.
     */
    pthread_mutex_lock(&operation_lock);

    int src_len = strlen(src);
    char src_copy[src_len + 1];
    memcpy(src_copy, src, src_len);
    src_copy[src_len] = '\0';

    struct ext2_inode *src_inode = get_inode_from_path(src_copy);

    if(src_inode == NULL){
        exit(ENOENT);
    }

    if (src_inode->i_mode == EXT2_S_IFDIR){
        exit(EISDIR);
    }
    int dest_len = strlen(dst);
    char dest_copy[dest_len + 1];
    memcpy(dest_copy, dst, dest_len);
    dest_copy[dest_len] = '\0';

    char link_name[EXT2_NAME_LEN];
    char dest_parent_path[PATH_MAX];

    get_parent_path(dest_parent_path, dest_copy);
    strcpy(link_name, get_last_entry(dest_copy));

    struct ext2_inode *dest_inode = get_inode_from_path(dest_copy);

    //if file already exists here
    if (dest_inode != NULL && !(dest_inode->i_mode & EXT2_S_IFDIR)){
        exit(EEXIST);
    }
    struct ext2_inode *parent_dest_inode = get_inode_from_path(dest_parent_path);
    
    //check if parent path exists and is a directory
    if (parent_dest_inode == NULL || !(parent_dest_inode->i_mode & EXT2_S_IFDIR)){
        exit(ENOENT);
    }

    unsigned int inode_num = allocate_inode(EXT2_FT_SYMLINK);

    add_dir_entry(parent_dest_inode, link_name, EXT2_FT_SYMLINK, inode_num);

    struct ext2_inode *new_inode = get_inode(inode_num);

    // Allocate block for symlink path
    int block_num = allocate_block();
    unsigned char* block = disk + block_num * EXT2_BLOCK_SIZE;
    memcpy(block, src_copy, src_len);

    new_inode->i_block[0] = block_num;
    new_inode->i_size = src_len;
    new_inode->i_blocks += 2;

    pthread_mutex_unlock(&operation_lock);

    return 0;
}
