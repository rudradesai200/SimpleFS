/**
 * @file fs_layer_1.cpp
 * @author Kairav Shah (shahkairav1702@gmail.com)
 * @brief Implementation of fs.h layer_1 functions
 * @date 2020-12-09
 * 
 * @details Mainly includes all the functions which are necessary
 * for managing data at Block level in terms of Inode and InodeBlock
 * These functions won't be accessible at shell level.
 * But are extensively used at layer_2.
 */

#include "sfs/fs.h"

#include <algorithm>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

using namespace std;

ssize_t FileSystem::create() {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity check */
    if(!mounted) return false;

    /**- read the superblock */
    Block block;
    fs_disk->read(0, block.Data);

    /**- locate free inode in inode table */    
    for(uint32_t i = 1; i <= MetaData.InodeBlocks; i++) {
        /**- check if inode block is full */
        if(inode_counter[i-1] == INODES_PER_BLOCK) continue;
        else fs_disk->read(i, block.Data);
        
        /**- find the first empty inode */
        for(uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
            /**- set the inode to default values */
            if(!block.Inodes[j].Valid) {
                block.Inodes[j].Valid = true;
                block.Inodes[j].Size = 0;
                block.Inodes[j].Indirect = 0;
                for(int ii = 0; ii < 5; ii++) {
                    block.Inodes[j].Direct[ii] = 0;
                }
                free_blocks[i] = true;
                inode_counter[i-1]++;

                fs_disk->write(i, block.Data);

                return (((i-1) * INODES_PER_BLOCK) + j);
            }
        }
    }

    return -1;
}


bool FileSystem::load_inode(size_t inumber, Inode *node) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity check */
    if(!mounted) return false;
    if((inumber > MetaData.Inodes) || (inumber < 1)){return false;}

    Block block;

    /**- find index of inode in the inode table */
    int i = inumber / INODES_PER_BLOCK;
    int j = inumber % INODES_PER_BLOCK;

    /**- load the inode into Inode *node */
    if(inode_counter[i]) {
        fs_disk->read(i+1, block.Data);
        if(block.Inodes[j].Valid) {
            *node = block.Inodes[j];
            return true;
        }
    }

    return false;
}


bool FileSystem::remove(size_t inumber) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity check */
    if(!mounted) return false;

    Inode node;

    /**- check if the node is valid; if yes, then load the inode */
    if(load_inode(inumber, &node)) {
        node.Valid = false;
        node.Size = 0;

        /**- decrement the corresponding inode block in inode counter 
         * if the inode counter decreases to 0, then set the free bit map to false */ 
        if(!(--inode_counter[inumber / INODES_PER_BLOCK])) {
            free_blocks[inumber / INODES_PER_BLOCK + 1] = false;
        }

        /**- free direct blocks */
        for(uint32_t i = 0; i < POINTERS_PER_INODE; i++) {
            free_blocks[node.Direct[i]] = false;
            node.Direct[i] = 0;
        }

        /**- free indirect blocks */
        if(node.Indirect) {
            Block indirect;
            fs_disk->read(node.Indirect, indirect.Data);
            free_blocks[node.Indirect] = false;
            node.Indirect = 0;

            for(uint32_t i = 0; i < POINTERS_PER_BLOCK; i++) {
                if(indirect.Pointers[i]) free_blocks[indirect.Pointers[i]] = false;
            }
        }

        Block block;
        fs_disk->read(inumber / INODES_PER_BLOCK + 1, block.Data);
        block.Inodes[inumber % INODES_PER_BLOCK] = node;
        fs_disk->write(inumber / INODES_PER_BLOCK + 1, block.Data);

        return true;
    }
    
    return false;
}


ssize_t FileSystem::stat(size_t inumber) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity check */
    if(!mounted) return -1;

    Inode node;

    /**- load inode; if valid, return its size */
    if(load_inode(inumber, &node)) return node.Size;

    return -1;
}

