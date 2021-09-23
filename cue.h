#ifndef _CUE_H
#define _CUE_H 1

struct cue_parser_t;
struct cdfs_disc_t;

void cue_parser_free (struct cue_parser_t *cue_parser);

struct cue_parser_t *cue_parser_from_data (const char *input);

struct cue_parser_t *cue_parser_from_fd (int fd);

struct cdfs_disc_t *cue_parser_to_cdfs_disc (const char *argv1_path, struct cue_parser_t *cue_parser);

#if 0

struct cue_parser_t *cue_parser_from_filename (const char *filename);

#endif

#endif
