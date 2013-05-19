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

PUBLIC void put_inode(struct inode * pinode)
{
	//assert(pinode->i_cnt > 0);
	pinode->i_cnt--;
}
