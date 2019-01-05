/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * made from cmd_ext2, which was:
 *
 * (C) Copyright 2004
 * esd gmbh <www.esd-electronics.com>
 * Reinhard Arlt <reinhard.arlt@esd-electronics.com>
 *
 * made from cmd_reiserfs by
 *
 * (C) Copyright 2003 - 2004
 * Sysgo Real-Time Solutions, AG <www.elinos.com>
 * Pavel Bartusek <pba@sysgo.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <config.h>
#include <command.h>
#include <part.h>
#include <vsprintf.h>
#include <stdlib.h>

#ifndef CONFIG_PARTITION_UUIDS
#error CONFIG_PARTITION_UUIDS must be enabled for CONFIG_CMD_PART to be enabled
#endif

enum cmd_part_info {
	CMD_PART_INFO_START = 0,
	CMD_PART_INFO_SIZE,
	CMD_PART_INFO_BLKSIZE,
};

int do_part_info(int argc, char * const argv[])
{
	int part;
	struct blk_desc *dev_desc;
	disk_partition_t info;
	char buf_convert[20];
	if (argc < 2)
		return CMD_RET_USAGE;
	if (argc > 5)
		return CMD_RET_USAGE;

	part = blk_get_device_part_str(argv[0], argv[1], &dev_desc, &info, 0);
	if (part < 0)
		return 1;

	snprintf(buf_convert, sizeof(buf_convert), LBAF, info.start);
	if (argc > 2)
		env_set(argv[2], buf_convert);
	else
		printf("Partition start :0x%s\n", buf_convert);

	snprintf(buf_convert, sizeof(buf_convert), LBAF, info.size);
	if (argc > 3)
		env_set(argv[3], buf_convert);
	else
		printf("Partition size  :0x%s\n", buf_convert);

	snprintf(buf_convert, sizeof(buf_convert), "%lx", info.blksz);
	if (argc > 4)
		env_set(argv[4], buf_convert);
	else
		printf("Block size      :0x%s bytes\n", buf_convert);
	return 0;
}

int do_part_find(int argc, char * const argv[])
{
	int part, dev, i, ret=1;
	struct blk_desc *dev_desc;
	disk_partition_t info;
	size_t mnb_of;
	const char *value_str;
	char *dup_str = NULL;
	const char *criteria_str;
	char buf_str[4];

	if (argc < 3)
		return CMD_RET_USAGE;
	if (argc > 4)
		return CMD_RET_USAGE;

	dev = blk_get_device_part_str(argv[0], argv[1], &dev_desc, &info, 0);
	if (dev < 0)
		return 1;

	/* check and split criteria:value */
	value_str = strchr(argv[2],':');
	if (!value_str)
		return CMD_RET_USAGE;

	dup_str = strdup(argv[2]);
	if(!dup_str)
		return 1;

	if ( (value_str - argv[2]) > strlen(dup_str) )
		return 1;

	dup_str[value_str - argv[2]] = 0;
	criteria_str = dup_str;
	value_str++;

	if(!strcmp(criteria_str, "uuid"))
		mnb_of = offsetof(disk_partition_t, uuid);
	else if(!strcmp(criteria_str, "label"))
		mnb_of = offsetof(disk_partition_t, name);
	else {
		printf("Bad criteria: %s\n", criteria_str);
		ret = CMD_RET_USAGE;
		goto end_of_part_find;
	}

	part=-1;
	for (i = 1; i <= MAX_SEARCH_PARTITIONS; i++) {
		ret = part_get_info(dev_desc, i, &info);
		if (ret)
			continue;

		if(!strcmp(((char*)&info + mnb_of), value_str)) {
			part = i;
			ret = 0;
			break;
		}
	}

	if ( part == -1) {
		printf("No partition found\n");
		ret = 1;
		goto end_of_part_find;
	}

	snprintf(buf_str, sizeof(buf_str), "%d", part);
	if ( argc > 3)
		env_set(argv[3],buf_str);
	else
		printf("Partition %s correspond to %s == %s\n",
				buf_str, criteria_str, value_str);

end_of_part_find:
	if( dup_str )
		free (dup_str);

	return ret;
}

static int do_part_uuid(int argc, char * const argv[])
{
	int part;
	struct blk_desc *dev_desc;
	disk_partition_t info;

	if (argc < 2)
		return CMD_RET_USAGE;
	if (argc > 3)
		return CMD_RET_USAGE;

	part = blk_get_device_part_str(argv[0], argv[1], &dev_desc, &info, 0);
	if (part < 0)
		return 1;

	if (argc > 2)
		env_set(argv[2], info.uuid);
	else
		printf("%s\n", info.uuid);

	return 0;
}

