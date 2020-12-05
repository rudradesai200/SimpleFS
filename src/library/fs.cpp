// fs.cpp: File System

#include "sfs/fs.h"

#include <algorithm>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

#define d1 (d2 | 0)
#define d2 0

using namespace std;


/*******************************************************
 * Debug - Done / Tests Passing
 * Format - Done / Tests Passing
 * Mount - Done / Tests Passing
 * Create - Done / Tests Passing
 * Remove - Done / Tests PENDING
 * Stat - Done / Tests Passing (ours is more efficient in terms of #reads)
 * Read - Done / Tests Passing
 * Write - TODO
 ********************************************************/

// Debug file system -----------------------------------------------------------

void FileSystem::debug(Disk *disk) {
    Block block;

    // read superblock
    disk->read(0, block.Data);

    printf("SuperBlock:\n");

    // verifying the integrity of the file system
    if(block.Super.MagicNumber == MAGIC_NUMBER) 
        printf("    magic number is valid\n");
    else {
        printf("    magic number is invalid\n");
        printf("    exiting...\n");
        return;
    }

    // printing the superblock
    printf("    %u blocks\n"         , block.Super.Blocks);
    printf("    %u inode blocks\n"   , block.Super.InodeBlocks);
    printf("    %u inodes\n"         , block.Super.Inodes);

    // reading the inode blocks
    uint32_t num_inode_blocks = block.Super.InodeBlocks;
    int ii = 0;

    for(uint32_t i = 1; i <= num_inode_blocks; i++) {
        disk->read(i, block.Data); // array of inodes

        for(uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
            
            if(block.Inodes[j].Valid) {
                printf("Inode %u:\n", ii);
                printf("    size: %u bytes\n", block.Inodes[j].Size);
                printf("    direct blocks:");

                for(uint32_t k = 0; k < POINTERS_PER_INODE; k++) {
                    if(block.Inodes[j].Direct[k]) printf(" %u", block.Inodes[j].Direct[k]);
                }
                printf("\n");

                if(block.Inodes[j].Indirect){
                    printf("    indirect block: %u\n    indirect data blocks:",block.Inodes[j].Indirect);
                    Block IndirectBlock;
                    disk->read(block.Inodes[j].Indirect,IndirectBlock.Data);
                    for(uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                        if(IndirectBlock.Pointers[k]) printf(" %u", IndirectBlock.Pointers[k]);
                    }
                    printf("\n");
                }
            }

            ii++;
        }    
    }

    return;
}

// Format file system ----------------------------------------------------------

bool FileSystem::format(Disk *disk) {

    if(disk->mounted())
        return false;

    // Write superblock
    Block block;

    block.Super.MagicNumber = FileSystem::MAGIC_NUMBER;
    block.Super.Blocks = (uint32_t)(disk->size());
    block.Super.InodeBlocks = (uint32_t)std::ceil((int(block.Super.Blocks) * 1.00)/10);
    block.Super.Inodes = block.Super.InodeBlocks * (FileSystem::INODES_PER_BLOCK);

    disk->write(0,block.Data);
    
    // Clear all other blocks
    for(uint32_t i = 1; i <= block.Super.InodeBlocks; i++){
        Block inodeblock;

        // Clear individual Inodes
        for(uint32_t j = 0; j < FileSystem::INODES_PER_BLOCK; j++){
            
            inodeblock.Inodes[j].Valid = false;
            inodeblock.Inodes[j].Size = 0;

            // Clear all Direct Pointers
            for(uint32_t k = 0; k < FileSystem::POINTERS_PER_INODE; k++)   
                inodeblock.Inodes[j].Direct[k] = 0;
    
            // Clear Indirect Pointer
            inodeblock.Inodes[j].Indirect = 0;
        }

        // Write back to the disk
        disk->write(i,inodeblock.Data);
    }

    for(uint32_t i = (block.Super.InodeBlocks) + 1; i < block.Super.Blocks; i++){
        Block DataBlock;
        memset(DataBlock.Data, 0, Disk::BLOCK_SIZE);
        disk->write(i, DataBlock.Data);
    }

    return true;
}

// Mount file system -----------------------------------------------------------

