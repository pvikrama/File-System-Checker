/* $cmuPDL: readwrite.c,v 1.3 2010/02/27 11:38:39 rajas Exp $ */

/* readwrite.c
 * author: Pradeep Kumar Vikraman(pvikrama@andrew.cmu.edu)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>	/* for memcpy() */
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include "genhd.h"
#include "ext2_fs.h"

#if defined(__FreeBSD__)
#define lseek64 lseek
#endif

#define entries_per_block 256

extern int64_t lseek64(int, int64_t, int);
struct stack
{
	int present_inode;
	int parent_inode;
};
struct stack stackarr[2008];

struct isector
{
	int sector_num;
	int offset_num;
};
struct isector inode_sector;

void filltable(int, int);
void superblock(void);
int inode_sect(int, int);
int pop();
void group_desc(void);
void push(int, int, struct stack *);
int traversal(int inode, char *pass);
void update_parent_lost_found(int);
void insert_lost_found(int);
void global_lost_found();
void write_bytes(int64_t, unsigned int, int, void *);
void correct_present_parent(int, int, unsigned int);
void check_link_count(int, int , int);
void inode_bitmap(char *);
void check_metadata_blocks(void);
void traverse_blocks(int);
void check_block_map(int);
void run_all_passes(int);
void single_indirect(int);
void double_indirect(int);
void triple_indirect(int);

int block_size = 0;
struct ext2_inode lostfound;
struct ext2_dir_entry_2 add_lost;
const unsigned int sector_size__bytes=512;
struct ext2_dir_entry_2 prev;
struct partition *part;
struct ext2_super_block *sblock;
struct ext2_group_desc *g_gdesc;
int device;  
int top = 0;
int counting = 0;
char *pass1 = "pass1";
char *pass2 = "pass2";
char *pass3 = "pass3";
char *pass4 = "pass4";

#define group_descriptor_size sizeof(struct ext2_group_desc) * ((sblock->s_blocks_count/sblock->s_blocks_per_group)+1)

/* print_sector: print the contents of a buffer containing one sector.
 *
 * inputs:
 *   char *buf: buffer must be >= 512 bytes.

 * outputs:
 *   the first 512 bytes of char *buf are printed to stdout.
 *
 * modifies:
 *   (none)
 */
	void
print_sector (unsigned char *buf)
{
	int i;
	for (i=0; i<512; i++) {
		printf("%02x", buf[i]);
		if (!((i+1)%32)) printf("\n");	/* line break after 32 bytes */
		else if (!((i+4)%1)) printf(" ");	/* space after 4 bytes */
	}
}


/* print_bytes: print the number of bytes requested from the buffer.
 *
 * inputs:
 *   char *buf: buffer must be >= 512 bytes.
 *   int numbytes: number of bytes to be copied
 * outputs:
 *   the first 512 bytes of char *buf are printed to stdout.
 *
 */
	void
print_bytes (unsigned char *buf, int numbytes)
{
	int i;
	for (i=0; i<numbytes; i++) {
		printf("%02x", buf[i]);
		if (!((i+1)%32)) printf("\n");	/* line break after 32 bytes */
		else if (!((i+1)%1)) printf(" ");	/* space after 4 bytes */
	}
}

/* read_sectors: read a specified number of sectors into a buffer.
 *
 * inputs:
 *   int64 block: the starting sector number to read.
 *                sector numbering starts with 0.
 *   int numsectors: the number of sectors to read.  must be >= 1.
 *   int device [GLOBAL]: the disk from which to read.
 *
 * outputs:
 *   void *into: the requested number of blocks are copied into here.
 *
 */
	void
read_sectors (int64_t block, unsigned int numsectors,  void *into)
{
	int ret;
	int64_t lret;

	if ((lret = lseek64(device, block * sector_size__bytes, SEEK_SET)) 
			!= block * sector_size__bytes) {
		fprintf(stderr, "Seek to position %"PRId64" failed: returned %"PRId64"\n", 
				block * sector_size__bytes, lret);
		exit(-1);
	}
	if ((ret = read(device, into, sector_size__bytes * numsectors)) 
			!= sector_size__bytes * numsectors) {
		fprintf(stderr, "Read block %"PRId64" length %d failed: returned %"PRId64"\n", 
				block, numsectors, ret);
		exit(-1);
	}
}


