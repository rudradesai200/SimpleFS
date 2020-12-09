/**
 * @file fs.h
 * @author Rudra Desai (rudrad200@gmail.com) & Kairav Shah (shahkairav1702@gmail.com)
 * @brief Interface for the FileSystem layer
 * @date 2020-12-09
 * 
 */

/*! \mainpage CS3500 Course Project
 * \image html SimpleFS.png
 * 
 * \section Introduction
 * The file system is a layer of abstraction between the storage medium and the operating system.
 * In the absence of a file system, it is tough to retrieve and manage data stored in the storage media.
 * File systems provide an interface to access and control the data. 
 * It is implemented by separating data into chunks of predetermined size and labeling them for better management and operability. 
 * There are several types of file systems available out there. Some major examples are as follows: Disk-Based, Network-Based, and Virtual based.
 * 
 * Here, we provide a simple implementation of a disk-based file system. A custom shell is also provided to access the file system.
 * The shell (sfs) and the disk emulator were pre-implemented by the University of Notre Dame. Our task was to build upon the given tools and implement the File System layer and, in doing so, bridge the gap between shell and disk emulator.
 * 
 * \section Features
 *-# <b> Ability to view and modify data stored in the disk.</b>
 *  - Disks can be accessed only in blocks of size 4KB (can be changed in the disk implementation).
 *  - A file system is an abstraction layer that allows the user to modify the disk without worrying about the block size.
 *  - The file system weighs the tradeoff between performance and storage to make the user’s life simpler.
 *-# <b> Format the disk and mount a new file system.</b>
 *  - Usually, a signature is used to verify the file system’s integrity every time the disk is mounted. SimpleFS uses one such “Magic Number” to ensure that a compatible file system is being mounted. This magic number is loaded upon formatting the disk. 
 *  - Our implementation of the format() function hard-resets the disk, i.e., it does not only delete the inodes but sets each byte to zero. 
 *  - The number of blocks for metadata and storage purposes is dependent on the type and implementation of the file system used. 
 *  - Our implementation uses one single block named “superblock” to store metadata, 10% blocks to store inodes, 1% blocks to store directories and files. The remaining blocks are used to store raw data.
 *-# <b> Create, Read and Write inodes.  </b>
 *  - Inodes are data structures that are used to store files. The data to be stored is saved across an array of direct blocks and indirect blocks. Such distinction is made so that the access time for small files is significantly reduced. 
 *  - The indirect block is simply an array of pointers to raw data blocks. This implementation has a major pitfall: it limits the maximum size of files. A simple workaround is to make the last pointer of the indirect block point to another indirect block. We have not yet implemented this scheme in order to keep the file system simple.
 *  - The implementation provides functions to create, read, and write inodes. Support for directly creating files and directories has been added as an extra feature.
 *  - The inodes store extra data in its member variables to enable features such as permissions. Though we have not implemented this feature, it can be easily achieved. 
 *
 * \section Additional-Features
 *-# <b> Added support for directly creating files and multi-level directories. </b>
 * - Used additional data structures to maintain directories.
 * - Files and directories are associated with a name for user’s ease and can be accessed from the root directory.
 * - Nested files and directories are stored as entries into the parent directory’s table.
 * - The maximum number of directories per block and entries per directory is an implementation choice. It can be changed by modifying the corresponding value in the code.
 * 
 *-# <b> Implemented 10 Linux shell commands.</b>
 * - ls - list the contents of the directory
 * - cd - change directory
 * - mkdir - create a new directory
 * - rmdir -  remove a directory
 * - stat - display information of all directories
 * - touch - create an empty file
 * - copyin - import a file into the file system
 * - copyout - export a file from the file system 
 * - rm - remove a file or directory
 * - Three password protection commands - change, set, and remove
 * 
 *-# <b> Added Security layer to encrypt and decrypt the disk.</b>
 * - Used a library to make use of SHA256 encryption.
 * - The user password is hashed using this function to prevent password leaks, if any.
 * - The hashed password is stored right in the superblock as most of the space in this block is left unutilised.
 * - Upon mounting the disk, the user is prompted to enter the password. Only after logging in can the user access the disk and make use of the user functions.
 *
 * \section Future-Aspects
 *-# <b> Extend support to devices </b>
 * - A device descriptor is a number used by the operating system to identify the device uniquely. An array of device descriptors is used to support multiple devices.
 * - Support for devices can be added by mapping these device descriptors to the device drivers.
 * - The standard read/write functions are overridden by the device driver’s corresponding functions. This ensures a consistent universal interface for  devices as well as files.
 * - The only difference is an additional field that is needed to store the device descriptor.
 * - Generally, the file systems are equipped with caching and buffer layers for files. Thus, the universal interface can ensure the same crash recovery for devices too.
 *
 *-# <b> Extend support to networks </b>
 * - Similar to devices, support for networks can be extended by mapping network descriptors to device drivers.
 * - A significant benefit of handling networks and devices as regular files is that the file system’s implementation remains universal and minimal.
 * - An end-user need not worry about the devices/networks as a separate entity. The OS and file system provides a consistent interface.  
 * - NIC - Network Interface Controller - is used to handle networks. The OS interacts with its device driver to transmit information across the network. These device drivers can be used by the file system to support networks.
 *
 *-# <b> Add a caching layer for faster access time. </b>
 * - A caching layer ensures faster access time by making use of “locality of reference.”
 * - To make use of the cache layer, read/write accesses are redirected to request the caching layer and access the main disk only if the requested data is absent.
 * - If the requested data is not present in the cache layer (known as “cache miss”), it is retrieved from the disk and stored in the cache for future use. Cache replacement algorithms such as LRU (Least Recently Used) can be used to ensure efficiency. 
 * - The “writes” to the disk are not immediately serviced - these changes are made in the cache layer and later stored to the disk only when absolutely necessary. This phenomenon is referred to as “cache hit”. 
 * - This feature can significantly reduce the number of read/write calls to the disk. This has immense ramifications as secondary storage accesses are costly, and any improvement at this level affects the entire pipeline.
 *
 *-# <b> Add a logging layer for crash recovery. </b>
 * - A system crash (or any interrupt for that matter) during a disk operation may leave the disk in an inconsistent state. It is imperative to implement crash recovery algorithms that can restore the disk to its original state.
 * - To implement this functionality, all system calls that request access to disk are maintained in log files. Once the system call has logged all its requests, a special commit record is maintained in the disk, indicating that the log represents a complete operation.
 * - The system call is now allowed to perform its disk operations. Only if the disk operations are executed successfully, the log file is deleted. In case of a crash, this log file is used to rollback the changes and restore the disk to a working state.
 * - This ensures that the operations are either completed in their entirety or no single operation is performed.
 *
 *-# <b> Add support for multi-processor computers. </b>
 * - The current implementation is dependent on several shared variables and objects. This prohibits the computer from making use of its efficient multiprocessing power because of issues such as race conditions.
 * - This issue can be resolved using tools such as mutexes, semaphores, or spinlocks.
 * - Spin-locks are an excellent choice when the critical section is small. In contrast, a more extensive critical section may require mutexes.
 * - Synchronized code ensures the correctness of the code even when executed on a multiprocessor system. 
 *
 *-# <b> Encryption of disk. </b> 
 * - The current password protection implementation is minimal as it does not prevent the adversary from creating their file system to access the data stored on the disk without ever entering the password. 
 * - Hence, to provide a much more secure system, the entire disk can be encrypted using advanced encryption algorithms.
 *
 * \section Results
 * - Passed all the tests provided by the University of Notre Dame (run “make test”).
 * - Added support for password protection.
 * - Added support for directories and files.
 * \image html Success.png
 * \section Refrences
 * - https://www3.nd.edu/~pbui/teaching/cse.30341.fa17/project06.html - Notre Dame University for providing the project and the boilerplate code
 * - https://drive.google.com/file/d/1oL5cUGzU7IchOZO_Jx7R9j8HqGxJ-wDX/view - File System lecture slides by Dr. Chester Rebeiro
 * - https://en.wikipedia.org/wiki/File_system - Wikipedia entry for File Systems
 * 
 * \section Contributions
 * - Kairav Shah: implemented the core functions: debug, format, mount, create, remove, write, read.
 * - Rudra Desai: added extra functionalities to the file system: password protection, several Linux commands, support for directories and files.
 *
*/

