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
    /** Sanity checks */
    if(!mounted){return false;}
    if(MetaData.Protected) return change_password();

    /** Initializations */
    SHA256 hasher;
    char pass[1000], line[1000];
    Block block;

    /** Get new password */
    printf("Enter new password: ");
    if (fgets(line, BUFSIZ, stdin) == NULL) return false;
    sscanf(line, "%s", pass);
    
    /** Set cached MetaData to Protected */
    MetaData.Protected = 1;
    strcpy(MetaData.PasswordHash,hasher(pass).c_str());
    
    // Write chanes back to the disk */
    block.Super = MetaData;
    fs_disk->write(0,block.Data);
    printf("New password set.\n");
    return true;
}

bool FileSystem::change_password(){
    if(!mounted){return false;}

    if(MetaData.Protected){
        /** Initializations */
        char pass[1000], line[1000];
        SHA256 hasher;

        /** Get curr password */
        printf("Enter current password: ");
        if (fgets(line, BUFSIZ, stdin) == NULL) return false;
        sscanf(line, "%s", pass);
        
        /** Check password */
        if(hasher(pass) != string(MetaData.PasswordHash)){
            printf("Old password incorrect.\n");
            return false;
        }

        /** Store it in cached MetaData */
        MetaData.Protected = 0;
    }
    return FileSystem::set_password();   
}

bool FileSystem::remove_password(){
    if(!mounted){return false;}

    if(MetaData.Protected){
        /** Initializations */
        char pass[1000], line[1000];
        SHA256 hasher;
        Block block;
        
        /** Get current password */
        printf("Enter old password: ");
        if (fgets(line, BUFSIZ, stdin) == NULL) return false;
        sscanf(line, "%s", pass);
        
        /** Curr password incorrect. Error */
        if(hasher(pass) != string(MetaData.PasswordHash)){
            printf("Old password incorrect.\n");
            return false;
        }
        
        /** Update cached MetaData */
        MetaData.Protected = 0;
        
        /** Write back the changes */
        block.Super = MetaData;
        fs_disk->write(0,block.Data);   
        printf("Password removed successfully.\n");
        
        return true;
    }
    return false;
}

FileSystem::Directory FileSystem::add_dir_entry(Directory dir, uint32_t inum, uint32_t type, char name[]){
    /** This will add Dirent to current Directory
     But it won't write back to the disk.
     dir write back should be done by the caller */

    Directory tempdir = dir;

    /** Find a free Dirent */
    uint32_t idx = 0;
    for(; idx < FileSystem::ENTRIES_PER_DIR; idx++){
        if(tempdir.Table[idx].valid == 0){break;}
    }

    /** If No Dirent Found, Error */
    if(idx == FileSystem::ENTRIES_PER_DIR){
        printf("Directory entry limit reached..exiting\n");
        tempdir.Valid = 0;
        return tempdir;
    }

    /** Free Dirent found, add the new one to the table */ 
    tempdir.Table[idx].inum = inum;
    tempdir.Table[idx].type = type;
    tempdir.Table[idx].valid = 1;
    strcpy(tempdir.Table[idx].Name,name);

    return tempdir;
}

FileSystem::Directory FileSystem::read_dir_from_offset(uint32_t offset){
    /**  Sanity Check */
    if(
        (offset < 0) ||
        (offset >= ENTRIES_PER_DIR) ||
        (curr_dir.Table[offset].valid == 0) || 
        (curr_dir.Table[offset].type != 0)
    ){Directory temp; temp.Valid=0; return temp;}

    /**  Get offsets and indexes */
    uint32_t inum = curr_dir.Table[offset].inum;
    uint32_t block_idx = (inum / FileSystem::DIR_PER_BLOCK);
    uint32_t block_offset = (inum % FileSystem::DIR_PER_BLOCK);
    
    /**  Read Block */
    Block blk;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, blk.Data);
    return (blk.Directories[block_offset]);
}

