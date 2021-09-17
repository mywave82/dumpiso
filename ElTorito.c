static int ElTorito_ValiationEntry (uint8_t buffer[0x20], uint8_t *arch)
{
	int retval = 0;
	uint16_t checksumA = 0;

	int i;

	printf ("  [ElTorito Valiation Header]\n");

	if (buffer[0] != 0x01)
	{
		printf ("  Warning - Header ID invalid (expected 0x01, got 0x%02" PRIx8 "\n", buffer[0]);
		retval = 1;
	}

	*arch = buffer[1];
	switch (buffer[1])
	{
		case 0x00: printf ("   PlatformID=80x86\n"); break;
		case 0x01: printf ("   PlatformID=PowerPC\n"); break;
		case 0x02: printf ("   PlatformID=Mac\n"); break;
		case 0xef: printf ("   PlatformID=EFI?\n"); break; /* guess based on data found */
		default:   printf ("   PlatformID=Unknown 0x%02" PRIx8 "\n", buffer[1]);
	}

	if (buffer[2] != 0x00)
	{
		printf ("   Warning - Reserved header at offset 2 if not 0x00, but 0x%02" PRIx8 "\n", buffer[2]);
	}

	if (buffer[3] != 0x00)
	{
		printf ("   Warning - Reserved header at offset 3 if not 0x00, but 0x%02" PRIx8 "\n", buffer[3]);
	}

	printf ("   IDstring=\"");
	for (i=0x04; i <= 0x1b; i++)
	{
		if (!buffer[i])
		{
			break;
		}
		putchar (buffer[i]);
	}
	printf ("\"\n");

	/* not much tested... */
	for (i=0; i < 0x10; i++)
	{
		uint16_t t = (buffer[(i<<1) + 0x01] << 8) |
                              buffer[(i<<1) + 0x00];
		checksumA += t;
	}
	if (checksumA)
	{
		printf ("   Warning, Checksum failed (got 0x%04" PRIx16", but expected 0x0000 )!\n", checksumA);
		retval = 1;
	} else {
		printf ("   Checksum correct\n");
	}

	if (buffer[0x1e] != 0x55)
	{
		printf ("   KeySignature #1 is not 0x55, but 0x%02" PRIx8 "\n", buffer[0x1e]);
	}

	if (buffer[0x1f] != 0xaa)
	{
		printf ("   KeySignature #2 is not 0xaa, but 0x%02" PRIx8 "\n", buffer[0x1f]);
	}

	return retval;
}

static int ElTorito_SectionHeaderEntry (uint8_t buffer[0x20], int index1, int *last, uint8_t *arch, uint16_t *sections)
{
	int retval = 0;
	int i;

	printf ("  [Section Header Entry %d]\n", index1);

	switch (buffer[0])
	{
		case 0x90: printf ("   More section headers will follow\n"); *last = 0; break;
		case 0x91: printf ("   This is the last header\n"); *last = 1; break;
		default: printf ("   Invalid header ID: 0x%02" PRIx8 "\n", buffer[0]); retval = 1; break;
	}

	*arch = buffer[1];
	switch (buffer[1])
	{
		case 0x00: printf ("   PlatformID=80x86\n"); break;
		case 0x01: printf ("   PlatformID=PowerPC\n"); break;
		case 0x02: printf ("   PlatformID=Mac\n"); break;
		case 0xef: printf ("   PlatformID=EFI?\n"); break; /* guess based on data found */
		default:   printf ("   PlatformID=Unknown 0x%02" PRIx8 "\n", buffer[1]);
	}

	*sections = (buffer[3] << 8) | buffer[2];
	printf ("   Sections: %" PRId16 "\n", *sections);

	printf ("   IDstring=\"");
	for (i=0x04; i <= 0x1f; i++)
	{
		if (!buffer[i])
		{
			break;
		}
		putchar(buffer[i]);
	}
	printf ("\"\n");

	return retval;
}