/* read_bytes: read a specified number of bytes from the given offset into a buffer.
 *
 * inputs:
 *   int64 block: the starting sector number to read.
 *                sector numbering starts with 0.
 *   int numbytes: the number of bytes to read.  must be >= 1.
 *   int offset: the offset in that sector from which the read is performed
 *   int device [GLOBAL]: the disk from which to read.
 *
 * outputs:
 *   void *into: the requested number of blocks are copied into here.
 *
 */
	void
read_bytes (int64_t block, unsigned int numbytes, unsigned int offset,  void *into)
{
	int ret;
	int64_t lret;

	if ((lret = lseek64(device, (block * sector_size__bytes)+offset, SEEK_SET)) 
			!= block * sector_size__bytes+offset) {
		fprintf(stderr, "Seek to position %"PRId64" failed: returned %"PRId64"\n", 
				block * sector_size__bytes, lret);
		exit(-1);
	}
	if ((ret = read(device, into, numbytes)) 
			!= numbytes) {
		fprintf(stderr, "Read block %"PRId64" length %d failed: returned %"PRId64"\n", 
				block, numbytes, ret);
		exit(-1);
	}
}


/* write_sectors: write a buffer into a specified number of sectors.
 *
 * inputs:
 *   int64 block: the starting sector number to write.
 *                sector numbering starts with 0.
 *   int numsectors: the number of sectors to write.  must be >= 1.
 *   void *from: the requested number of blocks are copied from here.
 *
 * outputs:
 *   int device [GLOBAL]: the disk into which to write.
 *
 */
	void
write_sectors (int64_t block, unsigned int numsectors, void *from)
{
	int ret;
	int64_t lret;

	if ((lret = lseek64(device, block * sector_size__bytes, SEEK_SET)) 
			!= block * sector_size__bytes) {
		fprintf(stderr, "Seek to position %"PRId64" failed: returned %"PRId64"\n", 
				block * sector_size__bytes, lret);
		exit(-1);
	}
	if ((ret = write(device, from, sector_size__bytes * numsectors)) 
			!= sector_size__bytes * numsectors) {
		fprintf(stderr, "Read block %"PRId64" length %d failed: returned %d\n", 
				block, numsectors, ret);
		exit(-1);
	}
}

/* write_bytes: write a given number of bytes from the mentioned offset in 
 *				a buffer onto the disk.
 *
 * inputs:
 *   int64 block: the starting sector number to write.
 *                sector numbering starts with 0.
 *   int numbytes: the number of bytes to write.  must be >= 1.
 *   int offset: the offset from which the write is performed.
 *
 *   void *from: the requested number of blocks are copied from here.
 *
 * outputs:
 *   int device [GLOBAL]: the disk into which to write.
 *
 */
	void
write_bytes (int64_t block, unsigned int numbytes, int offset,  void *from)
{
	int ret;
	int64_t lret;

	printf("Reading sector  %"PRId64"\n", block);

	if ((lret = lseek64(device, block * sector_size__bytes + offset, SEEK_SET)) 
			!= block * sector_size__bytes + offset) {
		fprintf(stderr, "Seek to position %"PRId64" failed: returned %"PRId64"\n", 
				block * sector_size__bytes + offset, lret);
		exit(-1);
	}
	if ((ret = write(device, from, numbytes)) 
			!= numbytes) {
		fprintf(stderr, "Read block %"PRId64" length %d failed: returned %d\n", 
				block, numbytes, ret);
		exit(-1);
	}
}

	int
