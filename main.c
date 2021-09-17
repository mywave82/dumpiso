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
#include "toc.h"
#include "udf.h"

iconv_t UTF16BE_cd;

static char *get_path(const char *sourcefile)
{
	char *lastslash = strrchr (sourcefile, '/');
	char *retval;
	if (!lastslash)
	{
		return strdup ("./");
	}
	retval = strdup (sourcefile);
	retval[lastslash - sourcefile + 1] = 0;
	return retval;
}

static int is_filename_toc (const char *filename)
{
	int len = strlen (filename);
	if (len < 4)
	{
		return 0;
	}

	if (!strcasecmp (filename + len - 4, ".toc"))
	{
		return 1;
	}

	return 0;
}


int main(int argc, char *argv[])
{
	uint32_t            isofile_sectorcount = 0;
	int                 isofile_fd = -1;
	//const char       *isofile_filename,
	enum cdfs_format_t  isofile_format = 0;

	struct cdfs_disc_t *disc;

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
		fprintf (stderr, "Usage:\n%s <file.iso file.bin>\n%s <file.toc>\n", argv[0], argv[0]);
		iconv_close (UTF16BE_cd);
		return 1;
	}

	isofile_fd = open (argv[1], O_RDONLY);

	if (isofile_fd < 0)
	{
		perror ("open(argv[1]");
		iconv_close (UTF16BE_cd);
		return 1;
	}
	if (fstat (isofile_fd, &st))
	{
		perror ("fstat(argv[1]");
		close (isofile_fd);
		iconv_close (UTF16BE_cd);
		return 1;
	}

	if (is_filename_toc (argv[1]))
	{
		struct toc_parser_t *toc = toc_parser_from_fd (isofile_fd);
		char *argv1_path;

		close (isofile_fd);
		isofile_fd = -1;
		if (!toc)
		{
			iconv_close (UTF16BE_cd);
			return 1;
		}

		argv1_path = get_path (argv[1]);
		disc = toc_parser_to_cdfs_disc (argv1_path, toc);
		free (argv1_path);
		toc_parser_free (toc);
		if (!disc)
		{
			iconv_close (UTF16BE_cd);
			return 1;
		}
	} else {
		disc = calloc (sizeof (*disc), 1);

		if (detect_isofile_sectorformat (isofile_fd, argv[1], st.st_size, &isofile_format, &isofile_sectorcount))
		{
			fprintf (stderr, "Unable to detect ISOFILE sector format\n");
			close (isofile_fd);
			iconv_close (UTF16BE_cd);
			return 1;
		}

		cdfs_disc_append_datasource (disc,
		                             0,                   /* sectoroffset */
		                             isofile_sectorcount,
		                             isofile_fd,
		                             argv[1],             /* filename */
		                             isofile_format,
		                             0,                   /* offset */
		                             st.st_size);         /* length */

		/* track 00 */
		cdfs_disc_track_append (disc,
		                        0,  /* pregap */
		                        0,  /* offset */
		                        0,  /* sectorcount */
		                        0,  /* title */
		                        0,  /* performer */
		                        0,  /* songwriter */
		                        0,  /* composer */
		                        0,  /* arranger */
		                        0); /* message */

		/* track 01 */
		cdfs_disc_track_append (disc,
		                        0,  /* pregap */
		                        0,  /* offset */
		                        disc->datasources_data[0].sectorcount,
		                        0,  /* title */
		                        0,  /* performer */
		                        0,  /* songwriter */
		                        0,  /* composer */
		                        0,  /* arranger */
		                        0); /* message */
	}

	while (!descriptorend)
	{
		uint32_t sector = 16 + descriptor;

		if (get_absolute_sector_2048 (disc, sector, buffer))
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
			UDF_Descriptor (disc);
			continue;
		}

		if ((buffer[1] == 'N') &&
		    (buffer[2] == 'S') &&
		    (buffer[3] == 'R') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '3'))
		{
			printf ("descriptor[%d] ECMA 167 3rd edition / UDF Descriptor\n", descriptor);
			UDF_Descriptor (disc);
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
			ISO9660_Descriptor (disc, buffer, sector, descriptor, &ISO9660descriptorend);
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

	if (disc->iso9660_session)
	{
		if (disc->iso9660_session->Primary_Volume_Description)
		{
			printf ("ISO9660 vanilla\n");
			DumpFS_dir_ISO9660 (disc->iso9660_session->Primary_Volume_Description, ".", disc->iso9660_session->Primary_Volume_Description->root_dirent.Absolute_Location);
		}
		if (disc->iso9660_session->Primary_Volume_Description && disc->iso9660_session->Primary_Volume_Description->RockRidge)
		{
			printf ("ISO9660 RockRidge\n");
			DumpFS_dir_RockRidge (disc->iso9660_session->Primary_Volume_Description, ".", disc->iso9660_session->Primary_Volume_Description->root_dirent.Absolute_Location);
		}
		if (disc->iso9660_session->Supplementary_Volume_Description && disc->iso9660_session->Supplementary_Volume_Description->UTF16)
		{
			printf ("ISO9660 Joliet\n");
			DumpFS_dir_Joliet (disc->iso9660_session->Supplementary_Volume_Description, ".", disc->iso9660_session->Supplementary_Volume_Description->root_dirent.Absolute_Location);
		}

		ISO9660_Session_Free (&disc->iso9660_session);
	}

	if (disc->udf_session)
	{
		DumpFS_UDF (disc);
		UDF_Session_Free (disc);
	}

	iconv_close (UTF16BE_cd);

	cdfs_disc_free (disc);

	return retval;
}
