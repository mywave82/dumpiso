#ifndef CDFS_H
#define CDFS_H

#define SECTORSIZE 2048

// All this is specific for CD-ROM, not for DVD

#define SECTORSIZE_XA1 2056
#define SECTORSIZE_XA2 2352

/*                SYNC HEADER SUBHEADER DATA EDC RESERVED ECC   R-W       HEADER = 3 byte address + 1 byte mode
CDA                                     2352                    96

CLEAR                                                                     (mode byte = 0x00, rest of sector should be 0..)

MODE-1            12      4     -       2048  4      8   276    96        (mode byte = 0x01) DATA (Red Book defines the ECC)    The sync pattern for Mode 1 CDs is 0x00ffffffffffffffffffff00
MODE-2            12      4     -       2336  -      -    -     96        (mode byte = 0x02) AUDIO VIDEO/PICTURE-DATA    if XA MODE-2, TOC should contain this information

(XA allows audio and data to be written on the same disk/session)
XA MODE-2-FORM-1  12      4     8       2048  4      -   276    96        computer DATA
XA MODE-2-FORM-2  12      4     8       2324  4      -    -     96        mode byte = 0x02, XA set in TOC - compressed AUDIO VIDEO/PICTURE-DATA
                                                                          SUBHEADER byte 0,1,3 ?   byte 2 mask 0x20   false=>FORM_1, true =>FORM_2   byte 6 mask 0x20 should be a copy of byte 2 mask... if they differ, we have FORMLESS ?



For Audio, P channel contains the pause-trigger signal (being high the last two seconds of a track and the first frame of a new track)

Q channel contains:
  Q bit 4 (in the first byte per frame) can be used to mute audio, to avoid data to make noise
    timing indication for the cd-player to put onto the display.
    absolute sector indication used for locating sectors when tracking
   or can for a frame contain
    ISRC number (uniqe song number) at the start of an audio track
    or MCN number in the lead-in area, giving a uniqe CD ID number

CD-TEXT (artist per album or per track, titles + languages) is written in the R and W channel within the lead-in/TOC (first two seconds of the medium.. lead-in is not readable on most drives, but specific commands are provided to download them from the drive)


SUBHEADER:
struct SubHeader
{
  uint8 FileNumber; // Used for interleaving, 0 = no interleaving
  uint8 AudioChannel;
  uint8 EndOfRecord:1;
  uint8 VideoData:1; // true if the sector contains video data
  uint8 ADCPM:1; // Audio data encoded with ADPCM
  uint8 Data.1; // true if sector contains data
  uint8 TriggerOn:1; // OS dependent
  uint8 Form2:1; // true => form 2, false => form 1
  uint8 RealTime:1; // Sector contains real time data
  uint8 EndOfFile:1; // true if last sector of file
  uint8 Encoding; // Don't know what is this
};



SYNC = 00 ff ff ff ff ff ff ff ff ff 00
HEADER = 3 byte minute:second:from + 1 byte mode


mkisofs
-sectype data    logical sector size = 2048,   iso file sector size = 2048
-sectype xa1     logical sector size = 2048,   iso file sector size = 2056 - includes subheader   byte 2+6 is XA_SUBH_DATA - 0x08
-sectype raw     logical sector size = 2048,   iso file sector size = 2352 ???



Q-data is part of the 98 bytes per sector of control data, for time and track codes

Green-book:
CD-I (Interactive) interleaves audio and data sectors
*/

#include "iso9660.h"
#include "udf.h"

enum cdfs_format_t
{
	FORMAT_RAW             = 0, /* AUDIO MODE1_RAW MODE1/2352 MODE2/2352 raw-files, CDA.. */
	FORMAT_RAW_RW          = 1, /* same as above, but including R-W Subchannels, in theory there is cooked vs uncoooked */
	FORMAT_RAW_XA          = 2, /* AUDIO MODE1_RAW MODE1/2352 MODE2/2352 raw-files, CDA..  For mode 2, it uses XA */
	FORMAT_RAW_RW_XA       = 3, /* same as above, but including R-W Subchannels, in theory there is cooked vs uncoooked... For mode 2, it uses XA */
//	FORMAT_RAW_MOTEROLA    = 4, /* Do 16bit endian swap on each word */
	FORMAT_DATA_2048       = 5, /* MODE1/2048 MODE2/2048 */
	FORMAT_XA1_DATA_2048   = 6, /* SUBHEADER prefixes 2048 bytes of data `mkisofs -sectype xa1`  */

	FORMAT_ERROR           = 255,
};

struct cdfs_datasource_t
{
	uint32_t sectoroffset; /* offset into disc */
	uint32_t sectorcount;  /* number of sectors */
	int fd;
	char *filename;
	enum cdfs_format_t format;
	uint32_t offset;
};

struct cdfs_disc_t
{
	int                       datasources_count; /* these are normally bound to a session, but easier to have them listed here */
	struct cdfs_datasource_t *datasources_data;

	/* can in theory be multiple sessions.... */
	struct ISO9660_session_t *iso9660_session;

	/* One UDF session can in theory cross sessions on disc */
	struct UDF_Session       *udf_session;
};

int get_absolute_sector (struct cdfs_disc_t *disc, uint32_t sector, uint8_t *buffer) /* 2048 byte modes */;

int detect_isofile_sectorformat (struct cdfs_datasource_t *isofile, off_t st_size);

#endif