main (int argc, char **argv)
{
	int all_partition;
	int	the_partition=0;
	int i=1;

	//Parsing the command line inputs
	while(i<argc)
	{
		switch (argv[i][1])
		{
			case 'i':
				if ((device = open(argv[i+1], O_RDWR)) == -1) 
				{
					perror("Could not open device file");
					exit(-1);
				}
				break;
			case 'p':
				the_partition = atoi(argv[i+1]);
				break;
			case 'f':
				all_partition = atoi(argv[i+1]);
				break;
			default:
				printf("Wrong Argument: %s\n", argv[i]);
				break;
		}
		i=i+2;
	}
	if(the_partition != 0)
	{
		filltable(the_partition, 1);
		close(device);
		return 0;
	}
	if(all_partition == 0)
		run_all_passes(all_partition);
	return 0;
}

/*
 *This function is used to run all the checks on a given partition
 *PASS 1: Check and correct inconsistencies in parent-child relationship
 *PASS 2: Check for unrefernced files and directories and place them in lost_found
 *PASS 3: Check for link_count and hard_link inconsistencies and correct them
 *PASS 4: Ensure all the used blocks are marked allocated
 */
void run_all_passes(int the_partition)
{
	if(the_partition != 0)
		filltable(the_partition,3);
	else if(the_partition == 0)
	{
		filltable(the_partition,2);
		close(device);
		return;	
	}
	superblock();
	group_desc();
	inode_bitmap(pass1);
	inode_bitmap(pass2);
	inode_bitmap(pass1);
	inode_bitmap(pass3);
	inode_bitmap(pass4);
	check_metadata_blocks();
}

/*
 *Option 1: For populating and printing the partition entry for a given partition_number
 *Option 2: For getting the partition number where EXT_2 file system resides 
 *Option 3: For populating the partition entry for a given  partition_number
 */
void filltable(int partition_num, int option)
{
	unsigned int i =1;
	unsigned char buf[sector_size__bytes];	/* temporary buffer */
	unsigned int extended_partition = 0; 
	unsigned int offset = 446; 
	struct partition *table;

	//Read Sector 0, since partition table exists there
	read_sectors(0, 1, buf);

	//For the 4 primary partitions
	while(i<=4)
	{
		table = (struct partition *)(buf+offset+(16*(i-1)));
		if(i==partition_num && option == 1)
		{
			printf("0x%02X %d %d\n", table->sys_ind, table->start_sect, table->nr_sects);
			part = (struct partition *)malloc(sizeof(struct partition));
			memcpy(part,table,16);
		}
		
		if(i==partition_num && option == 3)
		{
			part = (struct partition *)malloc(sizeof(struct partition));
			memcpy(part,table,16);
		}

		if(table->sys_ind == LINUX_EXT2_PARTITION && option == 2)
			run_all_passes(i);
		
		if(table->sys_ind == DOS_EXTENDED_PARTITION)
			extended_partition = table->start_sect;
		i++;
	}
	unsigned int offsets = extended_partition;
	
	//For the extended partition
	while(extended_partition)
	{ 

		read_sectors(extended_partition, 1, buf);
		table = (struct partition *)(buf+offset);
		table->start_sect=table->start_sect+extended_partition;
		if(i==partition_num && option == 1)
		{
			printf("0x%02X %d %d\n", table->sys_ind, table->start_sect, table->nr_sects);
			part = (struct partition *)malloc(sizeof(struct partition));
			memcpy(part,table,sizeof(struct partition));
		}
		
		if(i==partition_num && option == 3)
		{
			part = (struct partition *)malloc(sizeof(struct partition));
			memcpy(part,table,sizeof(struct partition));
		}

		if(table->sys_ind == LINUX_EXT2_PARTITION  && option == 2)
			run_all_passes(i);

		table = (struct partition *)(buf+offset+16);
		table->start_sect=table->start_sect+offsets;

		if(table->sys_ind == DOS_EXTENDED_PARTITION)
			extended_partition = table->start_sect;
		else
			extended_partition = 0;
		i=i+1;
	}
}

/*
 *To Make Superblock a Global Variable
 */
void superblock(void)
{
	unsigned char buf[1024];
	read_sectors((part->start_sect+2), 2, buf);
	struct ext2_super_block *super = (struct ext2_super_block *)buf;
	sblock = (struct ext2_super_block *)malloc(sizeof(struct ext2_super_block));
	memcpy(sblock,super,1024);
	block_size = 1024;
}