#pragma once

#include "sfs/disk.h"
#include <cstring>
#include <vector>
#include <stdint.h>
#include <vector>

using namespace std;

/**
 * @brief FileSytem Class.
 * Contains fs layer to access and store disk blocks.
 * Used by sfssh (shell) to provide access to the end-user.
 */
class FileSystem {
public:
    const static uint32_t MAGIC_NUMBER	     = 0xf0f03410;      //    Magic number helps in checking Validity of the FileSystem on disk   @hideinitializer
    const static uint32_t INODES_PER_BLOCK   = 128;             //    Number of Inodes which can be contained in a block of (4kb)   @hideinitializer
    const static uint32_t POINTERS_PER_INODE = 5;               //    Number of Direct block pointers in an Inode Block   @hideinitializer
    const static uint32_t POINTERS_PER_BLOCK = 1024;            //    Number of block pointers in one Indirect pointer  @hideinitializer
    const static uint32_t NAMESIZE           = 16;              //    Max Name size for files/directories   @hideinitializer
    const static uint32_t ENTRIES_PER_DIR    = 7;              //    Number of Files/Directory entries within a Directory   @hideinitializer
    const static uint32_t DIR_PER_BLOCK      = 8;               //    Number of Directories per 4KB block   @hideinitializer

private:
    /** 
     * @brief SuperBlock structure.
     * It is the first block in any disk.
     * It's main function is to help validating the disk.
     * Contains metadata of the disk.
    */
    struct SuperBlock {		    
    	uint32_t MagicNumber;	/**  File system magic number @hideinitializer*/
    	uint32_t Blocks;	    /**  Number of blocks in file system @hideinitializer*/
    	uint32_t InodeBlocks;	/**  Number of blocks reserved for inodes @hideinitializer*/
        uint32_t DirBlocks;     /**  Number of blocks reserved for directories @hideinitializer*/
    	uint32_t Inodes;	    /**  Number of inodes in file system @hideinitializer*/
        uint32_t Protected;     /**  Field to check if the disk is password protected @hideinitializer*/
        char PasswordHash[257]; /**  Password hash which is used to facilitate password checking @hideinitializer*/
    };

