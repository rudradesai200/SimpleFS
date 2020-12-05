// fs.h: File System

#pragma once

#include "sfs/disk.h"
#include <cstring>
#include <stdint.h>

class FileSystem {
public:
    const static uint32_t MAGIC_NUMBER	     = 0xf0f03410;
    const static uint32_t INODES_PER_BLOCK   = 128;
    const static uint32_t POINTERS_PER_INODE = 5;
    const static uint32_t POINTERS_PER_BLOCK = 1024;
    const static uint32_t NAMESIZE           = 16;
    const static uint32_t ENTRIES_PER_DIR    = 14;
    const static uint32_t DIR_PER_BLOCK      = 4;

private:
    struct SuperBlock {		    // Superblock structure
    	uint32_t MagicNumber;	// File system magic number
    	uint32_t Blocks;	    // Number of blocks in file system
    	uint32_t InodeBlocks;	// Number of blocks reserved for inodes
        uint32_t DirBlocks;     // Number of blocks reserved for directories
    	uint32_t Inodes;	    // Number of inodes in file system
        uint32_t Protected;
        char PasswordHash[257];
    };

    struct Dirent {
        uint8_t type;           // 1 for file 0 for dir
        uint8_t valid;          
        uint32_t inum;          // inum for Inodes or offset for dir
        char Name[FileSystem::NAMESIZE];
    };

    // Note, offsets for dir are allocated in reverse
    struct Directory {
        uint64_t Valid;
        uint32_t Size;
        uint32_t inum;
        char Name[FileSystem::NAMESIZE];
        struct Dirent Table[FileSystem::ENTRIES_PER_DIR]; //Contains 1 for '.' and 1 for ".."
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
        struct Directory    Directories[FileSystem::DIR_PER_BLOCK];      // Directory blocks
    };

    // Internal helper functions
    bool    load_inode(size_t inumber, Inode *node);

    ssize_t allocate_free_block();
    uint32_t allocate_block();

    // Internal member variables
    Disk* fs_disk; 
    bool* free_blocks;
    int* inode_counter;
    uint32_t* dir_counter;
    int num_free_blocks, num_inode_blocks;
    struct SuperBlock MetaData;

    // Directories
    struct Directory curr_dir;
    // bool    add_dir_entry(uint32_t inum, uint32_t type, char name[]);
    // void    write_dir_back(struct Directory dir);

public:
    // Initializations
    static void debug(Disk *disk);
    static bool format(Disk *disk);
    bool    mount(Disk *disk);

    // Inodes
    ssize_t create();
    bool    remove(size_t inumber);
    ssize_t stat(size_t inumber);

    ssize_t read(size_t inumber, char *data, int length, size_t offset);
    ssize_t write(size_t inumber, char *data, int length, size_t offset);
    ssize_t write_ret(size_t inumber, Inode* node, int ret);

    // Security Functions
    bool    set_password();
    bool    change_password();
    bool    remove_password();

    // Directories and Files
    bool    touch(char name[]);
    bool    mkdir(char name[]);
    bool    rmdir(char name[]);
    bool    cd(char name[]);
    void    ls();
};

// NOTE: For now, Path's are not valid for creating files or directories 