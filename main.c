#include <fcntl.h>
#include <iconv.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cdfs.h"
#include "iso9660.h"
#include "main.h"
#include "udf.h"

iconv_t UTF16BE_cd;

int main(int argc, char *argv[])
{
	struct cdfs_datasource_t isofile;
	struct cdfs_disc_t disc = {0};

	struct stat st;

	int descriptor = 0;
	int descriptorend = 0;
	int ISO9660descriptorend = 0 ;
	uint8_t buffer[SECTORSIZE];
	int retval = 0;

	UTF16BE_cd = iconv_open ("UTF-8", "UTF-16BE");
	if (UTF16BE_cd == ((iconv_t) -1))
	{
		perror ("iconv_open()");
		return 1;
	}

	if (argc != 2)
	{
		fprintf (stderr, "Usage:\n%s isofile\n", argv[0]);
		iconv_close (UTF16BE_cd);
		return 1;
	}

	isofile.sectoroffset = 0;
	isofile.sectorcount = 0;
	isofile.offset = 0;
	isofile.filename = strdup (argv[1]);
	isofile.fd = open (argv[1], O_RDONLY);
	if (isofile.fd < 0)
	{
		perror ("open(argv[1]");
		iconv_close (UTF16BE_cd);
		return 1;
	}
	if (fstat (isofile.fd, &st))
	{
		perror ("fstat(argv[1]");
		close (isofile.fd);
		free (isofile.filename);
		iconv_close (UTF16BE_cd);
		return 1;
	}

	if (detect_isofile_sectorformat (&isofile, st.st_size))
	{
		fprintf (stderr, "Unable to detect ISOFILE sector format\n");
		close (isofile.fd);
		free (isofile.filename);
		iconv_close (UTF16BE_cd);
		return 1;
	}

	disc.datasources_count = 1;
	disc.datasources_data = &isofile;

	while (!descriptorend)
	{
		uint32_t sector = 16 + descriptor;

		if (get_absolute_sector (&disc, sector, buffer))
		{
			retval = 1;
			break;
		}

		descriptor++; /* descriptor are 1, not zero based... for the user */

		if ((buffer[1] == 'B') &&
		    (buffer[2] == 'E') &&
		    (buffer[3] == 'A') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '1'))
		{
			printf ("descriptor[%d] Beginning Extended Area Descriptor (just a marker)\n", descriptor);
			continue;
		}

		if ((buffer[1] == 'T') &&
		    (buffer[2] == 'E') &&
		    (buffer[3] == 'A') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '1'))
		{
			printf ("descriptor[%d] Terminating Extended Area Descriptor (just a marker)\n", descriptor);
			descriptorend = 1;
			break;
		}

		if ((buffer[1] == 'B') &&
		    (buffer[2] == 'O') &&
		    (buffer[3] == 'O') &&
		    (buffer[4] == 'T') &&
		    (buffer[5] == '2'))
		{
#warning TODO ECMA 168 BOOT
			printf ("descriptor[%d] ECMA 167/168 Boot Descriptor\n", descriptor);
			continue;
		}

		if ((buffer[1] == 'C') &&
		    (buffer[2] == 'D') &&
		    (buffer[3] == 'W') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '2'))
		{
#warning TODO ECMA 168
			printf ("descriptor[%d] ISO/IEC 13490 / ECMA 168 Descriptor\n", descriptor);
			continue;
		}

		if ((buffer[1] == 'N') &&
		    (buffer[2] == 'S') &&
		    (buffer[3] == 'R') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '2'))
		{
			printf ("descriptor[%d] ISO/IEC 13346:1995 / ECMA 167 2nd edition / UDF Descriptor\n", descriptor);
			UDF_Descriptor (&disc);
			continue;
		}

		if ((buffer[1] == 'N') &&
		    (buffer[2] == 'S') &&
		    (buffer[3] == 'R') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '3'))
		{
			printf ("descriptor[%d] ECMA 167 3rd edition / UDF Descriptor\n", descriptor);
			UDF_Descriptor (&disc);
			continue;
		}


		if ((buffer[1] =='C') ||
		    (buffer[2] =='D') ||
		    (buffer[3] =='0') ||
		    (buffer[4] =='0') ||
		    (buffer[5] =='1'))
		{
			printf ("descriptor[%d] ISO 9660 / ECMA 119 Descriptor\n", descriptor);
			if (ISO9660descriptorend)
			{
				printf ("WARNING - this is unepected, CD001 parsing should be complete\n");
			}
			ISO9660_Descriptor (&disc, buffer, sector, descriptor, &ISO9660descriptorend);
			continue;
		} else {
			if (ISO9660descriptorend)
			{
				printf ("descriptor[%d] has invalid Identifier (got '%c%c%c%c%c'), but ISO9660 has already terminated list, so should be OK\n", descriptor, buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
				descriptorend = 1;
			} else {
				printf ("descriptor[%d] has invalid Identifier (got '%c%c%c%c%c')\n", descriptor, buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
			}
			retval = 1;
			break;
		}
	}

	if (disc.iso9660_session)
	{
		if (disc.iso9660_session->Primary_Volume_Description)
		{
			printf ("ISO9660 vanilla\n");
			DumpFS_dir_ISO9660 (disc.iso9660_session->Primary_Volume_Description, ".", disc.iso9660_session->Primary_Volume_Description->root_dirent.Absolute_Location);
		}
		if (disc.iso9660_session->Primary_Volume_Description && disc.iso9660_session->Primary_Volume_Description->RockRidge)
		{
			printf ("ISO9660 RockRidge\n");
			DumpFS_dir_RockRidge (disc.iso9660_session->Primary_Volume_Description, ".", disc.iso9660_session->Primary_Volume_Description->root_dirent.Absolute_Location);
		}
		if (disc.iso9660_session->Supplementary_Volume_Description && disc.iso9660_session->Supplementary_Volume_Description->UTF16)
		{
			printf ("ISO9660 Joliet\n");
			DumpFS_dir_Joliet (disc.iso9660_session->Supplementary_Volume_Description, ".", disc.iso9660_session->Supplementary_Volume_Description->root_dirent.Absolute_Location);
		}

		ISO9660_Session_Free (&disc.iso9660_session);
	}

	if (disc.udf_session)
	{
		DumpFS_UDF (&disc);
		UDF_Session_Free (&disc);
	}

	close (isofile.fd);
	free (isofile.filename);

	iconv_close (UTF16BE_cd);

	return retval;
}