    /**
     * @brief Directory Entry.
     * Contains necessary fields to locate the file and directory
     * Consumes 64 KB per object. Used to store information about a directory
     */
    struct Dirent {
        uint8_t type;           /**  type = 1 for file, type = 0 for directory @hideinitializer*/
        uint8_t valid;          /**  valid bit to check if the entry is valid @hideinitializer*/
        uint32_t inum;          /**  inum for Inodes or offset for dir @hideinitializer*/
        char Name[NAMESIZE];    /** File/Directory Name @hideinitializer*/
    };

    /**
     * @brief Directory Structure.
     * Contains a table of directory entries for storing hierarchy.
     * Also contains fields for Size and Valid bits.
     * Name is matched against the one in Directory entry.
     * Also, it is allocated from end for effectively using disk space.
     */
    struct Directory {
        uint16_t Valid;                 /** Valid bit for validation @hideinitializer*/
        uint32_t inum;                  /** inum = block_num * DIR_PER_BLOCK + offset @hideinitializer*/
        char Name[NAMESIZE];            /** Directory Name @hideinitializer*/
        Dirent Table[ENTRIES_PER_DIR];  /** Each Table by default contains 2 entries, "." and ".." @hideinitializer*/
    };

    /**
     * @brief Inode Structure
     * Corresponds to a file stored on the disk.
     * Contains the list of raw data blocks used to store the data.
     * Stores the size of the file.
    */
    struct Inode {
    	uint32_t Valid;		                                            /** Whether or not inode is valid @hideinitializer*/
    	uint32_t Size;		                                            /** Size of file @hideinitializer*/
    	uint32_t Direct[FileSystem::POINTERS_PER_INODE];                /** Direct pointers @hideinitializer*/
    	uint32_t Indirect;	                                            /** Indirect pointer @hideinitializer*/
    };

    /**
     * @brief Block Union
     * Corresponds to one block of disk of size Disk::BLOCKSIZE.
     * Can be used as a SuperBlock, Inode, Pointers block, or raw Data block.
    */
    union Block {
    	struct SuperBlock   Super;			                            /**  SuperBlock @hideinitializer*/
    	struct Inode	    Inodes[FileSystem::INODES_PER_BLOCK];	    /**  Inode block @hideinitializer*/
    	uint32_t            Pointers[FileSystem::POINTERS_PER_BLOCK];   /**  Contains indexes of Direct Blocks. 0 if null.ck @hideinitializer*/
    	char	            Data[Disk::BLOCK_SIZE];	                    /**  Data block @hideinitializer*/
        struct Directory    Directories[FileSystem::DIR_PER_BLOCK];      /**  Directory blocks @hideinitializer*/
    };

    // Internal member variables
    Disk* fs_disk;                      /**  Stores disk pointer after successful mounting */
    vector<bool> free_blocks;           /**  Stores whether a block is free or not */
    vector<int> inode_counter;          /**  Stores the number of Inode contained in an Inode Block */
    vector<uint32_t> dir_counter;       /**  Stores the number of Directory contianed in a Directory Block */
    struct SuperBlock MetaData;         //  Caches the SuperBlock to save a disk-read @hideinitializer
    bool mounted;                       //  Boolean to check if the disk is mounted and saved @hideinitializer

