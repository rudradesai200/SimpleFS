/** fs.h: File System */

#pragma once

#include "sfs/disk.h"

#include <stdint.h>
#include <vector>

using namespace std;

/**
 * @brief FileSytem class
 * Implements File System abstraction that enables access to disk blocks.
 * Used by sfssh (custom shell) to provide access to the end-user.
 */
class FileSystem {
public:
    const static uint32_t MAGIC_NUMBER	     = 0xf0f03410;              /**   used to verify integrity of the FS   @hideinitializer*/
    const static uint32_t INODES_PER_BLOCK   = 128;                     /**   number of inodes per block   @hideinitializer*/
    const static uint32_t POINTERS_PER_INODE = 5;                       /**   number of direct pointers per inode   @hideinitializer*/
    const static uint32_t POINTERS_PER_BLOCK = 1024;                    /**   number of pointers to raw data blocks per block  @hideinitializer*/

private:
    /** 
     * @brief Superblock structure.
     * It is the first block in the disk.
     * Contains metadata of the disk.
    */
    struct SuperBlock {		                                            
    	uint32_t MagicNumber;	                                        /**  File system magic number */
    	uint32_t Blocks;	                                            /** Number of blocks in file system */
    	uint32_t InodeBlocks;	                                        /** Number of blocks reserved for inodes */
    	uint32_t Inodes;	                                            /** Number of inodes in file system */
    };

    /**
     * @brief Inode Structure
     * Corresponds to a file stored on the disk.
     * Contains the list of raw data blocks used to store the data.
     * Stores the size of the file.
    */
    struct Inode {
    	uint32_t Valid;		                                            /** Whether or not inode is valid */
    	uint32_t Size;		                                            /** Size of file */
    	uint32_t Direct[FileSystem::POINTERS_PER_INODE];                /** Direct pointers */
    	uint32_t Indirect;	                                            /** Indirect pointer */
    };

    /**
     * @brief Block Union
     * Corresponds to one block of disk of size Disk::BLOCKSIZE.
     * Can be used as a Superblock, Inode, Pointers block, or raw Data block.
    */
    union Block {
    	struct SuperBlock   Super;			                            /** Superblock */
    	struct Inode	    Inodes[FileSystem::INODES_PER_BLOCK];	    /** Inode block */
    	uint32_t            Pointers[FileSystem::POINTERS_PER_BLOCK];   /** Contains indices of Direct Blocks. 0 if null.ck */
    	char	            Data[Disk::BLOCK_SIZE];	                    /** Data block */
    };

    // Internal member variables
    Disk* fs_disk;                                                      /** pointer to the disk */
    vector<bool> free_blocks;                                           /** free bit map */
    vector<int> inode_counter;                                          /** counts the inode per inode block */
    int num_free_blocks;                                                /** number of blocks on the disk */
    int  num_inode_blocks;                                              /** number of inode blocks on the disk; usually 0.1 * #blocks */
    struct SuperBlock MetaData;                                         /** metadata of the disk retrieved from SuperBlock */
    bool mounted;                                                       /** boolean variable to check if the disk has been mounted */

    // Internal helper functions

    /**
     * @brief 
     * @param inumber 
     * @param node 
     * @return  
    */
    bool        load_inode(size_t inumber, Inode *node);

    /**
     * @brief 
     * @param inumber 
     * @param node 
     * @return  
    */
    ssize_t     allocate_free_block();

    /**
     * @brief 
     * @param inumber 
     * @param node 
     * @return  
    */
    void        read_helper(uint32_t blocknum, int offset, int *length, char **data, char **ptr);
    
    /**
     * @brief 
     * @param inumber 
     * @param node 
     * @return  
    */
    ssize_t     write_ret(size_t inumber, Inode* node, int ret);
    
    /**
     * @brief 
     * @param inumber 
     * @param node 
     * @return  
    */
    void        read_buffer(int offset, int *read, int length, char *data, uint32_t blocknum);
    
    /**
     * @brief 
     * @param inumber 
     * @param node 
     * @return  
    */
    bool        check_allocation(Inode *node, int read, int orig_offset, uint32_t &blocknum, bool write_indirect, Block indirect);
    
    /**
     * @brief 
     * @param inumber 
     * @param node 
     * @return  
    */
    uint32_t    allocate_block();

public:
    static void debug(Disk *disk);
    static bool format(Disk *disk);
    bool        mount(Disk *disk);
    ssize_t     create();
    bool        remove(size_t inumber);
    ssize_t     stat(size_t inumber);
    ssize_t     read(size_t inumber, char *data, int length, size_t offset);
    ssize_t     write(size_t inumber, char *data, int length, size_t offset);
};
