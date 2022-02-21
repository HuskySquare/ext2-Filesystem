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
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
extern pthread_mutex_t operation_lock;


int32_t ext2_fsal_cp(const char *src,
                     const char *dst)
{
    /**
     * TODO: implement the ext2_cp command here ...
     * Arguments src and dst are the cp command arguments described in the handout.
     */
    pthread_mutex_lock(&operation_lock);
    if (strcmp(src, "") == 0 || strcmp(dst, "") == 0){\
        fprintf(stderr, "Invalid path\n");
        return ENOENT;
    }
    FILE *file;
    if ((file = fopen(src, "r"))== NULL){
        // Error handling if src file doesn't exist
        fprintf(stderr, "Source file does not exist\n");
        exit(ENOENT);
    }
    struct ext2_super_block *sb = get_super_block();
    // Check if it's a soft link
    //...

    // Making a copy of src
    int src_len = strlen(src);
    char src_copy[src_len + 1];
    memcpy(src_copy, src, src_len);
    src_copy[src_len] = '\0';

    // Making a copy of dst
    int dst_len = strlen(dst);
    char dst_copy[dst_len + 1];
    memcpy(dst_copy, dst, dst_len);
    dst_copy[dst_len] = '\0';

    char dest_parent_path[PATH_MAX];
    get_parent_path(dest_parent_path, dst_copy);


    char last_entry[EXT2_NAME_LEN];

    strcpy(last_entry, get_last_entry(dst_copy));


    struct ext2_inode *dst_inode = get_inode_from_path(dst_copy);


    struct ext2_inode *parent_dest_inode = get_inode_from_path(dest_parent_path);


    // Checking if file size exceeds limit (Done)
    struct stat st;
    if (stat(src_copy, &st) < 0){
        fprintf(stderr, "Unable to obtain file size\n");
        exit(-1);
    }
    int file_size = st.st_size;

    /* Maximum size for the inode is the size of the first 11 direct blocks + number of pointers to blocks we can fit in
    one block * size of one block */

    const int MAX_INODE_SIZE = ((EXT2_BLOCK_SIZE * 11) +
                                ((EXT2_BLOCK_SIZE / sizeof(unsigned int)) * EXT2_BLOCK_SIZE));


    if (file_size > sb->s_free_blocks_count * EXT2_BLOCK_SIZE || file_size > MAX_INODE_SIZE) {
        fclose(file);
        exit(ENOSPC);
    }



    if (parent_dest_inode == NULL || !(parent_dest_inode->i_mode & EXT2_S_IFDIR)){
        // If the parent inode is not a directory or it does not exist, then we return ENOENT
        exit(ENOENT);
    }

    char src_file_content[file_size + 1];
    fread(src_file_content, file_size + 1, 1, file);



    if (dst_inode){
            if (dst_inode->i_mode & EXT2_S_IFDIR) {
                // If the destination path is a directory...

                int inode_num = allocate_inode(EXT2_FT_REG_FILE);
                struct ext2_inode *new_inode = get_inode(inode_num);
                add_dir_entry(dst_inode, last_entry, EXT2_FT_REG_FILE, inode_num);

                write_inode(new_inode, src_file_content, file_size);

            }

            if (dst_inode->i_mode & EXT2_S_IFREG || dst_inode->i_mode & EXT2_S_IFLNK){
                if (dst[dst_len-1] == '/'){
                    // A trailing '/' indicates that it is a dir
                    exit(ENOENT);
                }

                rm_dir_entry(parent_dest_inode, last_entry);

                int inode_num = allocate_inode(EXT2_FT_REG_FILE);


                if (dst_inode->i_mode & EXT2_S_IFREG){
                    add_dir_entry(parent_dest_inode, last_entry, EXT2_FT_REG_FILE, inode_num);
                }
                else{
                    add_dir_entry(parent_dest_inode, last_entry, EXT2_FT_SYMLINK, inode_num);
                }

                struct ext2_inode *new_inode = get_inode(inode_num);
                write_inode(new_inode, src_file_content, file_size);

            }
            if (dst_inode->i_mode & EXT2_S_IFLNK){
                exit(EEXIST);
            }


    }
    else{

        int inode_num = allocate_inode(EXT2_FT_REG_FILE);
        struct ext2_inode *new_inode = get_inode(inode_num);
        add_dir_entry(parent_dest_inode, last_entry, EXT2_FT_REG_FILE, inode_num);
        write_inode(new_inode, src_file_content, file_size);

    }
    pthread_mutex_unlock(&operation_lock);
    return 0;
}