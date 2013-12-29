#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef unsigned char uchar;
#define EXE_CODE_SIZE (0x01bd - 0x0000 + 1)

/* 
 * Max CHS tuple = (1023, 254, 63), which stand for 
 * the 1024th cylinder, 255th head and 63rd sector,
 * due to the fact, cylinder and head counts begin at zero.
 */
#pragma pack(1)
struct primary_par {
	uchar boot_indicator; 
	uchar start_head;
	uchar start_cylinder_sector[2];
	uchar descriptor;
	uchar end_head;
	uchar end_cylinder_sector[2];
	uchar start_sector[4];
	uchar n_sectors[4];
};

/* 512 Bytes */
struct MBR {
	uchar exe_code[EXE_CODE_SIZE];
	struct primary_par par[4];
	uchar magic[2];
};

/* GPT (GUID Partition Table) */
/* 512 Bytes */
struct GPT_header {
	char     signature[8];	
	char     revision[4];
	uint32_t header_sz;
	uint32_t crc32;
	uint32_t reserved;
	uint64_t cur_lba;
	uint64_t bkup_lba;
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	uint64_t UUID_0; 
	uint64_t UUID_1; 
	uint64_t par_entries_start;
	uint32_t n_par_entries;
	uint32_t par_entry_sz;
	uint32_t crc32_entry;
	char     reversed2[420]; /* 512B in total */
};

/* 128 Bytes */
struct GPT_entry {
	uint64_t type_UUID_0; 
	uint64_t type_UUID_1; 
	uint64_t par_UUID_0;
	uint64_t par_UUID_1;
	uint64_t start_lba; 
	/* dd if=/dev/sda skip=lba+2 bs=512 count=1 2>/dev/null | hd */
	/* or */
	/* dd if=/dev/sda2 skip=2 bs=512 count=1 2>/dev/null | hd */
	uint64_t end_lba; 
	uint64_t attributes; 
	char     type_name[72]; 
};
#pragma pack()

uint64_t UUID_print(uint64_t uuid_0, uint64_t uuid_1)
{
	// ls -l /dev/disk/by-uuid
	// cat /etc/fstab
	
	// aaaabbbbcccccccc-ddddddddddddeeee
	// cccccccc-bbbb-aaaa-(eeee)-(dddddddddddd)
	char buff[64], tmp[16], res[64];
	int i;

	res[0] = '\0';
	sprintf(buff, "%016lx%016lx\n", uuid_0, uuid_1);

	for (i = 0; i < 8; i++) {
		tmp[i] = buff[8 + i];
	}
	tmp[i] = '\0';
	sprintf(res + strlen(res), "%8s-", tmp);

	for (i = 0; i < 4; i++) {
		tmp[i] = buff[4 + i];
	}
	tmp[i] = '\0';
	sprintf(res + strlen(res), "%4s-", tmp);
	
	for (i = 0; i < 4; i++) {
		tmp[i] = buff[i];
	}
	tmp[i] = '\0';
	sprintf(res + strlen(res), "%4s-", tmp);

	for (i = 0; i < 4; i += 2) {
		tmp[i] = buff[31 - i - 1];
		tmp[i + 1] = buff[31 - i];
	}
	tmp[i] = '\0';
	sprintf(res + strlen(res), "%4s-", tmp);

	for (i = 0; i < 12; i += 2) {
		tmp[i] = buff[27 - i - 1];
		tmp[i + 1] = buff[27 - i];
	}
	tmp[i] = '\0';
	sprintf(res + strlen(res), "%12s", tmp);
	
	printf("%s\n", res);
	if (strcmp(res, "21686148-6449-6e6f-744e-656564454649") == 0)
		printf("(BIOS boot partition)");
	else if (strcmp(res, "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7") == 0)
		printf("(Linux EXT / Windows basic partition)");
	else if (strcmp(res, "0657fd6d-a4ab-43c4-84e5-0933c84b4f4f") == 0)
		printf("(Linux Swap)");
	else 
		printf("(unknown)");
	printf("\n");
}

