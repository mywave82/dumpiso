#include <fcntl.h>
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

int detect_isofile_sectorformat (int isofile_fd, const char *filename, off_t st_size, enum cdfs_format_t *isofile_format, uint32_t *isofile_sectorcount)
{
	uint8_t buffer[6+8+4+12];

	/* Detect MODE1 / XA_MODE2_FORM1 - 2048 bytes of data */
	if (lseek (isofile_fd, SECTORSIZE * 16, SEEK_SET) == (off_t)-1)
	{
		perror ("lseek(argv[1]) #1");
		return 1;
	}
	if (read (isofile_fd, buffer, 6) != 6)
	{
		perror ("read(argv[1]) #1");
		return 1;
	}
	if (((buffer[1] == 'C') &&
	     (buffer[2] == 'D') &&
	     (buffer[3] == '0') &&
	     (buffer[4] == '0') &&
	     (buffer[5] == '1')) ||
	    ((buffer[1] == 'B') &&
	     (buffer[2] == 'E') &&
	     (buffer[3] == 'A') &&
	     (buffer[4] == '0') &&
	     (buffer[5] == '1')))
	{
		printf ("%s: detected as ISO file format, containing only data as is\n", filename);
		*isofile_format = FORMAT_MODE_1__XA_MODE2_FORM1___NONE;
		*isofile_sectorcount = st_size / 2048;
		return 0;
	}

	/* Detect FORMAT_XA1_MODE2_FORM1___NONE */
	if (lseek (isofile_fd, (SECTORSIZE + 8) * 16, SEEK_SET) == (off_t)-1)
	{
		perror ("lseek(argv[1]) #2");
		return 1;
	}
	if (read (isofile_fd, buffer, 6+8) != (6+8))
	{
		perror ("read(argv[1]) #2");
		return 1;
	}
	if ((!(buffer[2] & 0x20)) && // XA_SUBH_DATA - copy1
	    (!(buffer[6] & 0x20)) && // XA_SUBH_DATA - copy2
	    (((buffer[8+1] == 'C') &&
	      (buffer[8+2] == 'D') &&
	      (buffer[8+3] == '0') &&
	      (buffer[8+4] == '0') &&
	      (buffer[8+5] == '1')) ||
	     ((buffer[8+1] == 'B') &&
	      (buffer[8+2] == 'E') &&
	      (buffer[8+3] == 'A') &&
	      (buffer[8+4] == '0') &&
	      (buffer[8+5] == '1'))))
	{
		printf ("%s: detected as ISO file format, each sector prefixed with XA1 header (8 bytes)\n", filename);
		*isofile_format = FORMAT_XA1_MODE2_FORM1___NONE;
		*isofile_sectorcount = st_size / (2048 + 8);
		return 0;
	}

	/* Detect FORMAT_RAW___NONE MODE-1 / MODE-2 FORM-1 */
	if (lseek (isofile_fd, (SECTORSIZE_XA2) * 16, SEEK_SET) == (off_t)-1)
	{
		perror ("lseek(argv[1]) #3");
		return 1;
	}
	if (read (isofile_fd, buffer, 6+12+4+8) != (6+12+4+8))
	{
		perror ("read(argv[1]) #3");
		return 1;
	}
	if ((buffer[ 0] == 0x00) && // SYNC
	    (buffer[ 1] == 0xff) && // SYNC
	    (buffer[ 2] == 0xff) && // SYNC
	    (buffer[ 3] == 0xff) && // SYNC
	    (buffer[ 4] == 0xff) && // SYNC
	    (buffer[ 5] == 0xff) && // SYNC
	    (buffer[ 6] == 0xff) && // SYNC
	    (buffer[ 7] == 0xff) && // SYNC
	    (buffer[ 8] == 0xff) && // SYNC
	    (buffer[ 9] == 0xff) && // SYNC
	    (buffer[10] == 0xff) && // SYNC
	    (buffer[11] == 0x00)) // SYNC
	{
		if (buffer[12+3] == 1) // MODE 1
		{
	    		if (((buffer[12+4+1] =='C') &&
			     (buffer[12+4+2] =='D') &&
			     (buffer[12+4+3] =='0') &&
			     (buffer[12+4+4] =='0') &&
			     (buffer[12+4+5] =='1')) ||
			    ((buffer[12+4+1] =='B') &&
			     (buffer[12+4+2] =='E') &&
			     (buffer[12+4+3] =='A') &&
			     (buffer[12+4+4] =='0') &&
			     (buffer[12+4+5] =='1')))
			{
				printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 1 header\n", filename);
				*isofile_format = FORMAT_MODE1_RAW___NONE;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2);
				return 0;
			}
		} else if (buffer[12+3] == 2)
		{
			// MODE 2
			 if (((buffer[12+4+1] == 'C') &&
			      (buffer[12+4+2] == 'D') &&
			      (buffer[12+4+3] == '0') &&
			      (buffer[12+4+4] == '0') &&
			      (buffer[12+4+5] == '1')) ||
			     ((buffer[12+4+1] == 'B') &&
			      (buffer[12+4+2] == 'E') &&
			      (buffer[12+4+3] == 'A') &&
			      (buffer[12+4+4] == '0') &&
			      (buffer[12+4+5] == '1')))
			{
				printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 2\n", filename);
				*isofile_format = FORMAT_MODE2_RAW___NONE;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2);
				return 0;
			}
			// MODE 2 XA FORM 1
			if ((!(buffer[12+4+2] & 0x20)) && // XA_SUBH_DATA - copy1
			    (!(buffer[12+4+6] & 0x20)) && // XA_SUBH_DATA - copy2
			    (((buffer[12+4+8+1] == 'C') &&
			      (buffer[12+4+8+2] == 'D') &&
			      (buffer[12+4+8+3] == '0') &&
			      (buffer[12+4+8+4] == '0') &&
			      (buffer[12+4+8+5] == '1')) ||
			     ((buffer[12+4+8+1] == 'B') &&
			      (buffer[12+4+8+2] == 'E') &&
			      (buffer[12+4+8+3] == 'A') &&
			      (buffer[12+4+8+4] == '0') &&
			      (buffer[12+4+8+5] == '1'))))
			{
				printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 2 FORM 1 header\n", filename);
				*isofile_format = FORMAT_XA_MODE2_RAW;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2);
				return 0;
			}
		}
	}

	/* Detect FORMAT_RAW___RAW_RW / FORMAT_RAW___RW */
	if (lseek (isofile_fd, (SECTORSIZE_XA2 + 96) * 16, SEEK_SET) == (off_t)-1)
	{
		perror ("lseek(argv[1]) #5");
		return 1;
	}
	if (read (isofile_fd, buffer, 4+8+6+12) != (4+8+6+12))
	{
		perror ("read(argv[1]) #5");
		return 1;
	}
	if ((buffer[ 0] == 0x00) && // SYNC
	    (buffer[ 1] == 0xff) && // SYNC
	    (buffer[ 2] == 0xff) && // SYNC
	    (buffer[ 3] == 0xff) && // SYNC
	    (buffer[ 4] == 0xff) && // SYNC
	    (buffer[ 5] == 0xff) && // SYNC
	    (buffer[ 6] == 0xff) && // SYNC
	    (buffer[ 7] == 0xff) && // SYNC
	    (buffer[ 8] == 0xff) && // SYNC
	    (buffer[ 9] == 0xff) && // SYNC
	    (buffer[10] == 0xff) && // SYNC
	    (buffer[11] == 0x00)) // SYNC
	{
		if (buffer[12+3] == 1) // MODE 1
		{
			if (((buffer[12+4+1] == 'C') &&
			     (buffer[12+4+2] == 'D') &&
			     (buffer[12+4+3] == '0') &&
			     (buffer[12+4+4] == '0') &&
			     (buffer[12+4+5] == '1')) ||
			    ((buffer[12+4+1] == 'B') &&
			     (buffer[12+4+2] == 'E') &&
			     (buffer[12+4+3] == 'A') &&
			     (buffer[12+4+4] == '0') &&
			     (buffer[12+4+5] == '1')))
			{
				printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 1 header, and suffixed with SUBCHANNEL R-W\n", filename);
				*isofile_format = FORMAT_MODE1_RAW___RAW_RW;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2 + 96);
				return 0;
			}
		} else if (buffer[12+3] == 2) // MODE 2
		{
			// MODE 2
			 if (((buffer[12+4+1] == 'C') &&
			      (buffer[12+4+2] == 'D') &&
			      (buffer[12+4+3] == '0') &&
			      (buffer[12+4+4] == '0') &&
			      (buffer[12+4+5] == '1')) ||
			     ((buffer[12+4+1] == 'B') &&
			      (buffer[12+4+2] == 'E') &&
			      (buffer[12+4+3] == 'A') &&
			      (buffer[12+4+4] == '0') &&
			      (buffer[12+4+5] == '1')))
			{
				printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 2, and suffixed with SUBCHANNEL R-W\n", filename);
				*isofile_format = FORMAT_MODE2_RAW___RAW_RW;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2);
				return 0;
			}
			// MODE 2 XA FORM 1
			if ((!(buffer[12+4+2] & 0x20)) && // XA_SUBH_DATA - copy1
			    (!(buffer[12+4+6] & 0x20)) && // XA_SUBH_DATA - copy2
			    (((buffer[12+4+8+1] == 'C') &&
			      (buffer[12+4+8+2] == 'D') &&
			      (buffer[12+4+8+3] == '0') &&
			      (buffer[12+4+8+4] == '0') &&
			      (buffer[12+4+8+5] == '1')) ||
			     ((buffer[12+4+8+1] == 'B') &&
			      (buffer[12+4+8+2] == 'E') &&
			      (buffer[12+4+8+3] == 'A') &&
			      (buffer[12+4+8+4] == '0') &&
			      (buffer[12+4+8+5] == '1'))))
			{
				printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 2 FORM 1 header, and suffixed with SUBCHANNEL R-W\n", filename);
				*isofile_format = FORMAT_XA_MODE2_RAW___RAW_RW;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2 + 96);
				return 0;
			}
		}
	}

	return 1;
}