bool FileSystem::mount(Disk *disk) {
    if(disk->mounted())
        return false;

    // Read superblock
    Block block;
    disk->read(0,block.Data);
    if(block.Super.MagicNumber != MAGIC_NUMBER) return false;
    if(block.Super.InodeBlocks != std::ceil((block.Super.Blocks*1.00)/10)) return false;
    if(block.Super.Inodes != (block.Super.InodeBlocks * INODES_PER_BLOCK)) return false;

    // Set device and mount
    // Doubt : What is device?
    disk->mount();
    fs_disk = disk;

    // Copy metadata
    MetaData = block.Super;

    // Allocate free block bitmap 
    num_free_blocks = MetaData.Blocks;
    free_blocks = (bool*)malloc(num_free_blocks * sizeof(bool));
    memset(free_blocks,0,num_free_blocks);

    // Allocate inode_counter
    num_inode_blocks = MetaData.InodeBlocks;
    inode_counter = (int *)malloc(num_inode_blocks * sizeof(int));
    memset(inode_counter, 0, num_inode_blocks);   

    // Setting true for Super Block
    free_blocks[0] = true;

    // Read all the Inode Blocks
    for(uint32_t i = 1; i <= MetaData.InodeBlocks; i++) {
        disk->read(i, block.Data);

        // Read all the Inodes
        for(uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
            
            if(block.Inodes[j].Valid) {
                inode_counter[i-1]++;

                // Set the free_blocks for Inodes Block
                free_blocks[i] |= block.Inodes[j].Valid;

                // Set the free blocks for Direct Pointer
                for(uint32_t k = 0; k < POINTERS_PER_INODE; k++) {
                    if(block.Inodes[j].Direct[k]){
                        if(block.Inodes[j].Direct[k] < MetaData.Blocks)
                            free_blocks[block.Inodes[j].Direct[k]] = true;
                        else
                            return false;
                    }
                }

                // Set the free blocks for Indirect Pointer
                if(block.Inodes[j].Indirect){
                    if(block.Inodes[j].Indirect < MetaData.Blocks)
                        free_blocks[block.Inodes[j].Indirect] = true;
                    else
                        return false;
                }
            }
        }
    }

    return true;
}

// Create inode ----------------------------------------------------------------