static int ElTorito_SectionEntry (const uint8_t arch, uint8_t buffer[0x20], int index1, int index2)
{
	int retval = 0;
	int i;

	if (index1 == 0)
	{
		printf ("   [Initial/Default Entry]\n");
	} else {
		printf ("   [Section Entry %d.%d\n", index1, index2+1);
	}

	switch (buffer[0])
	{
		case 0x88: printf ("    Bootable\n"); break;
		case 0x00: printf ("    Not bootable\n"); break;
		default:   printf ("    Invalid indicator: 0x%02" PRIx8 "\n", buffer[0]); retval = 1; break;
	}

	switch (buffer[1] & 0x0f)
	{
		case 0x00: printf ("    No Emulation (?)\n"); break;
		case 0x01: printf ("    1.2 meg diskette\n"); break;
		case 0x02: printf ("    1.44 meg diskette\n"); break;
		case 0x03: printf ("    2.88 meg diskette\n"); break;
		case 0x04: printf ("    Hard Disk (drive 80)\n"); break;
		default: printf ("    Invalid media type: 0x%02" PRIx8 "\n", buffer[1]); retval = 1; break;
	}

	if (arch == 0x00) /* 80x86 */
	{
		uint16_t segment = (buffer[3] << 8) | buffer[2];
		if (segment == 0x0000)
		{
			printf ("    Load into default memory =>  [07c0:0000]\n");
		} else {
			printf ("    Load into memory [%04" PRIx16 ":0000]\n", segment);
		}
	} else {
		uint32_t addr = (buffer[2] << 12) | (buffer[3] << 4);
		printf ("    Load into memory [%08" PRIx32"]\n", addr);
	}

	printf ("    System Type (should match the active partion from disk image if hard drive): %s\n", PartitionType(buffer[4]));

	if (buffer[5] != 0x00)
	{
		printf ("    Warning - Reserved header at offset 5 if not 0x00, but 0x%02" PRIx8 "\n", buffer[5]);
	}

	{
		uint16_t sectorcount = (buffer[7] << 8) | buffer[6];
		printf ("    Sectors to load: %" PRId16 "\n", sectorcount);
	}

	{
		int32_t rba = (buffer[11] << 24) | (buffer[10] << 16) | (buffer[9] << 8) | buffer[8];
		printf ("    Relative offset to start of image: %" PRId32"\n", rba);
	}

	if (index1 == 0)
	{
		for (i=0x0c; i <= 0x1f; i++)
		{
			if (buffer[i])
			{
				printf ("    Warning - Reserved header at offset %d if not 0x00, but 0x%02" PRIx8 "\n", i, buffer[i]);
			}
		}
	} else {
		if (buffer[0x0c] == 0x00)
		{
			printf ("   No selection criteria\n");
		} else if (buffer[0x0c] == 0x01)
		{
			printf ("    Language and Version Information (IBM)\n");
			printf ("   ");
			for (i=0x0d; i <= 0x1f; i++)
			{
				printf (" 0x%02" PRIx8 "\n", buffer[i]);
			}
			printf ("\n");
		} else {
			printf ("   Invalid selection criteria type: 0x%02" PRIx8 "\n", buffer[0x0c]);
			retval = 1;
		}
	}

	return retval;
}

static int ElTorito_SectionEntryExtension (uint8_t buffer[0x20], int index1, int index2, int index3, int *last)
{
	int retval = 0;
	int i;

	printf ("    [Section Entry Extension %d.%d.%d\n", index1, index2+1, index3);

	if (buffer[0] != 0x44)
	{
		printf ("     Invalid ID indicator: 0x%02" PRIx8 "\n", buffer[0]);
		retval = 1;
	}

	*last = !!(buffer[1] & 0x20);

	for (i=0x02; i <= 0x1f; i++)
	{
		printf (" 0x%02" PRIx8 "\n", buffer[i]);
	}
	putchar ('\n');

	return retval;
}

static int ElTorito_check_offset (struct cdfs_disc_t *disc, uint32_t *sector, uint8_t *buffer, int *offset)
{
	if (*offset >= SECTORSIZE)
	{
		*sector = (*sector) + 1;
		*offset = 0;

		if (get_absolute_sector_2048 (disc, *sector, buffer))
		{
			printf ("\n Failed to fetch next El Torito boot description at absolute sector%"PRId32"\n", *sector);
			return -1;
		}

		printf ("\n ElTorito data at absolute sector %"PRId32"\n", *sector);
	}
	return 0;
}

static void ElTorito_abs_sector (struct cdfs_disc_t *disc, uint32_t sector)
{
	uint8_t buffer[SECTORSIZE];
	uint8_t arch;
	int i, j;
	int offset = 0;
	int lastheader = 0;

	if (get_absolute_sector_2048 (disc, sector, buffer))
	{
		printf ("Failed to fetch El Torito boot description at absolute sector%"PRId32"\n", sector);
		return;
	}

	printf ("\n ElTorito data at absolute sector %"PRId32"\n", sector);
	printf ("\n (offset=0x%04x)\n", offset);
	if (ElTorito_ValiationEntry (buffer + offset, &arch))
	{
		return;
	}
	offset += 0x020;

	printf ("\n (offset=0x%04x)\n", offset);
	if (ElTorito_SectionEntry (arch, buffer + offset, 0, 0))
	{
		return;
	}
	offset += 0x020;


	if (buffer[offset] == 0x00)
	{
		printf ("\n no sections available for further parsing\n");
	} else {
		for (i=1;; i++)
		{
			uint16_t entries = 0;

			if (ElTorito_check_offset (disc, &sector, buffer, &offset))
			{
				return;
			}

			printf ("\n (offset=0x%04x)\n", offset);
			if (ElTorito_SectionHeaderEntry (buffer + offset, i, &lastheader, &arch, &entries))
			{
				return;
			}
			offset += 0x020;

			for (j=0; j < entries; j++)
			{
				int last = 0;
				int k = 0;

				if (ElTorito_check_offset (disc, &sector, buffer, &offset))
				{
					return;
				}

				printf ("\n (offset=0x%04x)\n", offset);
				if (ElTorito_SectionEntry (arch, buffer + offset, i, j))
				{
					return;
				}
				offset += 0x020;

				while (!last)
				{
					k++;

					if (ElTorito_check_offset (disc, &sector, buffer, &offset))
					{
						return;
					}

					if (buffer[offset] == 0x00)
					{
						printf ("\n no sections entry extension available for further parsing (not according to standard)\n");
						last = 1;
						break;
					}

					printf ("\n (offset=0x%04x)\n", offset);
					if (ElTorito_SectionEntryExtension (buffer + offset, i, j, k, &last))
					{
						return;
					}
					offset += 0x020;
				}
			}

			if (lastheader)
			{
				break;
			}
		}
	}

	return;
}
