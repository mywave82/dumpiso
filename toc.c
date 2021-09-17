#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cdfs.h"
#include "toc.h"

enum TOC_tokens
{
	TOC_TOKEN_unknown = 0,
	TOC_TOKEN_string = 1, // "foobar"
	TOC_TOKEN_offset = 2, // #12345
	TOC_TOKEN_msf = 3,    // 01:23:45  (can be translated from any 1:1 1:12 12:1 12:12 1:1:1 1:1:12 1:12:12 1:12:1 1:12:12 12:1:1 12:1:12 12:12:1)
	TOC_TOKEN_number = 4, // 12345
	TOC_TOKEN_open = 5,   // {
	TOC_TOKEN_close = 6,   // }
	TOC_TOKEN_colon = 7, // :
	TOC_TOKEN_comma = 8, // ,   used by binary data in TOC_INFO1, TOC_INFO2 and SIZE_INFO
	TOC_TOKEN_CATALOG,
	TOC_TOKEN_CD_DA,
	TOC_TOKEN_CD_ROM,
	TOC_TOKEN_CD_ROM_XA,
	TOC_TOKEN_CD_TEXT,
		TOC_TOKEN_LANGUAGE_MAP,
			TOC_TOKEN_EN,
		TOC_TOKEN_LANGUAGE,
			TOC_TOKEN_TITLE,
			TOC_TOKEN_PERFORMER,
			TOC_TOKEN_SONGWRITER,
			TOC_TOKEN_COMPOSER,
			TOC_TOKEN_ARRANGER,
			TOC_TOKEN_MESSAGE,
			TOC_TOKEN_DISC_ID,
			TOC_TOKEN_GENRE,
			TOC_TOKEN_TOC_INFO1,
			TOC_TOKEN_TOC_INFO2,
			TOC_TOKEN_UPC_EAN,
			TOC_TOKEN_SIZE_INFO,
	TOC_TOKEN_TRACK,
		TOC_TOKEN_AUDIO,
		TOC_TOKEN_MODE1,
		TOC_TOKEN_MODE1_RAW,
		TOC_TOKEN_MODE2,
		TOC_TOKEN_MODE2_FORM1,
		TOC_TOKEN_MODE2_FORM2,
		TOC_TOKEN_MODE2_FORM_MIX,
		TOC_TOKEN_MODE2_RAW,
			TOC_TOKEN_RW,
			TOC_TOKEN_RW_RAW,
	TOC_TOKEN_NO,
	TOC_TOKEN_COPY,
	TOC_TOKEN_PRE_EMPHASIS,
	TOC_TOKEN_TWO_CHANNEL_AUDIO,
	TOC_TOKEN_FOUR_CHANNEL_AUDIO,
	TOC_TOKEN_ISRC,
	TOC_TOKEN_SILENCE,
	TOC_TOKEN_ZERO,
	TOC_TOKEN_FILE,
	TOC_TOKEN_AUDIOFILE,
		TOC_TOKEN_SWAP,
	TOC_TOKEN_DATAFILE,
	TOC_TOKEN_FIFO,
	TOC_TOKEN_START,
	TOC_TOKEN_PREGAP,
	TOC_TOKEN_INDEX
};

struct TOC_token_keyword_pair
{
	enum TOC_tokens  e;
	const char      *s;
};

enum TOC_parser_state
{
	TOC_PARSER_STATE_ready = 0,
	TOC_PARSER_STATE_catalog,        /* expects a string to follow */
	TOC_PARSER_STATE_cd_text_0,      /* waiting for { */
	TOC_PARSER_STATE_cd_text_1,
	TOC_PARSER_STATE_language_map_0, /* waiting for { */
	TOC_PARSER_STATE_language_map_1, /* waiting for id */
	TOC_PARSER_STATE_language_map_2, /* waiting for : */
	TOC_PARSER_STATE_language_map_3, /* waiting for EN */

	TOC_PARSER_STATE_language_0,     /* waiting for id */
	TOC_PARSER_STATE_language_1,     /* waiting for { */
	TOC_PARSER_STATE_language_2,     /* waiting for parameter name */
	TOC_PARSER_STATE_language_3,     /* waiting for parameter value or open */
	TOC_PARSER_STATE_language_4,     /* waiting for parameter numbers, comma or close */

	TOC_PARSER_STATE_track_0,        /* waiting for mode */
	TOC_PARSER_STATE_track_1,        /* waiting for optional subchannel */

	TOC_PARSER_STATE_no,             /* waiting for COPY or PRE_EMPHASIS */

	TOC_PARSER_STATE_isrc,           /* waiting for a string */

	TOC_PARSER_STATE_zero,           /* waiting for a msf */

	TOC_PARSER_STATE_audiofile_0,    /* waiting for a string */
	TOC_PARSER_STATE_audiofile_1,    /* waiting for SWAP offset or MSF */
	TOC_PARSER_STATE_audiofile_2,    /* waiting for optional MSF */

	TOC_PARSER_STATE_datafile_0,     /* waiting for a string */
	TOC_PARSER_STATE_datafile_1,     /* waiting for offset or MSF */

	TOC_PARSER_STATE_start,          /* waiting for optional msf */
	TOC_PARSER_STATE_pregap,         /* waiting for msf */

	TOC_PARSER_STATE_index,          /* waiting for msf */
};

enum storage_mode_t
{
	AUDIO = 0,
	MODE1,
	MODE1_RAW,
	MODE2,
	MODE2_FORM1,
	MODE2_FORM2,
	MODE2_FORM_MIX,
	MODE2_RAW
};

enum storage_mode_subchannel_t
{
	NONE = 0,
	SUBCHANNEL_RW,
	SUBCHANNEL_RW_RAW,
};

