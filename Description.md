File-System-Checker
===================

Time Span => 1 Month; 

Primary Source File => readwrite.c;

Support Files => ext2_fs.h, genhd.h, makefile;

Description:

FSCK (File System Checker Tool)

This tool is used to read, parse, manipulate and correct the on-disk image of an EXT2 file system. It runs 4 passes on the disk image and corrects for the following errors.

• Pass 1=> Directory pointers:
Verify for each directory: that the first directory entry is “.” and it self-references, and that the second directory entry is “..”  and it references its parent inode. If you find an error, notify the user and correct the entry.

• Pass 2=> Unreferenced inodes:
Check to make sure all allocated inodes are referenced in a directory entry somewhere. If you find an unreferenced inode, place it in the /lost+found directory—make the new filename the same as the inode number. (I.e., if the unreferenced inode has number 1074, make it the file or directory /lost+found/1074.)

• Pass 3=> Inode link count:
Count the number of directory entries that point to each inode (e.g., the number of hard links) and compare that to the inode link counter. If you find a discrepancy, notify the user and update the inode link counter.

• Pass 4=> Block allocation bitmap:
Walk the directory tree and verify that the block bitmap is correct. If you find a block that should (or should not) be marked in the bitmap, notify the user and correct the bitmap.

Additional Functionality:
• Prints out the partition table information

• Prints out the superblock information