/*
 *To Make Group_descriptor table a Global Variable
 */
void group_desc(void)
{
	unsigned char buf[sector_size__bytes];
	read_sectors((part->start_sect+4), 1, buf);
	struct ext2_group_desc *group_desc = (struct ext2_group_desc *)buf;
	g_gdesc = (struct ext2_group_desc *)malloc(group_descriptor_size);
	memcpy(g_gdesc, group_desc, group_descriptor_size);
}

/*
 *To find the sector number of the inode_table for the passed inode
 *Algorithm: Divide the inode number/inodes_per_group to get the block_group
 *           Use this as an index into the group_descriptor table
 *           Find the starting address of inode table from group_descriptor table
 *           Find the offset of this inode in its blocks group's inode table
 */
int inode_sect(int partition, int inode)
{
	unsigned char buf[sector_size__bytes];
	int itable, index, offset, block_group, node_sector, shifted_offset, offset_in_sector;
	struct ext2_group_desc *temp = g_gdesc;

	block_group = (inode-1)/EXT2_INODES_PER_GROUP(sblock);
	g_gdesc = (struct ext2_group_desc *)((char *)g_gdesc + (block_group * sizeof(struct ext2_group_desc)));		
	itable = g_gdesc -> bg_inode_table;	
	index = (inode-1) % EXT2_INODES_PER_GROUP(sblock);
	offset = (index * sblock->s_inode_size);
	offset_in_sector= offset/512;
	node_sector = part->start_sect + (itable*2) +  offset_in_sector;
	read_sectors(node_sector, 1, buf);
	shifted_offset = (128*index) % 512;
	inode_sector.offset_num = shifted_offset;
	inode_sector.sector_num = node_sector;
	g_gdesc=temp;
	return (node_sector+shifted_offset);
}

/*Depth First Traversal to cover the entire file system
 *PASS 1: Check and correct inconsistencies in parent-child relationship
 *PASS 2: Check for unrefernced files and directories and place them in lost_found
 *PASS 3: Check for link_count and hard_link inconsistencies and correct them
 *PASS 4: Ensure all the used blocks are marked allocated
 */
