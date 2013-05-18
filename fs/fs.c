/*
 * fs.c
 *
 *  Created on: 2013-5-18
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

PRIVATE void mkfs();
PUBLIC void init_fs()
{
	mkfs();
}

PUBLIC int sys_open(const char* pathname, int flags)
{
	int fd = -1;
	return fd;
}

PUBLIC int sys_close(int fd)
{
	return 0;
}
PRIVATE void mkfs()
{
	 //MESSAGE driver_msg;
		int i, j;

		/************************/
		/*      super block     */
		/************************/
		/* get the geometry of ROOTDEV */
		struct part_info* geo;
		geo = get_geo(ROOT_DEV);

		printl("{FS} dev size: 0x%x sectors\n", geo->size);

		int bits_per_sect = SECTOR_SIZE * 8; /* 8 bits per byte */
		/* generate a super block */
		struct super_block sb;
		sb.magic	  = MAGIC_V1; /* 0x111 */
		sb.nr_inodes	  = bits_per_sect;
		sb.nr_inode_sects = sb.nr_inodes * INODE_SIZE / SECTOR_SIZE;
		sb.nr_sects	  = geo->size; /* partition size in sector */
		sb.nr_imap_sects  = 1;
		sb.nr_smap_sects  = sb.nr_sects / bits_per_sect + 1;
		sb.n_1st_sect	  = 1 + 1 +   /* boot sector & super block */
		sb.nr_imap_sects + sb.nr_smap_sects + sb.nr_inode_sects;
		sb.root_inode	  = ROOT_INODE;
		sb.inode_size	  = INODE_SIZE;
		struct inode x;
		sb.inode_isize_off= (int)&x.i_size - (int)&x;
		sb.inode_start_off= (int)&x.i_start_sect - (int)&x;
		sb.dir_ent_size	  = DIR_ENTRY_SIZE;
		struct dir_entry de;
		sb.dir_ent_inode_off = (int)&de.inode_nr - (int)&de;
		sb.dir_ent_fname_off = (int)&de.name - (int)&de;

		memset(fsbuf, 0x90, SECTOR_SIZE);
		memcpy(fsbuf, &sb, SUPER_BLOCK_SIZE);

		/* write the super block */
		//WR_SECT(ROOT_DEV, 1);
		hd_rdwt_block(ROOT_DEV, 512, 512, fsbuf, 1);
		printl("{FS} devbase:0x%x00, sb:0x%x00, imap:0x%x00, smap:0x%x00\n"
		       "        inodes:0x%x00, 1st_sector:0x%x00\n",
		       geo->base * 2,
		       (geo->base + 1) * 2,
		       (geo->base + 1 + 1) * 2,
		       (geo->base + 1 + 1 + sb.nr_imap_sects) * 2,
		       (geo->base + 1 + 1 + sb.nr_imap_sects + sb.nr_smap_sects) * 2,
		       (geo->base + sb.n_1st_sect) * 2);

		/************************/
		/*       inode map      */
		/************************/
		memset(fsbuf, 0, SECTOR_SIZE);
		for (i = 0; i < (NR_CONSOLES + 3); i++)
			fsbuf[0] |= 1 << i;

		/*assert(fsbuf[0] == 0x3F); 0011 1111 :
					  *   || ||||
					  *   || |||`--- bit 0 : reserved
					  *   || ||`---- bit 1 : the first inode,
					  *   || ||              which indicates `/'
					  *   || |`----- bit 2 : /dev_tty0
					  *   || `------ bit 3 : /dev_tty1
					  *   |`-------- bit 4 : /dev_tty2
					  *   `--------- bit 5 : /cmd.tar
					  */
		//WR_SECT(ROOT_DEV, 2);
		hd_rdwt_block(ROOT_DEV, 2*512, 512, fsbuf, 1);
		/************************/
		/*      secter map      */
		/************************/
		memset(fsbuf, 0, SECTOR_SIZE);
		int nr_sects = NR_DEFAULT_FILE_SECTS + 1;
		/*             ~~~~~~~~~~~~~~~~~~~|~   |
		 *                                |    `--- bit 0 is reserved
		 *                                `-------- for `/'
		 */
		for (i = 0; i < nr_sects / 8; i++)
			fsbuf[i] = 0xFF;

		for (j = 0; j < nr_sects % 8; j++)
			fsbuf[i] |= (1 << j);

		//WR_SECT(ROOT_DEV, 2 + sb.nr_imap_sects);
		hd_rdwt_block(ROOT_DEV, (2 + sb.nr_imap_sects)*512, 512, fsbuf, 1);
		/* zeromemory the rest sector-map */
		memset(fsbuf, 0, SECTOR_SIZE);
		for (i = 1; i < sb.nr_smap_sects; i++)
			//WR_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + i);
			hd_rdwt_block(ROOT_DEV, ( 2 + sb.nr_imap_sects + i)*512, 512, fsbuf, 1);



		/************************/
		/*       inodes         */
		/************************/
		/* inode of `/' */
		memset(fsbuf, 0, SECTOR_SIZE);
		struct inode * pi = (struct inode*)fsbuf;
		pi->i_mode = I_DIRECTORY;
		pi->i_size = DIR_ENTRY_SIZE * 5; /* 5 files:
						  * `.',
						  * `dev_tty0', `dev_tty1', `dev_tty2',
						  * `cmd.tar'
						  */
		pi->i_start_sect = sb.n_1st_sect;
		pi->i_nr_sects = NR_DEFAULT_FILE_SECTS;
		/* inode of `/dev_tty0~2' */
		for (i = 0; i < NR_CONSOLES; i++) {
			pi = (struct inode*)(fsbuf + (INODE_SIZE * (i + 1)));
			pi->i_mode = I_CHAR_SPECIAL;
			pi->i_size = 0;
			pi->i_start_sect = 0;
			pi->i_nr_sects = 0;
		}
		//WR_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + sb.nr_smap_sects);
		hd_rdwt_block(ROOT_DEV, (2 + sb.nr_imap_sects + sb.nr_smap_sects)*512, 512, fsbuf, 1);
		/************************/
		/*          `/'         */
		/************************/
		memset(fsbuf, 0, SECTOR_SIZE);
		struct dir_entry * pde = (struct dir_entry *)fsbuf;

		pde->inode_nr = 1;
		strcpy(pde->name, ".");

		/* dir entries of `/dev_tty0~2' */
		for (i = 0; i < NR_CONSOLES; i++) {
			pde++;
			pde->inode_nr = i + 2; /* dev_tty0's inode_nr is 2 */
			sprintf(pde->name, "dev_tty%d", i);
		}
		//(++pde)->inode_nr = NR_CONSOLES + 2;
		//sprintf(pde->name, "cmd.tar", i);
		//WR_SECT(ROOT_DEV, sb.n_1st_sect);
		hd_rdwt_block(ROOT_DEV, ( sb.n_1st_sect)*512, 512, fsbuf, 1);

		printl("Init Filesystem Suceess \n");

}