int get_absolute_sector_2048 (struct cdfs_disc_t *disc, uint32_t sector, uint8_t *buffer) /* 2048 byte modes */
{
	int i;
	uint8_t xbuffer[16];
	int subchannel = 0;

	for (i=0; i < disc->datasources_count; i++)
	{
		if (  (disc->datasources_data[i].sectoroffset <= sector) &&
		     ((disc->datasources_data[i].sectoroffset + disc->datasources_data[i].sectorcount) >= sector))
		{
			uint32_t relsector = sector - disc->datasources_data[i].sectoroffset;
			if (!disc->datasources_data[i].filename)
			{
				bzero (buffer, 2048);
				return 0;
			}

			switch (disc->datasources_data[i].format)
			{
				case FORMAT_AUDIO_SWAP___RAW_RW:
				case FORMAT_AUDIO_SWAP___RW:
				case FORMAT_AUDIO___RAW_RW: /* we do not swap endian on 2048 byte fetches */
				case FORMAT_AUDIO___RW: /* we do not swap endian on 2048 byte fetches */
				case FORMAT_MODE1_RAW___RAW_RW:
				case FORMAT_MODE1_RAW___RW:
				case FORMAT_MODE2_RAW___RAW_RW:
				case FORMAT_MODE2_RAW___RW:
				case FORMAT_XA_MODE2_RAW___RAW_RW:
				case FORMAT_XA_MODE2_RAW___RW:
				case FORMAT_RAW___RAW_RW:
				case FORMAT_RAW___RW:
					subchannel = 96;
					/* fall-through */
				case FORMAT_RAW___NONE:
				case FORMAT_AUDIO___NONE:
				case FORMAT_AUDIO_SWAP___NONE: /* we do not swap endian on 2048 byte fetches */
				case FORMAT_MODE1_RAW___NONE:
				case FORMAT_MODE2_RAW___NONE:
				case FORMAT_XA_MODE2_RAW:
					if (lseek (disc->datasources_data[i].fd, ((uint64_t)relsector)*(SECTORSIZE_XA2 + subchannel), SEEK_SET) == (off_t)-1)
					{
						fprintf (stderr, "fseek(fd, (%"PRId32")*%d, SEEK_SET) failed\n", relsector, SECTORSIZE_XA2 + subchannel);
						return -1;
					}

					if (read (disc->datasources_data[i].fd, xbuffer, 16) != 16)
					{
						fprintf (stderr, "read(fd, xbuffer, 16) failed\n");
						return -1;
					}
					if (memcmp (xbuffer, "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00", 12))
					{
						fprintf (stderr, "Invalid sync in sector %"PRId32"\n", relsector);
						return -1;
					}
					// xbuffer[12, 13 and 14] should be the current sector adress
					switch (xbuffer[15])
					{
						case 0x00: /* CLEAR */
							fprintf (stderr, "Sector %"PRId32" is CLEAR\n", sector);
							return -1;
						case 0x01: /* MODE 1: DATA */
							if (read (disc->datasources_data[i].fd, buffer, SECTORSIZE) != SECTORSIZE)
							{
								fprintf (stderr, "read(fd, buffer, SECTORSIZE) failed\n");
								return -1;
							}
							return 0;
						case 0xe2: /* Seems to be second to last sector on CD-R, mode-2 */
						case 0x02: /* MODE 2 */
							/* assuming XA-FORM-1, that is the only mode2 that can provide 2048 bytes of data */
#warning ignoring sub-header in FORMAT_XA_MODE2_RAW for now..
							if (read (disc->datasources_data[i].fd, xbuffer, 8) != 8)
							{
								fprintf (stderr, "read(fd, xbuffer_subheader, 8) failed\n");
								return -1;
							}
							if (read (disc->datasources_data[i].fd, buffer, SECTORSIZE) != SECTORSIZE)
							{
								fprintf (stderr, "read(fd, buffer, SECTORSIZE) failed\n");
								return -1;
							}
							return 0;
						default:
							fprintf (stderr, "Sector %"PRId32" is of unknown type (0x%02x)\n", sector, xbuffer[15]);
							return -1;
					}
					return -1; /* not reachable */

				case FORMAT_XA_MODE2_FORM_MIX___RAW_RW: /* not tested */
				case FORMAT_XA_MODE2_FORM_MIX___RW: /* not tested */
					subchannel = 96;
					/* fall-through */
				case FORMAT_XA_MODE2_FORM_MIX___NONE: /* not tested */
					if (lseek (disc->datasources_data[i].fd, ((uint64_t)relsector)*(2324 + 8 + subchannel), SEEK_SET) == (off_t)-1)
					{
						fprintf (stderr, "fseek(fd, (%"PRId32")*(%d), SEEK_SET) failed\n", relsector, 2324 + 8 + subchannel);
						return -1;
					}

					if (read (disc->datasources_data[i].fd, xbuffer, 8) != 8)
					{
						fprintf (stderr, "read(fd, xbuffer, 8) failed\n");
						return -1;
					}
#warning ignoring sub-header in FORMAT_XA_MODE2_FORM_MIX for now..
					if (read (disc->datasources_data[i].fd, xbuffer, 8) != 8)
					{
						fprintf (stderr, "read(fd, xbuffer_subheader, 8) failed\n");
						return -1;
					}
					if (read (disc->datasources_data[i].fd, buffer, SECTORSIZE) != SECTORSIZE)
					{
						fprintf (stderr, "read(fd, buffer, SECTORSIZE) failed\n");
						return -1;
					}
					return 0;

				case FORMAT_MODE1___RAW_RW:
				case FORMAT_MODE1___RW:
				case FORMAT_XA_MODE2_FORM1___RAW_RW:
				case FORMAT_XA_MODE2_FORM1___RW:
				case FORMAT_MODE_1__XA_MODE2_FORM1___RAW_RW:
				case FORMAT_MODE_1__XA_MODE2_FORM1___RW:
					subchannel = 96;
					/* fall-through */
				case FORMAT_MODE1___NONE:
				case FORMAT_XA_MODE2_FORM1___NONE:
				case FORMAT_MODE_1__XA_MODE2_FORM1___NONE:

					if (lseek (disc->datasources_data[i].fd, ((uint64_t)relsector)*(SECTORSIZE + subchannel), SEEK_SET) == (off_t)-1)
					{
						fprintf (stderr, "fseek(fd, (%"PRId32")*%d, SEEK_SET) failed\n", relsector, SECTORSIZE + subchannel);
						return -1;
					}
					if (read (disc->datasources_data[i].fd, buffer, SECTORSIZE) != SECTORSIZE)
					{
						fprintf (stderr, "read(fd, buffer, SECTORSIZE) failed\n");
						return -1;
					}
					return 0;

				case FORMAT_XA1_MODE2_FORM1___RW:
				case FORMAT_XA1_MODE2_FORM1___RW_RAW:
					subchannel = 96;
					/* fall-through */
				case FORMAT_XA1_MODE2_FORM1___NONE:
					// Ignore the sub-header, for now */
					if (lseek (disc->datasources_data[i].fd, ((uint64_t)relsector)*(SECTORSIZE + 8 + subchannel) + 8, SEEK_SET) == (off_t)-1)
					{
						fprintf (stderr, "fseek(fd, (%"PRId32")*%d + 8, SEEK_SET) failed\n", relsector, SECTORSIZE + 8 + subchannel);
						return -1;
					}
					if (read (disc->datasources_data[i].fd, buffer, SECTORSIZE) != SECTORSIZE)
					{
						fprintf (stderr, "read(fd, buffer, SECTORSIZE) failed\n");
						return -1;
					}
					return 0;

				case FORMAT_MODE2___NONE:
				case FORMAT_MODE2___RAW_RW:
				case FORMAT_MODE2___RW:
					fprintf (stderr, "Sector %"PRIu32" does not contain 2048 bytes of data, but 2336\n", sector);
					return 1;

				case FORMAT_XA_MODE2_FORM2___NONE:
				case FORMAT_XA_MODE2_FORM2___RAW_RW:
				case FORMAT_XA_MODE2_FORM2___RW:
					fprintf (stderr, "Sector %"PRIu32" does not contain 2048 bytes of data, but 2324\n", sector);
					return 1;

				default:
					fprintf (stderr, "Unable to fetch absolute sector %" PRIu32 "\n", sector);
					return 1;
			}
		}
	}
	fprintf (stderr, "Unable to locate absolute sector %" PRId32 "\n", sector);
	return 1;
}