void FileSystem::write_dir_back(Directory dir){
    /**  Get block offset and index */
    uint32_t block_idx = (dir.inum / FileSystem::DIR_PER_BLOCK) ;
    uint32_t block_offset = (dir.inum % FileSystem::DIR_PER_BLOCK);

    /**  Read Block */
    Block block;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, block.Data);
    block.Directories[block_offset] = dir;

    /**  Write the Dirblock */
    fs_disk->write(MetaData.Blocks - 1 - block_idx, block.Data);
}

int FileSystem::dir_lookup(Directory dir,char name[]){
    /**  Search the curr_dir.Table for name */
    uint32_t offset = 0;
    for(;offset < FileSystem::ENTRIES_PER_DIR; offset++){
        if(
            (dir.Table[offset].valid == 1) &&
            (streq(dir.Table[offset].Name,name))
        ){break;}
    }

    /**  No such dir found */
    if(offset == FileSystem::ENTRIES_PER_DIR){return -1;}

    return offset;
}

bool FileSystem::ls_dir(char name[]){
    if(!mounted){return false;}

    /**  Get the directory entry offset  */
    int offset = dir_lookup(curr_dir,name);
    if(offset == -1){printf("No such Directory\n"); return false;}

    /**  Read directory from block */
    FileSystem::Directory dir = read_dir_from_offset(offset);

    /**  Sanity checks */
    if(dir.Valid == 0){printf("Directory Invalid\n"); return false;}

    /**  Print Directory Data */
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
    if(!mounted){return false;}

    /**  Find empty dirblock */
    uint32_t block_idx = 0;
    for(;block_idx < MetaData.DirBlocks; block_idx++)
        if(dir_counter[block_idx] < FileSystem::DIR_PER_BLOCK)
            break;

    if(block_idx == MetaData.DirBlocks){printf("Directory limit reached\n"); return false;}

    /**  Read empty dirblock */
    Block block;
    fs_disk->read(MetaData.Blocks - 1 - block_idx, block.Data);


    /**  Find empty directory in dirblock */
    uint32_t offset=0;
    for(;offset < FileSystem::DIR_PER_BLOCK; offset++)
        if(block.Directories[offset].Valid == 0)
            break;
    
    if(offset == DIR_PER_BLOCK){printf("Error in creating directory.\n"); return false;}

    /**  Create new directory */
    Directory new_dir, temp;
    memset(&new_dir,0,sizeof(Directory));
    new_dir.inum = block_idx*DIR_PER_BLOCK + offset;
    new_dir.Size = 0;
    new_dir.Valid = 1;
    strcpy(new_dir.Name,name);
    
    /**  Create 2 new entries for "." and ".." */
    char tstr1[] = ".", tstr2[] = "..";
    temp = new_dir;
    temp = add_dir_entry(temp,temp.inum,0,tstr1);
    temp = add_dir_entry(temp,curr_dir.inum,0,tstr2);
    if(temp.Valid == 0){printf("Error creating new directory\n"); return false;}
    new_dir = temp;

    /**  Write the new directory back to the disk */
    write_dir_back(new_dir);
    
    /**  Add new entry to the curr_dir */
    temp = add_dir_entry(curr_dir,new_dir.inum,0,new_dir.Name);
    if(temp.Valid == 0){printf("Error adding new directory\n"); return false;}
    curr_dir = temp;
    
    /**  Write the curr_dir back to the disk */
    write_dir_back(curr_dir);

    /**  Increment the counter */
    dir_counter[block_idx]++;

    return true;
    
}