    // Layer 1 Core Functions
    /**
     * @brief creates a new inode
     * @return the inumber of the newly created inode 
    */
    ssize_t     create();
    
    /**
     * @brief removes the inode
     * @param inumber index into the inode table of the inode to be removed
     * @return true if the remove operation was successful; false otherwise
    */
    bool        remove(size_t inumber);
    
    /**
     * @brief check size of an inode
     * @param inumber index into the inode table of inode whose size is to be determined
     * @return size of the inode; -1 if the inode is invalid
    */
    ssize_t     stat(size_t inumber);
    
    /**
     * @brief read from disk
     * @param inumber index into the inode table of the corresponding inode
     * @param data data buffer
     * @param length bytes to be read from disk
     * @param offset start point of the read operation
     * @return bytes read from disk; -1 in case of an error
    */
    ssize_t     read(size_t inumber, char *data, int length, size_t offset);
    
    /**
     * @brief write to the disk
     * @param inumber index into the inode table of the corresponding inode
     * @param data data buffer
     * @param length bytes to be written to disk
     * @param offset start point of the write operation
     * @return bytes written to disk; -1 in case of an error
    */
    ssize_t     write(size_t inumber, char *data, int length, size_t offset);

    //  Helper functions for Layer 1
    /**
     * @brief loads inode corresponding to inumber into node
     * @param inumber index into inode table
     * @param node pointer to inode
     * @return boolean value indicative of success of the load operation
    */
    bool        load_inode(size_t inumber, Inode *node);

    /**
     * @brief allocate the first free block from the disk
     * @return returns the blocknum of the disk; if the disk is full, returns 0
    */
    ssize_t     allocate_free_block();

    /**
     * @brief reads the block from disk and changes the pointers accordingly
     * @param blocknum index into the free block bitmap
     * @param offset start reading from index = offset
     * @param length number of bytes to be read
     * @param data data buffer
     * @param ptr buffer to store the read data
     * @return void function; returns nothing
    */
    void        read_helper(uint32_t blocknum, int offset, int *length, char **data, char **ptr);
    
    /**
     * @brief writes the node into the corresponding inode block
     * @param inumber index into inode table
     * @param node the inode to be written back to disk
     * @param ret the value that is returned by the function
     * @return returns the parameter ret
    */
    ssize_t     write_ret(size_t inumber, Inode* node, int ret);
    
    /**
     * @brief reads from buffer and writes to a block in the disk 
     * @param offset starts writing at index = offset
     * @param read bytes read from buffer so far
     * @param length bytes to be written to the disk
     * @param data data buffer
     * @param blocknum index of block in free block bitmap
     * @return  void function; returns nothing
    */
    void        read_buffer(int offset, int *read, int length, char *data, uint32_t blocknum);
    
    /**
     * @brief allocates a block if required; if no block is available in the disk; returns false
     * @param node used to set the size of inode in case allocation fails
     * @param read bytes read from buffer so far
     * @param orig_offset offset passed to write() function call
     * @param blocknum index of block in the free block bitmap
     * @param write_indirect true if the block is an indirect node
     * @param indirect the indirect node if required
     * @return true if allocation is successful; false otherwise
    */
    bool        check_allocation(Inode *node, int read, int orig_offset, uint32_t &blocknum, bool write_indirect, Block indirect);
    
    /**
     * @brief allocates the first free block from free block bitmap
     * @return block number of the block allocated; 0 if no block is available
    */
    uint32_t    allocate_block();

    
    /**  Caches curr dir to save a disk-read */
    Directory curr_dir;
    
    /**
     * @brief Adds directory entry to the cached curr_dir. dir_write_back should be done by the caller.
     * @param dir   Directory in which entry is to be added
     * @param inum  inum of the File/Directory
     * @param type  type = 1 for file , type = 0 for Directory
     * @param name  Name of the file/Directory
     * @return Directory with added entry or with valid bit set to 0 incase of error 
     * 
     */
    Directory add_dir_entry(Directory dir, uint32_t inum, uint32_t type, char name[]);

    /**
     * @brief Writes the Directory back to the disk. 
     *  
     * @param dir Valid Directory to be written back
     */
    void      write_dir_back(struct Directory dir);

    /**
     * @brief Finds a valid entry with the same name.
     * 
     * @param dir Lookup directory
     * @param name File/Directory Name
     * @return offset into the curr_dir table. -1 incase of error
     */
    int       dir_lookup(Directory dir, char name[]);