int traversal(int count_inode, char *pass)
{
	unsigned char buf_inode[sector_size__bytes];
	unsigned char buf[block_size];
	int parent, rec_len=0, offset_shifted, inode_table_sector;
	int x=count_inode;
	unsigned int links;
	int count = 0;
	struct ext2_dir_entry *dir;

	//The root's inode number is always two, push this first onto to the stack
	push(2,2,&stackarr[0]);
	while(stackarr[0].present_inode)
	{	
		int popped = pop();
		inode_sect(0,stackarr[popped].present_inode);
		inode_table_sector = inode_sector.sector_num;
		offset_shifted = inode_sector.offset_num;
		read_sectors(inode_table_sector, 1, buf_inode);
		char *inode_table = (char *)buf_inode;
		struct ext2_inode *inode = (struct ext2_inode *)(inode_table + offset_shifted);
		
		//Extract only the top 4 bits because only they define the type of inode
		inode->i_mode = inode->i_mode & 0xf000;
		if(stackarr[popped].present_inode == count_inode)
			links = inode->i_links_count;

		//If the inode is not a directory, then you dont have to push onto stack
		if(inode->i_mode != EXT2_S_IFDIR)
		{
			stackarr[popped].present_inode = 0;
			stackarr[popped].parent_inode = 0;
			continue;
		}			

		//Else push all the directory entries as children of the directory onto the stack
		else
		{	
			int j=0;
			read_sectors((part->start_sect+(inode->i_block[j]*2)), 2, buf);
			j++;
			dir = (struct ext2_dir_entry *)buf;
			int i = 0; 
			while(dir->rec_len!=0 && dir->inode != 0)
			{
				//If inode passed as argument to function is encountered increment count
				//Used to keep in track of link_count/unreferenced inodes etc.
				if(dir->inode==x)
					count++;
				//The first entry in a directory is a '.', which should point to itself
				//This is to check if their is any inconsistency in this and corrects them
				if(i==0)
				{
					if(stackarr[popped].present_inode != dir->inode && 
							(strcmp(pass,"pass1") == 0) && stackarr[popped].present_inode == count_inode)
					{
						correct_present_parent((part->start_sect+(inode->i_block[0]*2)), 
								rec_len, stackarr[popped].present_inode);
					}
					i++;
					rec_len=rec_len+dir->rec_len;	

					if(rec_len >= block_size)
					{
						rec_len=0;
						read_sectors((part->start_sect+(inode->i_block[j]*2)), 2, buf);
						dir = (struct ext2_dir_entry *)buf; 
						j++;
						continue;
					}	
					dir = (struct ext2_dir_entry *)((char *)dir + dir->rec_len);
					continue;
				}
				//The second entry in a directory is '..', which should point to parent
				//This code checks if there is any inconsistencies in this and corrects them
				if(i==1)
				{
					if(stackarr[popped].parent_inode != dir->inode && 
							(strcmp(pass,"pass1")==0) && stackarr[popped].present_inode == count_inode)
					{
						correct_present_parent((part->start_sect+(inode->i_block[0]*2)),
								rec_len, stackarr[popped].parent_inode);
					}
					i++;
					rec_len=rec_len+dir->rec_len;	
					if(rec_len >= block_size)
					{
						rec_len=0;
						read_sectors((part->start_sect+(inode->i_block[j]*2)), 2, buf);
						j++;

						dir = (struct ext2_dir_entry *)buf; 
						if(dir->rec_len == 0 || dir->inode == 0)
						{
							stackarr[popped].present_inode = 0;
							stackarr[popped].parent_inode = 0;
						}
						continue;

					}	
					dir = (struct ext2_dir_entry *)((char *)dir + dir->rec_len);
					if(dir->rec_len == 0 || dir->inode == 0)
					{
						stackarr[popped].present_inode = 0;
						stackarr[popped].parent_inode = 0;
					}
					continue;
				}
				if(i==2)
				{
					parent = stackarr[popped].present_inode;
					i++;
				}
				//Push the rest of the directory entries onto stack
				push(parent,dir->inode, &stackarr[0]); 	
				rec_len=rec_len+dir->rec_len;	
				if(rec_len >= block_size)
				{
					rec_len=0;
					read_sectors((part->start_sect+(inode->i_block[j]*2)), 2, buf);
					j++;
					dir = (struct ext2_dir_entry *)buf; 
					continue;
				}
				dir = (struct ext2_dir_entry *)((char *)dir + dir->rec_len);	
			}
		}

	}
	//Insert unrefenced inodes into lost and found
	if(count == 0 && (count_inode > 10 || count_inode == 2) && (strcmp(pass,"pass2")==0))
		insert_lost_found(count_inode);
	//Check for link_count inconsistencies and correct them
	if((count_inode > 10 || count_inode == 2) && (strcmp(pass,"pass3")==0))
		check_link_count(count, links, count_inode);
	return count;
}

/*
 * Pushes the parent and current inode onto the stack
 */
void push(int parent, int present, struct stack *array)
{

	array[top].present_inode=present;
	array[top].parent_inode = parent;
	top++;
}

/*
 * Pops the top element in stack
 */
int pop()
{
	top--;
	return top;
}

/*
 * This function runs through the entire inode_bitmap and does all the error checks
 * for each allocated inode
 */
void inode_bitmap(char *pass)
{
	int remaining_inodes = sblock->s_inodes_count;
	int x = sblock->s_inodes_per_group;
	int i;
	unsigned char buf[block_size];
	struct ext2_group_desc *my_gdesc;
	int j=0;
	int k=0;

	while(remaining_inodes != 0)
	{
		if(remaining_inodes <= x)
			x=remaining_inodes;
		k = sblock->s_inodes_per_group*j;
		my_gdesc = g_gdesc;
		my_gdesc = (struct ext2_group_desc *)((char *)my_gdesc + (j * sizeof(struct ext2_group_desc)));		
		j++;
		int ibitmap_block = my_gdesc -> bg_inode_bitmap;	

		//Reading the inode_bitmap for that block group 
		read_sectors((part->start_sect+(ibitmap_block*2)), 2, buf);
		for(i=1;i<=x;i++)
		{
			int quotient = (i-1) / 8;
			int remainder = (i-1) % 8;

			char and = 1 << remainder;
			k++;

			//Checks if the inode is allocated, if its allocated, it passes it to 
			//the respective checker functions as input
			if(!!(and & *(buf+quotient)))
			{
				if(strcmp(pass,"pass4")==0)
					traverse_blocks(k);
				else
					traversal(k,pass);
			}
		}
		remaining_inodes = remaining_inodes - x;
	}
}

