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

struct ext2_inode *getInode(int idx, unsigned char *dk, int offset)
{
	//printf("%d\n", offset);
	return (struct ext2_inode *) (dk + offset + idx * 128 / EXT2_BLOCK_SIZE + ((idx - 1) * 128) % EXT2_BLOCK_SIZE);
}

char *getBlock(unsigned char *dk, int blk_num)
{
	return (char *)dk + (blk_num) * EXT2_BLOCK_SIZE;
}

void listDir(char *img, char *dir)
{
	int fd = open(img, O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }


    // super block struct pointer
    int block_size = 1024;
    //struct ext2_super_block *sb = (struct ext2_super_block *)(disk + block_size);
	//printf("%d\n", sb->s_log_block_size);
    struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * block_size);
    //struct ext2_inode *inode = (struct ext2_inode *)(disk + bg->bg_inode_table*block_size);


	//printf("%d\n", sb->s_inodes_count);
    // store the inode information in inode_bitmap
    //int inode_bitmap[sb->s_inodes_count];

	// args 2 must be absolute path
	if(dir[0] != '/')
	{
        fprintf(stderr, "absolute path\n");
        exit(1);
	}


	char *pch = strtok(dir, "/");
	int search_inode = 2;
	int i;
	//struct ext2_inode *pinode = getInode(2, disk, bg->bg_inode_table * block_size);
	//printf("Inode size: %d\n", pinode->i_size);
	//for(i = 0; i < 15; i++)
	//{
	//	printf("%d\n", pinode->i_block[i]);
	//}
	// iterative to find the search inode
	char find_dir;
	while(pch != NULL)
	{
		char *next_pch = strtok(NULL, "/");
		struct ext2_inode *pinode = getInode(search_inode, disk, bg->bg_inode_table * block_size);
		find_dir = 0;

		for(i = 0; i < 12 && pinode->i_block[i] != 0 && !find_dir; i++)
		{
			//printf("%d %d\n", pinode->i_block[i], pinode->i_ctime);
			struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *) getBlock(disk, pinode->i_block[i]);
			int rec_len = 0;
			int acc_len = 0;
			
			while(acc_len < block_size)
			{
				//printf("heare\n");
				dir_entry = (void *) dir_entry + rec_len;
				//printf("%.*s\n", dir_entry->name_len, dir_entry->name);
				//printf("%s %d\n", pch, dir_entry->name_len);

				char path_match = 0;
				if(strncmp(pch, dir_entry->name, dir_entry->name_len) == 0 && strlen(pch) == dir_entry->name_len)
				{
					path_match = 1;
				}
				
				//printf("%d %d", dir_entry->file_type, EXT2_FT_DIR);
				if(path_match && dir_entry->file_type != EXT2_FT_DIR && next_pch == NULL)
				{
					printf("File: %.*s\n", dir_entry->name_len, dir_entry->name);
					return;
				}
				if(path_match && dir_entry->file_type == EXT2_FT_DIR)
				{
					find_dir = 1;
					search_inode = dir_entry->inode;
					break;
				}

				rec_len = dir_entry->rec_len;
				acc_len += rec_len;
			}
		}

		if(!find_dir)
		{
			fprintf(stderr, "No such file or diretory\n");
            exit(1);
		}

		pch = next_pch;
	}

	// it is dir
	struct ext2_inode *pinode = getInode(search_inode, disk, bg->bg_inode_table * block_size);
	for(i = 0; i < 12 && pinode->i_block[i] != 0; i++)
	{
		int acc_len = 0;
		int rec_len = 0;
		struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *)getBlock(disk, pinode->i_block[i]);

		while(acc_len < block_size)
		{
			dir_entry = (void *)dir_entry + rec_len;
			rec_len = dir_entry->rec_len;
			
			printf("%.*s\n", dir_entry->name_len, dir_entry->name);

			acc_len += rec_len;
		}
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
    
	listDir(argv[1], argv[2]);
   
    return 0;
}
