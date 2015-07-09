/*
 * Linux Ext2 mkdir implementation
 * Limitations:
 * 1. only allow direct inode block
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <linux/limits.h>
#include "ext2.h"

unsigned char *disk;

int getParentDirInodeIdx(char *path);
char *getDirName(char *path);
int allocateFreeInodeIdx();
char write2InodeAndBlock(int i_free_idx, char *dir_name);
char write2ParentDirBlock(int i_pdir_idx, int i_dir_idx, char *dir_name);


int getParentDirInodeIdx(char *path)
{
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *i_head = (struct ext2_inode *)(disk + bg->bg_inode_table * EXT2_BLOCK_SIZE);
	int search_inode = 2;
	char *pch = strtok(path, "/");
	char *next_pch = strtok(NULL, "/");

	while(next_pch != NULL)
	{
		//struct ext2_inode *pinode = INODE_PTR(search_inode, disk, bg->bg_inode_table * EXT2_BLOCK_SIZE);
		//printf("%d\n", bg->bg_inode_table);
		//struct ext2_inode *pinode = ((struct ext2_inode *)(disk + bg->bg_inode_table * EXT2_BLOCK_SIZE +
		//			search_inode * 128 / EXT2_BLOCK_SIZE +
		//			((search_inode - 1) * 128) % EXT2_BLOCK_SIZE));
		//int index = (search_inode - 1) / INODES_PER_BLOCK;
		struct ext2_inode *pinode = (struct ext2_inode *)(i_head + search_inode - 1);
		char dir_finded = 0;
		int i;

		printf("a\n");
		printf("%d\n", pinode->i_ctime);
		for(i = 0; i < 12 && pinode->i_block[i] != 0 && !dir_finded; i++)
		{
			struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *)(disk + pinode->i_block[i] * EXT2_BLOCK_SIZE);
			int acc_len = 0;
			int rec_len = 0;

			while(acc_len < EXT2_BLOCK_SIZE)
			{
				dir_entry = (void *)dir_entry + rec_len;
				rec_len = dir_entry->rec_len;
				acc_len += rec_len;
				//printf("%d\n", dir_entry->inode);
				//printf("%d\n", dir_entry->rec_len);

				printf("%.*s\n", dir_entry->name_len, dir_entry->name);
				//printf("%s\n", pch);
				if(strncmp(dir_entry->name, pch, dir_entry->name_len) == 0 && strlen(pch) == dir_entry->name_len)
				{
					dir_finded = 1;
					search_inode = dir_entry->inode;
					break;
				}
			}
		}

		if(!dir_finded)
		{
			fprintf(stderr, "No such file or diretory\n");
            exit(1);
		}

		pch = next_pch;
		next_pch = strtok(NULL, "/");
	}
	printf("parent Inode: %d\n", search_inode);
	return search_inode;	
}

char *getDirName(char *path)
{
	return NULL;	
}

int allocateFreeInodeIdx()
{
	return 1;	
}

char write2InodeAndBlock(int i_free_idx, char *dir_name)
{
	return 0;	
}

char write2ParentDirBlock(int i_pdir_idx, int i_dir_idx, char *dir_name)
{
	return 0;	
}

void mkDir(char *img, char *path)
{
	// image file maps to virtual memory
	int fd = open(img, O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

	// path should be absolute path
	if(path[0] != '/')
	{
        fprintf(stderr, "absolute path\n");
        exit(1);
	}

	// see if path is legal, if so get its parent dictory inode index
	int i_pdir_idx = getParentDirInodeIdx(path);
	if(i_pdir_idx == -1)
	{
        fprintf(stderr, "invalid path\n");
        exit(1);
	}

	// get directory name
	char *dir_name = getDirName(path);
	// get free inode
	int i_free_idx = allocateFreeInodeIdx();

	// create directory to inode and its blocks
	// record the directory in its current dictory blocks
	if(!write2InodeAndBlock(i_free_idx, dir_name) && (!write2ParentDirBlock(i_free_idx, dir_name, i_pdir_idx)))
	{
        fprintf(stderr, "failed to create directory\n");
        exit(1);
	}
}

int main(int argc, char **argv) 
{

    // there are 2 args for this command
    if(argc != 3) 
	{
        fprintf(stderr, "ext2_ls <image name> <absolute path on the disk>\n");
        exit(1);
    }
    
	mkDir(argv[1], argv[2]);
   
    return 0;
}