/*
 * This function checks if the number of hard_links counted from traversing the
 * tree is same as the field in the inode_table, if its not equal it corrects 
 * these inconsistencies
 */
void check_link_count(int count, int link_count, int count_inode)
{
	char *inode_table;
	struct ext2_inode *inode;
	int inode_table_sector;
	int offset_shifted;
	unsigned char buf_inode[sector_size__bytes];

	inode_sect(0, count_inode);
	inode_table_sector = inode_sector.sector_num;
	offset_shifted = inode_sector.offset_num;
	read_sectors(inode_table_sector, 1, buf_inode);
	inode_table = (char *)buf_inode;
	inode = (struct ext2_inode *)(inode_table + offset_shifted);
	inode->i_links_count = count;

	if(count != link_count)
		write_bytes(inode_table_sector, 2 , offset_shifted + 26,&inode->i_links_count);
}

/*
 * Populate the global variable lostfound which stores the inode of lost&found
 */
void global_lost_found()
{
	int inode_table_sector;
	int offset_shifted;
	struct ext2_inode *lost;
	unsigned char buf_inode[sector_size__bytes];
	char *inode_table;

	inode_sect(0,11);
	inode_table_sector = inode_sector.sector_num;
	offset_shifted = inode_sector.offset_num;
	read_sectors(inode_table_sector, 1, buf_inode);
	inode_table = (char *)buf_inode;
	lost = (struct ext2_inode *)(inode_table + offset_shifted);
	memcpy(&lostfound,lost,sizeof(struct ext2_inode));
}

/*
 * Populate the directory entry for the unrefenced inode and then write into the
 * directory entry block of lost&found directory
 */
void insert_lost_found(int add_inode)
{
	int len, mode;
	int j=0;
	int rec_len=0;
	struct ext2_dir_entry_2 *dir;
	struct ext2_inode *inodes;
	char *inode_table;
	unsigned char buf[block_size];
	unsigned char buf_inode[sector_size__bytes];
	int offset_new, inode_table_sector, offset_shifted;

	global_lost_found();

	//Populating the name, length and inode field of the directory entry structure
	sprintf(add_lost.name, "%d", add_inode);
	len = strlen(add_lost.name);
	add_lost.name_len = len;
	add_lost.inode = add_inode;

	inode_sect(0,add_inode);
	inode_table_sector = inode_sector.sector_num;
	offset_shifted = inode_sector.offset_num;
	read_sectors(inode_table_sector, 1, buf_inode);
	inode_table = (char *)buf_inode;
	inodes = (struct ext2_inode *)(inode_table + offset_shifted);

	//Populating mode of directory entry structure
	mode = inodes->i_mode & 0xf000;
	if (mode == EXT2_S_IFDIR)
		add_lost.file_type = 2;
	else if (mode == EXT2_S_IFREG)
		add_lost.file_type = 1;
	else if (mode == EXT2_S_IFLNK)
		add_lost.file_type = 7; 

	read_sectors((part->start_sect+(lostfound.i_block[j]*2)), 2, buf);
	dir = (struct ext2_dir_entry_2 *)buf;

	//Populating the record_length of directory entry
	while(dir->rec_len !=0 && dir->inode!=0)
	{
		rec_len = rec_len + dir->rec_len;
		if((rec_len % block_size )== 0)
		{
			j++;
			memcpy(&prev, dir, sizeof(struct ext2_dir_entry_2));
			read_sectors((part->start_sect + (lostfound.i_block[j]*2)), 2, buf);
			dir  = (struct ext2_dir_entry_2 *)buf;
			continue;
		}
		dir = (struct ext2_dir_entry_2 *)((char *)dir + dir->rec_len);
	}

	if((prev.rec_len - EXT2_DIR_REC_LEN(prev.name_len))<EXT2_DIR_REC_LEN(len))
	{
		offset_new = 0;
		add_lost.rec_len = block_size;	
	}
	else
	{
		int offset;
		j--;
		offset = rec_len - prev.rec_len;
		offset_new = offset + EXT2_DIR_REC_LEN(prev.name_len);
		add_lost.rec_len = block_size - offset_new;
		prev.rec_len = EXT2_DIR_REC_LEN(prev.name_len);
		write_bytes((part->start_sect+(lostfound.i_block[j]*2)),
				sizeof(struct ext2_dir_entry_2) , offset, &prev);
	}

	//Once all the updates are done write into the lost&found directory in disk
	write_bytes((part->start_sect+(lostfound.i_block[j]*2)),
			sizeof(struct ext2_dir_entry_2) , offset_new, &add_lost);
}

