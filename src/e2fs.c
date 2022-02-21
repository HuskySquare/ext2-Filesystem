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

/**
 * TODO: Make sure to add all necessary includes here
 */
#include <stdlib.h>
#include <errno.h>

#include "e2fs.h"


/**
 * TODO: Add any helper implementations here
 */
unsigned char *disk;
pthread_mutex_t operation_lock;


struct ext2_super_block *get_super_block(){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    return sb;
}

struct ext2_group_desc *get_group_desc(){
    struct ext2_group_desc *gd = (struct  ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
    return gd;
}

struct ext2_inode *get_inode_table(){
    struct ext2_group_desc *gd = (struct  ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + (EXT2_BLOCK_SIZE * gd->bg_inode_table));
    return inode_table;

}

unsigned char *get_inode_bitmap(){
    struct ext2_group_desc *gd = (struct  ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
    unsigned char *inode_bitmap = (unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
    return inode_bitmap;
}

unsigned char *get_block_bitmap(){
    struct ext2_group_desc *gd = (struct  ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
    unsigned char *block_bitmap = (unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
    return block_bitmap;
}
unsigned char *get_block (int block_num)
{
    unsigned char *block = (unsigned char *) (disk + block_num * EXT2_BLOCK_SIZE);
    return block;
}


struct ext2_inode *get_inode(int inode_num) {


    struct ext2_inode *inode = (struct ext2_inode *) (get_inode_table() + (inode_num - 1));


    return inode;
}

struct ext2_dir_entry *get_entry(int block_num, int block_pos){
    struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk +
            block_num * EXT2_BLOCK_SIZE + block_pos);
    return dir_entry;
}

/**
 *
 * @param inode
 * @param block_num
 * @return 1 if direct block, 0 if indirect block, -1 if direct block and indirect block has already been initialized
 */
int assign_block_to_inode(struct ext2_inode *inode, int block_num){
    for (int i = 0; i < 12; i++){
        if (!inode->i_block[i]){
            inode->i_block[i] = block_num;
            inode->i_blocks += 2;
            return 1;
        }
    }
    // Indirect block case:
    int i = 12;
    if (!inode->i_block[i]){
        // If the indirect block is not initialized

        inode->i_block[i] = block_num;
        inode->i_blocks += 2;
        return 0;
    }

    return -1;

}
/**
 * Helper function to write to inode.
 * @param inode
 * @param data
 * @param size
 * @return 0 on success, -1 otherwise
 */
static int write_inode_helper(struct ext2_inode *inode, char *data, size_t size){

    int block_num = allocate_block(); // No space already handled

    unsigned char* block = get_block(block_num);
    inode->i_size += size;



    int result = assign_block_to_inode(inode, block_num);


    if (result == 1) {
        memcpy(block, data, size);
    }
    else {
        // If indirect block
        unsigned int *indirect_block = (unsigned int *) get_block(inode->i_block[12]);
        for (int j = 0; j < EXT2_BLOCK_SIZE / sizeof(unsigned int); j++){
            if (!indirect_block[j]){
                if (result == 0) {
                    block_num = allocate_block();
                }
                indirect_block[j] = block_num;
                inode->i_blocks += 2;
                memcpy(get_block(block_num), data, size);
                break;
            }
        }
    }


    return 0;


}

/**
 * Write content of file into the specified inode. This is used in cp command.
 * @param inode Inode corresponding to the file
 * @param data File content that is to be written
 * @param size Size of the data (file) we want to write
 */
void write_inode(struct ext2_inode *inode, char *data, size_t size){

    while (size > EXT2_BLOCK_SIZE){
        write_inode_helper(inode, data, EXT2_BLOCK_SIZE);
        data += EXT2_BLOCK_SIZE;
        size -= EXT2_BLOCK_SIZE;
    }

    if (size > 0){
        // If there are remaining bytes still to be written
        write_inode_helper(inode, data, size);
    }


}

/**
 * Helper method to set the proper bit in a bitmap
 * @param bitmap inode_bitmap or block_bitmap
 * @param bit the index of the bit value we want to change
 * @param val the value of the bit, so 1 or 0
 */
static void set_bit(unsigned char *bitmap, int bit, int val) {
    unsigned char *byte = bitmap + (bit / 8);
    if (val == 1) {
        *byte |= 1 << (bit % 8);
        return;
    } else if (val == 0) {
        *byte &= ~(1 << (bit % 8));
        return;
    }


}



//find first unset inode bitmap and claim it
unsigned int find_avail_inode(){


    unsigned char *inode_bitmap = get_inode_bitmap();
    unsigned int inode_count = get_super_block()->s_inodes_count;

    int byte = 1;
    int bit = 3;

    int found = 0;
    while (!found && byte < inode_count / 8){
        while (bit < 8){
            int in_use = inode_bitmap[byte] & (1 << bit);
            if (!in_use){
                inode_bitmap[byte] |= (1 << bit);
                return byte * 8 + bit;
            }
            bit++;
        }
        byte++;
        bit = 0;
    }

    exit(ENOSPC);

}
/**
 * Find the first inode available to allocate for a file or a directory.
 */
unsigned int allocate_inode(unsigned char file_type){
    
    struct ext2_super_block *sb = get_super_block();
    struct ext2_group_desc  *gd = get_group_desc();

    unsigned int inode_index = find_avail_inode();

    sb->s_free_inodes_count--;
    gd->bg_free_inodes_count--;

    struct ext2_inode *new_inode = get_inode(inode_index + 1);

    memset(new_inode, 0, sizeof(struct ext2_inode));

    if(file_type == EXT2_FT_DIR){
        new_inode->i_mode = EXT2_S_IFDIR;
    }
    else if (file_type == EXT2_FT_REG_FILE){
        new_inode->i_mode = EXT2_S_IFREG;
    }
    else if (file_type == EXT2_FT_SYMLINK){
        new_inode->i_mode = EXT2_S_IFLNK;
    }


    // Timestamps:
    unsigned int cur_time = (unsigned int) time(NULL);
    new_inode->i_atime = cur_time;
    new_inode->i_ctime = cur_time;
    new_inode->i_mtime = cur_time;

    new_inode->i_links_count = 0;
    new_inode->i_size = 0;



    return inode_index + 1;

}
void deallocate_inode(int inode_num){
    set_bit(get_inode_bitmap(), inode_num - 1, 0);
    get_super_block()->s_free_inodes_count++;
    get_group_desc()->bg_free_inodes_count++;

}
unsigned int find_avail_block(){

    unsigned char *block_bitmap = get_block_bitmap();
    unsigned int block_count = get_super_block()->s_blocks_count;
    int byte, bit;
    for (byte = 0; byte < block_count/ 8; byte++){
        for (bit = 0; bit < 8; bit++){

            int in_use = block_bitmap[byte] & (1 << bit);
            if (!in_use){
                //set inode bitmap
                block_bitmap[byte] |= (1 << bit);
                return byte * 8 + bit;
            }

        }
    }
    exit(ENOMEM);

}

unsigned int allocate_block(){
    
    struct ext2_super_block *sb = get_super_block();
    struct ext2_group_desc  *gd = get_group_desc();

    unsigned int block_index = find_avail_block();
    if (block_index == -1){
        exit(ENOSPC);
    }
    sb->s_free_blocks_count--;
    gd->bg_free_blocks_count--;

    unsigned  char *block = get_block(block_index + 1);
    memset(block, 0, EXT2_BLOCK_SIZE);

    return block_index + 1;
}



struct ext2_inode *get_inode_from_path (char *path){

    if (strcmp(path, "") == 0){
        /* A special case when the empty string is passed, like getting the parent inode of the path  /home, then root inode will be
         returned
         */

        return get_inode(EXT2_ROOT_INO);
    }

    if (path[0] == '/'){
        int path_len = strlen(path);

        char path_copy[path_len + 1];

        strcpy(path_copy, path);


        struct ext2_inode *inode = get_inode(EXT2_ROOT_INO);

        char *token = strtok(path_copy, "/");

        while(token != NULL){

            struct ext2_dir_entry *entry = find_entry(inode, token);
            if (entry == NULL){

                return NULL;
            }
            inode = get_inode(entry->inode);

            token = strtok(NULL, "/");
        }
        return inode;
    }
    else{
        return NULL;
    }
}

unsigned int get_inode_number_from_path (char *path){
    if (strcmp(path, "") == 0){
        /* A special case when the empty string is passed, like getting the parent inode of the path  /home, then root inode will be
         returned
         */

        return EXT2_ROOT_INO;
    }
    if (path[0] == '/'){
        int path_len = strlen(path);
        char path_copy[path_len + 1];
        strcpy(path_copy, path);

        int inode_num = EXT2_ROOT_INO;
        char *token = strtok(path_copy, "/");

        while(token != NULL){
            struct ext2_dir_entry *entry = find_entry(get_inode(inode_num), token);
            if (entry == NULL){
                return -1;
            }
            inode_num = entry->inode;
            token = strtok(NULL, "/");
        }
        return inode_num;
    }
    else{
        return -1;
    }
}
/**
 * Search the parent_inode for an entry with the same name as the last entry in the path
 * @param starting_inode the inode to begin matching the path. This is usually EXT2_ROOT_INO if the path is a
 * @param path
 * @return the inode corresponding to the path (assuming it is valid). Returns null if the path is not valid.
 */
struct ext2_dir_entry *find_entry(struct ext2_inode *parent_inode, char *last_entry) {



    if (get_inode(EXT2_ROOT_INO) == parent_inode){

    }
    unsigned long block_pos = 0; //Position in a block (don't really need unsigned long since the size is only 1024)

    char name[EXT2_NAME_LEN];
    struct ext2_dir_entry *cur_entry;

    for (int i = 0; i < 12 && parent_inode->i_block[i] != 0; i++){
        block_pos = 0;
        while (block_pos < EXT2_BLOCK_SIZE){
            cur_entry = get_entry(parent_inode->i_block[i], block_pos);
            strcpy(name, cur_entry->name);
            name[cur_entry->name_len] = '\0';

            if (strcmp(name, last_entry) == 0){

                return cur_entry;
            }
            block_pos += cur_entry->rec_len;
        }
    }

    return NULL;



}

int get_aligned_bytes(int dir_size){

    if (dir_size % 4 != 0){
        int temp = dir_size / 4 + 1;
        dir_size = temp * 4;

    }
    return dir_size;
}

int check_any_block_avail(){
    int total_free = 0;
    unsigned char *block_bitmap = get_block_bitmap();
    unsigned int block_count = get_super_block()->s_blocks_count;
    int byte, bit;
    for (byte = 0; byte < block_count/ 8; byte++){
        for (bit = 0; bit < 8; bit++){

            int in_use = block_bitmap[byte] & (1 << bit);
            if (!in_use){
                total_free ++;
            }

        }
    }
    return total_free;
}

struct ext2_dir_entry *add_dir_entry(struct ext2_inode *parent_inode, char *dir_name, int file_type, int dir_inode){


    int dir_name_length = strlen(dir_name);

    int new_dir_size = get_aligned_bytes(dir_name_length + sizeof(struct ext2_dir_entry));

    int i = 0;
    while (i < 12 && parent_inode->i_block[i] != 0){
        int block_num = parent_inode->i_block[i];

        unsigned char * curr_pointer = disk + (block_num * EXT2_BLOCK_SIZE);
        unsigned char * block_end = EXT2_BLOCK_SIZE + curr_pointer;
        struct ext2_dir_entry *dir_entry;

        while (curr_pointer < block_end){
            dir_entry =  (struct ext2_dir_entry *) curr_pointer;
            curr_pointer += dir_entry->rec_len;
        }
        //check if space in block

        int actual_size = get_aligned_bytes((sizeof(struct ext2_dir_entry) + dir_entry->name_len));
        int available_space = dir_entry->rec_len - actual_size;

        if (available_space >= new_dir_size){

            // Need another block if entry  is a folder (for entries . and ..) check if space available before proceeding 
            if ((((file_type == EXT2_FT_DIR) || (file_type == EXT2_FT_SYMLINK)) && check_any_block_avail() >= 1) ||
            (file_type != EXT2_FT_DIR && file_type != EXT2_FT_SYMLINK)){
                dir_entry->rec_len = actual_size;


                // Point dir_entry to new entry
                dir_entry = (struct ext2_dir_entry *) ((unsigned char *) dir_entry + actual_size);

                dir_entry->inode = dir_inode;
                dir_entry->rec_len = available_space;
                dir_entry->name_len = strlen(dir_name);
                dir_entry->file_type = file_type;
                strncpy(dir_entry->name, dir_name, dir_entry->name_len);

                struct ext2_inode *dir_inode_struct = get_inode(dir_inode);
                dir_inode_struct->i_links_count++;
                return dir_entry;
            }
            // is a directory and not enough space
            else {
                deallocate_inode(dir_inode);
                exit(ENOSPC);
            }

        }
     i++;
        }

    //no space in blocks so allocate new direct block
    if (parent_inode->i_block[i] == 0 && i < 12){

            // Need space for two blocks before proceeding
            if ( (((file_type == EXT2_FT_DIR) || (file_type == EXT2_FT_SYMLINK)) && check_any_block_avail() >= 2) ||
            ((file_type != EXT2_FT_DIR && file_type != EXT2_FT_SYMLINK) && check_any_block_avail() >= 1)){

                int block_num = allocate_block();
                parent_inode->i_block[i] = block_num;
                parent_inode->i_size += EXT2_BLOCK_SIZE;
                //every block is worth two sectors
                parent_inode->i_blocks += 2;
                struct ext2_inode *dir_inode_struct = get_inode(dir_inode);
                dir_inode_struct->i_links_count++;

                unsigned char *block = disk + (block_num * EXT2_BLOCK_SIZE);
                struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) block;
                int rec_len = EXT2_BLOCK_SIZE;

                dir_entry->inode = dir_inode;
                dir_entry->rec_len = rec_len;
                dir_entry->name_len = strlen(dir_name);
                dir_entry->file_type = file_type;
                strncpy(dir_entry->name, dir_name, strlen(dir_name));

                return dir_entry;
           }
            // is a directory and not enough space
           else{
                deallocate_inode(dir_inode);
                exit(ENOSPC);
           }
        }
    //no space in direct blocks so allocate new indirect block
    else{
        if ( (((file_type == EXT2_FT_DIR) || (file_type == EXT2_FT_SYMLINK)) && check_any_block_avail() >= 3) ||
            (file_type != EXT2_FT_DIR && file_type != EXT2_FT_SYMLINK && check_any_block_avail() >= 2)) {


            if (parent_inode->i_block[12] == 0) {
                parent_inode->i_block[12] = allocate_block();
                parent_inode->i_size += EXT2_BLOCK_SIZE;
                //every block is worth two sectors
                parent_inode->i_blocks += 2;
            }
            //check space in array pointed by inode[12]
            int block_num = parent_inode->i_block[12];

            unsigned char *block_array_addr = disk + (block_num * EXT2_BLOCK_SIZE);

            unsigned int block = (unsigned int) *block_array_addr;
            int total = 0;
            while (block != 0 && total < EXT2_BLOCK_SIZE) {
                block_array_addr += sizeof(unsigned int);
                total += sizeof(unsigned int);
                block = (unsigned int) *block_array_addr;
            }
            int space_available = EXT2_BLOCK_SIZE - total;
            if (new_dir_size <= space_available) {
                unsigned int new_block_num = allocate_block();
                *block_array_addr = new_block_num;
                parent_inode->i_size += EXT2_BLOCK_SIZE;
                //every block is worth two sectors
                parent_inode->i_blocks += 2;
                unsigned char *new_block = disk + (new_block_num * EXT2_BLOCK_SIZE);
                struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) new_block;

                dir_entry->inode = dir_inode;
                dir_entry->rec_len = space_available;
                dir_entry->name_len = strlen(dir_name);
                dir_entry->file_type = file_type;
                strncpy(dir_entry->name, dir_name, strlen(dir_name));

                get_inode(dir_inode)->i_links_count++;

                return dir_entry;
            }
        }
        else{
            deallocate_inode(dir_inode);
            exit(ENOSPC);
        }
    }

    exit(ENOSPC);

}

void rm_dir_entry(struct ext2_inode *parent_inode, char *dir_name){
    int i = 0;
    size_t dir_name_len = strlen(dir_name);
    while (i < 12 && parent_inode->i_block[i] != 0){
        int block_num = parent_inode->i_block[i];

        unsigned char * curr_pointer = disk + (block_num * EXT2_BLOCK_SIZE);
        unsigned char * block_end = EXT2_BLOCK_SIZE + curr_pointer;
        struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) curr_pointer;
        struct ext2_dir_entry *prev_entry = NULL;
        int found = 0;

        while (curr_pointer < block_end){
            if (strncmp(dir_entry->name, dir_name, dir_name_len) == 0){
                found = 1;
                break;
            }
            prev_entry = dir_entry;
            curr_pointer += dir_entry->rec_len;
            dir_entry =  (struct ext2_dir_entry *) curr_pointer;


        }
        // Now dir_entry is the dir_entry we want to delete or we're at block
        struct ext2_group_desc *gd = get_group_desc();

        int inode_num = dir_entry->inode;

        struct ext2_inode *inode = get_inode(inode_num);

        if (found) {
            inode->i_links_count--;
            dir_entry->name_len = 0;
            dir_entry->inode = 0;

            if (inode->i_links_count == 0){
                for (int j = 0; j < 12; j++){

                    if (inode->i_block[j] != 0) {
                        set_bit(get_block_bitmap(), inode->i_block[j] - 1, 0);
                        gd->bg_free_blocks_count++;
                        get_super_block()->s_free_blocks_count++;
                    }
                }

                int block_num = inode->i_block[12];
                if (block_num != 0) {
                    unsigned char *block_array_addr = disk + (block_num * EXT2_BLOCK_SIZE);

                    unsigned int block = (unsigned int) *block_array_addr;
                    int total = 0;
                    while (block != 0 && total < EXT2_BLOCK_SIZE) {
                        set_bit(get_block_bitmap(), block - 1, 0);
                        gd->bg_free_blocks_count++;
                        get_super_block()->s_free_blocks_count++;
                        block_array_addr += sizeof(unsigned int);
                        total += sizeof(unsigned int);
                        block = (unsigned int) *block_array_addr;
                    }
                    set_bit(get_block_bitmap(), inode->i_block[12] - 1, 0);
                    gd->bg_free_blocks_count++;
                    get_super_block()->s_free_blocks_count++;
                }
                deallocate_inode(inode_num);
            }

            inode->i_dtime = (unsigned int) time(NULL);

            if (prev_entry != NULL) {
                // In the middle or tail
                prev_entry->rec_len += dir_entry->rec_len;
            }
            // Head case already handled

        }


        i++;
    }
}
int is_absolute_path(char *path){
    return path[0] == '/';
}

void get_parent_path(char *dest, char *path){
    int path_len = strlen(path);
    char path_copy[path_len + 1];
    memcpy(path_copy, path, path_len);
    path_copy[path_len] = '\0';

    char * token = strtok(path_copy, "/");
    int counter = 0;
    char string_array[255][255]; // Just a random maximum (might need to change later)
    while(token != NULL ) {
        strcpy(string_array[counter++], token);

        token = strtok(NULL, "/");
    }
    char parent_path[255] = "";
    for (int i = 0; i < counter - 1; i++){
        strcat(parent_path, "/");
        strcat(parent_path, string_array[i]);
    }
    strcpy(dest, parent_path);
}

char *get_last_entry(char *path){
    int path_len = strlen(path);

    char path_copy[path_len + 1];

    strcpy(path_copy, path);

    char * token = strtok(path_copy, "/");
    char *prev = NULL;
    while(token != NULL ) {

        prev = token;
        token = strtok(NULL, "/");
    }
    return prev;
}