ssize_t FileSystem::create() {
    Block block;
    fs_disk->read(0, block.Data);

    // Locate free inode in inode table    
    for(uint32_t i = 1; i <= MetaData.InodeBlocks; i++) {
        if(inode_counter[i-1] == INODES_PER_BLOCK) continue;

        fs_disk->read(i, block.Data);
        
        for(uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
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

// Load inode ------------------------------------------------------------------

bool FileSystem::load_inode(size_t inumber, Inode *node) {
    Block block;

    int i = inumber / INODES_PER_BLOCK;
    int j = inumber % INODES_PER_BLOCK;

    if(inode_counter[i]) {
        fs_disk->read(i+1, block.Data);
        if(block.Inodes[j].Valid) {
            *node = block.Inodes[j];
            return true;
        }
    }

    return false;
}

// Remove inode ----------------------------------------------------------------

bool FileSystem::remove(size_t inumber) {
    // Load inode information
    Inode *node = new Inode;

    if(load_inode(inumber, node)) {
        node->Valid = false;
        node->Size = 0;

        if(!(--inode_counter[inumber / INODES_PER_BLOCK])) {
            free_blocks[inumber / INODES_PER_BLOCK + 1] = false;
        }

        // Free direct blocks
        for(uint32_t i = 0; i < POINTERS_PER_INODE; i++) {
            free_blocks[node->Direct[i]] = false;
            node->Direct[i] = 0;
        }

        // Free indirect blocks
        if(node->Indirect) {
            Block indirect;
            fs_disk->read(node->Indirect, indirect.Data);
            free_blocks[node->Indirect] = false;
            node->Indirect = 0;

            for(uint32_t i = 0; i < POINTERS_PER_BLOCK; i++) {
                if(indirect.Pointers[i]) free_blocks[indirect.Pointers[i]] = false;
            }
        }

        Block block;
        fs_disk->read(inumber / INODES_PER_BLOCK + 1, block.Data);
        block.Inodes[inumber % INODES_PER_BLOCK] = *node;
        fs_disk->write(inumber / INODES_PER_BLOCK + 1, block.Data);

        return true;
    }
    
    return false;
}

// Inode stat ------------------------------------------------------------------

ssize_t FileSystem::stat(size_t inumber) {
    Inode* node = new Inode;

    if(load_inode(inumber, node)) {
        return node->Size;
    }

    return -1;
}

// Read from inode -------------------------------------------------------------

ssize_t FileSystem::read(size_t inumber, char *data, int length, size_t offset) {
    // IMPORTANT: start reading from index = offset
    int size_inode = stat(inumber);
    
    if((int)offset >= size_inode) {
        return 0;
    }
    else if(length + (int)offset > size_inode) {
        length = size_inode - offset;
    }

    Inode *node = new Inode;    
    char *ptr = data;     
    int to_read = length;

    if(load_inode(inumber, node)) {
        if(offset < POINTERS_PER_INODE * Disk::BLOCK_SIZE) {
            // offset within direct pointers
            uint32_t direct_node = offset / Disk::BLOCK_SIZE;
            offset %= Disk::BLOCK_SIZE;

            // check if the direct node is valid 
            if(node->Direct[direct_node]) {
                fs_disk->read(node->Direct[direct_node++], ptr);
                data += offset;
                ptr += Disk::BLOCK_SIZE;
                length -= (Disk::BLOCK_SIZE - offset);

                // read the direct blocks
                while(length > 0 && direct_node < POINTERS_PER_INODE && node->Direct[direct_node]) {
                    fs_disk->read(node->Direct[direct_node++], ptr);
                    ptr += Disk::BLOCK_SIZE;
                    length -= Disk::BLOCK_SIZE;
                }

                if(length <= 0) {
                    // enough data has been read
                    return to_read;
                }
                else {
                    // more data is to be read
                    if(direct_node == POINTERS_PER_INODE && node->Indirect) {
                        // if all the direct nodes have been read
                        Block indirect;
                        fs_disk->read(node->Indirect, indirect.Data);

                        // read the indirect nodes
                        for(uint32_t i = 0; i < POINTERS_PER_BLOCK; i++) {
                            if(indirect.Pointers[i] && length > 0) {
                                fs_disk->read(indirect.Pointers[i], ptr);
                                ptr += Disk::BLOCK_SIZE;
                                length -= Disk::BLOCK_SIZE;
                            }
                            else {
                                break;
                            }
                        }

                        if(length <= 0) {
                            // enough data has been read
                            return to_read;
                        }
                        else {
                            // data exhausted but length requested was higher
                            return (to_read - length);
                        }
                    }
                    else {
                        // data exhausted but length requested was higher
                        return (to_read - length);
                    }
                }
            }
            else {
                // no data in the inode
                return 0;
            }
        }
        else {
            if(node->Indirect) {
                // offset begins in the indirect block
                offset -= (POINTERS_PER_INODE * Disk::BLOCK_SIZE);
                uint32_t indirect_node = offset / Disk::BLOCK_SIZE;
                offset %= Disk::BLOCK_SIZE;

                Block indirect;
                fs_disk->read(node->Indirect, indirect.Data);

                if(indirect.Pointers[indirect_node] && length > 0) {
                    fs_disk->read(indirect.Pointers[indirect_node++], ptr);
                    data += offset;
                    ptr += Disk::BLOCK_SIZE;
                    length -= (Disk::BLOCK_SIZE - offset);
                }

                for(uint32_t i = indirect_node; i < POINTERS_PER_BLOCK; i++) {
                    if(indirect.Pointers[i] && length > 0) {
                        fs_disk->read(indirect.Pointers[i], ptr);
                        ptr += Disk::BLOCK_SIZE;
                        length -= Disk::BLOCK_SIZE;
                    }
                    else {
                        break;
                    }
                }

                if(length <= 0) {
                    // enough data has been read
                    return to_read;
                }
                else {
                    // data exhausted but length requested was higher
                    return (to_read - length);
                }
            }
            else {
                return 0;
            }
        }
    }
    
    // Inode invalid
    return -1;
}

// Allocates a block in the file system ----------------------------------------

uint32_t FileSystem::allocate_block() {
    for(int i = num_inode_blocks + 1; i < num_free_blocks; i++) {
        if(!free_blocks[i]) {
            free_blocks[i] = true;
            return (uint32_t)i;
        }
    }

    cout << "Disk is full; no free blocks available" << endl;
    return 0;
}

// Write to inode --------------------------------------------------------------

ssize_t FileSystem::write(size_t inumber, char *data, int length, size_t offset) {    
    Inode *node = new Inode;
    int read = 0;

    // insufficient size
    if(length + offset > (POINTERS_PER_BLOCK + POINTERS_PER_INODE) * Disk::BLOCK_SIZE) {
        cout << "size of file exceeding the maximum size of inode" << endl;
        return -1;
    }

    if(!load_inode(inumber, node)) {
        // allocate inode
        cout << "inode invalid\nallocating inode" << endl;
        int i = inumber / INODES_PER_BLOCK;
        int j = inumber % INODES_PER_BLOCK;

        inode_counter[i]++;
        Block block;
        fs_disk->read(i+1, block.Data);
        *node = block.Inodes[j];
        block.Inodes[j].Valid = true;
        block.Inodes[j].Size = 0;
        block.Inodes[j].Indirect = 0;
        for(uint32_t ii = 0; ii < POINTERS_PER_BLOCK; ii++) {
            block.Inodes[j].Direct[ii] = 0;
        }
        free_blocks[i+1] = true;
        fs_disk->write(i+1, block.Data);
    }

    // inode is valid
    if(offset < POINTERS_PER_INODE * Disk::BLOCK_SIZE) {
        cout << "offset is within direct pointers" << endl;
        // offset is within direct pointers
        int direct_node = offset / Disk::BLOCK_SIZE;
        offset %= Disk::BLOCK_SIZE;
        if(!node->Direct[direct_node]) node->Direct[direct_node] = allocate_block();
        char* ptr = (char *)malloc(Disk::BLOCK_SIZE * sizeof(char));
        for(int i = offset; i < (int)Disk::BLOCK_SIZE && read < length; i++) {
            ptr[i] = data[read++];
        }
        fs_disk->write(node->Direct[direct_node++], ptr);
        cout << "read " << read << " bytes" << endl;

        if(read == length) return length;
        else {
            // store in direct pointers till either one of the two things happen:
            // 1. all the data is stored in the direct pointers
            // 2. the data is stored in indirect pointers
            cout << "entering direct nodes" << endl;
            for(int i = direct_node; i < (int)POINTERS_PER_INODE; i++) {
                if(!node->Direct[direct_node]) node->Direct[direct_node] = allocate_block();
                char* ptr = (char *)malloc(Disk::BLOCK_SIZE * sizeof(char));
                for(int i = 0; i < (int)Disk::BLOCK_SIZE && read < length; i++) {
                    ptr[i] = data[read++];
                }
                fs_disk->write(node->Direct[direct_node++], ptr);
                cout << "read in direct: " << read << " bytes" << endl;
                if(read == length) return length;
            }
            cout << "exiting direct nodes" << endl;

            Block indirect;
            if(!node->Indirect) node->Indirect = allocate_block();
            fs_disk->read(node->Indirect, indirect.Data);
            cout << "entering indirect nodes" << endl;
            for(int j = 0; j < (int)POINTERS_PER_BLOCK; j++) {
                if(!indirect.Pointers[j])  indirect.Pointers[j] = allocate_block();
                char* ptr = (char *)malloc(Disk::BLOCK_SIZE * sizeof(char));
                for(int i = 0; i < (int)Disk::BLOCK_SIZE && read < length; i++) {
                    ptr[i] = data[read++];
                }
                fs_disk->write(indirect.Pointers[j], ptr);
                cout << "read in indirect " << read << " bytes" << endl;
                if(read == length) return length;
            }
            cout << "exiting indirect nodes" << endl;
        }
    }
    else {
        // offset is in indirect block
        offset -= Disk::BLOCK_SIZE * POINTERS_PER_BLOCK;
        cout << "OFFSET in indirect range" << endl;
        return -1;
    }
    
    return -1;
}