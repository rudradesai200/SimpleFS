/** fs.h: File System */

#pragma once

#include "sfs/disk.h"
#include <cstring>
#include <vector>
#include <stdint.h>

using namespace std;

/**
 * @brief FileSytem Class.
 * Contains fs layer to access and store disk blocks.
 * Used by sfssh (shell) to provide access to the end-user.
 */
class FileSystem {
private:
    const static uint32_t MAGIC_NUMBER	     = 0xf0f03410;      /**   Magic number helps in checking Validity of the FileSystem on disk   @hideinitializer*/
    const static uint32_t INODES_PER_BLOCK   = 128;             /**   Number of Inodes which can be contained in a block of (4kb)   @hideinitializer*/
    const static uint32_t POINTERS_PER_INODE = 5;               /**   Number of Direct block pointers in an Inode Block   @hideinitializer*/
    const static uint32_t POINTERS_PER_BLOCK = 1024;            /**   Number of block pointers in one Indirect pointer  @hideinitializer*/
    const static uint32_t NAMESIZE           = 16;              /**   Max Name size for files/directories   @hideinitializer*/
    const static uint32_t ENTRIES_PER_DIR    = 14;              /**   Number of Files/Directory entries within a Directory   @hideinitializer*/
    const static uint32_t DIR_PER_BLOCK      = 4;               /**   Number of Directories per 4KB block   @hideinitializer*/

    /** 
     * @brief Superblock structure.
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
        uint64_t Valid;                 /** Valid bit for validation @hideinitializer*/
        uint32_t Size;                  /** size of the directory @hideinitializer*/
        uint32_t inum;                  /** inum = block_num * DIR_PER_BLOCK + offset @hideinitializer*/
        char Name[NAMESIZE];            /** Directory Name @hideinitializer*/
        Dirent Table[ENTRIES_PER_DIR];  /** Each Table by default contains 2 entries, "." and ".." @hideinitializer*/
    };


    struct Inode {
    	uint32_t Valid;		                    /**  Whether or not inode is valid @hideinitializer*/
    	uint32_t Size;		                    /**  Size of file @hideinitializer*/
    	uint32_t Direct[FileSystem::POINTERS_PER_INODE];    /**  Direct pointers @hideinitializer*/
    	uint32_t Indirect;	                    /**  Indirect pointer @hideinitializer*/
    };

    union Block {
    	struct SuperBlock   Super;			                            /**  Superblock @hideinitializer*/
    	struct Inode	    Inodes[FileSystem::INODES_PER_BLOCK];	    /**  Inode block @hideinitializer*/
    	uint32_t            Pointers[FileSystem::POINTERS_PER_BLOCK];   /**  Contains indexes of Direct Blocks. 0 if null.ck @hideinitializer*/
    	char	            Data[Disk::BLOCK_SIZE];	                    /**  Data block @hideinitializer*/
        struct Directory    Directories[FileSystem::DIR_PER_BLOCK];      /**  Directory blocks @hideinitializer*/
    };

    // Internal member variables
    Disk* fs_disk;                      /** Stores disk pointer after successful mounting @hideinitializer*/
    vector<bool> free_blocks;           /** Stores whether a block is free or not @hideinitializer*/
    vector<int> inode_counter;          /** Stores the number of Inode contained in an Inode Block @hideinitializer*/ 
    vector<uint32_t> dir_counter;       /** Stores the number of Directory contianed in a Directory Block @hideinitializer*/
    struct SuperBlock MetaData;         /** Caches the Superblock to save a disk-read @hideinitializer*/
    bool mounted;                       /** Boolean to check if the disk is mounted and saved @hideinitializer*/

    // Layer 1 Core Functions
    ssize_t create();                   
    bool    remove(size_t inumber);
    ssize_t stat(size_t inumber);
    ssize_t read(size_t inumber, char *data, int length, size_t offset);
    ssize_t write(size_t inumber, char *data, int length, size_t offset);

    //  Helper functions for Layer 1
    ssize_t  write_ret(size_t inumber, Inode* node, int ret);
    bool     load_inode(size_t inumber, Inode *node);
    ssize_t  allocate_free_block();
    uint32_t allocate_block();

    // Helper functions for Layer 2
    
    Directory curr_dir; /** Caches curr dir to save a disk-read @hideinitializer*/

    /**
     * @brief Adds directory entry to cache curr_dir.
     * dir_write_back should be done by the caller.
     * 
     * @param dir   Directory in which entry is to be added
     * @param inum  inum of the File/Directory
     * @param type  type = 1 for file , type = 0 for Directory
     * @param name  Name of the file/Directory
     * @return Directory with added entry or with valid bit set to 0 incase of error 
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
    /**  Initializations */
    static void debug(Disk *disk);
    static bool format(Disk *disk);
    bool    mount(Disk *disk);

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
};

/**  NOTE: For now, Path's are not valid for creating files or directories  */