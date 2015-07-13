/*
 * Linux Ext2 ln implementation
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

void ln(char *img, char *dest, char *src);
int getSrcInode(char *src_dir);
char *getLinkName(int *i_pdir, char *des_dir);
char write2ParentInodeAndBlock(int i_pdir, char *ln_name, int i_src);
int allocateFreeBlockIdx();
char updateSrcInode(int i_src);

void ln(char *img, char *dest, char *src)
{
	// image file maps to virtual memory
	int fd = open(img, O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

	// path should be absolute path
	int i_src = getSrcInode(src);

	int i_pdir;
	char *ln_name = getLinkName(&i_pdir, dest);
	if(ln_name == NULL)
	{
		fprintf(stderr, "Link name is invalid\n");
		exit(1);
	}

	if(!write2ParentInodeAndBlock(i_pdir, ln_name, i_src) || !updateSrcInode(i_src))
	{
		fprintf(stderr, "Failed to create a link\n");
		exit(1);
	}
}

int getSrcInode(char *src_dir)
{
	int i_src = 2;
	char *pch = strtok(src_dir, "/");
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *i_head = (struct ext2_inode *)(disk + bg->bg_inode_table * EXT2_BLOCK_SIZE);

	while(pch != NULL)
	{
		char pth_matched = 0;	
		struct ext2_inode *inode = i_head + i_src - 1;
		int i;
		
		for(i = 0; i < 12 && inode->i_block[i] != 0 && !pth_matched; i++)
		{
			int acc_len = 0;
			int rec_len = 0;
			struct ext2_dir_entry_2 *pde = (struct ext2_dir_entry_2 *)(disk + inode->i_block[i] * EXT2_BLOCK_SIZE);

			while(acc_len < EXT2_BLOCK_SIZE)
			{
				pde = (void *)pde + rec_len;
				printf("%s\n", pde->name);
				if(strncmp(pch, pde->name, pde->name_len) == 0 && strlen(pch) == pde->name_len)
				{
					pth_matched = 1;
					i_src = pde->inode;
					printf("match\n");
					break;
				}

				rec_len = pde->rec_len;
				acc_len += rec_len;
			}
		}

		if(!pth_matched)
		{
			fprintf(stderr, "designated link source not found\n");
			exit(1);
		}
		pch = strtok(NULL, "/");
	}
	printf("%d\n", i_src);
	return i_src;
}

char *getLinkName(int *i_pdir, char *des_dir)
{
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *i_head = (struct ext2_inode *)(disk + bg->bg_inode_table * EXT2_BLOCK_SIZE);
	int search_inode = 2;
	char *pch = strtok(des_dir, "/");
	char *next_pch = strtok(NULL, "/");

	while(pch != NULL)
	{
		struct ext2_inode *pinode = (struct ext2_inode *)(i_head + search_inode - 1);
		char dir_finded = 0;
		int i;

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

				if(strncmp(dir_entry->name, pch, dir_entry->name_len) == 0 && strlen(pch) == dir_entry->name_len)
				{
					dir_finded = 1;
					search_inode = dir_entry->inode;
					break;
				}
			}
		}

		if(dir_finded && next_pch == NULL)
		{
			fprintf(stderr, "file already exists\n");
            exit(1);
		}
		else if(!dir_finded && next_pch != NULL)
		{
			fprintf(stderr, "Unable to create multi-layers link\n");
            exit(1);
		}
		else if(!dir_finded && next_pch == NULL)
		{
			break;
		}
		pch = next_pch;
		next_pch = strtok(NULL, "/");

	}
	printf("parent Inode: %d\n", search_inode);
	*i_pdir = search_inode;

	return pch;
}

char write2ParentInodeAndBlock(int i_pdir, char *ln_name, int i_src)
{

	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *i_head = (struct ext2_inode *)(disk + bg->bg_inode_table * EXT2_BLOCK_SIZE);
	struct ext2_inode *pinode = (struct ext2_inode *)(i_head + i_pdir - 1);

	int i = 0;

	while(i + 1 < 12 && pinode->i_block[i + 1] != 0) i++;

	int rest_rec_len = 0;
	int last_rec_len;
	int require_rec_len = 0;
	int acc_len = 0;
	int ln_name_len = strlen(ln_name);
	struct ext2_dir_entry_2* pde = (struct ext2_dir_entry_2 *)(disk + pinode->i_block[i] * EXT2_BLOCK_SIZE);

	while(acc_len < EXT2_BLOCK_SIZE)
	{
		pde = (void *)pde + rest_rec_len;
		rest_rec_len = pde->rec_len;
		acc_len += rest_rec_len;
	}
	// name with 4 * n + m characters will occpuy (n + 1) * 4 bytes
	last_rec_len = DIR_ENTRY_BASE_SIZE + pde->name_len / 4 + ((pde->name_len % 4 == 0) ? 0 : 1);
	rest_rec_len -= last_rec_len;
	require_rec_len = DIR_ENTRY_BASE_SIZE + ln_name_len / 4 + ((ln_name_len % 4 == 0) ? 0 : 1);

	if(require_rec_len <= rest_rec_len)
	{
		pde->rec_len = last_rec_len;
		pde = (void *)pde + last_rec_len;
		strcpy(pde->name, ln_name);
		pde->inode = i_src;
		pde->name_len = strlen(ln_name);
		pde->rec_len = EXT2_BLOCK_SIZE;
		pde->file_type = EXT2_FT_SYMLINK;
	}
	else
	{
		int b_free_idx = allocateFreeBlockIdx();	
		pinode->i_block[i + 1] = b_free_idx;
		pde = (void *)disk + b_free_idx * EXT2_BLOCK_SIZE;
		strcpy(pde->name, ln_name);
		pde->inode = i_src;
		pde->name_len = strlen(ln_name);
		pde->rec_len = EXT2_BLOCK_SIZE;
		pde->file_type = EXT2_FT_DIR;
	}
	return 1;	
}

int allocateFreeBlockIdx()
{
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	int b_size = bg->bg_free_blocks_count;

	if(b_size >= BLOCK_NUM) 
	{
		fprintf(stderr, "run out of block space\n");
		exit(1);
	}

	unsigned int mask = 1;
	unsigned int *b_bitmap_ptr = (unsigned int *)(disk + bg->bg_block_bitmap * EXT2_BLOCK_SIZE);
	int i, j;

	for(i = 0; i < BLOCK_BITMAP_NUM; i++)
	{
		unsigned int b_bit = *(b_bitmap_ptr + i);
		for(j = 0; j < 32; j++, b_bit >>= 1)
		{
			if((b_bit & mask) == 0)
			{
				// update block bitmap
				*(b_bitmap_ptr + i) |= (1 << j);	
				printf("allocate block: %d\n", i * 32 + j + 1);
				return i * 32 + j + 1;
			}
		}
	}

	return -1;
}

char updateSrcInode(int i_src)
{
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *i_head = (struct ext2_inode *)(disk + bg->bg_inode_table * EXT2_BLOCK_SIZE);
	struct ext2_inode *inode = i_head + i_src - 1;

	// update hard link number
	inode->i_links_count += 1;
	printf("inode: %d, hard link number: %d\n", i_src, inode->i_links_count);

	return 1;
}

int main(int argc, char **argv) 
{

    // there are 2 args for this command
    if(argc != 4) 
	{
        fprintf(stderr, "ext2_ln <image name> <absolute path on the disk> <absolute path on the disk>\n");
        exit(1);
    }
    
	ln(argv[1], argv[2], argv[3]);
   
    return 0;
}
