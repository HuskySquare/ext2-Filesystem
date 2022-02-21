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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


extern pthread_mutex_t operation_lock;

int32_t ext2_fsal_rm(const char *path)
{
    /**
     * TODO: implement the ext2_rm command here ...
     * the argument 'path' is the path to the file to be removed.
     */
    pthread_mutex_lock(&operation_lock);
    if (strlen(path) == 0 || path[0] != '/')
        exit(ENOENT);

    int path_len = strlen(path);
    char path_copy[path_len + 1];
    strcpy(path_copy, path);
    path_copy[path_len] = '\0';


    struct ext2_inode *dst_inode = get_inode_from_path(path_copy);

    if (dst_inode == NULL){
        exit(ENOENT);
    }
    if (dst_inode->i_mode == EXT2_FT_DIR){
        exit(EISDIR);
    }
    else if (path[path_len - 1] == '/'){
        exit(ENOENT);
    }


    char parent_path[PATH_MAX];
    char dir_name[EXT2_NAME_LEN];

    get_parent_path(parent_path, path_copy);
    strcpy(dir_name, get_last_entry(path_copy));

    struct ext2_inode *parent_inode = get_inode_from_path(parent_path);
    if (parent_inode == NULL){
        exit(ENOENT);
    }
    else{
        rm_dir_entry(parent_inode, dir_name);
    }
    pthread_mutex_unlock(&operation_lock);
    return 0;
}