/*
 * This function updates the parent of the newly added entry to point to lost&found
 */
void update_parent_lost_found(int add_inode)
{
	char *inode_table;
	struct ext2_inode *inode;
	int inode_table_sector;
	int offset_shifted;
	unsigned char buf[block_size];
	struct ext2_dir_entry *dir;
	unsigned char buf_inode[sector_size__bytes];

	inode_sect(0,add_inode);
	inode_table_sector = inode_sector.sector_num;
	offset_shifted = inode_sector.offset_num;
	read_sectors(inode_table_sector, 1, buf_inode);
	inode_table = (char *)buf_inode;
	inode = (struct ext2_inode *)(inode_table + offset_shifted);
	inode->i_mode = inode->i_mode & 0xf000;
	
	//Update parent of the newly added inode to lost&found only if
	//the inode is a directory, else return
	if(inode->i_mode != EXT2_S_IFDIR)
		return;
	else
	{
		int rec_len = 0;
		read_sectors((part->start_sect+(inode->i_block[0]*2)), 2, buf);
		dir = (struct ext2_dir_entry *)buf;
		dir = (struct ext2_dir_entry *)((char *)dir + dir->rec_len);
		rec_len = rec_len + dir->rec_len;
		dir->inode =11;
		write_bytes((part->start_sect+(inode->i_block[0]*2)), 4 , rec_len, &dir->inode);
	}
}

/* 
 * This entries corrects any inconsistencies in the first '.'(current) and 
 * '..'(parent) entries in each directory
 */
void correct_present_parent(int sector, int offset, unsigned int entry)
{
	struct ext2_dir_entry_2 *dir;
	unsigned char buf[block_size];
	read_sectors(sector, 2, buf);
	dir = (struct ext2_dir_entry_2 *)buf;

	dir = (struct ext2_dir_entry_2 *)((char *)dir + offset);

	dir->inode = entry;
	write_bytes(sector, 4, offset, &dir->inode);
}

/*
 * This runs through the entire tree and checks if all the used blocks
 * are marked as allocated in the block_bitmap and corrects inconsistencies
 */
void traverse_blocks(int count_inode)
{
	int inode_table_sector;
	int offset_shifted;
	unsigned char buf_inode[sector_size__bytes];
	inode_sect(0,count_inode);
	inode_table_sector = inode_sector.sector_num;		
	offset_shifted = inode_sector.offset_num;
	read_sectors(inode_table_sector, 1, buf_inode);
	char *inode_table = (char *)buf_inode;
	struct ext2_inode *inode = (struct ext2_inode *)(inode_table + offset_shifted);
	int i = 0;
    inode->i_mode = inode->i_mode & 0xf000;
	if(inode->i_mode == EXT2_S_IFDIR || inode->i_mode == EXT2_S_IFREG)
	{
		while(i<EXT2_N_BLOCKS && inode->i_block[i]!=0)
		{
			//Checks if the 12 direct blocks are allocated properly
			if(i < EXT2_NDIR_BLOCKS)
			{
				check_block_map(inode->i_block[i]);
			}

			//Checks if all the blocks is the 1st level of indirection are allocated properly
			else if(i == EXT2_IND_BLOCK)
			{
				check_block_map(inode->i_block[i]);
				single_indirect(inode->i_block[i]);
			}

			//Checks if all the blocks in the 2nd level of indirection are allocated properly
			else if(i == EXT2_DIND_BLOCK)
			{
				check_block_map(inode->i_block[i]);
				double_indirect(inode->i_block[i]);
			}

			//Checks if all the blocks in the 3rd level of indirection are allocated properly
			else if( i == EXT2_TIND_BLOCK)
			{
				check_block_map(inode->i_block[i]);
				triple_indirect(inode->i_block[i]);
			}
			i++;
		}
	}
}

