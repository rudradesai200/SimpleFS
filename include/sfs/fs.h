// fs.h: File System

#pragma once

#include "sfs/disk.h"

#include <stdint.h>

class FileSystem {
public:
    const static uint32_t MAGIC_NUMBER	     = 0xf0f03410;
    const static uint32_t INODES_PER_BLOCK   = 128;
    const static uint32_t POINTERS_PER_INODE = 5;
    const static uint32_t POINTERS_PER_BLOCK = 1024;

private:
    struct SuperBlock {		    // Superblock structure
    	uint32_t MagicNumber;	// File system magic number
    	uint32_t Blocks;	    // Number of blocks in file system
    	uint32_t InodeBlocks;	// Number of blocks reserved for inodes
    	uint32_t Inodes;	    // Number of inodes in file system
    };

    struct Inode {
    	uint32_t Valid;		                    // Whether or not inode is valid
    	uint32_t Size;		                    // Size of file
    	uint32_t Direct[FileSystem::POINTERS_PER_INODE];    // Direct pointers
    	uint32_t Indirect;	                    // Indirect pointer
    };

    union Block {
    	struct SuperBlock   Super;			                            // Superblock
    	struct Inode	    Inodes[FileSystem::INODES_PER_BLOCK];	    // Inode block
    	uint32_t            Pointers[FileSystem::POINTERS_PER_BLOCK];   // Contains indexes of Direct Blocks. 0 if null.ck
    	char	            Data[Disk::BLOCK_SIZE];	                    // Data block
    };

    // Internal helper functions
    bool    load_inode(size_t inumber, Inode *node);

    ssize_t allocate_free_block();
    uint32_t allocate_block();

    // Internal member variables
    Disk* fs_disk; 
    bool* free_blocks;
    int* inode_counter;
    int num_free_blocks, num_inode_blocks;
    struct SuperBlock MetaData;
    

public:
    static void debug(Disk *disk);
    static bool format(Disk *disk);

    bool    mount(Disk *disk);

    ssize_t create();
    bool    remove(size_t inumber);
    ssize_t stat(size_t inumber);

    ssize_t read(size_t inumber, char *data, int length, size_t offset);
    ssize_t write(size_t inumber, char *data, int length, size_t offset);
};
