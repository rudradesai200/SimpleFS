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

FileSystem::Directory FileSystem::add_dir_entry(Directory dir, uint32_t inum, uint32_t type, char name[]){
    // This will add directory entry to current directory
    // But it won't write back to the disk.
    // dir write back should be done by the caller

    struct Dirent temp;
    temp.inum = inum;
    temp.type = type;
    temp.valid = 1;
    strcpy(temp.Name,name);

    uint32_t idx = 0;
    for(; idx < FileSystem::ENTRIES_PER_DIR; idx++){
        if(dir.Table[idx].valid == 0){break;}
    }
    if(idx == FileSystem::ENTRIES_PER_DIR){
        printf("Directory entry limit reached..exiting\n");
        dir.Valid = 0;
        return dir;
    }
    dir.Table[idx] = temp;
    return dir;
}

FileSystem::Directory FileSystem::read_dir_from_offset(uint32_t offset){
    // Sanity Check
    if((curr_dir.Table[offset].valid == 0) || (curr_dir.Table[offset].type != 0)){Directory temp; temp.Valid=0; return temp;}

    // Get offsets and indexes
    uint32_t inum = curr_dir.Table[offset].inum;
    uint32_t block_idx = (inum / FileSystem::DIR_PER_BLOCK);
    uint32_t block_offset = (inum % FileSystem::DIR_PER_BLOCK);
    
    // Read Block
    Block blk;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, blk.Data);
    return blk.Directories[block_offset];
}

void FileSystem::write_dir_back(Directory dir){
    // Get block offset and index
    uint32_t block_idx = (dir.inum / FileSystem::DIR_PER_BLOCK) ;
    uint32_t block_offset = (dir.inum % FileSystem::DIR_PER_BLOCK);

    // Read Block
    Block block;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, block.Data);
    block.Directories[block_offset] = dir;

    // Write the Dirblock
    fs_disk->write(MetaData.Blocks - 1 - block_idx, block.Data);
}

int FileSystem::curr_dir_lookup(char name[]){
    // Search the current dir table for name
    uint32_t offset = 0;
    for(;offset < FileSystem::ENTRIES_PER_DIR; offset++){
        if(
            (curr_dir.Table[offset].valid) &&
            (streq(curr_dir.Table[offset].Name,name))
        ){break;}
    }

    // No such dir found
    if(offset == FileSystem::ENTRIES_PER_DIR){return -1;}

    return offset;
}

bool FileSystem::ls_dir(char name[]){
    // Get the directory entry offset 
    int offset = curr_dir_lookup(name);
    if(offset == -1){return false;}

    // Read directory from block
    FileSystem::Directory dir = read_dir_from_offset(offset);

    // Sanity checks
    if(!(dir.Valid)){return false;}

    // Print Directory Data
    printf("   inum    |       name       | type\n");
    for(uint32_t idx=0;idx<FileSystem::ENTRIES_PER_DIR;idx++){
        struct Dirent temp = dir.Table[idx];
        if(temp.valid == 1){
            if(temp.type == 1) printf("%-10u | %-16s | %-5s\n",temp.inum,temp.Name, "file");
            else printf("%-10u | %-16s | %-5s\n",temp.inum,temp.Name, "dir");
        }
    }
    return true;
}

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
    strcpy(new_dir.Name,name);
    
    // Create 2 new entries for "." and ".."
    char tstr1[] = ".", tstr2[] = "..";
    new_dir = add_dir_entry(new_dir,new_dir.inum,0,tstr1);
    new_dir = add_dir_entry(new_dir,curr_dir.inum,0,tstr2);
    if(new_dir.Valid == 0){return false;}

    // Write the new directory back to the disk
    write_dir_back(new_dir);
    
    // Add new entry to the curr_dir
    Directory temp = add_dir_entry(curr_dir,new_dir.inum,0,name);
    if(temp.Valid == 0){return false;}
    curr_dir = temp;
    
    // Write the curr_dir back to the disk
    write_dir_back(curr_dir);

    // Increment the counter
    dir_counter[block_idx]++;

    return true;
    
}

