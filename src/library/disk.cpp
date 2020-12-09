/*!
 * @file disk.cpp
 * @author University of Notre Dame (https://www.nd.edu/)
 * @brief Implementation of disk.h functions
 * @date 2017
 * 
 */

#include "sfs/disk.h"

#include <stdexcept>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

void Disk::open(const char *path, size_t nblocks) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- Open Filedescriptor  */
    FileDescriptor = ::open(path, O_RDWR|O_CREAT, 0600); 
    
    /**- Check if FileDescriptor is valid */
    if (FileDescriptor < 0) {
    	char what[BUFSIZ];
    	snprintf(what, BUFSIZ, "Unable to open %s: %s", path, strerror(errno));
    	throw std::runtime_error(what);
    }

    /**- Allocated blocks */
    if (ftruncate(FileDescriptor, nblocks*BLOCK_SIZE) < 0) {
    	char what[BUFSIZ];
    	snprintf(what, BUFSIZ, "Unable to open %s: %s", path, strerror(errno));
    	throw std::runtime_error(what);
    }

    /**- Initialize the disk */
    Blocks = nblocks;
    Reads  = 0;
    Writes = 0;
}

Disk::~Disk() {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- Check if FileDescriptor is set */
    if (FileDescriptor > 0) {
        /**- If set, print the required information and close. */
    	printf("%lu disk block reads\n", Reads);
    	printf("%lu disk block writes\n", Writes);
    	close(FileDescriptor);
    	FileDescriptor = 0;
    }
}

void Disk::sanity_check(int blocknum, char *data) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    char what[BUFSIZ]; /**- reason for exit. */

    /**- Check if block num is valid or not.  */
    if (blocknum < 0) {
    	snprintf(what, BUFSIZ, "blocknum (%d) is negative!", blocknum);
    	throw std::invalid_argument(what);
    }

    if (blocknum >= (int)Blocks) {
    	snprintf(what, BUFSIZ, "blocknum (%d) is too big!", blocknum);
    	throw std::invalid_argument(what);
    }

    /**- Check if data pointer is valid */
    if (data == NULL) {
    	snprintf(what, BUFSIZ, "null data pointer!");
    	throw std::invalid_argument(what);
    }
}

void Disk::read(int blocknum, char *data) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity_check blocknum and data */
    sanity_check(blocknum, data);

    /**- reposition read/write file offset and check validity */
    if (lseek(FileDescriptor, blocknum*BLOCK_SIZE, SEEK_SET) < 0) {
    	char what[BUFSIZ];
    	snprintf(what, BUFSIZ, "Unable to lseek %d: %s", blocknum, strerror(errno));
    	throw std::runtime_error(what);
    }

    /**- read the FileDescriptor for BLOCK_SIZE */
    if (::read(FileDescriptor, data, BLOCK_SIZE) != BLOCK_SIZE) {
    	char what[BUFSIZ];
    	snprintf(what, BUFSIZ, "Unable to read %d: %s", blocknum, strerror(errno));
    	throw std::runtime_error(what);
    }

    /**- Increment reads */
    Reads++;
}

void Disk::write(int blocknum, char *data) {
    /** <dl class="section implementation"> */
    /** <dt> Implementation details </dt>*/
    /** </dl> */

    /**- sanity_check blocknum and data */
    sanity_check(blocknum, data);

    /**- reposition read/write file offset and check validity */
    if (lseek(FileDescriptor, blocknum*BLOCK_SIZE, SEEK_SET) < 0) {
    	char what[BUFSIZ];
    	snprintf(what, BUFSIZ, "Unable to lseek %d: %s", blocknum, strerror(errno));
    	throw std::runtime_error(what);
    }

    /**- write the BLOCK_SIZE data to the FileDescriptor  */
    if (::write(FileDescriptor, data, BLOCK_SIZE) != BLOCK_SIZE) {
    	char what[BUFSIZ];
    	snprintf(what, BUFSIZ, "Unable to write %d: %s", blocknum, strerror(errno));
    	throw std::runtime_error(what);
    }

    /**- increment writes */
    Writes++;
}