static enum cdfs_format_t toc_storage_mode_to_cdfs_format (enum storage_mode_t mode, enum storage_mode_subchannel_t subchannel, int swap)
{
	enum cdfs_format_t retval = 0;
	switch (mode)
	{
		case AUDIO:          retval = swap ? FORMAT_AUDIO_SWAP___NONE : FORMAT_AUDIO___NONE; break;
		case MODE1:          retval =        FORMAT_MODE1___NONE;                            break;
		case MODE1_RAW:      retval =        FORMAT_MODE1_RAW___NONE;                        break;
		case MODE2:          retval =        FORMAT_MODE2___NONE;                            break;
		case MODE2_FORM1:    retval =        FORMAT_XA_MODE2_FORM1___NONE;                   break;
		case MODE2_FORM2:    retval =        FORMAT_XA_MODE2_FORM2___NONE;                   break;
		case MODE2_FORM_MIX: retval =        FORMAT_XA_MODE2_FORM_MIX___NONE;                break;
		case MODE2_RAW:      retval =        FORMAT_MODE2_RAW___NONE;                        break;
	}
	switch (subchannel)
	{
		case NONE:                           break;
		case SUBCHANNEL_RW:     retval += 1; break;
		case SUBCHANNEL_RW_RAW: retval += 2; break;
	}
	return retval;
}

static int medium_sector_size (enum storage_mode_t mode, enum storage_mode_subchannel_t subchannel)
{
	int retval = 0;
	switch (mode)
	{
		case AUDIO:
		case MODE1_RAW:
		case MODE2_RAW:      retval = 2352; break;

		case MODE1:
		case MODE2_FORM1:    retval = 2048; break;

		case MODE2:
		case MODE2_FORM_MIX: retval = 2336; break; /* these two matches for the wrong reason. MODE2 skips the HEADER, MODE2_FORM_MIX skips the EDC */

		case MODE2_FORM2:    retval = 2324; break;
	}
	switch (subchannel)
	{
		case NONE:                           break;

		case SUBCHANNEL_RW:
		case SUBCHANNEL_RW_RAW: retval += 96; break;
	}
	return retval;
}

struct toc_parser_datasource_t
{
	char *filename; /* can be NULL for zero insertion, if mode==AUDIO and filename ends with .wav, treat file as a wave-file with audio... */
	int64_t length; /* -1 == not initialized  - given in sectors*/
	uint64_t offset; /* given in bytes..... */
	int swap;
};

struct toc_parser_track_t
{
	enum storage_mode_t            storage_mode;
	enum storage_mode_subchannel_t storage_mode_subchannel;

	char *title;
	char *performer;
	char *songwriter;
	char *composer;
	char *arranger;
	char *message;

	int four_channel_audio;
	int32_t pregap; /* given in sectors */
/*
	char *genre;
	char *disc_id;
	char *toc_info1;
	char *toc_info2;
	char *upc_ean;
	char *size_info;
*/

	struct toc_parser_datasource_t *datasource;
	int datasourceN;
};

struct toc_parser_t
{
	enum TOC_parser_state state;
	char **parser_destination;
	int track;
	struct toc_parser_track_t trackdata[100]; /* track 0 is global-common info */
};

static int toc_parser_append_source (struct toc_parser_t *toc_parser, const char *src)
{
	void *temp = realloc (toc_parser->trackdata[toc_parser->track].datasource, sizeof (toc_parser->trackdata[toc_parser->track].datasource[0]) * (toc_parser->trackdata[toc_parser->track].datasourceN + 1));
	if (!temp)
	{
		return -1;
	}
	toc_parser->trackdata[toc_parser->track].datasource = temp;
	toc_parser->trackdata[toc_parser->track].datasource[toc_parser->trackdata[toc_parser->track].datasourceN].filename = src ? strdup(src) : 0;
	toc_parser->trackdata[toc_parser->track].datasource[toc_parser->trackdata[toc_parser->track].datasourceN].length = -1;
	toc_parser->trackdata[toc_parser->track].datasource[toc_parser->trackdata[toc_parser->track].datasourceN].offset = 0;
	toc_parser->trackdata[toc_parser->track].datasource[toc_parser->trackdata[toc_parser->track].datasourceN].swap = 0;
	toc_parser->trackdata[toc_parser->track].datasourceN++;
	return 0;
}

static void toc_parser_modify_length (struct toc_parser_t *toc_parser, const int64_t length)
{
	toc_parser->trackdata[toc_parser->track].datasource[toc_parser->trackdata[toc_parser->track].datasourceN - 1].length = length;
}

static void toc_parser_modify_offset (struct toc_parser_t *toc_parser, const char *src)
{
	unsigned long long length = strtoull (src + 1, 0, 10);
	toc_parser->trackdata[toc_parser->track].datasource[toc_parser->trackdata[toc_parser->track].datasourceN - 1].offset = length;
}

static void toc_parser_modify_SWAP (struct toc_parser_t *toc_parser)
{
	toc_parser->trackdata[toc_parser->track].datasource[toc_parser->trackdata[toc_parser->track].datasourceN - 1].swap = 1;
}

static void toc_parser_modify_pregap (struct toc_parser_t *toc_parser, const int length)
{
	toc_parser->trackdata[toc_parser->track].pregap = length;
}

static void toc_parser_append_index (struct toc_parser_t *toc_parser, const int length)
{
/*
	unsigned long long length = strtoull (src + 1, 0, 10);
	...
	we do not need use the index...
*/
}

