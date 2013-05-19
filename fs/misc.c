/*
 * misc.c
 *
 *  Created on: 2013-5-19
 *      Author: bear
 */
#include "type.h"
#include "config.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
#include "hd.h"
#include "fs.h"
PRIVATE int alloc_imap_bit(int dev);
PRIVATE int alloc_smap_bit(int dev, int nr_sects_to_alloc);
PRIVATE struct inode * new_inode(int dev, int inode_nr, int start_sect);
PRIVATE void new_dir_entry(struct inode *dir_inode,int inode_nr,char *filename);
PUBLIC struct inode * create_file(const char * path, int flags)
{
		char filename[MAX_PATH];
		struct inode * dir_inode;
		if (strip_path(filename, path, &dir_inode) != 0)
			return 0;

		int inode_nr = alloc_imap_bit(dir_inode->i_dev);
		int free_sect_nr = alloc_smap_bit(dir_inode->i_dev,
						  NR_DEFAULT_FILE_SECTS);
		struct inode *newino = new_inode(dir_inode->i_dev, inode_nr,
						 free_sect_nr);

		new_dir_entry(dir_inode, newino->i_num, filename);

		return newino;
	}

PUBLIC int search_file(const char * path)
{
	int i, j;

	char filename[MAX_PATH];
	memset(filename, 0, MAX_FILENAME_LEN);
	struct inode * dir_inode;
	if (strip_path(filename, path, &dir_inode) != 0)
		return 0;

	if (filename[0] == 0)	/* path: "/" */
		return dir_inode->i_num;

	/**
	 * Search the dir for the file.
	 */
	int dir_blk0_nr = dir_inode->i_start_sect;
	int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
	int nr_dir_entries =
	  dir_inode->i_size / DIR_ENTRY_SIZE; /**
					       * including unused slots
					       * (the file has been deleted
					       * but the slot is still there)
					       */
	int m = 0;
	struct dir_entry * pde;
	for (i = 0; i < nr_dir_blks; i++) {
		//RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
		hd_rdwt_block(dir_inode->i_dev, (dir_blk0_nr + i)*SECTOR_SIZE, SECTOR_SIZE, fsbuf, 0);
		pde = (struct dir_entry *)fsbuf;
		for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++) {
			if (memcmp(filename, pde->name, MAX_FILENAME_LEN) == 0)
				return pde->inode_nr;
			if (++m > nr_dir_entries)
				break;
		}
		if (m > nr_dir_entries) /* all entries have been iterated */
			break;
	}

	/* file not found */
	return 0;
}

PUBLIC int strip_path(char * filename, const char * pathname,
		      struct inode** ppinode)
{
	const char * s = pathname;
	char * t = filename;

	if (s == 0)
		return -1;

	if (*s == '/')
		s++;

	while (*s) {		/* check each character */
		if (*s == '/')
			return -1;
		*t++ = *s++;
		/* if filename is too long, just truncate it */
		if (t - filename >= MAX_FILENAME_LEN)
			break;
	}
	*t = 0;

	*ppinode = root_inode;

	return 0;
}

PRIVATE int alloc_imap_bit(int dev)
{
	int inode_nr = 0;
	int i, j, k;

	int imap_blk0_nr = 1 + 1; /* 1 boot sector & 1 super block */
	struct super_block * sb = get_super_block(dev);

	for (i = 0; i < sb->nr_imap_sects; i++) {
		//RD_SECT(dev, imap_blk0_nr + i);
		hd_rdwt_block(dev, (imap_blk0_nr + i)*SECTOR_SIZE, SECTOR_SIZE, fsbuf, 0);

		for (j = 0; j < SECTOR_SIZE; j++) {
			/* skip `11111111' bytes */
			if (fsbuf[j] == 0xFF)
				continue;
			/* skip `1' bits */
			for (k = 0; ((fsbuf[j] >> k) & 1) != 0; k++) {}
			/* i: sector index; j: byte index; k: bit index */
			inode_nr = (i * SECTOR_SIZE + j) * 8 + k;
			fsbuf[j] |= (1 << k);
			/* write the bit to imap */
			//WR_SECT(dev, imap_blk0_nr + i);
			hd_rdwt_block(dev, (imap_blk0_nr + i)*SECTOR_SIZE, SECTOR_SIZE, fsbuf, 1);
			break;
		}

		return inode_nr;
	}

	/* no free bit in imap */
	//panic("inode-map is probably full.\n");

	return 0;
}

