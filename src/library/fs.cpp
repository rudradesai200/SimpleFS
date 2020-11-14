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


/*******************************************************
 * Debug - Done / Tests Passing
 * Format - Done / Tests Passing
 * Mount - Done / Tests Passing
 * 
 ********************************************************/


// Debug file system -----------------------------------------------------------

void FileSystem::debug(Disk *disk) {
    Block block;

    // Read Superblock
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

    // Reading the Superblock
    printf("    %u blocks\n"         , block.Super.Blocks);
    printf("    %u inode blocks\n"   , block.Super.InodeBlocks);
    printf("    %u inodes\n"         , block.Super.Inodes);

    // Reading the Inode blocks
    uint32_t num_inode_blocks = block.Super.InodeBlocks;
    int ii = 0;

    for(uint32_t i = 1; i <= num_inode_blocks; i++) {
        disk->read(i, block.Data); // array of 128 inodes

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

    if(disk->mounted()){return false;}

    // Write superblock
    Block block;

    block.Super.MagicNumber = FileSystem::MAGIC_NUMBER;
    block.Super.Blocks = (uint32_t)(disk->size());
    block.Super.InodeBlocks = (uint32_t)std::ceil((int(block.Super.Blocks) * 1.00)/10);
    block.Super.Inodes = block.Super.InodeBlocks * (FileSystem::INODES_PER_BLOCK);

    disk->write(0,block.Data);
    
    // Clear all other blocks
    for(uint32_t i=1; i<=(block.Super.InodeBlocks); i++){
        Block inodeblock;

        // Clear individual Inodes
        for(uint32_t j=0; j<FileSystem::INODES_PER_BLOCK; j++){
            
            inodeblock.Inodes[j].Valid = false;
            inodeblock.Inodes[j].Size = 0;
            // Clear all Direct Pointers
            for(uint32_t k=0; k<FileSystem::POINTERS_PER_INODE; k++)   
                inodeblock.Inodes[j].Direct[k] = 0;
    
            // Clear Indirect Pointer
            inodeblock.Inodes[j].Indirect = 0;
        }

        // Write back to the disk
        disk->write(i,inodeblock.Data);
        
    }

    for(uint32_t i=(block.Super.InodeBlocks)+1; i<(block.Super.Blocks); i++){
        Block DataBlock;
        memset(DataBlock.Data,0,Disk::BLOCK_SIZE);
        disk->write(i,DataBlock.Data);
    }

    return true;
}

// Mount file system -----------------------------------------------------------

bool FileSystem::mount(Disk *disk) {
    if(disk->mounted()){return false;}

    // Read superblock
    Block block;
    disk->read(0,block.Data);
    if(block.Super.MagicNumber != MAGIC_NUMBER){return false;}
    if(block.Super.InodeBlocks != std::ceil((block.Super.Blocks*1.00)/10)){return false;}
    if(block.Super.Inodes != (block.Super.InodeBlocks * INODES_PER_BLOCK)){return false;}

    // Set device and mount
    // Doubt :: What is device?
    disk->mount();

    // Copy metadata
    MetaData = block.Super;

    // Allocate free block bitmap
    num_free_blocks = MetaData.Blocks;
    free_blocks = (bool*)malloc(num_free_blocks * sizeof(bool));
    memset(free_blocks,0,num_free_blocks);

    // Setting true for Super Block
    free_blocks[0] = true;

    // Read all the Inode Blocks
    for(uint32_t i = 1; i <= MetaData.InodeBlocks; i++) {
        disk->read(i, block.Data);

        // Read all the Inodes
        for(uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
            
            if(block.Inodes[j].Valid) {
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

                // Set the free blocks for InDirect Pointer
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
    // Locate free inode in inode table

    // Record inode if found
    return 0;
}

// Remove inode ----------------------------------------------------------------

bool FileSystem::remove(size_t inumber) {
    // Load inode information

    // Free direct blocks

    // Free indirect blocks

    // Clear inode in inode table
    return true;
}

// Inode stat ------------------------------------------------------------------

ssize_t FileSystem::stat(size_t inumber) {
    // Load inode information
    return 0;
}

// Read from inode -------------------------------------------------------------

ssize_t FileSystem::read(size_t inumber, char *data, size_t length, size_t offset) {
    // Load inode information

    // Adjust length

    // Read block and copy to data
    return 0;
}

// Write to inode --------------------------------------------------------------

ssize_t FileSystem::write(size_t inumber, char *data, size_t length, size_t offset) {
    // Load inode
    
    // Write block and copy to data
    return 0;
}

// Initialise free block bitmap ------------------------------------------------

void FileSystem::initialize_free_blocks(Disk *disk) {
    // reding the superblock
    Block block;
    disk->read(0, block.Data);

    this->num_free_blocks = block.Super.Blocks;
    this->free_blocks = (bool*)malloc(this->num_free_blocks * sizeof(bool));

    // Starting Index of datablocks 
    int datablocks_start = block.Super.InodeBlocks + 1;
    
    // Setting up free_blocks bitmap
    for(int i = datablocks_start; i < this->num_free_blocks; i++) 
        this->free_blocks[i] = false;
    
    // Superblock
    this->free_blocks[0] = true;

    // Inode blocks
    for(int i = 1; i<datablocks_start; i++)
        this->free_blocks[i] = true;
}