    /**
     * @brief Reads Directory from Dirent offset in curr_dir.
     * 
     * @param offset offset of curr_dir.Table which needs to be read.
     * @return Directory. Returns Directory with valid bit=0 incase of error.
     */
    Directory read_dir_from_offset(uint32_t offset);

    /**
     * @brief Helper function to remove directory from parent directory
     * 
     * @param parent Directory from which the other directory is to be removed.
     * @param name Name of the directory to be removed
     * @return Directory. Returns Directory with valid bit=0 incase of error.
     */
    Directory      rmdir_helper(Directory parent, char name[]);

    /**
     * @brief Helper function to remove file/directory from parent directory
     * 
     * @param parent Parent Directory which contains name 
     * @param name File/Directory to be removed
     * @return Directory. Returns Directory with valid bit=0 incase of error.
     */
    Directory      rm_helper(Directory parent, char name[]);

public:

    /**
     * @brief prints the basic outline of the disk
     * @param disk the disk to be debugged
     * @return void function; returns nothing
    */
    static void debug(Disk *disk);
    
    /**
     * @brief formats the entire disk
     * @param disk the disk to be formatted
     * @return true if the formatting was successful; false otherwise
    */
    static bool format(Disk *disk);


    /**
     * @brief mounts the file system onto the disk
     * @param disk the disk to be mounted
     * @return true if the mount operation was successful; false otherwise
    */
    bool        mount(Disk *disk);

    
    //  Security Functions

    /**
     * @brief Adds password to the mounted disk (fs_disk).
     * Gives the control to change_password if disk is pasword protected
     * 
     * @return true if New password is set
     * @return false incase of error
     */
    bool    set_password();

    /**
     * @brief Changes password in the mounted disk (fs_disk).
     * Checks the current password and gives control to set_password
     * 
     * @return true if password changed
     * @return false incase of error
     */
    bool    change_password();

    /**
     * @brief Removes password from the mounted disk (fs_disk).
     * 
     * @return true if password removed
     * @return false incase of error
     */
    bool    remove_password();

    //  Directories and Files

    /**
     * @brief Creates a file with size 0.
     * Main function is to allocate an Inode and 
     * add Dirent to the curr_dir.
     * Error if file already exists.
     * 
     * @param name Name of the file to be added
     * @return true if successful
     * @return false incase of error
     */
    bool    touch(char name[]);

    /**
     * @brief Creates an empty directory with the given name
     * Adds 2 Dirent , '.' and '..' in it's table.
     * 
     * @param name Name of the directory to be added.
     * @return true if successful
     * @return false incase of errors
     */
    bool    mkdir(char name[]);

    /**
     * @brief Removes the directory with given name.
     * Also removes all the Dirent in it's table.
     * 
     * @param name Name of the Directory to be deleted
     * @return true if successful
     * @return false incase of errors.
     */
    bool    rmdir(char name[]);

    /**
     * @brief Change curr_dir to the given Directory name.
     * 
     * @param name Directory name to be used.
     * @return true if successful
     * @return false incase of error
     */
    bool    cd(char name[]);

    /**
     * @brief List all the curr_dir Dirent.
     * 
     * @return true if successful
     * @return false incase of error
     */
    bool    ls();

    /**
     * @brief Removes the given file/Directory.
     * 
     * @param name Entry to be removed
     * @return true if successful
     * @return false incase of error
     */
    bool    rm(char name[]);

    /**
     * @brief Copies the file in curr_dir to the path provided.
     * Prints amount of bytes copied.
     * 
     * @param name Name of the file to be copied out
     * @param path Path to store the file outside.
     * @return true if successful
     * @return false incase of error.
     */
    bool    copyout(char name[],const char *path);

    /**
     * @brief Copies the file with path provided to the curr_dir.
     * Prints amount of bytes copied.
     * Creates a newfile if file doesn't exist.
     * Overwrites the file if already exists.
     * 
     * @param name Name of the file to be copied in
     * @param path Path to store the file outside.
     * @return true if successful
     * @return false incase of error.
     */
    bool    copyin(const char *path, char name[]);

    /**
     * @brief List the Directory given by the name.
     * Called by ls to print curr_dir.
     * Prints a table of Dirent.
     * 
     * @param name Name of the directory to be listed
     * @return true if successful
     * @return false incase of error.
     */
    bool    ls_dir(char name[]);

    /**
     * @brief Unmounts the disk and resets pointer.
     * 
     */
    void    exit();

    /**
     * @brief Returns the stat of the disk.
     * The stat contains information like number of directories,
     * number of files, block number etc.
     * 
     * @return true if successful
     * @return false incase of error
     */
    void stat();
};

/**  NOTE: For now, Path's are not valid for creating files or directories  */