void FileSystem::read_helper(uint32_t blocknum, int offset, int *length, char **data, char **ptr) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- read the block from disk and change the pointers accordingly */
    fs_disk->read(blocknum, *ptr);
    *data += offset;
    *ptr += Disk::BLOCK_SIZE;
    *length -= (Disk::BLOCK_SIZE - offset);

    return;
}


ssize_t FileSystem::read(size_t inumber, char *data, int length, size_t offset) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity check */
    if(!mounted) return -1;

    /**- IMPORTANT: start reading from index = offset */
    int size_inode = stat(inumber);
    
    /**- if offset is greater than size of inode, then no data can be read 
     * if length + offset exceeds the size of inode, adjust length accordingly
    */
    if((int)offset >= size_inode) return 0;
    else if(length + (int)offset > size_inode) length = size_inode - offset;

    Inode node;    

    /**- data is head; ptr is tail */
    char *ptr = data;     
    int to_read = length;

    /**- load inode; if invalid, return error */
    if(load_inode(inumber, &node)) {
        /**- the offset is within direct pointers */
        if(offset < POINTERS_PER_INODE * Disk::BLOCK_SIZE) {
            /**- calculate the node to start reading from */
            uint32_t direct_node = offset / Disk::BLOCK_SIZE;
            offset %= Disk::BLOCK_SIZE;

            /**- check if the direct node is valid */ 
            if(node.Direct[direct_node]) {
                read_helper(node.Direct[direct_node++], offset, &length, &data, &ptr);
                /**- read the direct blocks */
                while(length > 0 && direct_node < POINTERS_PER_INODE && node.Direct[direct_node]) {
                    read_helper(node.Direct[direct_node++], 0, &length, &data, &ptr);
                }

                /**- if length <= 0, then enough data has been read */
                if(length <= 0) return to_read;
                else {
                    /**- more data is to be read */

                    /**- check if all the direct nodes have been read completely 
                     *  and if the indirect pointer is valid
                    */
                    if(direct_node == POINTERS_PER_INODE && node.Indirect) {
                        Block indirect;
                        fs_disk->read(node.Indirect, indirect.Data);

                        /**- read the indirect nodes */
                        for(uint32_t i = 0; i < POINTERS_PER_BLOCK; i++) {
                            if(indirect.Pointers[i] && length > 0) {
                                read_helper(indirect.Pointers[i], 0, &length, &data, &ptr);
                            }
                            else break;
                        }

                        /**- if length <= 0, then enough data has been read */
                        if(length <= 0) return to_read;
                        else {
                            /**- data exhausted but the length requested was more */
                            /**- logically, this should never print */
                            return (to_read - length);
                        }
                    }
                    else {
                        /**- data exhausted but the length requested was more */
                        /**- logically, this should never print */
                        return (to_read - length);
                    }
                }
            }
            else {
                /**- inode has no stored data */
                return 0;
            }
        }
        else {
            /**- offset begins in the indirect block */
            /**- check if the indirect node is valid */
            if(node.Indirect) {
                /**- change offset accordingly and find the indirect node to start reading from */
                offset -= (POINTERS_PER_INODE * Disk::BLOCK_SIZE);
                uint32_t indirect_node = offset / Disk::BLOCK_SIZE;
                offset %= Disk::BLOCK_SIZE;

                Block indirect;
                fs_disk->read(node.Indirect, indirect.Data);

                if(indirect.Pointers[indirect_node] && length > 0) {
                    read_helper(indirect.Pointers[indirect_node++], offset, &length, &data, &ptr);
                }

                /**- iterate through the indirect nodes */
                for(uint32_t i = indirect_node; i < POINTERS_PER_BLOCK; i++) {
                    if(indirect.Pointers[i] && length > 0) {
                        read_helper(indirect.Pointers[i], 0, &length, &data, &ptr);
                    }
                    else break;
                }
                
                /**- if length <= 0, then enough data has been read */
                if(length <= 0) return to_read;
                else {
                    /**- data exhausted but the length requested was more */
                    /**- logically, this should never print */
                    return (to_read - length);
                }
            }
            else {
                /**- the indirect node is invalid */
                return 0;
            }
        }
    }
    
    /**- inode is invalid */
    return -1;
}


