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
//PRIVATE struct super_block* get_super_block(int device);
PRIVATE void read_super_block(int device);
PUBLIC void init_fs()
{

	int i;

	/* f_desc_table[] */
	for (i = 0; i < NR_FILE_DESC; i++)
		memset(&f_desc_table[i], 0, sizeof(struct file_desc));

	/* inode_table[] */
	for (i = 0; i < NR_INODE; i++)
		memset(&inode_table[i], 0, sizeof(struct inode));

	struct super_block * sb = super_block;
		for (; sb < &super_block[NR_SUPER_BLOCK]; sb++)
			sb->sb_dev = NO_DEV;

	mkfs();

	read_super_block(ROOT_DEV);

	sb = get_super_block(ROOT_DEV);
	//assert(sb->magic == MAGIC_V1);

	root_inode = get_inode(ROOT_DEV, ROOT_INODE);
	//printl("root node nr %d", root_inode->i_num);

	//for kernel kread fun test
	/*u8 buf[10];
	kread("/echo",(void*)buf,0,10);
	for(i=0;i<10;i++)
		printl("%d ",buf[i]);
	printl("\n");*/
}

PUBLIC int sys_writef(int fd, void* buf, int len)
{
	if (!(p_proc_ready->filp[fd]->fd_mode & O_RDWR))
			return 0;

	int pos = p_proc_ready->filp[fd]->fd_pos;

	struct inode * pin = p_proc_ready->filp[fd]->fd_inode;
	int imode = pin->i_mode & I_TYPE_MASK;
	int pos_end;
	pos_end = min(pos + len, pin->i_nr_sects * SECTOR_SIZE);

	int off = pos % SECTOR_SIZE;
	int rw_sect_min=pin->i_start_sect+(pos>>SECTOR_SIZE_SHIFT);
	int rw_sect_max=pin->i_start_sect+(pos_end>>SECTOR_SIZE_SHIFT);

	int chunk = min(rw_sect_max - rw_sect_min + 1,
			FSBUF_SIZE >> SECTOR_SIZE_SHIFT);

	int bytes_rw = 0;
	int bytes_left = len;
	int i;

	for (i = rw_sect_min; i <= rw_sect_max; i += chunk) {
				/* read/write this amount of bytes every time */
				int bytes = min(bytes_left, chunk * SECTOR_SIZE - off);
				/*rw_sector(DEV_READ,
					  pin->i_dev,
					  i * SECTOR_SIZE,
					  chunk * SECTOR_SIZE,
					  TASK_FS,
					  fsbuf);*/
				hd_rdwt_block( pin->i_dev,  i * SECTOR_SIZE, chunk * SECTOR_SIZE, fsbuf, 0);
				/* WRITE */
				phys_copy((void*)(fsbuf + off),
						  (void*)(buf + bytes_rw),
						  bytes);
					/*rw_sector(DEV_WRITE,
						  pin->i_dev,
						  i * SECTOR_SIZE,
						  chunk * SECTOR_SIZE,
						  TASK_FS,
						  fsbuf);*/
				hd_rdwt_block( pin->i_dev,  i * SECTOR_SIZE, chunk * SECTOR_SIZE, fsbuf, 1);
				off = 0;
				bytes_rw += bytes;
				p_proc_ready->filp[fd]->fd_pos += bytes;
				bytes_left -= bytes;
			}

			if (p_proc_ready->filp[fd]->fd_pos > pin->i_size) {
				/* update inode::size */
				pin->i_size = p_proc_ready->filp[fd]->fd_pos;
				/* write the updated i-node back to disk */
				sync_inode(pin);
			}
			return bytes_rw;
}

