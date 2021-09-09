static void decode_amiga_AS (struct Volume_Description_t *self, uint8_t *buffer)
{
	int l = buffer[2] - 4;
	uint8_t *b = buffer + 4;
	uint8_t flags;

	printf ("       Amiga\n");

	if (buffer[2] < 5)
	{
		printf ("WARNING - Length is way too short\n");
		return;
	}
	if (buffer[3] != 1)
	{
		printf ("WARNING - Version is wrong\n");
		return;
	}
	printf ("        Flags: 0x%02" PRIx8 "\n", buffer[4]);
	if (buffer[4] & 0x01) printf ("         Protection present\n");
	if (buffer[4] & 0x02) printf ("         Comment present\n");
	if (buffer[4] & 0x04) printf ("         Comment continues in next AS record\n");

	flags = buffer[4];

	if (flags & 0x01)
	{
		if (l < 4)
		{
			printf ("WARNING - Length is way too short #2\n");
			return;
		}
		printf ("        Protection User:       0x%02" PRIx8 "\n", b[0]);
		printf ("        Protection Reserved:   0x%02" PRIx8 "\n", b[1]);
		printf ("        Protection Multiuser:  0x%02" PRIx8 "\n", b[2]);
		if (b[2] & 0x01) printf ("         Deletable for group members\n");
		if (b[2] & 0x02) printf ("         Executable for group members\n");
		if (b[2] & 0x04) printf ("         Writable for group members\n");
		if (b[2] & 0x08) printf ("         Readable for group members\n");
		if (b[2] & 0x10) printf ("         Deletable for other users\n");
		if (b[2] & 0x20) printf ("         Executable for other users\n");
		if (b[2] & 0x40) printf ("         Writable for other users\n");
		if (b[2] & 0x80) printf ("         Readable for other users\n");
		printf ("        Protection Protection: 0x%02" PRIx8 "\n", b[3]);
		if (b[3] & 0x01) printf ("         Not deletable for owner\n");
		if (b[3] & 0x02) printf ("         Not executable for owner\n");
		if (b[3] & 0x04) printf ("         Not writable for owner\n");
		if (b[3] & 0x08) printf ("         Not readable for owner\n");
		if (b[3] & 0x10) printf ("         Archived\n");
		if (b[2] & 0x20) printf ("         Reentrant executable\n");
		if (b[3] & 0x40) printf ("         Executable script\n");

		b+=4;
		l-=4;
	}

	if (flags & 0x02)
	{
		int i;
		if ((l < 1) || (l < b[0]))
		{
			printf ("WARNING - Length is way too short #3\n");
			return;
		}
		printf ("        Comment: \"");
		for (i=1; i < l; i++)
		{
			putchar (b[i]);
		}
		printf ("\"\n");
		b += i;
		l -= i;
	}
}