void cdfs_disc_datasource_append (struct cdfs_disc_t *disc,
                                  uint32_t            sectoroffset,
                                  uint32_t            sectorcount,
                                  int                 fd,
                                  const char         *filename,
                                  enum cdfs_format_t  format,
                                  uint64_t            offset,
                                  uint64_t            length)
{
	struct cdfs_datasource_t *temp;

	// append to previous datasource if possible
	if ( disc->datasources_count && // there is an entry there already
	     ((disc->datasources_data[disc->datasources_count-1].sectoroffset + disc->datasources_data[disc->datasources_count-1].sectorcount) == sectoroffset) &&
	     ((disc->datasources_data[disc->datasources_count-1].fd >= 0) == (fd >= 0)) && // both entries are either both files or zero-fills
	     ((fd < 0) || (!strcmp (disc->datasources_data[disc->datasources_count-1].filename, filename))) && // if entries are files, filenames matches
	     (disc->datasources_data[disc->datasources_count-1].format == format) &&
	     ((disc->datasources_data[disc->datasources_count-1].offset + disc->datasources_data[disc->datasources_count-1].length) == offset) )
	{
		disc->datasources_data[disc->datasources_count-1].sectorcount += sectorcount;
		disc->datasources_data[disc->datasources_count-1].length += length;
		close (fd);
		return;
	}

	temp = realloc (disc->datasources_data, sizeof (disc->datasources_data[0]) * (disc->datasources_count + 1));
	if (!temp)
	{
		if (fd >= 0)
		{
			close (fd);
		}
		fprintf (stderr, "cdfs_disc_datasource_append() realloc failed\n");
		return;
	}
	disc->datasources_data = temp;
	disc->datasources_data[disc->datasources_count].sectoroffset = sectoroffset;
	disc->datasources_data[disc->datasources_count].sectorcount = sectorcount;
	disc->datasources_data[disc->datasources_count].fd = fd;
	disc->datasources_data[disc->datasources_count].filename = filename ? strdup (filename) : 0;
	disc->datasources_data[disc->datasources_count].format = format;
	disc->datasources_data[disc->datasources_count].offset = offset;
	disc->datasources_data[disc->datasources_count].length = length;
	disc->datasources_count++;
}

