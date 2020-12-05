// fs.cpp: File System

#include "sfs/fs.h"
#include "sfs/sha256.h"

#include <algorithm>
#include <string>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

#define d1 (d2 | 0)
#define d2 0
#define streq(a, b) (strcmp((a), (b)) == 0)

using namespace std;


/*******************************************************
 * Debug - Done / Tests Passing
 * Format - Done / Tests Passing
 * Mount - Done / Tests Passing
 * Create - Done / Tests Passing
 * Remove - Done / Tests PENDING
 * Stat - Done / Tests Passing (ours is more efficient in terms of #reads)
 * Read - Done / Tests Passing
 * Write - Done / Tests Passing
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
    block.Super.DirBlocks = (uint32_t)std::ceil((int(block.Super.Blocks) * 1.00)/100);
    block.Super.Inodes = block.Super.InodeBlocks * (FileSystem::INODES_PER_BLOCK);


    // Reinitialising password protection
    block.Super.Protected = 0;
    memset(block.Super.PasswordHash,0,257);

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

    // Free Data Blocks and Directory Blocks
    for(uint32_t i = (block.Super.InodeBlocks) + 1; i < block.Super.Blocks; i++){
        Block DataBlock;
        memset(DataBlock.Data, 0, Disk::BLOCK_SIZE);
        disk->write(i, DataBlock.Data);
    }

    // Create Root directory
    struct Directory root;
    strcpy(root.Name,"/");
    root.Size = 0;
    root.inum = 0;
    root.Valid = 1;

    // Create table entries for "." and ".."
    struct Dirent temp;
    memset(&temp, 0, sizeof(temp));
    temp.inum = 0;
    temp.type = 0;
    temp.valid = 1;
    char tstr1[] = ".";
    char tstr2[] = "..";
    strcpy(temp.Name,tstr1);
    memcpy(&(root.Table[0]),&temp,sizeof(Dirent));
    strcpy(temp.Name,tstr2);
    memcpy(&(root.Table[1]),&temp,sizeof(Dirent));

    // Empty the directories
    Block Dirblock;
    for(uint32_t offset = 0; offset < FileSystem::DIR_PER_BLOCK; offset++){
        Dirblock.Directories[offset].inum = -1;
        Dirblock.Directories[offset].Valid = 0;
        Dirblock.Directories[offset].Size = 0;
    }

    // Set root directoy to the first node
    memcpy(&(Dirblock.Directories[0]),&root,sizeof(root));
    disk->write(block.Super.Blocks -1, Dirblock.Data);
    return true;
}

// Mount file system -----------------------------------------------------------

bool FileSystem::mount(Disk *disk) {
    if(disk->mounted()) return false;

    // Read superblock
    Block block;
    disk->read(0,block.Data);

    // Sanity Checks
    if(block.Super.MagicNumber != MAGIC_NUMBER) return false;
    if(block.Super.InodeBlocks != std::ceil((block.Super.Blocks*1.00)/10)) return false;
    if(block.Super.Inodes != (block.Super.InodeBlocks * INODES_PER_BLOCK)) return false;
    
    if(block.Super.DirBlocks != (uint32_t)std::ceil((int(block.Super.Blocks) * 1.00)/100)) return false;
    
    // Handling Password Protection
    if(block.Super.Protected){
        char pass[1000], line[1000];
    	printf("Enter password: ");
        if (fgets(line, BUFSIZ, stdin) == NULL) {
    	    return false;
    	}
        sscanf(line, "%s", pass);
        SHA256 hasher;
        if(hasher(pass) == string(block.Super.PasswordHash)){
            printf("Disk Unlocked\n");
            return true;
        }
        else{
            printf("Password Failed. Exiting...\n");
            return false;
        }
    }

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
                    if(block.Inodes[j].Indirect < MetaData.Blocks) {
                        free_blocks[block.Inodes[j].Indirect] = true;
                        Block indirect;
                        fs_disk->read(block.Inodes[j].Indirect, indirect.Data);
                        for(uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                            if((int)indirect.Pointers[k] < num_free_blocks) {
                                free_blocks[indirect.Pointers[k]] = true;
                            }
                            else return false;
                        }
                    }
                    else
                        return false;
                }
            }
        }
    }

    // Allocate dir_counter
    dir_counter = (uint32_t *)malloc(sizeof(uint32_t) * MetaData.DirBlocks);
    memset(dir_counter,0,sizeof(uint32_t) * MetaData.DirBlocks);

    Block dirblock;
    for(uint32_t dirs = 0; dirs < MetaData.DirBlocks; dirs++){
        disk->read(MetaData.Blocks-1-dirs, dirblock.Data);
        for(uint32_t offset = 0; offset < FileSystem::DIR_PER_BLOCK; offset++){
            if(dirblock.Directories[offset].Valid == 1){
                dir_counter[dirs]++;
            }
        }
        if(dirs == 0){
            curr_dir = dirblock.Directories[0];
        }
        printf("%u ",dir_counter[dirs]);
    }
    printf("\n");

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

bool FileSystem::load_inode(size_t inumber,struct Inode *node) {
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

// Return helper for write() function ------------------------------------------

ssize_t FileSystem::write_ret(size_t inumber, Inode* node, int ret) {
    int i = inumber / INODES_PER_BLOCK;
    int j = inumber % INODES_PER_BLOCK;

    Block block;
    fs_disk->read(i+1, block.Data);
    block.Inodes[j] = *node;

    fs_disk->write(i+1, block.Data);

    return (ssize_t)ret;
}

// Write to inode --------------------------------------------------------------

ssize_t FileSystem::write(size_t inumber, char *data, int length, size_t offset) { 
    Inode *node = new Inode;
    int read = 0;
    int orig_offset = offset;

    // insufficient size
    if(length + offset > (POINTERS_PER_BLOCK + POINTERS_PER_INODE) * Disk::BLOCK_SIZE) {
        return -1;
    }

    if(!load_inode(inumber, node)) {
        // allocate inode
        // will write to disk during write_ret()
        node->Valid = true;
        node->Size = length + offset;
        for(uint32_t ii = 0; ii < POINTERS_PER_INODE; ii++) {
            node->Direct[ii] = 0;
        }
        node->Indirect = 0;
        inode_counter[inumber / INODES_PER_BLOCK]++;
        free_blocks[inumber / INODES_PER_BLOCK + 1] = true;
    }
    else {
        node->Size = max((int)node->Size, length + (int)offset);
    }

    if(offset < POINTERS_PER_INODE * Disk::BLOCK_SIZE) {
        // offset is within direct pointers
        int direct_node = offset / Disk::BLOCK_SIZE;
        offset %= Disk::BLOCK_SIZE;

        if(!node->Direct[direct_node]) {
            node->Direct[direct_node] = allocate_block();
            if(node->Direct[direct_node] == 0) {
                node->Size = read + orig_offset;
                return write_ret(inumber, node, read);
            }
        }
        char* ptr = (char *)calloc(Disk::BLOCK_SIZE, sizeof(char));
        for(int i = offset; i < (int)Disk::BLOCK_SIZE && read < length; i++) {
            ptr[i] = data[read++];
        }
        fs_disk->write(node->Direct[direct_node++], ptr);

        if(read == length) return write_ret(inumber, node, length);
        else {
            // store in direct pointers till either one of the two things happen:
            // 1. all the data is stored in the direct pointers
            // 2. the data is stored in indirect pointers
            
            for(int i = direct_node; i < (int)POINTERS_PER_INODE; i++) {
                if(!node->Direct[direct_node]) {
                    node->Direct[direct_node] = allocate_block();
                    if(node->Direct[direct_node] == 0) {
                        node->Size = read + orig_offset;
                        return write_ret(inumber, node, read);
                    }
                }
                char* ptr = (char *)calloc(Disk::BLOCK_SIZE, sizeof(char));
                for(int i = 0; i < (int)Disk::BLOCK_SIZE && read < length; i++) {
                    ptr[i] = data[read++];
                }
                fs_disk->write(node->Direct[direct_node++], ptr);
                if(read == length) return write_ret(inumber, node, length);
            }

            Block indirect;
            if(!node->Indirect) {
                node->Indirect = allocate_block();
                if(node->Indirect == 0) {
                    node->Size = read + orig_offset;
                    return write_ret(inumber, node, read);
                }
                fs_disk->read(node->Indirect, indirect.Data);
                for(int i = 0; i < (int)POINTERS_PER_BLOCK; i++) {
                    indirect.Pointers[i] = 0;
                }
            }
            else {
                fs_disk->read(node->Indirect, indirect.Data);
            }
            
            for(int j = 0; j < (int)POINTERS_PER_BLOCK; j++) {
                if(!indirect.Pointers[j]) {
                    indirect.Pointers[j] = allocate_block();
                    if(indirect.Pointers[j] == 0) {
                        node->Size = read + orig_offset;
                        fs_disk->write(node->Indirect, indirect.Data);
                        return write_ret(inumber, node, read);
                    }
                }
                char* ptr = (char *)calloc(Disk::BLOCK_SIZE, sizeof(char));
                for(int i = 0; i < (int)Disk::BLOCK_SIZE && read < length; i++) {
                    ptr[i] = data[read++];
                }
                fs_disk->write(indirect.Pointers[j], ptr);
                if(read == length) {
                    fs_disk->write(node->Indirect, indirect.Data);
                    return write_ret(inumber, node, length);
                }
            }

            // space exhausted
            fs_disk->write(node->Indirect, indirect.Data);
            return write_ret(inumber, node, read);
        }
    }
    else {
        // offset is in indirect block
        offset -= (Disk::BLOCK_SIZE * POINTERS_PER_INODE);
        int indirect_node = offset / Disk::BLOCK_SIZE;
        offset %= Disk::BLOCK_SIZE;

        Block indirect;
        if(!node->Indirect) {
            node->Indirect = allocate_block();
            if(node->Indirect == 0) {
                node->Size = read + orig_offset;
                return write_ret(inumber, node, read);
            }
            fs_disk->read(node->Indirect, indirect.Data);
            for(int i = 0; i < (int)POINTERS_PER_BLOCK; i++) {
                indirect.Pointers[i] = 0;
            }
        }
        else {
            fs_disk->read(node->Indirect, indirect.Data);
        }

        if(!indirect.Pointers[indirect_node]) {
            indirect.Pointers[indirect_node] = allocate_block();
            if(indirect.Pointers[indirect_node] == 0) {
                node->Size = read + orig_offset;
                fs_disk->write(node->Indirect, indirect.Data);
                return write_ret(inumber, node, read);
            }
        }

        char* ptr = (char *)calloc(Disk::BLOCK_SIZE, sizeof(char));
        for(int i = offset; i < (int)Disk::BLOCK_SIZE && read < length; i++) {
            ptr[i] = data[read++];
        }
        fs_disk->write(indirect.Pointers[indirect_node++], ptr);

        if(read == length) {
            fs_disk->write(node->Indirect, indirect.Data);
            return write_ret(inumber, node, length);
        }
        else {
            for(int j = indirect_node; j < (int)POINTERS_PER_BLOCK; j++) {
                if(!indirect.Pointers[j]) {
                    indirect.Pointers[j] = allocate_block();
                    if(indirect.Pointers[j] == 0) {
                        node->Size = read + orig_offset;
                        fs_disk->write(node->Indirect, indirect.Data);
                        return write_ret(inumber, node, read);
                    }
                }
                char* ptr = (char *)calloc(Disk::BLOCK_SIZE, sizeof(char));
                for(int i = 0; i < (int)Disk::BLOCK_SIZE && read < length; i++) {
                    ptr[i] = data[read++];
                }
                fs_disk->write(indirect.Pointers[j], ptr);
                if(read == length) {
                    fs_disk->write(node->Indirect, indirect.Data);
                    return write_ret(inumber, node, length);
                }
            }

            // space exhausted
            fs_disk->write(node->Indirect, indirect.Data);
            return write_ret(inumber, node, read);
        }
    }
    
    return -1;
}

bool FileSystem::set_password(){
    if(MetaData.Protected){
        return FileSystem::change_password();
    }
    char pass[1000], line[1000];
    printf("Enter new password: ");
    if (fgets(line, BUFSIZ, stdin) == NULL) {
    	    return false;
    	}
    sscanf(line, "%s", pass);
    MetaData.Protected = 1;
    SHA256 hasher;
    strcpy(MetaData.PasswordHash,hasher(pass).c_str());
    Block block;
    block.Super = MetaData;
    fs_disk->write(0,block.Data);
    printf("New password set.\n");
    return true;
}

bool FileSystem::change_password(){
    if(MetaData.Protected){
        char pass[1000], line[1000];
        printf("Enter old password: ");
        if (fgets(line, BUFSIZ, stdin) == NULL) {
    	    return false;
    	}
        sscanf(line, "%s", pass);
        
        SHA256 hasher;
        if(hasher(pass) != string(MetaData.PasswordHash)){
            printf("Old password incorrect.\n");
            return false;
        }
        MetaData.Protected = 0;
    }
    return FileSystem::set_password();   
}

bool FileSystem::remove_password(){
    if(MetaData.Protected){
        char pass[1000], line[1000];
        printf("Enter old password: ");
        if (fgets(line, BUFSIZ, stdin) == NULL) {
    	    return false;
    	}
        sscanf(line, "%s", pass);
        
        SHA256 hasher;
        if(hasher(pass) != string(MetaData.PasswordHash)){
            printf("Old password incorrect.\n");
            return false;
        }
        MetaData.Protected = 0;
        Block block;
        block.Super = MetaData;
        fs_disk->write(0,block.Data);   
        printf("Password removed successfully.\n");
        return true;
    }
    else{
        return false;
    }
}

// bool FileSystem::add_dir_entry(uint32_t inum, uint32_t type, char name[]){
//     // This will add directory entry to current directory
//     // But it won't write back to the disk.
//     // dir write back should be done by the caller

    
//     struct Dirent temp;
//     temp.inum = inum;
//     temp.type = type;
//     temp.valid = 1;
//     strcpy(temp.Name,name);

//     uint32_t idx = 0;
//     for(; idx < FileSystem::ENTRIES_PER_DIR; idx++){
//         if(curr_dir.Table[idx].valid == 0){break;}
//     }
//     if(idx == FileSystem::ENTRIES_PER_DIR){
//         printf("Directory entry limit reached..exiting\n");
//         return false;
//     }
//     curr_dir.Table[idx] = temp;
//     return true;
// }

// void FileSystem::write_dir_back(Directory dir){
//     uint32_t block_idx = MetaData.Blocks - 1 - int(dir.inum / FileSystem::DIR_PER_BLOCK) ;
//     Block block;
//     fs_disk->read(block_idx, block.Data);
//     block.Directories[dir.inum % FileSystem::DIR_PER_BLOCK] = dir;
//     fs_disk->write(block_idx, block.Data);
// }

bool FileSystem::mkdir(char name[FileSystem::NAMESIZE]){
    // Find empty dirblock
    uint32_t block_idx = 0;
    for(;block_idx < MetaData.DirBlocks; block_idx++)
        if(dir_counter[block_idx] < FileSystem::DIR_PER_BLOCK)
            break;

    if(block_idx == MetaData.DirBlocks){printf("Directory limit reached\n"); return false;}

    // Read empty dirblock
    Block block;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, block.Data);

    // Find empty directory in dirblock
    uint32_t offset=0;
    for(;offset < FileSystem::DIR_PER_BLOCK; offset++)
        if(block.Directories[offset].Valid == 0)
            break;
    
    // Create new directory
    struct Directory new_dir;
    new_dir.inum = block_idx*FileSystem::DIR_PER_BLOCK + offset;
    new_dir.Size = 0;
    new_dir.Valid = 1;
    
    // Create 2 new entries for "." and ".."
    struct Dirent temp;
    temp.inum = new_dir.inum;
    temp.valid = 1;
    temp.type = 0;
    char tstr1[] = ".", tstr2[] = "..";
    strcpy(temp.Name,tstr1);
    memcpy(&(new_dir.Table[0]),&(temp),sizeof(Dirent));
    temp.inum = curr_dir.inum;
    strcpy(temp.Name,tstr2);
    memcpy(&(new_dir.Table[1]),&(temp),sizeof(Dirent));

    // Write the new directory back to the disk
    memcpy(&(block.Directories[offset]),&(new_dir),sizeof(Directory));
    fs_disk->write(MetaData.Blocks - 1-block_idx, block.Data);

    // Add new entry to the curr_dir
    temp.inum = new_dir.inum;
    strcpy(temp.Name, name);
    offset = 0;
    for(; offset < FileSystem::ENTRIES_PER_DIR; offset++)
        if(curr_dir.Table[offset].valid == 0)
            break;
    

    // Write the curr_dir back to the disk
    memcpy(&(curr_dir.Table[offset]),&temp,sizeof(Dirent));
    block_idx = (curr_dir.inum / DIR_PER_BLOCK);
    fs_disk->read(MetaData.Blocks - 1 - block_idx, block.Data);
    block.Directories[curr_dir.inum % DIR_PER_BLOCK] = curr_dir;
    fs_disk->write(MetaData.Blocks - 1 - block_idx, block.Data);

    // Increment the counter
    dir_counter[block_idx]++;

    return true;
    
}

bool FileSystem::rmdir(char name[FileSystem::NAMESIZE]){
    // Find the directory
    uint32_t offset = 0;
    for(;offset < FileSystem::DIR_PER_BLOCK; offset++)
        if(
            (curr_dir.Table[offset].valid) && 
            (curr_dir.Table[offset].type == 0) &&
            (streq(curr_dir.Table[offset].Name,name))
        )
            break;
    

    // Directory not found
    if(offset == FileSystem::DIR_PER_BLOCK){return false;}

    // Update the dirblock
    Block block;
    uint32_t block_idx = curr_dir.Table[offset].inum / FileSystem::DIR_PER_BLOCK;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, block.Data);
    block.Directories[curr_dir.Table[offset].inum % FileSystem::DIR_PER_BLOCK].Valid = 0;
    fs_disk->write(MetaData.Blocks - 1 - block_idx, block.Data);

    // Update curr_dir
    curr_dir.Table[offset].valid = 0;
    dir_counter[curr_dir.inum / FileSystem::DIR_PER_BLOCK]--;
    block_idx = (curr_dir.inum / DIR_PER_BLOCK);
    fs_disk->read(MetaData.Blocks - 1 - block_idx, block.Data);
    block.Directories[curr_dir.inum % DIR_PER_BLOCK] = curr_dir;
    fs_disk->write(MetaData.Blocks - 1 - block_idx, block.Data);
    return true;
}

bool FileSystem::touch(char name[FileSystem::NAMESIZE]){
    // Allocate new inode for the file
    ssize_t new_node_idx = FileSystem::create();
    if(new_node_idx == -1){return false;}

    // Add the directory entry in curr_directory
    struct Dirent temp;
    temp.inum = new_node_idx;
    strcpy(temp.Name,name);
    temp.type = 1;
    temp.valid = 1;

    // Find the offset for placing the entry
    uint32_t offset = 0;
    for(;offset < ENTRIES_PER_DIR; offset++)
        if(
            (curr_dir.Table[offset].valid == 0) &&
            !(streq(curr_dir.Table[offset].Name,name))
        )
            break;
    
    // No valid entries in the directory
    if(offset == DIR_PER_BLOCK) return false;
    
    // Write back the changes
    Block block;
    memcpy(&(curr_dir.Table[offset]),&temp,sizeof(temp));
    uint32_t block_idx = MetaData.Blocks - 1 - (curr_dir.inum / DIR_PER_BLOCK);
    fs_disk->read(block_idx, block.Data);
    block.Directories[curr_dir.inum % DIR_PER_BLOCK] = curr_dir;
    fs_disk->write(block_idx, block.Data);

    return true;
}

bool FileSystem::cd(char name[FileSystem::NAMESIZE]){

    // Search the current dir table for name
    uint32_t offset = 0;
    for(;offset < FileSystem::ENTRIES_PER_DIR; offset++){
        if(
            (curr_dir.Table[offset].valid) &&
            (curr_dir.Table[offset].type == 0) &&
            (streq(curr_dir.Table[offset].Name,name))
        ){break;}
    }

    // No such dir found
    if(offset == FileSystem::ENTRIES_PER_DIR){return false;}

    // Get the direntry
    struct Dirent entry;
    memcpy(&entry, &(curr_dir.Table[offset]),sizeof(Dirent));
    uint32_t block_idx = entry.inum / FileSystem::DIR_PER_BLOCK;
    uint32_t block_offset = entry.inum % FileSystem::DIR_PER_BLOCK;

    // Read the dirblock from the disk
    Block block;
    fs_disk->read(block_idx, block.Data);

    // Read the directory from the block;
    Directory dir;
    memcpy(&dir,&(block.Directories[block_offset]),sizeof(Directory));
    memcpy(&curr_dir,&dir,sizeof(dir));

    return true;
}

void FileSystem::ls(){
    printf("   inum    |       name       | type\n");
    for(uint32_t offset=0;offset<FileSystem::ENTRIES_PER_DIR;offset++){
        struct Dirent temp;
        memcpy(&temp,&(curr_dir.Table[offset]),sizeof(Dirent));
        if(temp.valid){
            if(temp.type == 1) printf("%-10u | %-16s | %-5s\n",temp.inum,temp.Name, "file");
            else printf("%-10u | %-16s | %-5s\n",temp.inum,temp.Name, "dir");
        }
    }
}