PUBLIC int sys_read(int fd, void* buf, int len)
{
		if (!(p_proc_ready->filp[fd]->fd_mode & O_RDWR))
			return 0;

		int pos = p_proc_ready->filp[fd]->fd_pos;

		struct inode * pin = p_proc_ready->filp[fd]->fd_inode;

		//assert(pin >= &inode_table[0] && pin < &inode_table[NR_INODE]);

		int imode = pin->i_mode & I_TYPE_MASK;
			int pos_end;
			pos_end = min(pos + len, pin->i_size);


			int off = pos % SECTOR_SIZE;
			int rw_sect_min=pin->i_start_sect+(pos>>SECTOR_SIZE_SHIFT);
			int rw_sect_max=pin->i_start_sect+(pos_end>>SECTOR_SIZE_SHIFT);

			int chunk = min(rw_sect_max - rw_sect_min + 1,
					FSBUF_SIZE >> SECTOR_SIZE_SHIFT);

			int bytes_rw = 0;
			int bytes_left = len;
			int i;
			for (i = rw_sect_min; i <= rw_sect_max; i += chunk) {
				/* read/write this amount of bytes every time */
				int bytes = min(bytes_left, chunk * SECTOR_SIZE - off);
				hd_rdwt_block( pin->i_dev,  i * SECTOR_SIZE, chunk * SECTOR_SIZE, fsbuf, 0);
				phys_copy((void*)(buf + bytes_rw),
						  (void*)(fsbuf + off),
						  bytes);
				off = 0;
				bytes_rw += bytes;
				p_proc_ready->filp[fd]->fd_pos += bytes;
				bytes_left -= bytes;
			}

			if (p_proc_ready->filp[fd]->fd_pos > pin->i_size) {
				/* update inode::size */
				pin->i_size = p_proc_ready->filp[fd]->fd_pos;
				/* write the updated i-node back to disk */
				sync_inode(pin);
			}

			return bytes_rw;

}
PUBLIC int sys_open(const char* pathname, int flags)
{
		int fd = -1; /* return value */

		/* find a free slot in PROCESS::filp[] */
		int i;
		for (i = 0; i < NR_FILES; i++) {
			if (p_proc_ready->filp[i] == 0) {
				fd = i;
				break;
			}
		}

		int inode_nr = search_file(pathname);

		struct inode * pin = 0;

		if (flags & O_CREAT) {
			if (inode_nr) {
				printl("{FS} file exists.\n");
				return -1;
			} else {
				pin = create_file(pathname, flags);
			}
		} else {
			//assert(flags & O_RDWR);

			char filename[MAX_PATH];
			struct inode * dir_inode;
			if (strip_path(filename, pathname, &dir_inode) != 0)
				return -1;
			pin = get_inode(dir_inode->i_dev, inode_nr);
		}
		if (pin) {
			/* connects proc with file_descriptor */
			p_proc_ready->filp[fd] = &f_desc_table[i];

			/* connects file_descriptor with inode */
			f_desc_table[i].fd_inode = pin;

			f_desc_table[i].fd_mode = flags;
			f_desc_table[i].fd_cnt = 1;
			f_desc_table[i].fd_pos = 0;

			int imode = pin->i_mode & I_TYPE_MASK;
		} else {
			return -1;
		}

		return fd;


}


PUBLIC int sys_close(int fd)
{
	//int fd = fs_msg.FD;
	put_inode(p_proc_ready->filp[fd]->fd_inode);
	if (--p_proc_ready->filp[fd]->fd_cnt == 0)
		p_proc_ready->filp[fd]->fd_inode = 0;
	p_proc_ready->filp[fd] = 0;

		return 0;
}

