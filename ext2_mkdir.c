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

//int getParentDirInodeIdx(char *path);
//void setParentDirInodeIdxAndDirName(char **dir_name, int *i_pdir_idx, char *path);
char *getDirNameAndsetParentDirInodeIdx(int *i_pdir_idx, char *path);
char *getDirName(char *path);
int allocateFreeInodeIdx();
int allocateFreeBlockIdx();
const char *byte_to_binary(int x);
char write2InodeAndBlock(int i_free_idx, int b_free_idx, int i_pdir_idx, char *dir_name);
char write2ParentDirBlock(int i_pdir_idx, int i_dir_idx, char *dir_name);


char *getDirNameAndsetParentDirInodeIdx(int *i_pdir_idx, char *path)
{
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *i_head = (struct ext2_inode *)(disk + bg->bg_inode_table * EXT2_BLOCK_SIZE);
	int search_inode = 2;
	char *pch = strtok(path, "/");
	char *next_pch = strtok(NULL, "/");

	while(pch != NULL)
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
				printf("rec len: %d inode: %d\n", dir_entry->rec_len, dir_entry->inode);
				//printf("size: %d\n", 8 + dir_entry->name_len);

				printf("%.*s\n", dir_entry->name_len, dir_entry->name);
				//printf("%s\n", pch);
				if(strncmp(dir_entry->name, pch, dir_entry->name_len) == 0 && strlen(pch) == dir_entry->name_len && dir_entry->file_type == EXT2_FT_DIR)
				{
					dir_finded = 1;
					search_inode = dir_entry->inode;
					break;
				}
			}
		}

		if(dir_finded && next_pch == NULL)
		{
			fprintf(stderr, "directory already exists\n");
            exit(1);
		}
		else if(!dir_finded && next_pch != NULL)
		{
			fprintf(stderr, "Unable to create multi-layers directorie\n");
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
	//dir_name = &pch;
	//printf("Dir name: %s\n", *dir_name);
	*i_pdir_idx = search_inode;
	//return search_inode;	
	return pch;
}

char *getDirName(char *path)
{
	printf("%.*s\n", 7, path);
	char *pch = strtok(path, "/");
	char *next_pch = strtok(NULL, path);

	while(next_pch != NULL)
	{
		printf("ss\n");
		pch = next_pch;
		next_pch = strtok(NULL, "/");
	}
	printf("Dir Name: %s\n", pch);
	return pch;	
}

int allocateFreeInodeIdx()
{
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	//printf("%s\n", byte_to_binary(bg->bg_inode_bitmap));
		
	int mask = 1;
	int i;
	//int i_bit = bg->bg_inode_bitmap;
	int *i_bitmap_ptr = (int *)(disk + bg->bg_inode_bitmap * EXT2_BLOCK_SIZE);
	//printf("%d\n", *i_bitmap);
	int i_bit = *i_bitmap_ptr;
	for(i = 0; i < 32; i++, i_bit >>= 1)
	{
		int bit = i_bit & mask;
		if(!bit)	
		{
			printf("%d\n", i + 1);
			break;
		}
	}

	// update inode bitmap
	*i_bitmap_ptr |= (1<<i);	

	printf("free inode num: %d\n", bg->bg_free_inodes_count);
	printf("allocate free node: %d\n", i + 1);
	return i + 1;	
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

	//struct ext2_inode *i_head = (struct ext2_inode *)(disk + bg->bg_inode_table * EXT2_BLOCK_SIZE);
	//struct ext2_inode *pinode = (struct ext2_inode *)(i_head + 13 - 1);
	//printf("block: %d\n", pinode->i_block[1]);
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
			//else printf("%d\n", b_bit & mask);
		}
	}

	return -1;
}

const char *byte_to_binary(int x)
{
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = (1<<30); z > 0; z >>= 1)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}

char write2InodeAndBlock(int i_free_idx, int b_free_idx, int i_pdir_idx, char *dir_name)
{
	// write to inode
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *i_head = (struct ext2_inode *)(disk + bg->bg_inode_table * EXT2_BLOCK_SIZE);
	struct ext2_inode *pinode = (struct ext2_inode *)(i_head + i_free_idx - 1);
	//struct ext2_inode *pinode = (struct ext2_inode *)(i_head + 13 - 1);
	printf("blocks: %d\n", pinode->i_block[0]);
	pinode->i_mode = EXT2_S_IFDIR,
	pinode->i_size = EXT2_BLOCK_SIZE;
	pinode->i_links_count = 2;
	pinode->i_blocks = 2;
	pinode->i_block[0] = b_free_idx;

	//// write to block
	struct ext2_dir_entry_2 *pde = (struct ext2_dir_entry_2 *)(disk + b_free_idx * EXT2_BLOCK_SIZE);
	strcpy(pde->name, ".");
	pde->inode = i_free_idx;
	pde->name_len = 1;
	pde->rec_len = DIR_ENTRY_BASE_SIZE + 4;
	pde->file_type = EXT2_FT_DIR;

	pde = (void *)pde + pde->rec_len;
	strcpy(pde->name, "..");
	pde->inode = i_pdir_idx;
	pde->name_len = 2;
	pde->rec_len = DIR_ENTRY_BASE_SIZE + 4;
	pde->file_type = EXT2_FT_DIR;

	return 1;	
}