bool FileSystem::rmdir(char name[FileSystem::NAMESIZE]){
    // Find the directory
    int offset = curr_dir_lookup(name);
    if(offset == -1){return false;}

    // Update the dirblock
    Block block;
    uint32_t block_idx = curr_dir.Table[offset].inum / FileSystem::DIR_PER_BLOCK;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, block.Data);
    block.Directories[curr_dir.Table[offset].inum % FileSystem::DIR_PER_BLOCK].Valid = 0;
    fs_disk->write(MetaData.Blocks - 1 - block_idx, block.Data);

    // Decrement dir counter
    dir_counter[curr_dir.Table[offset].inum / FileSystem::DIR_PER_BLOCK]--;

    // Update curr_dir
    curr_dir.Table[offset].valid = 0;
    write_dir_back(curr_dir);
    return true;
}

bool FileSystem::touch(char name[FileSystem::NAMESIZE]){
    // Check if such file exists
    for(uint32_t offset=0; offset<FileSystem::ENTRIES_PER_DIR; offset++){
        if(curr_dir.Table[offset].valid){
            if(streq(curr_dir.Table[offset].Name,name)){
                return false;
            }
        }
    }
    // Allocate new inode for the file
    ssize_t new_node_idx = FileSystem::create();
    if(new_node_idx == -1){return false;}

    // Add the directory entry in curr_directory
    Directory temp = add_dir_entry(curr_dir,new_node_idx,1,name);
    if(temp.Valid == 0){return false;}
    curr_dir = temp;
    
    // Write back the changes
    write_dir_back(curr_dir);

    return true;
}

bool FileSystem::cd(char name[FileSystem::NAMESIZE]){
    int offset = curr_dir_lookup(name);
    if(offset == -1){return false;}

    // Read the dirblock from the disk
    curr_dir = read_dir_from_offset(offset);
    if(curr_dir.Valid == 0){return false;}
    return true;
}

bool FileSystem::ls(){
    char name[] = ".";
    return ls_dir(name);
}

bool FileSystem::rm(char name[]){
    // Get the offset for removal
    int offset = curr_dir_lookup(name);
    if(offset == -1){return false;}

    // Check if directory
    if(curr_dir.Table[offset].type == 0){
        return rmdir(name);
    }

    // Get number
    uint32_t inum = curr_dir.Table[offset].inum;

    // Remove the inode
    if(!remove(inum)){return false;}

    // Remove the entry
    curr_dir.Table[offset].valid = 0;

    // Write back the changes
    write_dir_back(curr_dir);

    return true;
}

void FileSystem::exit(){
    fs_disk->unmount();
}

bool FileSystem::copyout(char name[],const char *path) {
    int offset = curr_dir_lookup(name);
    if(offset == -1){return false;}

    if(curr_dir.Table[offset].type == 0){return false;}

    uint32_t inum = curr_dir.Table[offset].inum;

    FILE *stream = fopen(path, "w");
    if (stream == nullptr) {
    	fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
    	return false;
    }

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
    
    printf("%d bytes copied\n", offset);
    fclose(stream);
    return true;
}

bool FileSystem::copyin(const char *path, char name[]) {
    touch(name);
    int offset = curr_dir_lookup(name);
    if(offset == -1){return false;}

    if(curr_dir.Table[offset].type == 0){return false;}

    uint32_t inum = curr_dir.Table[offset].inum;

	FILE *stream = fopen(path, "r");
    if (stream == nullptr) {
    	fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
    	return false;
    }

    char buffer[4*BUFSIZ] = {0};
    offset = 0;
    while (true) {
    	ssize_t result = fread(buffer, 1, sizeof(buffer), stream);
    	if (result <= 0) {
    	    break;
	}

	ssize_t actual = write(inum, buffer, result, offset);
	if (actual < 0) {
	    fprintf(stderr, "fs.write returned invalid result %ld\n", actual);
	    break;
	}
	offset += actual;
	if (actual != result) {
	    fprintf(stderr, "fs.write only wrote %ld bytes, not %ld bytes\n", actual, result);
	    break;
	}
    }
    
    printf("%d bytes copied\n", offset);
    fclose(stream);
    return true;
}