static int toc_parse_token (struct toc_parser_t *toc_parser, enum TOC_tokens token, const char *src)
{
	if (toc_parser->state == TOC_PARSER_STATE_catalog)
	{
		if ((token == TOC_TOKEN_string) ||
		    (token == TOC_TOKEN_number))
		{
			toc_parser->state = TOC_PARSER_STATE_ready;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_cd_text_0)
	{
		if (token == TOC_TOKEN_open)
		{
			toc_parser->state = TOC_PARSER_STATE_cd_text_1;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_cd_text_1)
	{
		switch (token)
		{
			case TOC_TOKEN_close:
				toc_parser->state = TOC_PARSER_STATE_ready;
				return 0;
			case TOC_TOKEN_LANGUAGE_MAP:
				if (toc_parser->track == 0) /* only valid in the header */
				{
					toc_parser->state = TOC_PARSER_STATE_language_map_0;
					return 0;
				}
				return -1;
			case TOC_TOKEN_LANGUAGE:
				toc_parser->state = TOC_PARSER_STATE_language_0;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_map_0)
	{
		if (token == TOC_TOKEN_open)
		{
			toc_parser->state = TOC_PARSER_STATE_language_map_1;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_map_1)
	{
		switch (token)
		{
			case TOC_TOKEN_close:
				toc_parser->state = TOC_PARSER_STATE_cd_text_1;
				return 0;
			case TOC_TOKEN_number:
				toc_parser->state = TOC_PARSER_STATE_language_map_2;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_map_2)
	{
		if (token == TOC_TOKEN_colon)
		{
			toc_parser->state = TOC_PARSER_STATE_language_map_3;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_map_3)
	{
		/* seems like EN is the only accepted language...? */
		if ((token == TOC_TOKEN_EN) || (token == TOC_TOKEN_number))
		{
			toc_parser->state = TOC_PARSER_STATE_language_map_1;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_0)
	{
		if (token == TOC_TOKEN_number)
		{
			toc_parser->state = TOC_PARSER_STATE_language_1;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_1)
	{
		if (token == TOC_TOKEN_open)
		{
			toc_parser->state = TOC_PARSER_STATE_language_2;
			return 0;
		}
		return -1;
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_2)
	{
		switch (token)
		{
			case TOC_TOKEN_close:
				toc_parser->state = TOC_PARSER_STATE_cd_text_1;
				return 0;
			case TOC_TOKEN_TITLE:
				toc_parser->parser_destination = &toc_parser->trackdata[toc_parser->track].title;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_PERFORMER:
				toc_parser->parser_destination = &toc_parser->trackdata[toc_parser->track].performer;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_SONGWRITER:
				toc_parser->parser_destination = &toc_parser->trackdata[toc_parser->track].songwriter;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_COMPOSER:
				toc_parser->parser_destination = &toc_parser->trackdata[toc_parser->track].composer;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_ARRANGER:
				toc_parser->parser_destination = &toc_parser->trackdata[toc_parser->track].arranger;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_MESSAGE:
				toc_parser->parser_destination = &toc_parser->trackdata[toc_parser->track].message;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			case TOC_TOKEN_GENRE:
			case TOC_TOKEN_DISC_ID:
			case TOC_TOKEN_TOC_INFO1:
			case TOC_TOKEN_TOC_INFO2:
			case TOC_TOKEN_UPC_EAN:
			case TOC_TOKEN_SIZE_INFO:
			case TOC_TOKEN_ISRC:
				/* we ignore these */
				toc_parser->parser_destination = 0;
				toc_parser->state = TOC_PARSER_STATE_language_3;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_3)
	{
		switch (token)
		{
			case TOC_TOKEN_string:
				if ((toc_parser->parser_destination) && (!*toc_parser->parser_destination))
				{
					*toc_parser->parser_destination = strdup (src);
				}
				toc_parser->state = TOC_PARSER_STATE_language_2;
				return 0;
			case TOC_TOKEN_open:
				toc_parser->state = TOC_PARSER_STATE_language_4;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_language_4)
	{
		switch (token)
		{
			case TOC_TOKEN_number:
			case TOC_TOKEN_comma:
				return 0;
			case TOC_TOKEN_close:
				toc_parser->state = TOC_PARSER_STATE_language_2;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_track_0)
	{
		switch (token)
		{
			case TOC_TOKEN_AUDIO:          toc_parser->trackdata[toc_parser->track].storage_mode = AUDIO;          break; /* 2352 bytes of raw audio data */
			case TOC_TOKEN_MODE1:          toc_parser->trackdata[toc_parser->track].storage_mode = MODE1;          break; /* 2048 bytes of data */
			case TOC_TOKEN_MODE1_RAW:      toc_parser->trackdata[toc_parser->track].storage_mode = MODE1_RAW;      break; /* 2352 bytes of raw data */
			case TOC_TOKEN_MODE2:          toc_parser->trackdata[toc_parser->track].storage_mode = MODE2;          break; /* 2336 bytes of data - special sector size known as XA */
			case TOC_TOKEN_MODE2_FORM1:    toc_parser->trackdata[toc_parser->track].storage_mode = MODE2_FORM1;    break; /* 2048 bytes of data - different wrapper, not visible for filesystem layer */
			case TOC_TOKEN_MODE2_FORM2:    toc_parser->trackdata[toc_parser->track].storage_mode = MODE2_FORM2;    break; /* 2324 bytes of data - different wrapper, special sector size */
			case TOC_TOKEN_MODE2_FORM_MIX: toc_parser->trackdata[toc_parser->track].storage_mode = MODE2_FORM_MIX; break; /* 2336 XA MODE-FORM1/2, includes the prefix SUBHEADER - different wrapper and two possible sector sizes */
			case TOC_TOKEN_MODE2_RAW:      toc_parser->trackdata[toc_parser->track].storage_mode = MODE2_RAW;      break; /* 2352 bytes of raw data */
			default: return -1;
		}
		toc_parser->state = TOC_PARSER_STATE_track_1;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_track_1)
	{
		switch (token)
		{
			case TOC_TOKEN_RW_RAW: toc_parser->trackdata[toc_parser->track].storage_mode_subchannel = SUBCHANNEL_RW_RAW; toc_parser->state = TOC_PARSER_STATE_ready; return 0;
			case TOC_TOKEN_RW:     toc_parser->trackdata[toc_parser->track].storage_mode_subchannel = SUBCHANNEL_RW;     toc_parser->state = TOC_PARSER_STATE_ready; return 0;
			default:
				break;
		}
		toc_parser->state = TOC_PARSER_STATE_ready;
		/* pass-throught */
	}

	if (toc_parser->state == TOC_PARSER_STATE_no)
	{
		switch (token)
		{
			case TOC_TOKEN_COPY: break;
			case TOC_TOKEN_PRE_EMPHASIS: break;
			default: return -1;
		}
		toc_parser->state = TOC_PARSER_STATE_ready;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_isrc)
	{
		if (token != TOC_TOKEN_string)
		{
			return -1;
		}
		toc_parser->state = TOC_PARSER_STATE_ready;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_zero)
	{
		if (token != TOC_TOKEN_msf)
		{
			return -1;
		}
		if (toc_parser_append_source (toc_parser, 0))
		{
			return -1;
		}
		toc_parser_modify_length (toc_parser, (src[0]-'0') * 75*60*10 +
		                                      (src[1]-'0') * 75*60    +
		                                      (src[3]-'0') * 75   *10 +
		                                      (src[4]-'0') * 75       +
		                                      (src[6]-'0') *       10 +
		                                      (src[7]-'0'));
		toc_parser->state = TOC_PARSER_STATE_ready;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_audiofile_0)
	{
		if (token != TOC_TOKEN_string)
		{
			return -1;
		}
		if (toc_parser_append_source (toc_parser, src))
		{
			return -1;
		}
		toc_parser->state = TOC_PARSER_STATE_audiofile_1;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_audiofile_1)
	{
		switch (token)
		{
			case TOC_TOKEN_SWAP:
				toc_parser_modify_SWAP (toc_parser);
				return 0;
			case TOC_TOKEN_offset:
				toc_parser_modify_offset (toc_parser, src);
				return 0;
			case TOC_TOKEN_msf:
				toc_parser_modify_pregap (toc_parser, (src[0]-'0') * 75*60*10 +
			                                              (src[1]-'0') * 75*60    +
			                                              (src[3]-'0') * 75   *10 +
			                                              (src[4]-'0') * 75       +
			                                              (src[6]-'0')        *10 +
			                                              (src[7]-'0'));
				toc_parser->state = TOC_PARSER_STATE_audiofile_2;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_audiofile_2)
	{
		if (token == TOC_TOKEN_msf)
		{
			toc_parser_modify_length (toc_parser, (src[0]-'0') * 75*60*10 +
		                                              (src[1]-'0') * 75*60    +
		                                              (src[3]-'0') * 75   *10 +
		                                              (src[4]-'0') * 75       +
		                                              (src[6]-'0')        *10 +
		                                              (src[7]-'0'));
			toc_parser->state = TOC_PARSER_STATE_ready;
			return 0;
		}
		toc_parser->state = TOC_PARSER_STATE_ready;
		/* fall-through */
	}

	if (toc_parser->state == TOC_PARSER_STATE_datafile_0)
	{
		if (token != TOC_TOKEN_string)
		{
			return -1;
		}
		if (toc_parser_append_source (toc_parser, src))
		{
			return -1;
		}
		toc_parser->state = TOC_PARSER_STATE_datafile_1;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_datafile_1)
	{
		switch (token)
		{
			case TOC_TOKEN_offset:
				toc_parser_modify_offset (toc_parser, src);
				return 0;
			case TOC_TOKEN_msf:
				toc_parser_modify_pregap (toc_parser, (src[0]-'0') * 75*60*10 +
			                                              (src[1]-'0') * 75*60    +
			                                              (src[3]-'0') * 75   *10 +
			                                              (src[4]-'0') * 75       +
			                                              (src[6]-'0')        *10 +
			                                              (src[7]-'0'));
				toc_parser->state = TOC_PARSER_STATE_ready;
				return 0;
			default:
				return -1;
		}
	}

	if (toc_parser->state == TOC_PARSER_STATE_start)
	{
		if (token == TOC_TOKEN_msf)
		{
			toc_parser_modify_pregap (toc_parser, (src[0]-'0') * 75*60*10 +
		                                              (src[1]-'0') * 75*60    +
		                                              (src[3]-'0') * 75   *10 +
		                                              (src[4]-'0') * 75       +
		                                              (src[6]-'0')        *10 +
		                                              (src[7]-'0'));
			toc_parser->state = TOC_PARSER_STATE_ready;
			return 0;
		}
		toc_parser->state = TOC_PARSER_STATE_ready;
		/* fall-through */
	}

	if (toc_parser->state == TOC_PARSER_STATE_pregap)
	{
		if (token != TOC_TOKEN_msf)
		{
			return -1;
		}
		if (toc_parser_append_source (toc_parser, 0))
		{
			return -1;
		}
		toc_parser_modify_length (toc_parser, (src[0]-'0') * 75*60*10 +
		                                      (src[1]-'0') * 75*60    +
		                                      (src[3]-'0') * 75   *10 +
		                                      (src[4]-'0') * 75       +
		                                      (src[6]-'0')        *10 +
		                                      (src[7]-'0'));
		toc_parser_modify_pregap (toc_parser, (src[0]-'0') * 75*60*10 +
	                                              (src[1]-'0') * 75*60    +
	                                              (src[3]-'0') * 75   *10 +
	                                              (src[4]-'0') * 75       +
	                                              (src[6]-'0')        *10 +
	                                              (src[7]-'0'));
		toc_parser->state = TOC_PARSER_STATE_ready;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_start)
	{
		if (token != TOC_TOKEN_msf)
		{
			return -1;
		}
		toc_parser_append_index (toc_parser, (src[0]-'0') * 75*60*10 +
		                                     (src[1]-'0') * 75*60    +
		                                     (src[3]-'0') * 75   *10 +
		                                     (src[4]-'0') * 75       +
		                                     (src[6]-'0')        *10 +
		                                     (src[7]-'0'));
		toc_parser->state = TOC_PARSER_STATE_ready;
		return 0;
	}

	if (toc_parser->state == TOC_PARSER_STATE_ready)
	{
		switch (token)
		{
			case TOC_TOKEN_CD_DA:
			case TOC_TOKEN_CD_ROM:
			case TOC_TOKEN_CD_ROM_XA: /* we do need the information this keyword provides */
				if (toc_parser->track) return -1;
				return 0;
			case TOC_TOKEN_CATALOG:
				toc_parser->state = TOC_PARSER_STATE_catalog;
				return 0;
			case TOC_TOKEN_CD_TEXT:
				toc_parser->state = TOC_PARSER_STATE_cd_text_0;
				return 0;
			case TOC_TOKEN_TRACK:
				if (toc_parser->track >= 99)
				{
					return -1;
				}
				toc_parser->track++;
				toc_parser->state = TOC_PARSER_STATE_track_0;
				return 0;
			case TOC_TOKEN_LANGUAGE:
				toc_parser->state = TOC_PARSER_STATE_language_0;
				return 0;
			case TOC_TOKEN_NO:
				toc_parser->state = TOC_PARSER_STATE_no;
				return 0;
			case TOC_TOKEN_COPY:
			case TOC_TOKEN_PRE_EMPHASIS: /* we need some examples of this in order to know what to do */
			case TOC_TOKEN_TWO_CHANNEL_AUDIO:
				return 0;
			case TOC_TOKEN_FOUR_CHANNEL_AUDIO:
				toc_parser->trackdata[toc_parser->track].four_channel_audio = 1;
				return 0;
			case TOC_TOKEN_ISRC:
				toc_parser->state = TOC_PARSER_STATE_isrc;
				return 0;
			case TOC_TOKEN_SILENCE: /* same as ZERO, but should be used for AUDIO */
			case TOC_TOKEN_ZERO:
				toc_parser->state = TOC_PARSER_STATE_zero;
				return 0;
			case TOC_TOKEN_FILE:
			case TOC_TOKEN_AUDIOFILE:
				toc_parser->state = TOC_PARSER_STATE_audiofile_0;
				return 0;
			case TOC_TOKEN_DATAFILE:
				toc_parser->state = TOC_PARSER_STATE_datafile_0;
				return 0;
			case TOC_TOKEN_FIFO: /* we can not handle these.... */
				return -1;
			case TOC_TOKEN_START:
				toc_parser_modify_pregap (toc_parser, -1); /* default to use the entire data availably until now */
				toc_parser->state = TOC_PARSER_STATE_start;
				return 0;
			case TOC_TOKEN_PREGAP:
				toc_parser->state = TOC_PARSER_STATE_pregap;
				return 0;
			case TOC_TOKEN_INDEX:
				toc_parser->state = TOC_PARSER_STATE_index;
				return 0;
			default:
				return -1;
		}
	}

	return -1;
}

static int toc_check_keyword (const char *input, int l, const char *needle)
{
	int nl = strlen (needle);
	if (l < nl) return 0;
	if (!memcmp (input, needle, nl))
	{
		if (nl == l) return 1;
		if (input[nl] == ' ') return 1;
		if (input[nl] == '\t') return 1;
		if (input[nl] == '\r') return 1;
		if (input[nl] == '\n') return 1;
	}
	return 0;
}

static void toc_parse_error (const char *orig, const char *input, const char *eol, const int lineno)
{
	int i = 0;
	fprintf (stderr, "Failed to parse .TOC file at line %d\n", lineno + 1);
	while (1)
	{
		if (orig[i] == '\r') break;
		if (orig[i] == '\n') break;
		if (orig[i] == '\t') fprintf (stderr, " ");
		else fprintf (stderr, "%c", orig[i]);
		i++;
	}
	i = 0;
	fprintf (stderr, "\n");
	while (1)
	{
		if (orig[i] == '\r') break;
		if (orig[i] == '\n') break;
		if (orig[i] == '\t') fprintf (stderr, " ");
		if (orig + i == input)
		{
			fprintf (stderr, "^ here\n");
			break;
		}
		fprintf (stderr, " ");
		i++;
	}
	fprintf (stderr, "\n");
}

static int _toc_parse_token (struct toc_parser_t *toc_parser, const char *orig, const char *input, const char *eol, const int lineno, enum TOC_tokens token, const char *buffer)
{
	if (toc_parse_token (toc_parser, token, buffer))
	{
		toc_parse_error (orig, input, eol, lineno);
		return -1;
	}
	return 0;
}

static int toc_parse_line (struct toc_parser_t *toc_parser, const char *input, const char *eol, const int lineno)
{
	const char *orig = input;
	char buffer[2048];
	int bufferfill = 0;
	int l = eol - input;

	while (l)
	{
		const char *current = input;
		bufferfill = 0;
		if ((input[0] == ' ') || (input[0] == '\t'))
		{
			input++; l--;
		} else if ((l >= 2) && (input[0] == '/') && (input[1] == '/'))
		{
			/* rest of the line is a comment */
			return 0;
		} else if (input[0] == '\"')
		{
			input++; l--;
			while (l)
			{
				char toadd = 0;
				if (input[0] == '\"')
				{
					input++; l--;
					break;
				}
				if (input[0] == '\\' && l > 1)
				{
					switch (input[1])
					{
						case 'n': toadd = '\n'; break;
						case 'r': toadd = '\r'; break;
						case 't': toadd = '\t'; break;
						default: toadd = input[1]; break;
					}
					input+=2; l-=2;
				} else {
					toadd = input[0];
					input++; l--;
				}
				if (bufferfill >= (sizeof(buffer)-2))
				{
					toc_parse_error (orig, current, eol, lineno);
					return -1;
				}
				buffer[bufferfill++] = toadd;
			}
			buffer[bufferfill] = 0;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_string, buffer)) return -1;
		} else if (input[0] == '#')
		{
			buffer[bufferfill++] = input[0];
			input++; l--;
			while (l && isdigit(input[0]))
			{
				if (bufferfill >= (sizeof(buffer)-2))
				{
					toc_parse_error (orig, current, eol, lineno);
					return -1;
				}
				buffer[bufferfill++] = input[0];
				input++; l--;
			}
			buffer[bufferfill] = 0;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_offset, buffer)) return -1;
		} else if (isdigit(input[0]))
		{
			int d0 = 1;
			int d1 = (l >= 2) && isdigit(input[1]);
			int d2 = (l >= 3) && isdigit(input[2]);
			int d3 = (l >= 4) && isdigit(input[3]);
			int d4 = (l >= 5) && isdigit(input[4]);
			int d5 = (l >= 6) && isdigit(input[5]);
			int d6 = (l >= 7) && isdigit(input[6]);
			int d7 = (l >= 8) && isdigit(input[7]);
			int c1 = (l >= 2) && (input[1] == ':');
			int c2 = (l >= 3) && (input[2] == ':');
			int c3 = (l >= 4) && (input[3] == ':');
			int c4 = (l >= 5) && (input[4] == ':');
			int c5 = (l >= 6) && (input[5] == ':');

			     if (d0 && d1 && c2 && d3 && d4 && c5 && d6 && d7) { sprintf (buffer, "%c%c:%c%c:%c%c", input[0], input[1], input[3], input[4], input[6], input[7]); input += 8; l-= 8; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } // 12:12:12
			else if (      d0 && c1 && d2 && d3 && c4 && d5 && d6) { sprintf (buffer, "0%c:%c%c:%c%c",            input[0], input[2], input[3], input[5], input[6]); input += 7; l-= 7; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //  1:12:12
			else if (d0 && d1 && c2 &&       d3 && c4 && d5 && d6) { sprintf (buffer, "%c%c:0%c:%c%c",  input[0], input[1],           input[3], input[5], input[6]); input += 7; l-= 7; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } // 12: 1:12
			else if (d0 && d1 && c2 && d3 && d4 && c5 &&       d6) { sprintf (buffer, "%c%c:%c%c:0%c",  input[0], input[1], input[3], input[4],           input[6]); input += 7; l-= 7; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } // 12:12: 1
			else if (      d0 && c1 &&       d2 && c3 && d4 && d5) { sprintf (buffer, "0%c:0%c:%c%c",             input[0],           input[2], input[4], input[5]); input += 6; l-= 6; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //  1: 1:12
			else if (      d0 && c1 && d2 && d3 && c4 &&       d5) { sprintf (buffer, "0%c:%c%c:0%c",             input[0], input[2], input[3],           input[5]); input += 6; l-= 6; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //  1:12: 1
			else if (d0 && d1 && c2 &&       d3 && c3 &&       d5) { sprintf (buffer, "%c%c:0%c:0%c",   input[0], input[1],           input[3],           input[5]); input += 6; l-= 6; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } // 12: 1: 1
			else if (      d0 && c1 &&       d2 && c3 &&       d4) { sprintf (buffer, "0%c:0%c:0%c",              input[0],           input[2],           input[4]); input += 5; l-= 5; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //  1: 1: 1
			else if (                  d0 && d1 && c2 && d3 && d4) { sprintf (buffer, "00:%c%c:%c%c",                       input[0], input[1], input[3], input[4]); input += 5; l-= 5; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //    12:12
			else if (                        d0 && c1 && d2 && d3) { sprintf (buffer, "00:0%c:%c%c",                                  input[0], input[2], input[3]); input += 4; l-= 4; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //     1:12
			else if (                  d0 && d1 && c2 &&       d3) { sprintf (buffer, "00:%c%c:0%c",                                  input[0], input[1], input[3]); input += 4; l-= 4; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //    12: 1
			else if (                        d0 && c1 &&       d2) { sprintf (buffer, "00:0%c:0%c",                                   input[0],           input[2]); input += 3; l-= 3; if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_msf, buffer)) return -1; } //     1: 1
			else {
				while (l && isdigit(input[0]))
				{
					if (bufferfill >= (sizeof(buffer)-2))
					{
						toc_parse_error (orig, current, eol, lineno);
						return -1;
					}
					buffer[bufferfill++] = input[0];
					input++; l--;
				}
				buffer[bufferfill] = 0;
				if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_number, buffer)) return -1;
			}
		} else if (input[0] == '{')
		{
			input++; l--;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_open, "{")) return -1;
		} else if (input[0] == '}')
		{
			input++; l--;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_close, "}")) return -1;
		} else if (input[0] == ':')
		{
			input++; l--;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_colon, ":")) return -1;
		} else if (input[0] == ',')
		{
			input++; l--;
			if (_toc_parse_token (toc_parser, orig, current, eol, lineno, TOC_TOKEN_comma, ",")) return -1;
		} else {
			int i;
			struct TOC_token_keyword_pair d[] =
			{
				{TOC_TOKEN_CATALOG,            "CATALOG"},
				{TOC_TOKEN_CD_DA,              "CD_DA"},
				{TOC_TOKEN_CD_ROM,             "CD_ROM"},
				{TOC_TOKEN_CD_ROM_XA,          "CD_ROM_XA"},
				{TOC_TOKEN_CD_TEXT,            "CD_TEXT"},
				{TOC_TOKEN_LANGUAGE_MAP,       "LANGUAGE_MAP"},
				{TOC_TOKEN_EN,                 "EN"},
				{TOC_TOKEN_LANGUAGE,           "LANGUAGE"},
				{TOC_TOKEN_TITLE,              "TITLE"},
				{TOC_TOKEN_PERFORMER,          "PERFORMER"},
				{TOC_TOKEN_SONGWRITER,         "SONGWRITER"},
				{TOC_TOKEN_COMPOSER,           "COMPOSER"},
				{TOC_TOKEN_ARRANGER,           "ARRANGER"},
				{TOC_TOKEN_MESSAGE,            "MESSAGE"},
				{TOC_TOKEN_DISC_ID,            "DISC_ID"},
				{TOC_TOKEN_GENRE,              "GENRE"},
				{TOC_TOKEN_TOC_INFO1,          "TOC_INFO1"},
				{TOC_TOKEN_TOC_INFO2,          "TOC_INFO2"},
				{TOC_TOKEN_UPC_EAN,            "UPC_EAN"},
				{TOC_TOKEN_SIZE_INFO,          "SIZE_INFO"},
				{TOC_TOKEN_TRACK,              "TRACK"},
				{TOC_TOKEN_AUDIO,              "AUDIO"},
				{TOC_TOKEN_MODE1,              "MODE1"},
				{TOC_TOKEN_MODE1_RAW,          "MODE1_RAW"},
				{TOC_TOKEN_MODE2,              "MODE2"},
				{TOC_TOKEN_MODE2_FORM1,        "MODE2_FORM1"},
				{TOC_TOKEN_MODE2_FORM2,        "MODE2_FORM2"},
				{TOC_TOKEN_MODE2_FORM_MIX,     "MODE2_FORM_MIX"},
				{TOC_TOKEN_MODE2_RAW,          "MODE2_RAW"},
				{TOC_TOKEN_RW,                 "RW"},
				{TOC_TOKEN_RW_RAW,             "RW_RAW"},
				{TOC_TOKEN_NO,                 "NO"},
				{TOC_TOKEN_COPY,               "COPY"},
				{TOC_TOKEN_PRE_EMPHASIS,       "PRE_EMPHASIS"},
				{TOC_TOKEN_TWO_CHANNEL_AUDIO,  "TWO_CHANNEL_AUDIO"},
				{TOC_TOKEN_FOUR_CHANNEL_AUDIO, "FOUR_CHANNEL_AUDIO"},
				{TOC_TOKEN_ISRC,               "ISRC"},
				{TOC_TOKEN_SILENCE,            "SILENCE"},
				{TOC_TOKEN_ZERO,               "ZERO"},
				{TOC_TOKEN_FILE,               "FILE"},
				{TOC_TOKEN_AUDIOFILE,          "AUDIOFILE"},
				{TOC_TOKEN_DATAFILE,           "DATAFILE"},
				{TOC_TOKEN_SWAP,               "SWAP"},
				{TOC_TOKEN_FIFO,               "FIFO"},
				{TOC_TOKEN_START,              "START"},
				{TOC_TOKEN_PREGAP,             "PREGAP"},
				{TOC_TOKEN_INDEX,              "INDEX"}
			};
			for (i=0; i < (sizeof (d) / sizeof (d[0])); i++)
			{
				if (toc_check_keyword (input, l, d[i].s))
				{
					input+=strlen (d[i].s); l-= strlen (d[i].s);
					if (_toc_parse_token (toc_parser, orig, current, eol, lineno, d[i].e, d[i].s)) return -1;
					break;
				}
			}
			if (i != (sizeof (d) / sizeof (d[0])))
			{
				continue;
			}
			toc_parse_error (orig, current, eol, lineno);
			return -1;
		}
	}
	return 0;
}

void toc_parser_free (struct toc_parser_t *toc_parser)
{
	int i, j;
	for (i = 0; i < 100; i++)
	{
		free (toc_parser->trackdata[i].title);
		free (toc_parser->trackdata[i].performer);
		free (toc_parser->trackdata[i].songwriter);
		free (toc_parser->trackdata[i].composer);
		free (toc_parser->trackdata[i].arranger);
		free (toc_parser->trackdata[i].message);
		for (j = 0; j < toc_parser->trackdata[i].datasourceN; j++)
		{
			free (toc_parser->trackdata[i].datasource[j].filename);
		}
		free (toc_parser->trackdata[i].datasource);
	}
	free (toc_parser);
}

struct toc_parser_t *toc_parser_from_data (const char *input)
{
	struct toc_parser_t *retval;
	const char *eof = input + strlen (input);

	int rN = 0;
	int rR = 0;

	retval = calloc (sizeof (*retval), 1);
	if (!retval)
	{
		fprintf (stderr, "toc_parser() calloc() failed\n");
		return 0;
	}

	while (*input)
	{
		const char *eol = eof;
		char *eol1 = strchr (input, '\r');
		char *eol2 = strchr (input, '\n');
		if (eol1 && eol1 < eol) eol = eol1;
		if (eol2 && eol2 < eol) eol = eol2;

		if (input != eol)
		{
			if (*eol == '\r') rR++;
			if (*eol == '\n') rN++;

			if (toc_parse_line (retval, input, eol, rR > rN ? rR : rN))
			{
				toc_parser_free (retval);
				return 0;
			}
		}
		input = eol + 1;
	}

	return retval;
}

static int wave_filename(const char *filename)
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

static int wave_openfile (const char *cwd, const char *filename, int *fd, uint64_t *offset, uint64_t *length)
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

static int data_openfile (const char *cwd, const char *filename, int *fd, uint64_t *length)
{
	int l = strlen (cwd);
	char *f = malloc (l + 1 + strlen(filename) + 1);
	struct stat st;

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
		fprintf (stderr, "data_openfile() failed to open %s\n", filename);
		return -1;
	}

	if (fstat (*fd, &st))
	{
		fprintf (stderr, "data_openfile() fstat() failed\n");
		close (*fd);
		*fd = -1;
		return -1;
	}

	*length = st.st_size;

	return 0;
}

struct cdfs_disc_t *toc_parser_to_cdfs_disc (const char *argv1_path, struct toc_parser_t *toc_parser)
{
	struct cdfs_disc_t *retval = calloc (sizeof (*retval), 1);
	int i;
	uint32_t trackoffset = 0; /* start of current track */

	if (!retval)
	{
		fprintf (stderr, "toc_parser_to_cdfs_disc(): calloc() failed\n");
		return 0;
	}

	for (i=0; i <= toc_parser->track; i++)
	{
		uint32_t tracklength = 0; /* in sectors */
		int j;
		for (j=0; j < toc_parser->trackdata[i].datasourceN; j++)
		{
			if (!toc_parser->trackdata[i].datasource[j].length)
			{ /* ignore zero length entries */
				continue;
			}

			if (!toc_parser->trackdata[i].datasource[j].filename)
			{ /* zero-fill */
				if (toc_parser->trackdata[i].datasource[j].length < 0)
				{
					goto fail_out;
				}
				tracklength += toc_parser->trackdata[i].datasource[j].length;
				continue;
			}

			if ((toc_parser->trackdata[i].storage_mode == AUDIO) &&
			    (toc_parser->trackdata[i].storage_mode_subchannel == NONE) &&
			    wave_filename (toc_parser->trackdata[i].datasource[j].filename))
			{
				int fd = -1;
				uint64_t offset = 0;
				uint64_t length = 0;
				uint32_t lengthsectors;

				if (wave_openfile (argv1_path, toc_parser->trackdata[i].datasource[j].filename, &fd, &offset, &length))
				{
					fprintf (stderr, "Failed to open wave file %s (format must be stereo, 16bit, 44100 sample-rate)\n", toc_parser->trackdata[i].datasource[j].filename);
					goto fail_out;
				}

				if (toc_parser->trackdata[i].datasource[j].offset>=0)
				{
					if (toc_parser->trackdata[i].datasource[j].offset >= length)
					{
						fprintf (stderr, "Wave file shorter than offset in .toc file\n");
						close (fd);
						goto fail_out;
					}
					offset += toc_parser->trackdata[i].datasource[j].offset;
					length -= toc_parser->trackdata[i].datasource[j].offset;
				}

				lengthsectors = (length + 2352 - 1) / 2352; /* round up to nearest sector */
				cdfs_disc_append_datasource (retval,
				                             trackoffset + tracklength,                       /* medium sector offset */
				                             lengthsectors,                                   /* medium sector count */
				                             fd,                                              /* source file-descriptor */
				                             toc_parser->trackdata[i].datasource[j].filename, /* source file-name */
				                             FORMAT_AUDIO___NONE,                             /* source sector encoding */
				                             offset,                                          /* source byte offset */
				                             length);                                         /* source byte length */
				tracklength += lengthsectors;
			} else {
				int fd = -1;
				uint64_t offset = 0;
				uint64_t length = 0;
				uint32_t lengthsectors;
				int ss;

				if (data_openfile (argv1_path, toc_parser->trackdata[i].datasource[j].filename, &fd, &length))
				{
					fprintf (stderr, "Failed to open data file %s\n", toc_parser->trackdata[i].datasource[j].filename);
					goto fail_out;
				}

				if (toc_parser->trackdata[i].datasource[j].offset>=0)
				{
					if (toc_parser->trackdata[i].datasource[j].offset >= length)
					{
						fprintf (stderr, "Data file shorter than offset in .toc file\n");
						close (fd);
						goto fail_out;
					}
					offset += toc_parser->trackdata[i].datasource[j].offset;
					length -= toc_parser->trackdata[i].datasource[j].offset;
				}

				ss = medium_sector_size (toc_parser->trackdata[i].storage_mode,
				                         toc_parser->trackdata[i].storage_mode_subchannel);

				lengthsectors = (length + ss - 1) / ss; /* round up to nearest sector */
				cdfs_disc_append_datasource (retval,
				                             trackoffset + tracklength,                       /* medium sector offset */
				                             lengthsectors,                                   /* medium sector count */
				                             fd,                                              /* source file-descriptor */
				                             toc_parser->trackdata[i].datasource[j].filename, /* source file-name */
				                             toc_storage_mode_to_cdfs_format (toc_parser->trackdata[i].storage_mode,
				                                                              toc_parser->trackdata[i].storage_mode_subchannel,
				                                                              toc_parser->trackdata[i].datasource[j].swap),
				                                                                              /* source sector encoding */
				                             offset,                                          /* source byte offset */
				                             length);                                         /* source byte length */
				tracklength += lengthsectors;
			}
		}

		cdfs_disc_track_append (retval,
		                        toc_parser->trackdata[i].pregap,
		                        trackoffset,
		                        tracklength,
		                        toc_parser->trackdata[i].title,
		                        toc_parser->trackdata[i].performer,
		                        toc_parser->trackdata[i].songwriter,
		                        toc_parser->trackdata[i].composer,
		                        toc_parser->trackdata[i].arranger,
		                        toc_parser->trackdata[i].message
		                       /* we ignore four_channel_audio - never used in real-life
				        *           [no] copy
		                        *           [no] pre-emphasis
		                        */
		                       );

		trackoffset += tracklength;
	}

	return retval;
fail_out:
	cdfs_disc_free (retval);
	return 0;
}

struct toc_parser_t *toc_parser_from_fd (int fd)
{
	char buffer[32 * 1024];
	int len;

	lseek (fd, 0, SEEK_SET);

	len = read(fd, buffer, sizeof (buffer) - 1);
	if (len < 0)
	{
		fprintf (stderr, "Failed to read(): %s\n", strerror (errno));
		return 0;
	}
	buffer[len] = 0;

	return toc_parser_from_data (buffer);
}

#if 0
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

struct toc_parser_t *toc_parser_from_filename (const char *filename)
{
	struct toc_parser_t *toc = 0;
	struct cdfs_disc_t *disc = 0;
	char buffer[32 * 1024];
	int fd;
	int len;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		fprintf (stderr, "Failed to open %s: %s\n", argv[1], strerror (errno));
		return -1;
	}

	len = read(fd, buffer, sizeof (buffer) - 1);
	if (len < 0)
	{
		fprintf (stderr, "Failed to read(): %s\n", strerror (errno));
		close (fd);
		return -1;
	}
	buffer[len] = 0;
	close (fd);

	return toc_parser_from_data (buffer);
}

int main (int argc, char *argv[])
{
	struct toc_parser_t *toc = 0;
	struct cdfs_disc_t *disc = 0;
	char buffer[32 * 1024];
	int fd;
	int len;

	if (argc != 2)
	{
		fprintf (stderr, "Usage: %s <file.toc>\n", argv[0]);
		return -1;
	}

	fd = open (argv[1], O_RDONLY);
	if (fd < 0)
	{
		fprintf (stderr, "Failed to open %s: %s\n", argv[1], strerror (errno));
		return -1;
	}

	len = read(fd, buffer, sizeof (buffer) - 1);
	if (len < 0)
	{
		fprintf (stderr, "Failed to read(): %s\n", strerror (errno));
		close (fd);
		return -1;
	}
	buffer[len] = 0;
	close (fd);

	toc = toc_parser (buffer);
	if (toc)
	{
		char *argv1_path = get_path (argv[1]);
		disc = toc_parser_to_cdfs_disc (argv1_path, toc);
		free (argv1_path);
		toc_parser_free (toc);
	}

	if (disc)
	{
		#warning TODO dump the disc....
		cdfs_disc_free (disc);
	}

	return 0;
}
#endif