PUBLIC int sys_delete(const char* pathname)
{
		//char pathname[MAX_PATH];

		/* get parameters from the message */
		/*int name_len = fs_msg.NAME_LEN;	 length of filename
		int src = fs_msg.source;	 caller proc nr.
		assert(name_len < MAX_PATH);
		phys_copy((void*)va2la(TASK_FS, pathname),
			  (void*)va2la(src, fs_msg.PATHNAME),
			  name_len);
		pathname[name_len] = 0;*/

		if (strcmp(pathname , "/") == 0) {
			printl("{FS} FS:do_unlink():: cannot unlink the root\n");
			return -1;
		}

		int inode_nr = search_file(pathname);
		if (inode_nr == INVALID_INODE) {	/* file not found */
			printl("{FS} FS::do_unlink():: search_file() returns "
				"invalid inode: %s\n", pathname);
			return -1;
		}

		char filename[MAX_PATH];
		struct inode * dir_inode;
		if (strip_path(filename, pathname, &dir_inode) != 0)
			return -1;

		struct inode * pin = get_inode(dir_inode->i_dev, inode_nr);

		if (pin->i_mode != I_REGULAR) { /* can only remove regular files */
			printl("{FS} cannot remove file %s, because "
			       "it is not a regular file.\n",
			       pathname);
			return -1;
		}

		if (pin->i_cnt > 1) {	/* the file was opened */
			printl("{FS} cannot remove file %s, because pin->i_cnt is %d.\n",
			       pathname, pin->i_cnt);
			return -1;
		}

		struct super_block * sb = get_super_block(pin->i_dev);

		/*************************/
		/* free the bit in i-map */
		/*************************/
		int byte_idx = inode_nr / 8;
		int bit_idx = inode_nr % 8;
		//assert(byte_idx < SECTOR_SIZE);	/* we have only one i-map sector */
		/* read sector 2 (skip bootsect and superblk): */
		//RD_SECT(pin->i_dev, 2);
		hd_rdwt_block( pin->i_dev,  2* SECTOR_SIZE, SECTOR_SIZE, fsbuf, 0);
		//assert(fsbuf[byte_idx % SECTOR_SIZE] & (1 << bit_idx));
		fsbuf[byte_idx % SECTOR_SIZE] &= ~(1 << bit_idx);
		//WR_SECT(pin->i_dev, 2);
		hd_rdwt_block( pin->i_dev,  2* SECTOR_SIZE, SECTOR_SIZE, fsbuf, 1);
		/**************************/
		/* free the bits in s-map */
		/**************************/
		/*
		 *           bit_idx: bit idx in the entire i-map
		 *     ... ____|____
		 *                  \        .-- byte_cnt: how many bytes between
		 *                   \      |              the first and last byte
		 *        +-+-+-+-+-+-+-+-+ V +-+-+-+-+-+-+-+-+
		 *    ... | | | | | |*|*|*|...|*|*|*|*| | | | |
		 *        +-+-+-+-+-+-+-+-+   +-+-+-+-+-+-+-+-+
		 *         0 1 2 3 4 5 6 7     0 1 2 3 4 5 6 7
		 *  ...__/
		 *      byte_idx: byte idx in the entire i-map
		 */
		bit_idx  = pin->i_start_sect - sb->n_1st_sect + 1;
		byte_idx = bit_idx / 8;
		int bits_left = pin->i_nr_sects;
		int byte_cnt = (bits_left - (8 - (bit_idx % 8))) / 8;

		/* current sector nr. */
		int s = 2  /* 2: bootsect + superblk */
			+ sb->nr_imap_sects + byte_idx / SECTOR_SIZE;

		//RD_SECT(pin->i_dev, s);
		hd_rdwt_block( pin->i_dev,  s* SECTOR_SIZE, SECTOR_SIZE, fsbuf, 0);
		int i;
		/* clear the first byte */
		for (i = bit_idx % 8; (i < 8) && bits_left; i++,bits_left--) {
			//assert((fsbuf[byte_idx % SECTOR_SIZE] >> i & 1) == 1);
			fsbuf[byte_idx % SECTOR_SIZE] &= ~(1 << i);
		}

		/* clear bytes from the second byte to the second to last */
		int k;
		i = (byte_idx % SECTOR_SIZE) + 1;	/* the second byte */
		for (k = 0; k < byte_cnt; k++,i++,bits_left-=8) {
			if (i == SECTOR_SIZE) {
				i = 0;
				//WR_SECT(pin->i_dev, s);
				hd_rdwt_block( pin->i_dev,  s* SECTOR_SIZE, SECTOR_SIZE, fsbuf, 1);
				//RD_SECT(pin->i_dev, ++s);
				hd_rdwt_block( pin->i_dev,  ++s* SECTOR_SIZE, SECTOR_SIZE, fsbuf, 0);
			}
			//assert(fsbuf[i] == 0xFF);
			fsbuf[i] = 0;
		}

		/* clear the last byte */
		if (i == SECTOR_SIZE) {
			i = 0;
			//WR_SECT(pin->i_dev, s);
			hd_rdwt_block( pin->i_dev,  s* SECTOR_SIZE, SECTOR_SIZE, fsbuf, 1);
			//RD_SECT(pin->i_dev, ++s);
			hd_rdwt_block( pin->i_dev,  ++s* SECTOR_SIZE, SECTOR_SIZE, fsbuf, 0);
		}
		unsigned char mask = ~((unsigned char)(~0) << bits_left);
		//assert((fsbuf[i] & mask) == mask);
		fsbuf[i] &= (~0) << bits_left;
		//WR_SECT(pin->i_dev, s);
		hd_rdwt_block( pin->i_dev,  s* SECTOR_SIZE, SECTOR_SIZE, fsbuf, 1);

		/***************************/
		/* clear the i-node itself */
		/***************************/
		pin->i_mode = 0;
		pin->i_size = 0;
		pin->i_start_sect = 0;
		pin->i_nr_sects = 0;
		sync_inode(pin);
		/* release slot in inode_table[] */
		put_inode(pin);

		/************************************************/
		/* set the inode-nr to 0 in the directory entry */
		/************************************************/
		int dir_blk0_nr = dir_inode->i_start_sect;
		int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE) / SECTOR_SIZE;
		int nr_dir_entries =
			dir_inode->i_size / DIR_ENTRY_SIZE; /* including unused slots
							     * (the file has been
							     * deleted but the slot
							     * is still there)
							     */
		int m = 0;
		struct dir_entry * pde = 0;
		int flg = 0;
		int dir_size = 0;

		for (i = 0; i < nr_dir_blks; i++) {
			//RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
			hd_rdwt_block( pin->i_dev,  (dir_blk0_nr + i)* SECTOR_SIZE, SECTOR_SIZE, fsbuf, 0);
			pde = (struct dir_entry *)fsbuf;
			int j;
			for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++) {
				if (++m > nr_dir_entries)
					break;

				if (pde->inode_nr == inode_nr) {
					/* pde->inode_nr = 0; */
					memset(pde, 0, DIR_ENTRY_SIZE);
					//WR_SECT(dir_inode->i_dev, dir_blk0_nr + i);
					hd_rdwt_block( pin->i_dev,  (dir_blk0_nr + i)* SECTOR_SIZE, SECTOR_SIZE, fsbuf, 1);
					flg = 1;
					break;
				}

				if (pde->inode_nr != INVALID_INODE)
					dir_size += DIR_ENTRY_SIZE;
			}

			if (m > nr_dir_entries || /* all entries have been iterated OR */
			    flg) /* file is found */
				break;
		}
		//assert(flg);
		if (m == nr_dir_entries) { /* the file is the last one in the dir */
			dir_inode->i_size = dir_size;
			sync_inode(dir_inode);
		}

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
					  *   `--------- bit 5 : /echo
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
		//for echo command
		int bit_offset = INSTALL_START_SECT -
				sb.n_1st_sect + 1; /* sect M <-> bit (M - sb.n_1stsect + 1) */
			int bit_off_in_sect = bit_offset % (SECTOR_SIZE * 8);
			int bit_left = INSTALL_NR_SECTS;
			int cur_sect = bit_offset / (SECTOR_SIZE * 8);
			//RD_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + cur_sect);
			hd_rdwt_block(ROOT_DEV, ( 2 + sb.nr_imap_sects + cur_sect)*512, 512, fsbuf, 0);
			while (bit_left) {
				int byte_off = bit_off_in_sect / 8;
				/* this line is ineffecient in a loop, but I don't care */
				fsbuf[byte_off] |= 1 << (bit_off_in_sect % 8);
				bit_left--;
				bit_off_in_sect++;
				if (bit_off_in_sect == (SECTOR_SIZE * 8)) {
					//WR_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + cur_sect);
					hd_rdwt_block(ROOT_DEV, ( 2 + sb.nr_imap_sects + cur_sect)*512, 512, fsbuf, 1);
					cur_sect++;
					//RD_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + cur_sect);
					hd_rdwt_block(ROOT_DEV, ( 2 + sb.nr_imap_sects + cur_sect)*512, 512, fsbuf, 0);
					bit_off_in_sect = 0;
				}
			}
			//WR_SECT(ROOT_DEV, 2 + sb.nr_imap_sects + cur_sect);
			hd_rdwt_block(ROOT_DEV, (2 + sb.nr_imap_sects + cur_sect)*512, 512, fsbuf, 1);

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
		pi = (struct inode*)(fsbuf + (INODE_SIZE * (NR_CONSOLES + 1)));
		pi->i_mode = I_REGULAR;
		pi->i_size = INSTALL_NR_SECTS * SECTOR_SIZE;
		pi->i_start_sect = INSTALL_START_SECT;
		pi->i_nr_sects = INSTALL_NR_SECTS;
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
		(++pde)->inode_nr = NR_CONSOLES + 2;
		sprintf(pde->name, "echo", i);
		//WR_SECT(ROOT_DEV, sb.n_1st_sect);
		hd_rdwt_block(ROOT_DEV, ( sb.n_1st_sect)*512, 512, fsbuf, 1);

		printl("Init Filesystem Suceess \n");

}

PRIVATE void read_super_block(int dev)
{
	int i;

	hd_rdwt_block(dev, 1*SECTOR_SIZE, 512, fsbuf, 0);
	/* find a free slot in super_block[] */
	for (i = 0; i < NR_SUPER_BLOCK; i++)
		if (super_block[i].sb_dev == NO_DEV)
			break;
	//if (i == NR_SUPER_BLOCK)
		//panic("super_block slots used up");

	//assert(i == 0); /* currently we use only the 1st slot */

	struct super_block * psb = (struct super_block *)fsbuf;

	super_block[i] = *psb;
	super_block[i].sb_dev = dev;
}

PUBLIC  struct super_block * get_super_block(int dev)
{
	struct super_block * sb = super_block;
	for (; sb < &super_block[NR_SUPER_BLOCK]; sb++)
		if (sb->sb_dev == dev)
			return sb;

	//panic("super block of devie %d not found.\n", dev);

	return 0;
}

PUBLIC struct inode * get_inode(int dev, int num)
{
	if (num == 0)
		return 0;

	struct inode * p;
	struct inode * q = (struct inode *)0;
	for (p = &inode_table[0]; p < &inode_table[NR_INODE]; p++) {
		if (p->i_cnt) {	/* not a free slot */
			if ((p->i_dev == dev) && (p->i_num == num)) {
				/* this is the inode we want */
				p->i_cnt++;
				return p;
			}
		}
		else {		/* a free slot */
			if (!q) /* q hasn't been assigned yet */
				q = p; /* q <- the 1st free slot */
		}
	}

	/*if (!q)
		panic("the inode table is full");*/

	q->i_dev = dev;
	q->i_num = num;
	q->i_cnt = 1;

	struct super_block * sb = get_super_block(dev);
	int blk_nr = 1 + 1 + sb->nr_imap_sects + sb->nr_smap_sects +
		((num - 1) / (SECTOR_SIZE / INODE_SIZE));
	//RD_SECT(dev, blk_nr);
	hd_rdwt_block(dev, (blk_nr)*512, 512, fsbuf, 0);
	struct inode * pinode =
		(struct inode*)((u8*)fsbuf +
				((num - 1 ) % (SECTOR_SIZE / INODE_SIZE))
				 * INODE_SIZE);
	q->i_mode = pinode->i_mode;
	q->i_size = pinode->i_size;
	q->i_start_sect = pinode->i_start_sect;
	q->i_nr_sects = pinode->i_nr_sects;
	return q;
}

PUBLIC int kread(const char* pathname, void* buf, int pos, int len)
{
		int inode_nr = search_file(pathname);
		printl("find echo node_nr %d \n",inode_nr);
		struct inode * dir_inode;
		struct inode * pin=0;
		char filename[MAX_PATH];
		if (strip_path(filename, pathname, &dir_inode) != 0)
						return -1;
		pin = get_inode(dir_inode->i_dev, inode_nr);
		printl("get node Success %d  start %x size %d\n", pin->i_num, pin->i_start_sect ,pin->i_size);
		int imode = pin->i_mode & I_TYPE_MASK;
		int pos_end;
		pos_end = min(pos + len, pin->i_size);

		int off = pos % SECTOR_SIZE;
		int rw_sect_min = pin->i_start_sect + (pos >> SECTOR_SIZE_SHIFT);
		int rw_sect_max = pin->i_start_sect + (pos_end >> SECTOR_SIZE_SHIFT);

		/*int chunk = min(rw_sect_max - rw_sect_min + 1,
				FSBUF_SIZE >> SECTOR_SIZE_SHIFT);*/
		//printl("chunk is %d \n",chunk);
		int bytes_rw = 0;
		int bytes_left = len;
		int i;
		for (i = rw_sect_min; i <= rw_sect_max; i += 1) {
			/* read/write this amount of bytes every time */
			int bytes = min(bytes_left,  SECTOR_SIZE - off);
			hd_rdwt_block(pin->i_dev, i * SECTOR_SIZE, SECTOR_SIZE, fsbuf,
					0);

			phys_copy((void*) (buf + bytes_rw), (void*) (fsbuf + off), bytes);
			off = 0;
			bytes_rw += bytes;
			bytes_left -= bytes;
		}
		printl("Read success \n");
		return bytes_rw;

}
PUBLIC void put_inode(struct inode * pinode)
{
	//assert(pinode->i_cnt > 0);
	pinode->i_cnt--;
}
