#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "wave.h"

int wave_filename(const char *filename)
{
	int len = strlen (filename);
	if (len < 4)
	{
		return 0;
	}

	if (!strcasecmp (filename + len - 4, ".wav"))
	{
		return 1;
	}

	if (len < 5)
	{
		return 0;
	}

	if (!strcasecmp (filename + len - 5, ".wave"))
	{
		return 1;
	}

	return 0;
}

static int wave_openfile2 (int fd, uint64_t *offset, uint64_t *length)
{
	uint8_t b16[16];
	// RIFFoffset = 0
	uint32_t  RIFFlen;
	uint32_t _RIFFlen;

	/* RIFF + len
	 * {
	 *   WAVE + len
	 *   {
	 *     fmt  + len
	 *     {
	 *        compression, alignement, bitrate, ....
	 *     }
	 *     ....
	 *     data + len
	 *     {
	 *       pcm data....
	 *     }
	 *   }
	 * }
	 */

	if (read (fd, b16, 8) != 8)
	{
		fprintf (stderr, "wave_openfile() failed to read RIFF header\n");
		return -1;
	}
	if ((b16[0] != 'R') ||
	    (b16[1] != 'I') ||
	    (b16[2] != 'F') ||
	    (b16[3] != 'F'))
	{
		fprintf (stderr, "wave_openfile() failed to verify RIFF header\n");
		return -1;
	}
	_RIFFlen = RIFFlen = b16[4] | (b16[5] << 8) | (b16[6] << 16) | (b16[7] << 24);
	if (RIFFlen < (4 + 8 + 16 + 8 + 1))
	{
		fprintf (stderr, "wave_openfile() RIFF length is smaller than absolute minimum size\n");
		return -1;
	}

	if (read (fd, b16, 4) != 4)
	{
		fprintf (stderr, "wave_openfile() failed to read WAVE subheader\n");
		return -1;
	}
	_RIFFlen -= 4;
	if ((b16[0] != 'W') ||
	    (b16[1] != 'A') ||
	    (b16[2] != 'V') ||
	    (b16[3] != 'E'))
	{
		fprintf (stderr, "wave_openfile() failed to verify WAVE subheader\n");
		return -1;
	}

	/* locate "fmt ", it always appears before data */
	while (1)
	{
		uint32_t sublen;
		if (_RIFFlen < 8)
		{
			fprintf (stderr, "wave_openfile() ran out of space inside RIFF header when searching for fmt subheader #1\n");
			return -1;
		}

		if (read (fd, b16, 8) != 8)
		{
			fprintf (stderr, "wave_openfile() failed to read WAVE subheader\n");
			return -1;
		}
		_RIFFlen -= 8;
		sublen = b16[4] | (b16[5] << 8) | (b16[6] << 16) | (b16[7] << 24);
		if (_RIFFlen < sublen)
		{
			fprintf (stderr, "wave_openfile() ran out of space inside RIFF header when searching for fmt subheader #2\n");
			return -1;
		}
		if ((b16[0] != 'f') ||
		    (b16[1] != 'm') ||
		    (b16[2] != 't') ||
		    (b16[3] != ' '))
		{
			if (lseek (fd, sublen, SEEK_CUR) == (off_t) -1)
			{
				fprintf (stderr, "wave_openfile() lseek caused EOF when skipping chunk while searching for fmt subheader\n");
				return -1;
			}
			_RIFFlen -= sublen;
			continue;
		}
		if (sublen < 16)
		{
			fprintf (stderr, "wave_openfile() fmt subheader is way too small\n");
			return -1;
		}
		if (read (fd, b16, 16) != 16)
		{
			fprintf (stderr, "wave_openfile() failed to read fmt data\n");
			return -1;
		}
		if (sublen > 16)
		{
			if (lseek (fd, sublen - 16, SEEK_CUR) == (off_t) -1)
			{
				fprintf (stderr, "wave_openfile() lseek caused EOF when skipping end of fmt chunk\n");
				return -1;
			}
		}
		_RIFFlen -= sublen;
		if ((b16[ 0] != 0x01) || // PCM-16bit
		    (b16[ 1] != 0x00) || // ^

		    (b16[ 2] != 0x02) || // 2-channels
		    (b16[ 3] != 0x00) || // ^

		    (b16[ 4] != 0x44) || // 44100Hz sample rate
		    (b16[ 5] != 0xac) || // ^^^
		    (b16[ 6] != 0x00) || // ^^
		    (b16[ 7] != 0x00) || // ^

		    (b16[ 8] != 0x10) || // 176400 bytes per second
		    (b16[ 9] != 0xb1) || // ^^^
		    (b16[10] != 0x02) || // ^^
		    (b16[11] != 0x00) || // ^

		    (b16[12] != 0x04) || // 4 bytes block align (bytes per sample)
		    (b16[13] != 0x00) || // ^

		    (b16[14] != 0x10) || // 16 bits per sample
		    (b16[15] != 0x00))   // ^
		{
			fprintf (stderr, "wave_openfile() WAV file is not 16bit stereo 44100Hz PCM formatted\n");
			return -1;
		}
		break; 
	}

	/* locate "data" */
	while (1)
	{
		uint32_t sublen;
		if (_RIFFlen < 8)
		{
			fprintf (stderr, "wave_openfile() ran out of space inside RIFF header when searching for data subheader #1\n");
			return -1;
		}

		if (read (fd, b16, 8) != 8)
		{
			fprintf (stderr, "wave_openfile() failed to read WAVE subheader\n");
			return -1;
		}
		_RIFFlen -= 8;
		sublen = b16[4] | (b16[5] << 8) | (b16[6] << 16) | (b16[7] << 24);
		if (_RIFFlen < sublen)
		{
			fprintf (stderr, "wave_openfile() ran out of space inside RIFF header when searching for data subheader #2\n");
			return -1;
		}
		if ((b16[0] != 'd') ||
		    (b16[1] != 'a') ||
		    (b16[2] != 't') ||
		    (b16[3] != 'a'))
		{
			if (lseek (fd, sublen, SEEK_CUR) == (off_t) -1)
			{
				fprintf (stderr, "wave_openfile() lseek caused EOF when skipping chunk while searching for fmt subheader\n");
				return -1;
			}
			_RIFFlen -= sublen;
			continue;
		}
		*offset = RIFFlen - _RIFFlen + 8;
		*length = sublen;
		return 0;
	}
}

int wave_openfile (const char *cwd, const char *filename, int *fd, uint64_t *offset, uint64_t *length)
{
	int l = strlen (cwd);
	char *f = malloc (l + 1 + strlen(filename) + 1);

#warning Cache of already parsed files is needed....

	if (!f)
	{
		return -1;
	}
	sprintf (f, "%s/%s", cwd, filename);
	*fd = open (f, O_RDONLY);
	free (f);
	if (*fd < 0)
	{
		fprintf (stderr, "wave_openfile() failed to open %s\n", filename);
		return -1;
	}

	if (wave_openfile2(*fd, offset, length))
	{
		close (*fd);
		*fd = -1;
		*offset = 0;
		*length = 0;
		return -1;
	}
	return 0;
}