static int do_part_list(int argc, char * const argv[])
{
	int ret;
	struct blk_desc *desc;
	char *var = NULL;
	bool bootable = false;
	int i;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (argc > 2) {
		for (i = 2; i < argc ; i++) {
			if (argv[i][0] == '-') {
				if (!strcmp(argv[i], "-bootable")) {
					bootable = true;
				} else {
					printf("Unknown option %s\n", argv[i]);
					return CMD_RET_USAGE;
				}
			} else {
				var = argv[i];
				break;
			}
		}

		/* Loops should have been exited at the last argument, which
		 * as it contained the variable */
		if (argc != i + 1)
			return CMD_RET_USAGE;
	}

	ret = blk_get_device_by_str(argv[0], argv[1], &desc);
	if (ret < 0)
		return 1;

	if (var != NULL) {
		int p;
		char str[512] = { '\0', };
		disk_partition_t info;

		for (p = 1; p < MAX_SEARCH_PARTITIONS; p++) {
			char t[5];
			int r = part_get_info(desc, p, &info);

			if (r != 0)
				continue;

			if (bootable && !info.bootable)
				continue;

			sprintf(t, "%s%x", str[0] ? " " : "", p);
			strcat(str, t);
		}
		env_set(var, str);
		return 0;
	}

	part_print(desc);

	return 0;
}

static int get_part_info(int argc, char * const argv[], enum cmd_part_info param)
{
	struct blk_desc *desc;
	disk_partition_t info;
	char buf[512] = { 0 };
	char *endp;
	int part;
	int err;
	int ret;

	if (argc < 3)
		return CMD_RET_USAGE;
	if (argc > 4)
		return CMD_RET_USAGE;

	ret = blk_get_device_by_str(argv[0], argv[1], &desc);
	if (ret < 0)
		return 1;

	part = simple_strtoul(argv[2], &endp, 0);
	if (*endp == '\0') {
		err = part_get_info(desc, part, &info);
		if (err)
			return 1;
	} else {
		part = part_get_info_by_name(desc, argv[2], &info);
		if (part == -1)
			return 1;
	}

	switch (param) {
	case CMD_PART_INFO_START:
		snprintf(buf, sizeof(buf), LBAF, info.start);
		break;
	case CMD_PART_INFO_SIZE:
		snprintf(buf, sizeof(buf), LBAF, info.size);
		break;
	case CMD_PART_INFO_BLKSIZE:
	  snprintf(buf, sizeof(buf), LBAF, info.blksz);
	  break;
	default:
		printf("** Unknown cmd_part_info value: %d\n", param);
		return 1;
	}

	if (argc > 3)
		env_set(argv[3], buf);
	else
		printf("%s\n", buf);

	return 0;
}

static int do_part_start(int argc, char * const argv[])
{
	return get_part_info(argc, argv, CMD_PART_INFO_START);
}

static int do_part_size(int argc, char * const argv[])
{
	return get_part_info(argc, argv, CMD_PART_INFO_SIZE);
}

static int do_part_block_size(int argc, char * const argv[])
{
	return get_part_info(argc, argv, CMD_PART_INFO_BLKSIZE);
}

static int do_part(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "uuid"))
		return do_part_uuid(argc - 2, argv + 2);
	else if (!strcmp(argv[1], "info"))
      return do_part_info(argc - 2, argv + 2);
	else if (!strcmp(argv[1], "find"))
      return do_part_find(argc - 2, argv + 2);
	else if (!strcmp(argv[1], "list"))
		return do_part_list(argc - 2, argv + 2);
	else if (!strcmp(argv[1], "start"))
		return do_part_start(argc - 2, argv + 2);
	else if (!strcmp(argv[1], "size"))
		return do_part_size(argc - 2, argv + 2);
	else if (!strcmp(argv[1], "blksz"))
		return do_part_block_size(argc - 2, argv + 2);

	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	part,	CONFIG_SYS_MAXARGS,	1,	do_part,
	"disk partition related commands",
	"part find <interface> <dev> <criteria>:<value>\n"
	"    - print first partition number corresponding to criteria:valuei\n"
	"    - criteria could be label or uuid\n"
	"part find <interface> <dev> <criteria>:<value> <varname>\n"
	"    - set environment variable to first partition number corresponding to criteria:valuei\n"
	"    - criteria could be label or uuid\n"
	"part info <interface> <dev>:<part> <varname-start> <varname-size> <varname-blcksize>\n"
	"    - set environment variable varname-start to partition start in blocks\n"
	"    - set environment variable varname-size to partition size in blocks\n"
	"    - set environment variable varname-blcksize to partition block size in bytes\n"
	"part uuid <interface> <dev>:<part>\n"
	"    - print partition UUID\n"
	"part uuid <interface> <dev>:<part> <varname>\n"
	"    - set environment variable to partition UUID\n"
	"part list <interface> <dev>\n"
	"    - print a device's partition table\n"
	"part list <interface> <dev> [flags] <varname>\n"
	"    - set environment variable to the list of partitions\n"
	"      flags can be -bootable (list only bootable partitions)\n"
	"part start <interface> <dev> <part> <varname>\n"
	"    - set environment variable to the start of the partition (in blocks)\n"
	"      part can be either partition number or partition name\n"
	"part size <interface> <dev> <part> <varname>\n"
	"    - set environment variable to the size of the partition (in blocks)\n"
	"      part can be either partition number or partition name\n"
	"part blksz <interface> <dev> <part> <varname>\n"
	"    - set environment variable to the size of the block (in partition)\n"
	"      part can be either partition number or partition name"
);