uint32_t FileSystem::allocate_block() {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity check */
    if(!mounted) return 0;

    /**- iterate through the free bit map and allocate the first free block */
    for(uint32_t i = MetaData.InodeBlocks + 1; i < MetaData.Blocks; i++) {
        if(free_blocks[i] == 0) {
            free_blocks[i] = true;
            return (uint32_t)i;
        }
    }

    /**- disk is full */
    return 0;
}


ssize_t FileSystem::write_ret(size_t inumber, Inode* node, int ret) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity check */
    if(!mounted) return -1;

    /**- find index of inode in the inode table */
    int i = inumber / INODES_PER_BLOCK;
    int j = inumber % INODES_PER_BLOCK;

    /**- store the node into the block */
    Block block;
    fs_disk->read(i+1, block.Data);
    block.Inodes[j] = *node;
    fs_disk->write(i+1, block.Data);

    /**- return ret */
    return (ssize_t)ret;
}


void FileSystem::read_buffer(int offset, int *read, int length, char *data, uint32_t blocknum) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity check */
    if(!mounted) return;
    
    /**- allocate memory to ptr which acts as buffer for reading from disk */
    char* ptr = (char *)calloc(Disk::BLOCK_SIZE, sizeof(char));

    /**- read data into ptr and change pointers accordingly */ 
    for(int i = offset; i < (int)Disk::BLOCK_SIZE && *read < length; i++) {
        ptr[i] = data[*read];
        *read = *read + 1;
    }
    fs_disk->write(blocknum, ptr);

    /**- free the allocated memory */
    free(ptr);

    return;
}


bool FileSystem::check_allocation(Inode* node, int read, int orig_offset, uint32_t &blocknum, bool write_indirect, Block indirect) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */
    
    /**- sanity check */
    if(!mounted) return false;
    
    /**- if blocknum is 0, then allocate a new block */
    if(!blocknum) {
        blocknum = allocate_block();
        /**- set size of node and write back to disk if it is an indirect node */
        if(!blocknum) {
            node->Size = read + orig_offset;
            if(write_indirect) fs_disk->write(node->Indirect, indirect.Data);
            return false;
        }
    }

    return true;
}


