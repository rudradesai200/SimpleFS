# Project Report

_Implementation of RAM File System_

![](RackMultipart20201209-4-1kxbehg_html_2258041eab0c04c2.png)

Rudra Desai (CS18B012) &amp; Kairav Shah (CS18B071)

07/12/2020

CS3500 Course Project

# INTRODUCTION

The file system is a layer of abstraction between the storage medium and the operating system. In the absence of a file system, it is tough to retrieve and manage data stored in the storage media. File systems provide an interface to access and control the data. It is implemented by separating data into chunks of predetermined size and labeling them for better management and operability. There are several types of file systems available out there. Some major examples are as follows: Disk-Based, Network-Based, and Virtual based.

Here, we provide a simple implementation of a disk-based file system. A custom shell is also provided to access the file system. The shell (sfs) and the disk emulator were pre-implemented by the University of Notre Dame. Our task was to build upon the given tools and implement the **File System layer** and, in doing so, bridge the gap between shell and disk emulator.

# IMPLEMENTATION DETAILS

- Please [click here](https://rudradesai200.github.io/SimpleFS/) for implementation details. (In the rare case that the hyperlink does not work - [https://rudradesai200.github.io/SimpleFS/](https://rudradesai200.github.io/SimpleFS/) )
- The report has been generated automatically by **Doxygen** using the comments added throughout the code.

# RESULTS

- Passed all the tests provided by the University of Notre Dame (run &quot;make test&quot;).
- Added support for password protection.
- Added support for directories and files.

![](RackMultipart20201209-4-1kxbehg_html_bcf940216c5ae8f9.png)

# REFERENCES

1. [https://www3.nd.edu/~pbui/teaching/cse.30341.fa17/project06.html](https://www3.nd.edu/~pbui/teaching/cse.30341.fa17/project06.html) - Notre Dame University for providing the project and the boilerplate code
2. [https://drive.google.com/file/d/1oL5cUGzU7IchOZO\_Jx7R9j8HqGxJ-wDX/view](https://drive.google.com/file/d/1oL5cUGzU7IchOZO_Jx7R9j8HqGxJ-wDX/view) - File System lecture slides by Dr. Chester Rebeiro
3. [https://en.wikipedia.org/wiki/File\_system](https://en.wikipedia.org/wiki/File_system) - Wikipedia entry for File Systems

# CONTRIBUTIONS:

- **Kairav Shah:** implemented the core functions: debug, format, mount, create, remove, write, read.
- **Rudra Desai:** added extra functionalities to the file system: password protection, several Linux commands, support for directories and files.

####
