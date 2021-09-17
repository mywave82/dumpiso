/* System Use Sharing Protocol */

static void decode_susp_CE (struct cdfs_disc_t *disc, struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer, int isrootnode, int *loopcount); /* Continuation Area - more susp data located somewhere else */
static void decode_susp_PD (                                   uint8_t *buffer);                                 /* Padding           - void filler */
static void decode_susp_SP (struct Volume_Description_t *self, uint8_t *buffer);                                 /* system use Sharing Protocol  */
static void decode_susp_ST (struct Volume_Description_t *self, uint8_t *buffer);                                 /* STOP or SUSP Terminiate  */
static void decode_susp_ER (struct Volume_Description_t *self, uint8_t *buffer);                                 /* Extension Record  */
static void decode_susp_ES (struct Volume_Description_t *self, uint8_t *buffer);                                 /* Extension Sequence */

static void decode_rrip_RR (struct Volume_Description_t *self, uint8_t *buffer);                                 /* Rock Ridge */
static void decode_rrip_PX (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer);                                 /* POSIX */
static void decode_rrip_PN (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer);                                 /* Node (char/block device major/minor) */
static void decode_rrip_SL (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer);        /* Symlink */
static void decode_rrip_NM (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer);        /* Alternate name */
static void decode_rrip_CL (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer);        /* Child Location */
static void decode_rrip_PL (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer);        /* Parent Location */
static void decode_rrip_RE (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer);        /* Relocated Entry */
static void decode_rrip_TF (struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer);                                 /* Time fields */

static void decode_amiga_AS (struct Volume_Description_t *self, uint8_t *buffer);                                 /* Amiga / Angela Schmidt<Angela.Schmidt@stud.uni-karlsruhe.de> */