/*
 * Checker function for 3rd level of indirection
 */
void triple_indirect(int start)
{
	unsigned char buf[block_size];
	unsigned int *current;
	int count = 0;

	read_sectors((part->start_sect+(start*2)), 2, buf);
	current = (unsigned int *)buf;
	while(count < entries_per_block && *current!=0)
	{
		check_block_map(*current);
		double_indirect(*current);
		current++;
		count++;
	}
}

/*
 * Checker function for 2nd level of indirection
 */
void double_indirect(int start)
{
	unsigned char buf[block_size];
	unsigned int *current;
	int count = 0;

	read_sectors((part->start_sect+(start*2)), 2, buf);
	current = (unsigned int *)buf;
	while(count < entries_per_block && *current!=0)
	{
		check_block_map(*current);
		single_indirect(*current);
		current++;
		count++;
	}
}

/*
 * Checker function for 1st level of indirection
 */
void single_indirect(int start)
{
	unsigned char buf[block_size];
	unsigned int *current;
	int count = 0;

	read_sectors((part->start_sect+(start*2)), 2, buf);
	current = (unsigned int *)buf;
	while(count < entries_per_block && *current!=0)
	{
		check_block_map(*current);
		current++;
		count++;
	}
}

/*
 * This function checks if the block_bitmap is set for the inputed block_num
 */
void check_block_map(int block_num)
{
   struct ext2_group_desc *my_owngdesc;
   unsigned char bufs[block_size];
   int block_group = (block_num-1)/EXT2_BLOCKS_PER_GROUP(sblock);
   my_owngdesc = g_gdesc;
   block_num = (block_num-1) % (EXT2_BLOCKS_PER_GROUP(sblock));
   my_owngdesc = (struct ext2_group_desc *)((char *)my_owngdesc + (block_group * sizeof(struct ext2_group_desc)));		
   int bbitmap_block = my_owngdesc -> bg_block_bitmap;	
   read_sectors((part->start_sect+(bbitmap_block*2)), 2, bufs);
   int quotient = (block_num) / 8;
   int remainder = (block_num) % 8;

   char and = 1 << remainder;
   int allocated = !!(and & *(bufs+quotient));
   if(allocated == 0)
   {
	   char changed = *(bufs+quotient)+and;
       write_bytes((part->start_sect+(bbitmap_block*2)), 1 , quotient, &changed);
   }
}

/*
 * Since, only data blocks have been checked so far, this function ensures that even 
 * the metadata blocks are allocated properly in the block_bitmap
 */
void check_metadata_blocks()
{
	struct ext2_group_desc *my_gdesc = g_gdesc;
	int start = 0, i;
	int inode_table_length = (sblock->s_inodes_per_group * 128)/block_size;
	int block_group_count = sblock->s_blocks_count / sblock->s_blocks_per_group;
	while(start < block_group_count)
	{

		my_gdesc = (struct ext2_group_desc *)((char *)my_gdesc + (start * sizeof(struct ext2_group_desc)));		
		check_block_map(my_gdesc->bg_block_bitmap);
		check_block_map(my_gdesc->bg_inode_bitmap);
		for(i=my_gdesc->bg_inode_table;i<(my_gdesc->bg_inode_table+inode_table_length);i++)
		{
			check_block_map(i);
			i++;
		}
	    start++;
   }
}