void cdfs_disc_track_append (struct cdfs_disc_t *disc,
                             uint32_t            pregap,
                             uint32_t            start,
                             uint32_t            length,
                             const char         *title,
                             const char         *performer,
                             const char         *songwriter,
                             const char         *composer,
                             const char         *arranger,
                             const char         *message)
{
	if (disc->tracks_count >= 100)
	{
		fprintf (stderr, "cdfs_disc_track_append() too many tracks\n");
		return;
	}

	disc->tracks_data[disc->tracks_count].pregap = pregap;
	disc->tracks_data[disc->tracks_count].start  = start;
	disc->tracks_data[disc->tracks_count].length = length;
	disc->tracks_data[disc->tracks_count].title      = title      ? strdup (title)      : 0;
	disc->tracks_data[disc->tracks_count].performer  = performer  ? strdup (performer)  : 0;
	disc->tracks_data[disc->tracks_count].songwriter = songwriter ? strdup (songwriter) : 0;
	disc->tracks_data[disc->tracks_count].composer   = composer   ? strdup (composer)   : 0;
	disc->tracks_data[disc->tracks_count].arranger   = arranger   ? strdup (arranger)   : 0;
	disc->tracks_data[disc->tracks_count].message    = message    ? strdup (message)    : 0;

	disc->tracks_count++;
}

void cdfs_disc_free (struct cdfs_disc_t *disc)
{
	int i;

	for (i=0; i < disc->datasources_count; i++)
	{
		if (disc->datasources_data[i].fd >= 0)
		{
			close (disc->datasources_data[i].fd);
		}
		free (disc->datasources_data[i].filename);
	}
	free (disc->datasources_data);

	for (i=0; i < 100; i++)
	{
		free (disc->tracks_data[i].title);
		free (disc->tracks_data[i].performer);
		free (disc->tracks_data[i].songwriter);
		free (disc->tracks_data[i].composer);
		free (disc->tracks_data[i].arranger);
		free (disc->tracks_data[i].message);
	}
	free (disc);
}