static int decode_susp (struct cdfs_disc_t *disc, struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer, int len, int isrootnode, int isrecursive /* from CE block? */, int *loopcount /* recursion protection */)
{
	int CE_count = 0;
	int SP_precount = *loopcount;

/*
{
	int i;
	for (i=0; i < len; i++)
	{
		fprintf (stderr, " 0x%02x", buffer[i]);
	}
	fprintf (stderr, "\n");
}
*/
	if (!isrecursive)
	{
		if ((self->XA1) && (len >= 14))
		{
/*
			printf ("(DEBUG) %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				buffer[ 0], buffer[ 1], buffer[ 2], buffer[ 3],
				buffer[ 4], buffer[ 5], buffer[ 6], buffer[ 7],
				buffer[ 8], buffer[ 9], buffer[10], buffer[11],
				buffer[12], buffer[13]);
*/
			if ((buffer[6] == 'X') && (buffer[7] == 'A') && (buffer[9] == 0))
			{
				printf ("      XA1\n");
				de->XA = 1;
				de->XA_GID = decode_uint16_msb (buffer + 0, "       GID");
				de->XA_UID = decode_uint16_msb (buffer + 2, "       UID");
				de->XA_attr = decode_uint16_msb (buffer + 4, "       attr");
				if (de->XA_attr & XA_ATTR__OWNER_READ)  printf ("        r"); /* owner read */
				printf ("-");
				if (de->XA_attr & XA_ATTR__OWNER_EXEC)  printf (        "x"); /* owner exec */
				if (de->XA_attr & XA_ATTR__GROUP_READ)  printf (        "r"); /* group read */
				printf ("-");
				if (de->XA_attr & XA_ATTR__GROUP_EXEC)  printf (        "x"); /* group exec */
				if (de->XA_attr & XA_ATTR__OTHER_READ)  printf (        "r"); /* other read */
				printf ("-");
				if (de->XA_attr & XA_ATTR__OTHER_EXEC)  printf (        "x"); /* other exec */
				if (de->XA_attr & XA_ATTR__MODE2_FORM1) printf (" MODE2-FORM1-DATA/2048");
				if (de->XA_attr & XA_ATTR__MODE2_FORM2) printf (" MODE2-FORM2-DATA/2324"); /* A regular 2048 sector format ISO file can not contain this */
				if (de->XA_attr & XA_ATTR__INTERLEAVED) printf (" INTERLEAVED-DATA/AUDIO"); /* A regular 2048 sector format ISO file can not contain this */
				if (de->XA_attr & XA_ATTR__CDDA)        printf (" CDDA"); /* AUDIO */ /* A regular 2048 sector format ISO file can not contain this */
				if (de->XA_attr & XA_ATTR__DIR)         printf (" DIR");
				printf ("\n");
				printf ("       FileNumber: %d\n", buffer[8]);
			}
		}

		buffer += self->SystemUse_Skip; /* Normally 14 in XA1 mode*/
		   len -= self->SystemUse_Skip;

		if (isrootnode && (len >= 4))
		{
		}

	}

	if ((*loopcount) > 1000)
	{
		printf ("WARNING - decode_susp recursion limit reached\n");
		return -1;
	}

	(*loopcount)++;
	while (len >= 4)
	{
		int i;
		if (buffer[2] < 4)
		{
			printf ("WARNING - invalid length for entry\n");
			return -1;
		}
		if (buffer[2] > len)
		{
			printf ("WARNING - overflow parsing entry\n");
			return -1;
		}
		printf ("      %c%c version %d  ", buffer[0], buffer[1], buffer[3]);
		for (i=4; i < buffer[2]; i++)
		{
			printf (" 0x%02" PRIx8, buffer[i]);
		}
		putchar ('\n');

		if (((buffer[0] != 'S') || (buffer[1] != 'P')) && ((SP_precount==0)) && isrootnode)
		{
			printf ("WARNING - first entry in the rootnode should have been a SP node\n");
		}

		if ((buffer[0] == 'C') && (buffer[1] == 'E'))
		{
			if (CE_count)
			{
				printf ("WARNING - multiple CE entries in the same block is not allowed\n");
			}
			decode_susp_CE (disc, self, de, buffer, isrootnode, loopcount);
			CE_count++;
		} else if ((buffer[0] == 'P') && (buffer[1] == 'D')) decode_susp_PD (buffer);
		  else if ((buffer[0] == 'S') && (buffer[1] == 'P'))
		{
			if (!isrootnode)
			{
				printf ("WARNING - only rootnode is allowed to contain SP\n");
			} else {
				if (SP_precount)
				{
					printf ("WARNING - SP should be the first entry (in the rootnode)\n");
				}
				decode_susp_SP (self, buffer);
			}
		} else if ((buffer[0] == 'S') && (buffer[1] == 'T'))
		{
			decode_susp_ST (self, buffer);
			break;
		} else if ((buffer[0] == 'E') && (buffer[1] == 'R'))
		{
			if (!isrootnode)
			{
				printf ("WARNING - only rootnode is allowed to contain ER\n");
			} else {
				decode_susp_ER (self, buffer);
			}
		} else if ((buffer[0] == 'E') && (buffer[1] == 'S'))
		{
			if (!isrootnode)
			{
				printf ("WARNING - only rootnode is allowed to contain ER\n");
			} else {
				decode_susp_ES (self, buffer);
			}
		} else if ((buffer[0] == 'R') && (buffer[1] == 'R')) decode_rrip_RR (self, buffer);
		  else if ((buffer[0] == 'P') && (buffer[1] == 'X')) decode_rrip_PX (self, de, buffer);
		  else if ((buffer[0] == 'P') && (buffer[1] == 'N')) decode_rrip_PN (self, de, buffer);
		  else if ((buffer[0] == 'S') && (buffer[1] == 'L')) decode_rrip_SL (self, de, buffer);
		  else if ((buffer[0] == 'N') && (buffer[1] == 'M')) decode_rrip_NM (self, de, buffer);
		  else if ((buffer[0] == 'C') && (buffer[1] == 'L')) decode_rrip_CL (self, de, buffer);
		  else if ((buffer[0] == 'P') && (buffer[1] == 'L')) decode_rrip_PL (self, de, buffer);
		  else if ((buffer[0] == 'R') && (buffer[1] == 'E')) decode_rrip_RE (self, de, buffer);
		  else if ((buffer[0] == 'T') && (buffer[1] == 'F')) decode_rrip_TF (self, de, buffer);
		  else if ((buffer[0] == 'A') && (buffer[1] == 'S')) decode_amiga_AS (self, buffer);

		SP_precount++;

		len -= buffer[2];
		buffer += buffer[2];
	}

	return 0;
}

#include "amiga.c"
#include "rockridge.c"