char write2ParentDirBlock(int i_pdir_idx, int i_dir_idx, char *dir_name)
{
	struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *i_head = (struct ext2_inode *)(disk + bg->bg_inode_table * EXT2_BLOCK_SIZE);
	struct ext2_inode *pinode = (struct ext2_inode *)(i_head + i_pdir_idx - 1);

	int i = 0;

	printf("%d\n", pinode->i_block[0]);
	while(i + 1 < 12 && pinode->i_block[i + 1] != 0) i++;
	printf("%d %d\n", i, pinode->i_block[i]);

	int rest_rec_len = 0;
	int last_rec_len;
	int require_rec_len = 0;
	int acc_len = 0;
	printf("%s\n", dir_name);
	int dir_name_len = strlen(dir_name);
	struct ext2_dir_entry_2* pde = (struct ext2_dir_entry_2 *)(disk + pinode->i_block[i] * EXT2_BLOCK_SIZE);
	while(acc_len < EXT2_BLOCK_SIZE)
	{
		pde = (void *)pde + rest_rec_len;
		printf("%d %d\n", rest_rec_len, acc_len);
		rest_rec_len = pde->rec_len;
		acc_len += rest_rec_len;
	}
	printf("xd\n");
	last_rec_len = DIR_ENTRY_BASE_SIZE + pde->name_len / 4 + ((pde->name_len % 4 == 0) ? 0 : 1);
	printf("shit: %d %d\n", last_rec_len, pde->rec_len);
	rest_rec_len -= last_rec_len;
	require_rec_len = DIR_ENTRY_BASE_SIZE + dir_name_len / 4 + ((dir_name_len % 4 == 0) ? 0 : 1);
	printf("%d %d\n", DIR_ENTRY_BASE_SIZE, dir_name_len);

	printf("%d %d %d\n", require_rec_len, rest_rec_len, last_rec_len);
	if(require_rec_len <= rest_rec_len)
	{
		pde->rec_len = last_rec_len;
		pde = (void *)pde + last_rec_len;
		strcpy(pde->name, dir_name);
		pde->inode = i_dir_idx;
		pde->name_len = strlen(dir_name);
		pde->rec_len = EXT2_BLOCK_SIZE;
		pde->file_type = EXT2_FT_DIR;
	}
	else
	{
		int b_free_idx = allocateFreeBlockIdx();	
		pinode->i_block[i + 1] = b_free_idx;
		pde = (void *)disk + b_free_idx * EXT2_BLOCK_SIZE;
		strcpy(pde->name, dir_name);
		pde->inode = i_dir_idx;
		pde->name_len = strlen(dir_name);
		pde->rec_len = EXT2_BLOCK_SIZE;
		pde->file_type = EXT2_FT_DIR;
	}
	return 1;	
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
	//char **dir_name = 'a';
	int i_pdir_idx;

	//setParentDirInodeIdxAndDirName(dir_name, &i_pdir_idx, path);
	char *dir_name = getDirNameAndsetParentDirInodeIdx(&i_pdir_idx, path);
	printf("Dir name: %s\n", dir_name);
	//printf("%s\n", *dir_name);
	if(i_pdir_idx == -1)
	{
        fprintf(stderr, "invalid path\n");
        exit(1);
	}

	// get directory name
	//char *dir_name = getDirName(path);
	// get free inode
	int i_free_idx = allocateFreeInodeIdx();
	int b_free_idx = allocateFreeBlockIdx();

	// create directory to inode and its blocks
	// record the directory in its current dictory blocks
	if((!write2InodeAndBlock(i_free_idx, b_free_idx, i_pdir_idx, dir_name)) || (!write2ParentDirBlock(i_pdir_idx, i_free_idx, dir_name)))
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
        fprintf(stderr, "ext2_mkdir <image name> <absolute path on the disk>\n");
        exit(1);
    }
    
	mkDir(argv[1], argv[2]);
   
    return 0;
}