PRIVATE int alloc_smap_bit(int dev, int nr_sects_to_alloc)
{
	/* int nr_sects_to_alloc = NR_DEFAULT_FILE_SECTS; */

	int i; /* sector index */
	int j; /* byte index */
	int k; /* bit index */

	struct super_block * sb = get_super_block(dev);

	int smap_blk0_nr = 1 + 1 + sb->nr_imap_sects;
	int free_sect_nr = 0;

	for (i = 0; i < sb->nr_smap_sects; i++) { /* smap_blk0_nr + i :
						     current sect nr. */
		//RD_SECT(dev, smap_blk0_nr + i);
		hd_rdwt_block(dev, (smap_blk0_nr + i)*SECTOR_SIZE, SECTOR_SIZE, fsbuf, 0);

		/* byte offset in current sect */
		for (j = 0; j < SECTOR_SIZE && nr_sects_to_alloc > 0; j++) {
			k = 0;
			if (!free_sect_nr) {
				/* loop until a free bit is found */
				if (fsbuf[j] == 0xFF) continue;
				for (; ((fsbuf[j] >> k) & 1) != 0; k++) {}
				free_sect_nr = (i * SECTOR_SIZE + j) * 8 +
					k - 1 + sb->n_1st_sect;
			}

			for (; k < 8; k++) { /* repeat till enough bits are set */
				//assert(((fsbuf[j] >> k) & 1) == 0);
				fsbuf[j] |= (1 << k);
				if (--nr_sects_to_alloc == 0)
					break;
			}
		}

		if (free_sect_nr) /* free bit found, write the bits to smap */
			//WR_SECT(dev, smap_blk0_nr + i);
			hd_rdwt_block(dev, (smap_blk0_nr + i)*SECTOR_SIZE, SECTOR_SIZE, fsbuf, 1);

		if (nr_sects_to_alloc == 0)
			break;
	}

	//assert(nr_sects_to_alloc == 0);

	return free_sect_nr;
}

PRIVATE struct inode * new_inode(int dev, int inode_nr, int start_sect)
{
	struct inode * new_inode = get_inode(dev, inode_nr);

	new_inode->i_mode = I_REGULAR;
	new_inode->i_size = 0;
	new_inode->i_start_sect = start_sect;
	new_inode->i_nr_sects = NR_DEFAULT_FILE_SECTS;

	new_inode->i_dev = dev;
	new_inode->i_cnt = 1;
	new_inode->i_num = inode_nr;

	/* write to the inode array */
	sync_inode(new_inode);

	return new_inode;
}

PUBLIC void sync_inode(struct inode * p)
{
	struct inode * pinode;
	struct super_block * sb = get_super_block(p->i_dev);
	int blk_nr = 1 + 1 + sb->nr_imap_sects + sb->nr_smap_sects +
		((p->i_num - 1) / (SECTOR_SIZE / INODE_SIZE));
	//RD_SECT(p->i_dev, blk_nr);
	hd_rdwt_block(p->i_dev, (blk_nr)*SECTOR_SIZE, SECTOR_SIZE, fsbuf, 0);
	pinode = (struct inode*)((u8*)fsbuf +
				 (((p->i_num - 1) % (SECTOR_SIZE / INODE_SIZE))
				  * INODE_SIZE));
	pinode->i_mode = p->i_mode;
	pinode->i_size = p->i_size;
	pinode->i_start_sect = p->i_start_sect;
	pinode->i_nr_sects = p->i_nr_sects;
	//WR_SECT(p->i_dev, blk_nr);
	hd_rdwt_block(p->i_dev, (blk_nr)*SECTOR_SIZE, SECTOR_SIZE, fsbuf, 1);
}

PRIVATE void new_dir_entry(struct inode *dir_inode,int inode_nr,char *filename)
{
	/* write the dir_entry */
	int dir_blk0_nr = dir_inode->i_start_sect;
	int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE) / SECTOR_SIZE;
	int nr_dir_entries =
		dir_inode->i_size / DIR_ENTRY_SIZE; /**
						     * including unused slots
						     * (the file has been
						     * deleted but the slot
						     * is still there)
						     */
	int m = 0;
	struct dir_entry * pde;
	struct dir_entry * new_de = 0;

	int i, j;
	for (i = 0; i < nr_dir_blks; i++) {
		//RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
		hd_rdwt_block(dir_inode->i_dev, (dir_blk0_nr + i)*SECTOR_SIZE, SECTOR_SIZE, fsbuf, 0);
		pde = (struct dir_entry *)fsbuf;
		for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++) {
			if (++m > nr_dir_entries)
				break;

			if (pde->inode_nr == 0) { /* it's a free slot */
				new_de = pde;
				break;
			}
		}
		if (m > nr_dir_entries ||/* all entries have been iterated or */
		    new_de)              /* free slot is found */
			break;
	}
	if (!new_de) { /* reached the end of the dir */
		new_de = pde;
		dir_inode->i_size += DIR_ENTRY_SIZE;
	}
	new_de->inode_nr = inode_nr;
	strcpy(new_de->name, filename);

	/* write dir block -- ROOT dir block */
	//WR_SECT(dir_inode->i_dev, dir_blk0_nr + i);
	hd_rdwt_block(dir_inode->i_dev, (dir_blk0_nr + i)*SECTOR_SIZE, SECTOR_SIZE, fsbuf, 1);
	/* update dir inode */
	sync_inode(dir_inode);
}
