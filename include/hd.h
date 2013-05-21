/*
 * hd.h
 *
 *  Created on: 2013-5-17
 *      Author: bear
 */

#ifndef HD_H_
#define HD_H_

struct part_ent {
	u8 boot_ind;

	u8 start_head;

	u8 start_sector;
	u8 start_cyl;

	u8 sys_id;

	u8 end_head;
	u8 end_sector;

	u8 end_cyl;

	u32 start_sect;
	u32 nr_sects;
} PARTITION_ENTRY;

//device num
#define	MAX_DRIVES		2
#define	NR_PART_PER_DRIVE	4
#define	NR_SUB_PER_PART		16
#define	NR_SUB_PER_DRIVE	(NR_SUB_PER_PART * NR_PART_PER_DRIVE)
#define	NR_PRIM_PER_DRIVE	(NR_PART_PER_DRIVE + 1)

#define	DRV_OF_DEV(dev) (dev <= MAX_PRIM ? \
			 dev / NR_PRIM_PER_DRIVE : \
			 (dev - MINOR_hd1a) / NR_SUB_PER_DRIVE)

#define	MINOR_hd1a		0x10
#define	MINOR_hd2a		(MINOR_hd1a+NR_SUB_PER_PART)

#define	P_PRIMARY	0
#define	P_EXTENDED	1

#define ORANGES_PART	0x99	/* Orange'S partition */
#define NO_PART		0x00	/* unused entry */
#define EXT_PART	0x05	/* extended partition */
/**
 * @def MAX_PRIM
 * Defines the max minor number of the primary partitions.
 * If there are 2 disks, prim_dev ranges in hd[0-9], this macro will
 * equals 9.
 */
#define	MAX_PRIM		(MAX_DRIVES * NR_PRIM_PER_DRIVE - 1)

#define	MAX_SUBPARTITIONS	(NR_SUB_PER_DRIVE * MAX_DRIVES)

#define REG_DATA	0x1F0
#define REG_FEATURES	0x1F1
#define REG_ERROR	REG_FEATURES

#define REG_NSECTOR	0x1F2
#define REG_LBA_LOW	0x1F3
#define REG_LBA_MID	0x1F4
#define REG_LBA_HIGH	0x1F5
#define REG_DEVICE	0x1F6

#define REG_STATUS	0x1F7
#define	STATUS_BSY	0x80
#define	STATUS_DRDY	0x40
#define	STATUS_DFSE	0x20
#define	STATUS_DSC	0x10
#define	STATUS_DRQ	0x08
#define	STATUS_CORR	0x04
#define	STATUS_IDX	0x02
#define	STATUS_ERR	0x01

#define REG_CMD		REG_STATUS
#define REG_DEV_CTRL	0x3F6
#define REG_ALT_STATUS	REG_DEV_CTRL
#define REG_DRV_ADDR	0x3F7

struct hd_cmd {
	u8 	features;
	u8	    count;
	u8 	lba_low;
	u8 	lba_mid;
	u8 	lba_high;
	u8		device;
	u8		command;
};

struct part_info {
	u32	base;	/* # of start sector (NOT byte offset, but SECTOR) */
	u32	size;	/* how many sectors in this partition */
};

struct hd_info
{
	int			open_cnt;
	struct part_info	primary[NR_PRIM_PER_DRIVE];
	struct part_info	logical[NR_SUB_PER_DRIVE];
};

#define	PARTITION_TABLE_OFFSET	0x1BE
#define ATA_IDENTIFY		0xEC
#define ATA_READ		0x20
#define ATA_WRITE		0x30
#define SECTOR_SIZE	512
#define SECTOR_SIZE_SHIFT	9

#define	MAKE_DEVICE_REG(lba,drv,lba_highest) (((lba) << 6) |		\
					      ((drv) << 4) |		\
					      (lba_highest & 0xF) | 0xA0)

PUBLIC void	init_hd	();
PUBLIC void hd_rdwt_block(int device, u64 pos, int count, void* buf, int rw);
PUBLIC struct part_info* get_geo(int device);
#endif