static void decode_susp_CE (struct cdfs_disc_t *disc, struct Volume_Description_t *self, struct iso_dirent_t *de, uint8_t *buffer, int isrootnode, int *loopcount)
{
	uint32_t BlockLocation;
	uint32_t Offset;
	uint32_t Length;

	uint8_t newbuffer[SECTORSIZE];

	printf ("       Continuation Area:\n");
	if (buffer[2] != 28)
	{
		printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		printf ("WARNING - Version is wrong\n");
		return;
	}
	BlockLocation = decode_uint32_both (buffer + 4, "        BlockLocation");
	Offset = decode_uint32_both (buffer + 12, "        Offset");
	Length = decode_uint32_both (buffer + 20, "        Length");
	if (Offset > SECTORSIZE)
	{
		printf ("WARNING - Offset is > SECTORSIZE\n");
		return;
	}
	if (Length == 0)
	{
		return;
	}
	if ((Length > SECTORSIZE) || (Offset + Length > SECTORSIZE))
	{
		printf ("WARNING - Length+Offset is > SECTORSIZE\n");
		return;
	}

	if (get_absolute_sector_2048 (disc, BlockLocation, newbuffer))
	{
		return;
	}

	decode_susp (disc, self, de, newbuffer + Offset, Length, isrootnode, /* recursive */ 1, loopcount);
}

static void decode_susp_PD (uint8_t *buffer)
{
	printf ("       Padding:\n");
	if (buffer[3] != 1)
	{
		printf ("WARNING - Version is wrong\n");
		return;
	}
	/* no-op */
}

static void decode_susp_SP (struct Volume_Description_t *self, uint8_t *buffer)
{
	printf ("       system use Sharing Protocol:\n");
	if (buffer[2] != 7)
	{
		printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		printf ("WARNING - Version is wrong\n");
		return;
	}
	if (buffer[4] != 0xbe)
	{
		printf ("WARNING - CheckByte1 is wrong\n");
	}
	if (buffer[5] != 0xef)
	{
		printf ("WARNING - CheckByte2 is wrong\n");
	}
	printf ("        Skip Bytes per record: %" PRId8 "\n", buffer[6]);
	self->SystemUse_Skip = buffer[6];

	return;
}

static void decode_susp_ST (struct Volume_Description_t *self, uint8_t *buffer)
{
	printf ("       SUSP Terminator:\n");
	if (buffer[2] != 4)
	{
		printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		printf ("WARNING - Version is wrong\n");
		return;
	}
	/* parent handles us */
	return;
}

static void decode_susp_ER (struct Volume_Description_t *self, uint8_t *buffer)
{
	int i;
	printf ("       Extension Record\n");
	if (buffer[2] < 8)
	{
		printf ("WARNING - Length is way too short\n");
		return;
	}
	if (buffer[3] != 1)
	{
		printf ("WARNING - Version is wrong\n");
		return;
	}
	if ((8 + buffer[4] + buffer[5] + buffer[6]) > buffer[2])
	{
		printf ("WARNING - Length is too short\n");
		return;
	} else if ((8 + buffer[4] + buffer[5] + buffer[6]) < buffer[2])
	{
		printf ("WARNING - Length is too long\n");
	}

	printf ("        Identifier: \"");
	for (i=0; i < buffer[4]; i++)
	{
		putchar (buffer[8 + i]);
	}
	printf ("\"\n");

	printf ("        Descriptor: \"");
	for (i=0; i < buffer[5]; i++)
	{
		putchar (buffer[8 + buffer[4] + i]);
	}
	printf ("\"\n");

	printf ("        Source: \"");
	for (i=0; i < buffer[6]; i++)
	{
		putchar (buffer[8 + buffer[4] + buffer[5] + i]);
	}
	printf ("\"\n");

	printf ("        Version: %" PRId8 "\n", buffer[7]);
}

static void decode_susp_ES (struct Volume_Description_t *self, uint8_t *buffer)
{
	printf ("       Extension Sequence\n");
	if (buffer[2] < 5)
	{
		printf ("WARNING - Length is wrong\n");
		return;
	}
	if (buffer[3] != 1)
	{
		printf ("WARNING - Version is wrong\n");
		return;
	}
	printf ("        Sequence: %" PRId8 "\n", buffer[4]);
}
