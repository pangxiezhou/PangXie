/*
 * hd.c
 *
 *  Created on: 2013-5-17
 *      Author: bear
 */
#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
#include "hd.h"

PRIVATE void hd_handler();
PRIVATE void	hd_cmd_out(struct hd_cmd* cmd);
PRIVATE void	get_part_table(int drive, int sect_nr, struct part_ent * entry);
PRIVATE void	partition(int device, int style);
PRIVATE void	print_hdinfo(struct hd_info * hdi);
PRIVATE void hd_identify(int drive);
PRIVATE void print_identify_info(u16* hdinfo);
PRIVATE void hd_open(int device);

PRIVATE int DataReady = 0;
PRIVATE	u8		hdbuf[SECTOR_SIZE];
PRIVATE	struct hd_info	hd_info[1];
PUBLIC  void init_hd()
{
	int i;

	/* Get the number of drives from the BIOS data area */
	u8 * pNrDrives = (u8*)(0x475);
	printl("{HD} NrDrives:%d. \n", *pNrDrives);
	//printl("Test ,Test , Test ");

	//assert(*pNrDrives);

	put_irq_handler(AT_WINI_IRQ, hd_handler);
	enable_irq(CASCADE_IRQ);
	enable_irq(AT_WINI_IRQ);

	for (i = 0; i < (sizeof(hd_info) / sizeof(hd_info[0])); i++)
		memset(&hd_info[i], 0, sizeof(hd_info[0]));
	hd_info[0].open_cnt = 0;
	hd_open(1);
	//debug

}

PRIVATE void hd_open(int device)
{
	//printl("Device %d open \n", device);
	int drive = DRV_OF_DEV(device);
	//assert(drive == 0);	/* only one drive */

	hd_identify(drive);
	if(hd_info[drive].open_cnt++==0){
		//printl("first open get partition table \n");
		partition(drive * (NR_PART_PER_DRIVE + 1), P_PRIMARY);
		print_hdinfo(&hd_info[drive]);
	}
	//disk rdwt test
		/*printl("disk rw test \n");
		u8 wbuf[512]={1,2,3,4,5};
		u8 rbuf[512];
		hd_rdwt_block(1, 2048, 512, (void*)wbuf, 1);
		printl("disk write success \n");
		hd_rdwt_block(1, 2048, 512, (void*)rbuf, 0);
		u8 index = 0;
		for(index=0;index<5;index++){
			printl("%d", rbuf[index]);
		}
		printl("\n");
		printl("disk read success\n");*/

}

PRIVATE void hd_handler()
{
	DataReady = 1;
}

PRIVATE void hd_identify(int drive)
{
	struct hd_cmd cmd;
	cmd.device  = MAKE_DEVICE_REG(0, drive, 0);
	cmd.command = ATA_IDENTIFY;
	hd_cmd_out(&cmd);
	while(!DataReady) ;
	DataReady = 0;
	port_read(REG_DATA, hdbuf, SECTOR_SIZE);

	print_identify_info((u16*)hdbuf);

	u16* hdinfo = (u16*)hdbuf;

	hd_info[drive].primary[0].base = 0;
	 //Total Nr of User Addressable Sectors
	hd_info[drive].primary[0].size = ((int)hdinfo[61] << 16) + hdinfo[60];



}



PRIVATE void print_identify_info(u16* hdinfo)
{
	int i, k;
	char s[64];

	struct iden_info_ascii {
		int idx;
		int len;
		char * desc;
	} iinfo[] = {{10, 20, "HD SN"}, /* Serial number in ASCII */
		     {27, 40, "HD Model"} /* Model number in ASCII */ };

	for (k = 0; k < sizeof(iinfo)/sizeof(iinfo[0]); k++) {
		char * p = (char*)&hdinfo[iinfo[k].idx];
		for (i = 0; i < iinfo[k].len/2; i++) {
			s[i*2+1] = *p++;
			s[i*2] = *p++;
		}
		s[i*2] = 0;
		printl("{HD} %s: %s\n", iinfo[k].desc, s);
	}

	int capabilities = hdinfo[49];
	printl("{HD} LBA supported: %s\n",
	       (capabilities & 0x0200) ? "Yes" : "No");

	int cmd_set_supported = hdinfo[83];
	printl("{HD} LBA48 supported: %s\n",
	       (cmd_set_supported & 0x0400) ? "Yes" : "No");

	int sectors = ((int)hdinfo[61] << 16) + hdinfo[60];
	printl("{HD} HD size: %dMB\n", sectors * 512 / 1000000);
}

PRIVATE void print_hdinfo(struct hd_info * hdi)
{
	int i;
	for (i = 0; i < NR_PART_PER_DRIVE + 1; i++) {
		printl("{HD} %sPART_%d: base %d(0x%x), size %d(0x%x) (in sector)\n",
		       i == 0 ? " " : "     ",
		       i,
		       hdi->primary[i].base,
		       hdi->primary[i].base,
		       hdi->primary[i].size,
		       hdi->primary[i].size);
	}
	for (i = 0; i < NR_SUB_PER_DRIVE; i++) {
		if (hdi->logical[i].size == 0)
			continue;
		printl("{HD}          "
		       "%d: base %d(0x%x), size %d(0x%x) (in sector)\n",
		       i,
		       hdi->logical[i].base,
		       hdi->logical[i].base,
		       hdi->logical[i].size,
		       hdi->logical[i].size);
	}
}

