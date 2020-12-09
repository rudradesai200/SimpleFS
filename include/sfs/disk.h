/** disk.h: Disk emulator */

#pragma once

#include <stdlib.h>

/**
 * @brief Disk class
 * Implements Disk abstraction that enables emulation of a disk image.
 * Used by file system to access and make changes to the disk.
 */
class Disk {
private:
    int	    FileDescriptor;                                         /** File descriptor of disk image */
    size_t  Blocks;	                                                /** Number of blocks in disk image */
    size_t  Reads;	                                                /** Number of reads performed */
    size_t  Writes;	                                                /** Number of writes performed */
    size_t  Mounts;	                                                /** Number of mounts */

    /** 
     * @brief check if the block is within valid range
     * @param blocknum index of the block into the free block bitmap
     * @param data data buffer
     * @return void function; returns nothing. throws invalid_argument on error
     */
    void sanity_check(int blocknum, char *data);

public:
    const static size_t BLOCK_SIZE = 4096;                          /** Number of bytes per block */
    
    /**
     * @brief constructor of Disk class
     * @return an instance of Disk class
     */
    Disk() : FileDescriptor(0), Blocks(0), Reads(0), Writes(0), Mounts(0) {}
    
    /**
     * @brief destructor of Disk class
     * @return returns nothing; deletes the Disk object 
     */
    ~Disk();

    /**
     * @brief opens the disk image
     * @param path path to the disk image
     * @param nblocks number of blocks in the disk image
     * @return void function; returns nothing. throws runtime_error exception on error.
     */
    void    open(const char *path, size_t nblocks);

    /**
     * @brief check size of disk
     * @return size of disk in terms of blocks
     */
    size_t  size() const { return Blocks; }

    /**
     * @brief check if the disk has been mounted
     * @return true if the disk has been mounted; false otherwise
     */
    bool    mounted() const { return Mounts > 0; }

    /** 
     * @brief mount the disk
     * @return void function; returns nothing
     */
    void    mount() { Mounts++; }

    /** 
     * @brief unmount the disk
     * @return void function; returns nothing
     */
    void    unmount() { if (Mounts > 0) Mounts--; }

    /**
     * @brief read from disk
     * @param blocknum block to read from
     * @param data data buffer to write into
     */
    void    read(int blocknum, char *data);
    
    /**
     * @brief write to disk
     * @param blocknum block to write into
     * @param data data buffer to read from
     */
    void    write(int blocknum, char *data);
};
