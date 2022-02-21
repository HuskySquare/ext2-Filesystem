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

int32_t ext2_fsal_mkdir(const char *path)
{
    /**
     * TODO: implement the ext2_mkdir command here ...
     * the argument path is the path to the directory that is to be created.
     */
    // Making a copy of the path
    pthread_mutex_lock(&operation_lock);
    int path_len = strlen(path);
    char path_copy[path_len + 1];
    memcpy(path_copy, path, path_len);
    path_copy[path_len] = '\0';


    //check if path destination is invalid or already folder


    char dir_name[EXT2_NAME_LEN];
    char parent_path[PATH_MAX];


    get_parent_path(parent_path, path_copy);


    strcpy(dir_name, get_last_entry(path_copy));



    struct ext2_inode *dir_inode = get_inode_from_path(path_copy);


    if (dir_inode != NULL && (dir_inode->i_mode & EXT2_S_IFDIR)){
        exit(EEXIST);
    }



    struct ext2_inode *parent_dir_inode = get_inode_from_path(parent_path);


    //check if parent path exists and is a directory
    if (parent_dir_inode == NULL || !(parent_dir_inode->i_mode & EXT2_S_IFDIR)){
        exit(ENOENT);
    }

    unsigned int inode_num = allocate_inode(EXT2_FT_DIR);

    //initializing dir


    add_dir_entry(parent_dir_inode, dir_name, EXT2_FT_DIR, inode_num);


    struct ext2_inode *new_parent_inode = get_inode(inode_num);

    add_dir_entry(new_parent_inode, ".", EXT2_FT_DIR, inode_num);

    struct ext2_dir_entry *parent_dot_entry = find_entry(parent_dir_inode, ".");

    add_dir_entry(new_parent_inode, "..", EXT2_FT_DIR, parent_dot_entry->inode);


    get_group_desc()->bg_used_dirs_count++;
    pthread_mutex_unlock(&operation_lock);
    return 0;
}