PRIVATE void partition(int device, int style)
{
	//printl("partition start \n");
	int i;
	int drive = DRV_OF_DEV(device);
	struct hd_info * hdi = &hd_info[drive];

	struct part_ent part_tbl[NR_SUB_PER_DRIVE];

	if (style == P_PRIMARY) {
		get_part_table(drive, drive, part_tbl);

		int nr_prim_parts = 0;
		for (i = 0; i < NR_PART_PER_DRIVE; i++) { /* 0~3 */
			if (part_tbl[i].sys_id == NO_PART)
				continue;

			nr_prim_parts++;
			int dev_nr = i + 1;		  /* 1~4 */
			hdi->primary[dev_nr].base = part_tbl[i].start_sect;
			hdi->primary[dev_nr].size = part_tbl[i].nr_sects;

			if (part_tbl[i].sys_id == EXT_PART) /* extended */
				partition(device + dev_nr, P_EXTENDED);
		}
		//assert(nr_prim_parts != 0);
	}
	else if (style == P_EXTENDED) {
		int j = device % NR_PRIM_PER_DRIVE; /* 1~4 */
		int ext_start_sect = hdi->primary[j].base;
		int s = ext_start_sect;
		int nr_1st_sub = (j - 1) * NR_SUB_PER_PART; /* 0/16/32/48 */

		for (i = 0; i < NR_SUB_PER_PART; i++) {
			int dev_nr = nr_1st_sub + i;/* 0~15/16~31/32~47/48~63 */

			get_part_table(drive, s, part_tbl);

			hdi->logical[dev_nr].base = s + part_tbl[0].start_sect;
			hdi->logical[dev_nr].size = part_tbl[0].nr_sects;

			s = ext_start_sect + part_tbl[1].start_sect;

			/* no more logical partitions
			   in this extended partition */
			if (part_tbl[1].sys_id == NO_PART)
				break;
		}
	}
	else {
		//assert(0);
	}
}
PRIVATE void get_part_table(int drive, int sect_nr, struct part_ent * entry)
{
	struct hd_cmd cmd;
	cmd.features	= 0;
	cmd.count	= 1;
	cmd.lba_low	= sect_nr & 0xFF;
	cmd.lba_mid	= (sect_nr >>  8) & 0xFF;
	cmd.lba_high	= (sect_nr >> 16) & 0xFF;
	cmd.device	= MAKE_DEVICE_REG(1, /* LBA mode*/
					  drive,
					  (sect_nr >> 24) & 0xF);
	cmd.command	= ATA_READ;
	hd_cmd_out(&cmd);
	while(!DataReady) ;
	DataReady = 0;
	port_read(REG_DATA, hdbuf, SECTOR_SIZE);
	memcpy(entry,
	       hdbuf + PARTITION_TABLE_OFFSET,
	       sizeof(struct part_ent) * NR_PART_PER_DRIVE);
}

PUBLIC void hd_rdwt_block(int device, u64 pos, int count, void* buf, int rw)
{
	int drive = DRV_OF_DEV(device);


	//assert((pos >> SECTOR_SIZE_SHIFT) < (1 << 31));

	/**
	 * We only allow to R/W from a SECTOR boundary:
	 */
	//assert((pos & 0x1FF) == 0);

	u32 sect_nr = (u32)(pos >> SECTOR_SIZE_SHIFT); /* pos / SECTOR_SIZE */
	int logidx = (device - MINOR_hd1a) % NR_SUB_PER_DRIVE;
	sect_nr += device < MAX_PRIM ?
		hd_info[drive].primary[device].base :
		hd_info[drive].logical[logidx].base;
	//printl("sector_nr %d \n",sect_nr);
	struct hd_cmd cmd;
	cmd.features	= 0;
	cmd.count	= (count + SECTOR_SIZE - 1) / SECTOR_SIZE;
	cmd.lba_low	= sect_nr & 0xFF;
	cmd.lba_mid	= (sect_nr >>  8) & 0xFF;
	cmd.lba_high	= (sect_nr >> 16) & 0xFF;
	cmd.device	= MAKE_DEVICE_REG(1, drive, (sect_nr >> 24) & 0xF);
	cmd.command	= (rw) ? ATA_WRITE:ATA_READ ;
	hd_cmd_out(&cmd);

	int bytes_left = count;
	void * la = buf;

	while (bytes_left>0) {
		//printl("Read Start Loop \n");
		int bytes = min(SECTOR_SIZE, bytes_left);
		if (rw==0) {
			while(!DataReady) ;
			DataReady = 0;
			//printl("Read Data Ready \n");
			port_read(REG_DATA, hdbuf, SECTOR_SIZE);
			phys_copy(la,  hdbuf, bytes);
		}
		else {

			while(in_byte(REG_STATUS)&STATUS_DRQ!=STATUS_DRQ) ;
			//printl("Data Require Input \n");
			port_write(REG_DATA, la, bytes);
			while(!DataReady) ;
			DataReady = 0;
		}
		bytes_left -= SECTOR_SIZE;
		la += SECTOR_SIZE;
	}
}

PUBLIC struct part_info* get_geo(int device)
{
	int drive = DRV_OF_DEV(device);
	struct hd_info* hdi = &hd_info[drive];
	return  device < MAX_PRIM ?
					   &hdi->primary[device] :
					   &hdi->logical[(device - MINOR_hd1a) %
							NR_SUB_PER_DRIVE];
}
PRIVATE void hd_cmd_out(struct hd_cmd* cmd)
{

	out_byte(REG_DEV_CTRL, 0);
	out_byte(REG_FEATURES, cmd->features);
	out_byte(REG_NSECTOR,  cmd->count);
	out_byte(REG_LBA_LOW,  cmd->lba_low);
	out_byte(REG_LBA_MID,  cmd->lba_mid);
	out_byte(REG_LBA_HIGH, cmd->lba_high);
	out_byte(REG_DEVICE,  cmd->device);
	out_byte(REG_CMD,     cmd->command);

}
