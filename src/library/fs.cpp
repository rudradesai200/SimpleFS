// fs.cpp: File System

#include "sfs/fs.h"

#include <algorithm>

#include <assert.h>
#include <stdio.h>
#include <string.h>

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
            Inode curr = block.Inodes[j];
            
            if(curr.Valid) {
                printf("Inode %u:\n", ii);
                printf("    size: %u bytes\n", curr.Size);
                printf("    direct blocks: ");

                for(uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                    if(curr.Direct[k]) printf("%u ", curr.Direct[k]);
                }
                printf("\n");
            }

            ii++;
        }    
    }

    return;
}

// Format file system ----------------------------------------------------------

bool FileSystem::format(Disk *disk) {

    // if(disk->mounted()){return false;}

    // // Write superblock
    // Block block;
    // disk->read(0,block.Data);

    // block.Super.MagicNumber = FileSystem::MAGIC_NUMBER;
    // // block.Super.Blocks = disk->
    // block.Super.InodeBlocks = (uint32_t)(0.1 * block.Super.Blocks);
    // block.Super.Inodes = block.Super.InodeBlocks * (FileSystem::INODES_PER_BLOCK)

    // Clear all other blocks
    return true;
}

// Mount file system -----------------------------------------------------------

bool FileSystem::mount(Disk *disk) {
    // Read superblock

    // Set device and mount

    // Copy metadata

    // Allocate free block bitmap

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

// void FileSystem::initialize_free_blocks(Disk *disk) {
//     // reding the superblock
//     Block block;
//     disk->read(0, block.Data);

//     this->num_free_blocks = block.Super.Blocks;
//     this->free_blocks = (bool*)malloc(this->num_free_blocks * sizeof(bool));

//     for(int i = 0; i < this->num_free_blocks; i++) {
//         this->free_blocks[i] = false;
