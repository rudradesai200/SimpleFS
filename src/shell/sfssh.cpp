// sfssh.cpp: Simple file system shell

#include "sfs/disk.h"
#include "sfs/fs.h"

#include <sstream>
#include <string>
#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Macros

#define streq(a, b) (strcmp((a), (b)) == 0)

// Command prototypes

void do_debug(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_format(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_mount(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_help(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_password(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_mkdir(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_rmdir(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_touch(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_rm(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_file_copyout(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_file_copyin(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_cd(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_ls(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_stat(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);


int main(int argc, char *argv[]) {
    Disk	disk;
    FileSystem	fs;

    if (argc != 3) {
    	fprintf(stderr, "Usage: %s <diskfile> <nblocks>\n", argv[0]);
    	return EXIT_FAILURE;
    }

    try {
    	disk.open(argv[1], atoi(argv[2]));
    } catch (std::runtime_error &e) {
    	fprintf(stderr, "Unable to open disk %s: %s\n", argv[1], e.what());
    	return EXIT_FAILURE;
    }

    while (true) {
	char line[BUFSIZ], cmd[BUFSIZ], arg1[BUFSIZ], arg2[BUFSIZ];

    	fprintf(stderr, "sfs> ");
    	fflush(stderr);

    	if (fgets(line, BUFSIZ, stdin) == NULL) {
    	    break;
    	}

    	int args = sscanf(line, "%s %s %s", cmd, arg1, arg2);
    	if (args == 0) {
    	    continue;
	}

	if (streq(cmd, "debug")) {
	    do_debug(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "format")) {
	    do_format(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "mount")) {
	    do_mount(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "help")) {
	    do_help(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "password")) {
	    do_password(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "mkdir")) {
	    do_mkdir(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "rmdir")) {
	    do_rmdir(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "touch")) {
	    do_touch(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "rm")) {
	    do_rm(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "cd")) {
	    do_cd(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "ls")) {
	    do_ls(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "stat")) {
	    do_stat(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "copyout")) {
	    do_file_copyout(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "copyin")) {
	    do_file_copyin(disk, fs, args, arg1, arg2);
	} else if (streq(cmd, "exit") || streq(cmd, "quit")) {
	    fs.exit();
		break;
	} else {
	    printf("Unknown command: %s", line);
	    printf("Type 'help' for a list of commands.\n");
	}
    }

    return EXIT_SUCCESS;
}

// Command functions

void do_debug(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
    	printf("Usage: debug\n");
    	return;
    }

    fs.debug(&disk);
}

void do_format(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
    	printf("Usage: format\n");
    	return;
    }

    if (fs.format(&disk)) {
    	printf("disk formatted.\n");
    } else {
    	printf("format failed!\n");
    }
}

void do_mount(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
    	printf("Usage: mount\n");
    	return;
    }

    if (fs.mount(&disk)) {
    	printf("disk mounted.\n");
    } else {
    	printf("mount failed!\n");
    }
}

void do_password(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2){
	if(args != 2){
		printf("Usage: password <change|set|remove>\n");
		return;
	}
	
	bool ret;
	if(streq(arg1,"change")){
		ret = fs.change_password();
	} else if (streq(arg1, "set")){
		ret = fs.set_password();
	} else if (streq(arg1, "remove")){
		ret = fs.remove_password();
	} else {
		printf("Usage: password <change|set|remove>\n");
		return;
	}

	if(!ret){
		printf("password %s failed!\n",arg1);
	}
}

void do_mkdir(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("Usage: mkdir <dirname>\n");
    	return;
    }

	if(!fs.mkdir(arg1)){
		printf("mkdir failed\n");
	}
}

void do_rmdir(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("Usage: rmdir <dirname>\n");
    	return;
    }

	if(!fs.rmdir(arg1)){
		printf("rmdir failed\n");
	}
}

void do_touch(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("Usage: touch <name>\n");
    	return;
    }

	if(!fs.touch(arg1)){
		printf("touch failed\n");
	}
}

void do_rm(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("Usage: rm <name>\n");
    	return;
    }

	if(!fs.rm(arg1)){
		printf("rm failed\n");
	}
}

void do_file_copyout(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
	if (args != 3) {
    	printf("Usage: copyout <filename> <path>\n");
    	return;
    }

	if(!fs.copyout(arg1,arg2)){
		printf("copyout failed\n");
	}
}
void do_file_copyin(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
	if(args != 3){
		printf("Usage: copyin <path> <filename>\n");
	}

	if(!fs.copyin(arg1,arg2)){
		printf("copyin failed\n");
	}
}

void do_cd(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("Usage: cd <dirname>\n");
    	return;
    }

	if(!fs.cd(arg1)){
		printf("cd failed\n");
	}
}

void do_ls(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (!((args == 1) || (args == 2))) {
    	printf("Usage: ls <dirname>\n");
    	return;
    }
	if(args == 1){
		if(!fs.ls()){
			printf("ls failed\n");
		}
	}
	else{
		if(!fs.ls_dir(arg1)){
			printf("ls failed\n");
		}
	}
}

void do_stat(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if ((args != 1)) {
    	printf("Usage: stat\n");
    	return;
    }
	fs.stat();
}

void do_help(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    printf("Commands are:\n");
    printf("    format\n");
    printf("    mount\n");
    printf("    debug\n");
	printf("    password <change|set|remove>\n");
	printf("    mkdir <dirname>\n");
	printf("    rmdir <dirname>\n");
	printf("    cd <dirname>\n");
	printf("    ls <dirname>\n");
	printf("    stat\n");
	printf("    touch <filename>\n");
	printf("    rm <name>\n");
	printf("    copyout <filename> <path>\n");
	printf("    copyin <path> <filename>\n");
    printf("    help\n");
    printf("    quit\n");
    printf("    exit\n");
}