FileSystem::Directory FileSystem::rmdir_helper(Directory parent, char name[]){
    
    /** initializations */
    Directory dir, temp;
    uint32_t inum, blk_idx, blk_off;
    Block blk;

    /** Sanity Checks */
    if(!mounted){dir.Valid = 0; return dir;}

    /** Get offset of the directory to be removed */
    int offset = dir_lookup(parent, name);
    if(offset == -1){dir.Valid = 0; return dir;}

    /** Get block */
    inum = parent.Table[offset].inum;
    blk_idx = inum / DIR_PER_BLOCK;
    blk_off = inum % DIR_PER_BLOCK;
    fs_disk->read(MetaData.Blocks - 1 -blk_idx, blk.Data);

    /** Check Directory */
    dir = blk.Directories[blk_off];
    if(dir.Valid == 0){return dir;}

    /** Remove all Dirent in the directory to be removed */
    for(uint32_t ii=0; ii<ENTRIES_PER_DIR; ii++){
        if((ii>1)&&(dir.Table[ii].valid == 1)){
            temp = rm_helper(dir, dir.Table[ii].Name);
            if(temp.Valid == 0) return temp;
            dir = temp;
        }
        dir.Table[ii].valid = 0;
    }

    /** Read the block again, because the block may have changed by Dirent */
    fs_disk->read(MetaData.Blocks - 1 -blk_idx, blk.Data);

    /** Write it back */
    dir.Valid = 0;
    blk.Directories[blk_off] = dir;
    fs_disk->write(MetaData.Blocks - 1-blk_idx, blk.Data);

    /** Remove it from the parent */
    parent.Table[offset].valid = 0;
    write_dir_back(parent);

    /** Update the counter */
    dir_counter[blk_idx]--;

    return parent;
}

FileSystem::Directory FileSystem::rm_helper(Directory dir, char name[FileSystem::NAMESIZE]){
    
    if(!mounted){dir.Valid = 0; return dir;}

    /**  Get the offset for removal */
    int offset = dir_lookup(dir,name);
    if(offset == -1){printf("No such file/directory\n"); dir.Valid=0; return dir;}

    /**  Check if directory */
    if(dir.Table[offset].type == 0){
        return rmdir_helper(dir,name);
    }

    /**  Get inumber */
    uint32_t inum = dir.Table[offset].inum;

    printf("%u\n",inum);

    /**  Remove the inode */
    if(!remove(inum)){printf("Failed to remove Inode\n"); dir.Valid = 0; return dir;}

    /**  Remove the entry */
    dir.Table[offset].valid = 0;

    /**  Write back the changes */
    write_dir_back(dir);

    return dir;
}

bool FileSystem::rmdir(char name[FileSystem::NAMESIZE]){
    Directory temp = rmdir_helper(curr_dir,name);
    if(temp.Valid == 0){
        curr_dir = temp;
        return true;
    }
    return false;
}

bool FileSystem::touch(char name[FileSystem::NAMESIZE]){
    if(!mounted){return false;}

    /**  Check if such file exists */
    for(uint32_t offset=0; offset<FileSystem::ENTRIES_PER_DIR; offset++){
        if(curr_dir.Table[offset].valid){
            if(streq(curr_dir.Table[offset].Name,name)){
                printf("File already exists\n");
                return false;
            }
        }
    }
    /**  Allocate new inode for the file */
    ssize_t new_node_idx = FileSystem::create();
    if(new_node_idx == -1){printf("Error creating new inode\n"); return false;}

    /**  Add the directory entry in the curr_directory */
    Directory temp = add_dir_entry(curr_dir,new_node_idx,1,name);
    if(temp.Valid == 0){printf("Error adding new file\n"); return false;}
    curr_dir = temp;
    
    /**  Write back the changes */
    write_dir_back(curr_dir);

    return true;
}

bool FileSystem::cd(char name[FileSystem::NAMESIZE]){
    if(!mounted){return false;}

    int offset = dir_lookup(curr_dir,name);
    if((offset == -1) || (curr_dir.Table[offset].type == 1)){
        printf("No such directory\n");
        return false;
    }

    /**  Read the dirblock from the disk */
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
    if(!mounted){return false;}

    int offset = dir_lookup(curr_dir,name);
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
    if(!mounted){return false;}

    touch(name);
    int offset = dir_lookup(curr_dir,name);
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