int main(int argc, const char *argv[])
{
	FILE* fp;
	struct MBR mbr;
	unsigned int i, j, k;
	char type[3];
	char sys_cmd[64];
	struct GPT_header gpt_h;
	struct GPT_entry  gpt_ent;

	if (argc != 2) {
		printf("%s\n", "need one arg.");
		return 0;
	}

	fp = fopen(argv[1], "r");

	if (!fp) {
		printf("%s\n", "fail to open.");
		return 0;
	}

	fread(&mbr, sizeof(struct MBR), 1, fp);
	fread(&gpt_h, sizeof(struct GPT_header), 1, fp);

	for (i = 0; i < 4; i++) {
		printf("===================\n");
		printf("primary partition #%d:\n", i);
		printf("bootable:\t 0x%x\n", mbr.par[i].boot_indicator);
		j = mbr.par[i].start_head;

		printf("start head:\t 0x%x(%d)\n", j, j);
		j = (mbr.par[i].start_cylinder_sector[1] & 
				0xc0) << 2;
		j = mbr.par[i].start_cylinder_sector[2] + j;
		printf("start cylinder:\t 0x%x(%d)\n", j, j);
		j = (mbr.par[i].start_cylinder_sector[1]) & 0x3f;
		printf("start sector:\t 0x%x(%d)\n", j, j);

		printf("partition type:\t 0x%x\n", 
				mbr.par[i].descriptor);
		sprintf(type, "%02x", mbr.par[i].descriptor);
		sys_cmd[0] = '\0';
		strcat(sys_cmd, "cat list | grep ^");
		strcat(sys_cmd, type);
		system(sys_cmd);

		printf("end head:\t 0x%x(%d)\n", j, j);
		j = (mbr.par[i].end_cylinder_sector[1] & 
				0xc0) << 2;
		j = mbr.par[i].end_cylinder_sector[2] + j;
		printf("end cylinder:\t 0x%x(%d)\n", j, j);
		j = (mbr.par[i].end_cylinder_sector[1]) & 0x3f;
		printf("end sector:\t 0x%x(%d)\n", j, j);
		
		j = *((uint32_t*)mbr.par[i].start_sector);
		k = *((uint32_t*)mbr.par[i].n_sectors);
		printf("LBA: from %d", j);
		printf(" to %d", j + k - 1);
		printf(" (%d sectors)\n", k);
	}
	printf("0x%x\n", mbr.magic[0]);
	printf("0x%x\n", mbr.magic[1]);

	printf("====GPT====\n");
	printf("sig:\t\t %s\n", gpt_h.signature);
	printf("revision:\t %s\n", gpt_h.revision);
	printf("header sz:\t %d\n", gpt_h.header_sz);
	printf("crc32:\t\t %d\n", gpt_h.crc32);
	if (gpt_h.reserved != 0)
		printf("reserved section not equal to zero!\n");
	printf("current lba:\t %lu\n", gpt_h.cur_lba);
	printf("backup lba:\t %lu\n", gpt_h.bkup_lba);
	printf("first usable lba:\t %lu\n", gpt_h.first_usable_lba);
	printf("last usable lba:\t %lu\n", gpt_h.last_usable_lba);
	printf("entry start:\t %lu\n", gpt_h.par_entries_start);
	printf("entry number:\t %d\n", gpt_h.n_par_entries);
	printf("entry size:\t %d\n", gpt_h.par_entry_sz);

	//for (i = 0; i < gpt_h.n_par_entries; i++) {
	for (i = 0; i < 3; i++) {
		fread(&gpt_ent, sizeof(struct GPT_entry), 1, fp);
		UUID_print(gpt_ent.type_UUID_0, gpt_ent.type_UUID_1);
		printf("partition start:\t %lu\n", gpt_ent.start_lba);
		printf("partition end:\t\t %lu\n", gpt_ent.end_lba);
	}

	return 0;
}