ssize_t FileSystem::write(size_t inumber, char *data, int length, size_t offset) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity check */
    if(!mounted) return -1;
    
    Inode node;
    Block indirect;
    int read = 0;
    int orig_offset = offset;

    /**- insufficient size */
    if(length + offset > (POINTERS_PER_BLOCK + POINTERS_PER_INODE) * Disk::BLOCK_SIZE) {
        return -1;
    }

    /**- if the inode is invalid, allocate inode.
     *  need not write to disk right now; will be taken care of in write_ret()
     */
    if(!load_inode(inumber, &node)) {
        node.Valid = true;
        node.Size = length + offset;
        for(uint32_t ii = 0; ii < POINTERS_PER_INODE; ii++) {
            node.Direct[ii] = 0;
        }
        node.Indirect = 0;
        inode_counter[inumber / INODES_PER_BLOCK]++;
        free_blocks[inumber / INODES_PER_BLOCK + 1] = true;
    }
    else {
        /**- set size of the node */
        node.Size = max((int)node.Size, length + (int)offset);
    }

    /**- check if the offset is within direct pointers */
    if(offset < POINTERS_PER_INODE * Disk::BLOCK_SIZE) {
        /**- find the first node to start writing at and change offset accordingly */
        int direct_node = offset / Disk::BLOCK_SIZE;
        offset %= Disk::BLOCK_SIZE;

        /**- check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
        if(!check_allocation(&node, read, orig_offset, node.Direct[direct_node], false, indirect)) { 
            return write_ret(inumber, &node, read);
        }
        /**- read from data buffer */       
        read_buffer(offset, &read, length, data, node.Direct[direct_node++]);

        /**- enough data has been read from data buffer */
        if(read == length) return write_ret(inumber, &node, length);
        /**- store in direct pointers till either one of the two things happen:
        * 1. all the data is stored in the direct pointers
        * 2. the data is stored in indirect pointers
        */
        else {
            /**- start writing into direct nodes */
            for(int i = direct_node; i < (int)POINTERS_PER_INODE; i++) {
                /**- check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
                if(!check_allocation(&node, read, orig_offset, node.Direct[direct_node], false, indirect)) { 
                    return write_ret(inumber, &node, read);
                }
                read_buffer(0, &read, length, data, node.Direct[direct_node++]);

                /**- enough data has been read from data buffer */
                if(read == length) return write_ret(inumber, &node, length);
            }

            /**- check if the indirect node is valid */
            if(node.Indirect) fs_disk->read(node.Indirect, indirect.Data);
            else {
                /**- check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
                if(!check_allocation(&node, read, orig_offset, node.Indirect, false, indirect)) { 
                    return write_ret(inumber, &node, read);
                }
                fs_disk->read(node.Indirect, indirect.Data);

                /**- initialise the indirect nodes */
                for(int i = 0; i < (int)POINTERS_PER_BLOCK; i++) {
                    indirect.Pointers[i] = 0;
                }
            }
            
            /**- write into indirect nodes */
            for(int j = 0; j < (int)POINTERS_PER_BLOCK; j++) {
                /**- check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
                if(!check_allocation(&node, read, orig_offset, indirect.Pointers[j], true, indirect)) { 
                    return write_ret(inumber, &node, read);
                }
                read_buffer(0, &read, length, data, indirect.Pointers[j]);

                /**- enough data has been read from data buffer */
                if(read == length) {
                    fs_disk->write(node.Indirect, indirect.Data);
                    return write_ret(inumber, &node, length);
                }
            }

            /**- space exhausted */
            fs_disk->write(node.Indirect, indirect.Data);
            return write_ret(inumber, &node, read);
        }
    }
    /**- offset begins in indirect blocks */
    else {
        /**- find the first indirect node to write into and change offset accordingly */
        offset -= (Disk::BLOCK_SIZE * POINTERS_PER_INODE);
        int indirect_node = offset / Disk::BLOCK_SIZE;
        offset %= Disk::BLOCK_SIZE;

        /**- check if the indirect node is valid */
        if(node.Indirect) fs_disk->read(node.Indirect, indirect.Data);
        else {
            /**- check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
            if(!check_allocation(&node, read, orig_offset, node.Indirect, false, indirect)) { 
                return write_ret(inumber, &node, read);
            }
            fs_disk->read(node.Indirect, indirect.Data);

            /**- initialise the indirect nodes */
            for(int i = 0; i < (int)POINTERS_PER_BLOCK; i++) {
                indirect.Pointers[i] = 0;
            }
        }

        /**- check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
        if(!check_allocation(&node, read, orig_offset, indirect.Pointers[indirect_node], true, indirect)) { 
            return write_ret(inumber, &node, read);
        }
        read_buffer(offset, &read, length, data, indirect.Pointers[indirect_node++]);

        /**- enough data has been read from data buffer */
        if(read == length) {
            fs_disk->write(node.Indirect, indirect.Data);
            return write_ret(inumber, &node, length);
        }
        /**- write into indirect nodes */
        else {
            for(int j = indirect_node; j < (int)POINTERS_PER_BLOCK; j++) {
                /**- check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
                if(!check_allocation(&node, read, orig_offset, indirect.Pointers[j], true, indirect)) { 
                    return write_ret(inumber, &node, read);
                }
                read_buffer(0, &read, length, data, indirect.Pointers[j]);

                /**- enough data has been read from data buffer */
                if(read == length) {
                    fs_disk->write(node.Indirect, indirect.Data);
                    return write_ret(inumber, &node, length);
                }
            }

            /**- space exhausted */
            fs_disk->write(node.Indirect, indirect.Data);
            return write_ret(inumber, &node, read);
        }
    }

    /**- error */   
    return -1;
}