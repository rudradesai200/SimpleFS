/**
 * @file fs_layer_2.cpp
 * @author Rudra Desai (rudrad200@gmail.com)
 * @brief Implementation of fs.h layer_2 functions
 * @date 2020-12-09
 * 
 * @details Mainly includes all the functions at Directory & Files level
 * These functions are directly accessed by the shell and are declared public.
 * They are build upon the functions in fs_layer_1.cpp.
 */

#include "sfs/fs.h"
#include "sfs/sha256.h"

#include <algorithm>
#include <string>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

#define streq(a, b) (strcmp((a), (b)) == 0)

using namespace std;


void FileSystem::debug(Disk *disk) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    Block block;

    /**- read superblock */
    disk->read(0, block.Data);

    printf("SuperBlock:\n");

    /**- verifying the integrity of the file system */
    if(block.Super.MagicNumber == MAGIC_NUMBER) 
        printf("    magic number is valid\n");
    else {
        printf("    magic number is invalid\n");
        printf("    exiting...\n");
        return;
    }

    /**- printing the superblock */
    printf("    %u blocks\n"         , block.Super.Blocks);
    printf("    %u inode blocks\n"   , block.Super.InodeBlocks);
    printf("    %u inodes\n"         , block.Super.Inodes);

    /**- reading the inode blocks */
    int ii = 0;

    for(uint32_t i = 1; i <= block.Super.InodeBlocks; i++) {
        disk->read(i, block.Data); /**-  array of inodes */
        for(uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
            /**- iterating through INODES_PER_BLOCK inodes */
            if(block.Inodes[j].Valid) {
                printf("Inode %u:\n", ii);
                printf("    size: %u bytes\n", block.Inodes[j].Size);
                printf("    direct blocks:");

                /** iterating through the direct nodes */
                for(uint32_t k = 0; k < POINTERS_PER_INODE; k++) {
                    if(block.Inodes[j].Direct[k]) printf(" %u", block.Inodes[j].Direct[k]);
                }
                printf("\n");

                /**- iterate through the indirect nodes */
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


bool FileSystem::format(Disk *disk) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    if(disk->mounted()) return false;

    /**- write superblock */
    Block block;
    memset(&block, 0, sizeof(Block));

    block.Super.MagicNumber = FileSystem::MAGIC_NUMBER;
    block.Super.Blocks = (uint32_t)(disk->size());
    block.Super.InodeBlocks = (uint32_t)std::ceil((int(block.Super.Blocks) * 1.00)/10);
    block.Super.Inodes = block.Super.InodeBlocks * (FileSystem::INODES_PER_BLOCK);
    block.Super.DirBlocks = (uint32_t)std::ceil((int(block.Super.Blocks) * 1.00)/100);

    disk->write(0,block.Data);
    
    /**- Reinitialising password protection */
    block.Super.Protected = 0;
    memset(block.Super.PasswordHash,0,257);

    /**- clear all other blocks */
    for(uint32_t i = 1; i <= block.Super.InodeBlocks; i++){
        Block inodeblock;

        /**- clear individual Inodes */
        for(uint32_t j = 0; j < FileSystem::INODES_PER_BLOCK; j++){
            inodeblock.Inodes[j].Valid = false;
            inodeblock.Inodes[j].Size = 0;

            /**- clear all direct Pointers */
            for(uint32_t k = 0; k < FileSystem::POINTERS_PER_INODE; k++)   
                inodeblock.Inodes[j].Direct[k] = 0;
    
            /**- clear indirect Pointer */
            inodeblock.Inodes[j].Indirect = 0;
        }

        disk->write(i,inodeblock.Data);
    }

     /**- Free Data Blocks */
    for(uint32_t i = (block.Super.InodeBlocks) + 1; i < block.Super.Blocks - block.Super.DirBlocks; i++){
        Block DataBlock;
        memset(DataBlock.Data, 0, Disk::BLOCK_SIZE);
        disk->write(i, DataBlock.Data);
    }

    /**- Free Directory Blocks */
    for(uint32_t i = block.Super.Blocks - block.Super.DirBlocks; i < block.Super.Blocks; i++){
        Block DataBlock;
        Directory dir;
        dir.inum = -1;
        dir.Valid = 0;
        memset(dir.Table,0,sizeof(Dirent)*ENTRIES_PER_DIR);
        for(uint32_t j=0; j<FileSystem::DIR_PER_BLOCK; j++){
            DataBlock.Directories[j] = dir;
        }
        disk->write(i, DataBlock.Data);
    }

    /**-  Create Root directory */
    struct Directory root;
    strcpy(root.Name,"/");
    root.inum = 0;
    root.Valid = 1;

    /**-  Create table entries for "." and ".." */
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

    /**-  Empty the directories */
    Block Dirblock;
    memcpy(&(Dirblock.Directories[0]),&root,sizeof(root));
    disk->write(block.Super.Blocks -1, Dirblock.Data);

    return true;
}


bool FileSystem::mount(Disk *disk) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity check */
    if(disk->mounted()) return false;

    /**- read superblock */
    Block block;
    disk->read(0,block.Data);
    if(block.Super.MagicNumber != MAGIC_NUMBER) return false;
    if(block.Super.InodeBlocks != std::ceil((block.Super.Blocks*1.00)/10)) return false;
    if(block.Super.Inodes != (block.Super.InodeBlocks * INODES_PER_BLOCK)) return false;
    if(block.Super.DirBlocks != (uint32_t)std::ceil((int(block.Super.Blocks) * 1.00)/100)) return false;

    /**- Handle Password Protection */
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

    disk->mount();
    fs_disk = disk;

    /**- copy metadata */
    MetaData = block.Super;

    /**- allocate free block bitmap */ 
    free_blocks.resize(MetaData.Blocks, false);

    /**- allocate inode counter */
    inode_counter.resize(MetaData.InodeBlocks, 0);

    /**- setting free bit map node 0 to true for superblock */
    free_blocks[0] = true;

    /**- read inode blocks */
    for(uint32_t i = 1; i <= MetaData.InodeBlocks; i++) {
        disk->read(i, block.Data);

        for(uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
            if(block.Inodes[j].Valid) {
                inode_counter[i-1]++;

                /**- set free bit map for inode blocks */
                if(block.Inodes[j].Valid) free_blocks[i] = true;

                /**- set free bit map for direct pointers */
                for(uint32_t k = 0; k < POINTERS_PER_INODE; k++) {
                    if(block.Inodes[j].Direct[k]){
                        if(block.Inodes[j].Direct[k] < MetaData.Blocks)
                            free_blocks[block.Inodes[j].Direct[k]] = true;
                        else
                            return false;
                    }
                }

                /**- set free bit map for indirect pointers */
                if(block.Inodes[j].Indirect){
                    if(block.Inodes[j].Indirect < MetaData.Blocks) {
                        free_blocks[block.Inodes[j].Indirect] = true;
                        Block indirect;
                        fs_disk->read(block.Inodes[j].Indirect, indirect.Data);
                        for(uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                            if(indirect.Pointers[k] < MetaData.Blocks) {
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

    /**- Allocate dir_counter */
    dir_counter.resize(MetaData.DirBlocks,0);

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
    }

    mounted = true;

    return true;
}

bool FileSystem::set_password(){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**-  Sanity checks  */
    if(!mounted){return false;}
    if(MetaData.Protected) return change_password();

    /**-  Initializations  */
    SHA256 hasher;
    char pass[1000], line[1000];
    Block block;

    /**-  Get new password  */
    printf("Enter new password: ");
    if (fgets(line, BUFSIZ, stdin) == NULL) return false;
    sscanf(line, "%s", pass);
    
    /**-  Set cached MetaData to Protected  */
    MetaData.Protected = 1;
    strcpy(MetaData.PasswordHash,hasher(pass).c_str());
    
    // Write chanes back to the disk */
    block.Super = MetaData;
    fs_disk->write(0,block.Data);
    printf("New password set.\n");
    return true;
}

bool FileSystem::change_password(){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- Sanity Checks */
    if(!mounted){return false;}

    if(MetaData.Protected){
        /**-  Initializations  */
        char pass[1000], line[1000];
        SHA256 hasher;

        /**-  Get curr password  */
        printf("Enter current password: ");
        if (fgets(line, BUFSIZ, stdin) == NULL) return false;
        sscanf(line, "%s", pass);
        
        /**-  Check password  */
        if(hasher(pass) != string(MetaData.PasswordHash)){
            printf("Old password incorrect.\n");
            return false;
        }

        /**-  Store it in cached MetaData  */
        MetaData.Protected = 0;
    }
    return FileSystem::set_password();   
}

bool FileSystem::remove_password(){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    if(!mounted){return false;}

    if(MetaData.Protected){
        /**-  Initializations  */
        char pass[1000], line[1000];
        SHA256 hasher;
        Block block;
        
        /**-  Get current password  */
        printf("Enter old password: ");
        if (fgets(line, BUFSIZ, stdin) == NULL) return false;
        sscanf(line, "%s", pass);
        
        /**-  Curr password incorrect. Error  */
        if(hasher(pass) != string(MetaData.PasswordHash)){
            printf("Old password incorrect.\n");
            return false;
        }
        
        /**-  Update cached MetaData  */
        MetaData.Protected = 0;
        
        /**-  Write back the changes  */
        block.Super = MetaData;
        fs_disk->write(0,block.Data);   
        printf("Password removed successfully.\n");
        
        return true;
    }
    return false;
    
}

FileSystem::Directory FileSystem::add_dir_entry(Directory dir, uint32_t inum, uint32_t type, char name[]){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    Directory tempdir = dir;

    /**-  Find a free Dirent in the Table to be modified */
    uint32_t idx = 0;
    for(; idx < FileSystem::ENTRIES_PER_DIR; idx++){
        if(tempdir.Table[idx].valid == 0){break;}
    }

    /**-  If No Dirent Found, Error  */
    if(idx == FileSystem::ENTRIES_PER_DIR){
        printf("Directory entry limit reached..exiting\n");
        tempdir.Valid = 0;
        return tempdir;
    }

    /**-  Free Dirent found, add the new one to the table  */ 
    tempdir.Table[idx].inum = inum;
    tempdir.Table[idx].type = type;
    tempdir.Table[idx].valid = 1;
    strcpy(tempdir.Table[idx].Name,name);

    return tempdir;
}

FileSystem::Directory FileSystem::read_dir_from_offset(uint32_t offset){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**-   Sanity Check  */
    if(
        (offset < 0) ||
        (offset >= ENTRIES_PER_DIR) ||
        (curr_dir.Table[offset].valid == 0) || 
        (curr_dir.Table[offset].type != 0)
    ){Directory temp; temp.Valid=0; return temp;}

    /**-   Get offsets and indexes  */
    uint32_t inum = curr_dir.Table[offset].inum;
    uint32_t block_idx = (inum / FileSystem::DIR_PER_BLOCK);
    uint32_t block_offset = (inum % FileSystem::DIR_PER_BLOCK);
    
    /**-   Read Block  */
    Block blk;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, blk.Data);
    return (blk.Directories[block_offset]);
}

void FileSystem::write_dir_back(Directory dir){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**-   Get block offset and index  */
    uint32_t block_idx = (dir.inum / FileSystem::DIR_PER_BLOCK) ;
    uint32_t block_offset = (dir.inum % FileSystem::DIR_PER_BLOCK);

    /**-   Read Block  */
    Block block;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, block.Data);
    block.Directories[block_offset] = dir;

    /**-   Write the Dirblock  */
    fs_disk->write(MetaData.Blocks - 1 - block_idx, block.Data);
}

int FileSystem::dir_lookup(Directory dir,char name[]){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**-   Search the curr_dir.Table for name  */
    uint32_t offset = 0;
    for(;offset < FileSystem::ENTRIES_PER_DIR; offset++){
        if(
            (dir.Table[offset].valid == 1) &&
            (streq(dir.Table[offset].Name,name))
        ){break;}
    }

    /**-   No such dir found  */
    if(offset == FileSystem::ENTRIES_PER_DIR){return -1;}

    return offset;
}

bool FileSystem::ls_dir(char name[]){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    if(!mounted){return false;}

    /**-   Get the directory entry offset   */
    int offset = dir_lookup(curr_dir,name);
    if(offset == -1){printf("No such Directory\n"); return false;}

    /**-   Read directory from block  */
    FileSystem::Directory dir = read_dir_from_offset(offset);

    /**-   Sanity checks  */
    if(dir.Valid == 0){printf("Directory Invalid\n"); return false;}

    /**-   Print Directory Data  */
    printf("   inum    |       name       | type\n");
    for(uint32_t idx=0; idx<FileSystem::ENTRIES_PER_DIR; idx++){
        struct Dirent temp = dir.Table[idx];
        if(temp.valid == 1){
            if(temp.type == 1) printf("%-10u | %-16s | %-5s\n",temp.inum,temp.Name, "file");
            else printf("%-10u | %-16s | %-5s\n",temp.inum,temp.Name, "dir");
        }
    }
    return true;
}

bool FileSystem::mkdir(char name[FileSystem::NAMESIZE]){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    if(!mounted){return false;}

    /**-   Find empty dirblock  */
    uint32_t block_idx = 0;
    for(;block_idx < MetaData.DirBlocks; block_idx++)
        if(dir_counter[block_idx] < FileSystem::DIR_PER_BLOCK)
            break;

    if(block_idx == MetaData.DirBlocks){printf("Directory limit reached\n"); return false;}

    /**-   Read empty dirblock  */
    Block block;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, block.Data);


    /**-   Find empty directory in dirblock  */
    uint32_t offset=0;
    for(;offset < FileSystem::DIR_PER_BLOCK; offset++)
        if(block.Directories[offset].Valid == 0)
            break;
    
    if(offset == DIR_PER_BLOCK){printf("Error in creating directory.\n"); return false;}

    /**-   Create new directory  */
    Directory new_dir, temp;
    memset(&new_dir,0,sizeof(Directory));
    new_dir.inum = block_idx*DIR_PER_BLOCK + offset;
    new_dir.Valid = 1;
    strcpy(new_dir.Name,name);
    
    /**-   Create 2 new entries for "." and ".."  */
    char tstr1[] = ".", tstr2[] = "..";
    temp = new_dir;
    temp = add_dir_entry(temp,temp.inum,0,tstr1);
    temp = add_dir_entry(temp,curr_dir.inum,0,tstr2);
    if(temp.Valid == 0){printf("Error creating new directory\n"); return false;}
    new_dir = temp;
    
    /**-   Add new entry to the curr_dir  */
    temp = add_dir_entry(curr_dir,new_dir.inum,0,new_dir.Name);
    if(temp.Valid == 0){printf("Error adding new directory\n"); return false;}
    curr_dir = temp;
    
    /**-   Write the new directory back to the disk  */
    write_dir_back(new_dir);

    /**-   Write the curr_dir back to the disk  */
    write_dir_back(curr_dir);

    /**-   Increment the counter  */
    dir_counter[block_idx]++;

    return true;
    
}

FileSystem::Directory FileSystem::rmdir_helper(Directory parent, char name[]){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**-  initializations  */
    Directory dir, temp;
    uint32_t inum, blk_idx, blk_off;
    Block blk;

    /**-  Sanity Checks  */
    if(!mounted){dir.Valid = 0; return dir;}

    /**-  Get offset of the directory to be removed  */
    int offset = dir_lookup(parent, name);
    if(offset == -1){dir.Valid = 0; return dir;}

    /**-  Get block  */
    inum = parent.Table[offset].inum;
    blk_idx = inum / DIR_PER_BLOCK;
    blk_off = inum % DIR_PER_BLOCK;
    fs_disk->read(MetaData.Blocks - 1 -blk_idx, blk.Data);

    /**-  Check Directory  */
    dir = blk.Directories[blk_off];
    if(dir.Valid == 0){return dir;}

    /**- Check if it is root directory */
    if(streq(dir.Name,curr_dir.Name)){printf("Current Directory cannot be removed.\n"); dir.Valid=0; return dir;}

    /**-  Remove all Dirent in the directory to be removed  */
    for(uint32_t ii=0; ii<ENTRIES_PER_DIR; ii++){
        if((ii>1)&&(dir.Table[ii].valid == 1)){
            temp = rm_helper(dir, dir.Table[ii].Name);
            if(temp.Valid == 0) return temp;
            dir = temp;
        }
        dir.Table[ii].valid = 0;
    }

    /**-  Read the block again, because the block may have changed by Dirent  */
    fs_disk->read(MetaData.Blocks - 1 -blk_idx, blk.Data);

    /**-  Write it back  */
    dir.Valid = 0;
    blk.Directories[blk_off] = dir;
    fs_disk->write(MetaData.Blocks - 1-blk_idx, blk.Data);

    /**-  Remove it from the parent  */
    parent.Table[offset].valid = 0;
    write_dir_back(parent);

    /**-  Update the counter  */
    dir_counter[blk_idx]--;

    return parent;
}

FileSystem::Directory FileSystem::rm_helper(Directory dir, char name[FileSystem::NAMESIZE]){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    if(!mounted){dir.Valid = 0; return dir;}

    /**-   Get the offset for removal  */
    int offset = dir_lookup(dir,name);
    if(offset == -1){printf("No such file/directory\n"); dir.Valid=0; return dir;}

    /**-   Check if directory  */
    if(dir.Table[offset].type == 0){
        return rmdir_helper(dir,name);
    }

    /**-   Get inumber  */
    uint32_t inum = dir.Table[offset].inum;

    printf("%u\n",inum);

    /**-   Remove the inode  */
    if(!remove(inum)){printf("Failed to remove Inode\n"); dir.Valid = 0; return dir;}

    /**-   Remove the entry  */
    dir.Table[offset].valid = 0;

    /**-   Write back the changes  */
    write_dir_back(dir);

    return dir;
}

bool FileSystem::rmdir(char name[FileSystem::NAMESIZE]){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    Directory temp = rmdir_helper(curr_dir,name);
    if(temp.Valid == 0){
        curr_dir = temp;
        return true;
    }
    return false;
}

bool FileSystem::touch(char name[FileSystem::NAMESIZE]){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    if(!mounted){return false;}

    /**-   Check if such file exists  */
    for(uint32_t offset=0; offset<FileSystem::ENTRIES_PER_DIR; offset++){
        if(curr_dir.Table[offset].valid){
            if(streq(curr_dir.Table[offset].Name,name)){
                printf("File already exists\n");
                return false;
            }
        }
    }
    /**-   Allocate new inode for the file  */
    ssize_t new_node_idx = FileSystem::create();
    if(new_node_idx == -1){printf("Error creating new inode\n"); return false;}

    /**-   Add the directory entry in the curr_directory  */
    Directory temp = add_dir_entry(curr_dir,new_node_idx,1,name);
    if(temp.Valid == 0){printf("Error adding new file\n"); return false;}
    curr_dir = temp;
    
    /**-   Write back the changes  */
    write_dir_back(curr_dir);

    return true;
}

bool FileSystem::cd(char name[FileSystem::NAMESIZE]){
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    if(!mounted){return false;}

    int offset = dir_lookup(curr_dir,name);
    if((offset == -1) || (curr_dir.Table[offset].type == 1)){
        printf("No such directory\n");
        return false;
    }

    /**-   Read the dirblock from the disk  */
    Directory temp = read_dir_from_offset(offset);
    if(temp.Valid == 0){return false;}
    curr_dir = temp;
    return true;
}

bool FileSystem::ls(){
    if(!mounted){return false;}

    char name[] = ".";
    return ls_dir(name);
}

bool FileSystem::rm(char name[]){
    Directory temp = rm_helper(curr_dir,name);
    if(temp.Valid == 1){
        curr_dir = temp;
        return true;
    }
    return false;
}

void FileSystem::exit(){
    if(!mounted){return;}

    fs_disk->unmount();
    mounted = false;
    fs_disk = nullptr;
}

bool FileSystem::copyout(char name[],const char *path) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- Sanity Checks */
    if(!mounted){return false;}

    /**- Get table offset for the filename */
    int offset = dir_lookup(curr_dir,name);
    if(offset == -1){return false;}

    if(curr_dir.Table[offset].type == 0){return false;}

    uint32_t inum = curr_dir.Table[offset].inum;

    /**- Open File for copyout */
    FILE *stream = fopen(path, "w");
    if (stream == nullptr) {
    	fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
    	return false;
    }

    /**- Read from the inode and write it to the File */
    char buffer[4*BUFSIZ] = {0};
    offset = 0;
    while (true) {
    	ssize_t result = read(inum, buffer, sizeof(buffer), offset);
    	if (result <= 0) {
    	    break;
		}
		fwrite(buffer, 1, result, stream);
		offset += result;
    }
    
    /**- Endings */
    printf("%d bytes copied\n", offset);
    fclose(stream);
    return true;
}

bool FileSystem::copyin(const char *path, char name[]) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- Sanity Checks */
    if(!mounted){return false;}

    /**- Check if file exists. Else create one */
    touch(name);
    int offset = dir_lookup(curr_dir,name);
    if(offset == -1){return false;}

    if(curr_dir.Table[offset].type == 0){return false;}

    /**- Get inode of the created file */
    uint32_t inum = curr_dir.Table[offset].inum;

    /**- Open File for reading */
	FILE *stream = fopen(path, "r");
    if (stream == nullptr) {
    	fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
    	return false;
    }

    /**- Read File and get the Data */
    char buffer[4*BUFSIZ] = {0};
    offset = 0;
    while (true) {
    	ssize_t result = fread(buffer, 1, sizeof(buffer), stream);
    	if (result <= 0) {
    	    break;
	}

    /**- Save the file */
	ssize_t actual = write(inum, buffer, result, offset);
	if (actual < 0) {
	    fprintf(stderr, "fs.write returned invalid result %ld\n", actual);
	    break;
	}

    /**- Checks to ensure proper write */
	offset += actual;
	if (actual != result) {
	    fprintf(stderr, "fs.write only wrote %ld bytes, not %ld bytes\n", actual, result);
	    break;
	}
    }
    
    /**- Endings */
    printf("%d bytes copied\n", offset);
    fclose(stream);
    return true;
}

// Directory stat ------------------------------------------------------------------

void FileSystem::stat() {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- Sanity checks */
    if(!mounted){return;}

    /**- Read Super Block and print MetaData*/
    Block blk;
    fs_disk->read(0,blk.Data);
    printf("Total Blocks : %u\n",blk.Super.Blocks);
    printf("Total Directory Blocks : %u\n",blk.Super.DirBlocks);
    printf("Total Inode Blocks : %u\n",blk.Super.InodeBlocks);
    printf("Total Inode : %u\n",blk.Super.Inodes);
    printf("Password protected : %u\n\n",blk.Super.Protected);

    printf("Max Directories per block : %u\n",DIR_PER_BLOCK);
    printf("Max Namsize : %u\n",NAMESIZE);
    printf("Max Inodes per block : %u\n",INODES_PER_BLOCK);
    printf("Max Entries per directory : %u\n\n",ENTRIES_PER_DIR);

    /**- Read directory blocks */
    for(uint32_t blk_idx=0; blk_idx<MetaData.DirBlocks; blk_idx++){
        fs_disk->read(MetaData.Blocks - 1- blk_idx,blk.Data);
        printf("Block %u\n",blk_idx);

        /**- Read Directoreis in each directory block */
        for(uint32_t offset=0; offset<DIR_PER_BLOCK; offset++){
            Directory dir = blk.Directories[offset]; 
            if(dir.Valid){
                printf("    Offset %u: Directory Name - \"%s\"\n",offset,dir.Name);

                /**- Read Table Entries for each directory */
                for(uint32_t tbl_idx=0; tbl_idx < ENTRIES_PER_DIR; tbl_idx++){
                    Dirent ent = dir.Table[tbl_idx];
                    if(ent.valid){
                        printf("        tbl_idx %u: Entry Name - \"%s\", type - %u, inum - %u\n",tbl_idx,ent.Name,ent.type,ent.inum);
                    }
                }
            }
        }
    }
}