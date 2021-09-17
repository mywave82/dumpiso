#ifndef _TOC_H
#define _TOC_H 1

struct toc_parser_t;
struct cdfs_disc_t;

void toc_parser_free (struct toc_parser_t *toc_parser);

struct toc_parser_t *toc_parser_from_data (const char *input /* null terminated */);

struct toc_parser_t *toc_parser_from_fd (int fd);

struct cdfs_disc_t *toc_parser_to_cdfs_disc (const char *argv1_path, struct toc_parser_t *toc_parser);

#if 0

struct toc_parser_t *toc_parser_from_filename (const char *filename);

#endif

#endif
