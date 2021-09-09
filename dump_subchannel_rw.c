#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int sector = 0;
	int fd;

	if (argc != 2)
	{
		fprintf (stderr, "Usage: %s <.bin file>\n", argv[0]);
		fprintf (stderr, " .bin file much be in full sector format with rw subchannel: cdrdao --dump_raw ----read-subchan rw_raw\n");
		return 1;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0)
	{
		fprintf (stderr, "Failed to open file\n");
		return 1;
	}

	while (1)
	{
		unsigned char buffer[96];
		unsigned char P[12];
		unsigned char Q[12];
		unsigned char R[12];
		unsigned char S[12];
		unsigned char T[12];
		unsigned char U[12];
		unsigned char V[12];
		unsigned char W[12];

		int i;

		printf ("%08d:", sector);
		if (lseek (fd, sector*(2352+96) + 2352, SEEK_SET) == (off_t) -1)
		{
			break;
		}
		if (read (fd, buffer, 96) != 96)
		{
			break;
		}
		
		for (i=0; i < 12; i++)
		{
			P[i] = ((buffer[i*8+0] & 0x80)?0x80:0x00) |
			       ((buffer[i*8+1] & 0x80)?0x40:0x00) |
			       ((buffer[i*8+2] & 0x80)?0x20:0x00) |
			       ((buffer[i*8+3] & 0x80)?0x10:0x00) |
			       ((buffer[i*8+4] & 0x80)?0x08:0x00) |
			       ((buffer[i*8+5] & 0x80)?0x04:0x00) |
			       ((buffer[i*8+6] & 0x80)?0x02:0x00) |
			       ((buffer[i*8+7] & 0x80)?0x01:0x00);

			Q[i] = ((buffer[i*8+0] & 0x40)?0x80:0x00) |
			       ((buffer[i*8+1] & 0x40)?0x40:0x00) |
			       ((buffer[i*8+2] & 0x40)?0x20:0x00) |
			       ((buffer[i*8+3] & 0x40)?0x10:0x00) |
			       ((buffer[i*8+4] & 0x40)?0x08:0x00) |
			       ((buffer[i*8+5] & 0x40)?0x04:0x00) |
			       ((buffer[i*8+6] & 0x40)?0x02:0x00) |
			       ((buffer[i*8+7] & 0x40)?0x01:0x00);

			R[i] = ((buffer[i*8+0] & 0x20)?0x80:0x00) |
			       ((buffer[i*8+1] & 0x20)?0x40:0x00) |
			       ((buffer[i*8+2] & 0x20)?0x20:0x00) |
			       ((buffer[i*8+3] & 0x20)?0x10:0x00) |
			       ((buffer[i*8+4] & 0x20)?0x08:0x00) |
			       ((buffer[i*8+5] & 0x20)?0x04:0x00) |
			       ((buffer[i*8+6] & 0x20)?0x02:0x00) |
			       ((buffer[i*8+7] & 0x20)?0x01:0x00);

			S[i] = ((buffer[i*8+0] & 0x10)?0x80:0x00) |
			       ((buffer[i*8+1] & 0x10)?0x40:0x00) |
			       ((buffer[i*8+2] & 0x10)?0x20:0x00) |
			       ((buffer[i*8+3] & 0x10)?0x10:0x00) |
			       ((buffer[i*8+4] & 0x10)?0x08:0x00) |
			       ((buffer[i*8+5] & 0x10)?0x04:0x00) |
			       ((buffer[i*8+6] & 0x10)?0x02:0x00) |
			       ((buffer[i*8+7] & 0x10)?0x01:0x00);

			T[i] = ((buffer[i*8+0] & 0x08)?0x80:0x00) |
			       ((buffer[i*8+1] & 0x08)?0x40:0x00) |
			       ((buffer[i*8+2] & 0x08)?0x20:0x00) |
			       ((buffer[i*8+3] & 0x08)?0x10:0x00) |
			       ((buffer[i*8+4] & 0x08)?0x08:0x00) |
			       ((buffer[i*8+5] & 0x08)?0x04:0x00) |
			       ((buffer[i*8+6] & 0x08)?0x02:0x00) |
			       ((buffer[i*8+7] & 0x08)?0x01:0x00);

			U[i] = ((buffer[i*8+0] & 0x04)?0x80:0x00) |
			       ((buffer[i*8+1] & 0x04)?0x40:0x00) |
			       ((buffer[i*8+2] & 0x04)?0x20:0x00) |
			       ((buffer[i*8+3] & 0x04)?0x10:0x00) |
			       ((buffer[i*8+4] & 0x04)?0x08:0x00) |
			       ((buffer[i*8+5] & 0x04)?0x04:0x00) |
			       ((buffer[i*8+6] & 0x04)?0x02:0x00) |
			       ((buffer[i*8+7] & 0x04)?0x01:0x00);

			V[i] = ((buffer[i*8+0] & 0x02)?0x80:0x00) |
			       ((buffer[i*8+1] & 0x02)?0x40:0x00) |
			       ((buffer[i*8+2] & 0x02)?0x20:0x00) |
			       ((buffer[i*8+3] & 0x02)?0x10:0x00) |
			       ((buffer[i*8+4] & 0x02)?0x08:0x00) |
			       ((buffer[i*8+5] & 0x02)?0x04:0x00) |
			       ((buffer[i*8+6] & 0x02)?0x02:0x00) |
			       ((buffer[i*8+7] & 0x02)?0x01:0x00);

			W[i] = ((buffer[i*8+0] & 0x01)?0x80:0x00) |
			       ((buffer[i*8+1] & 0x01)?0x40:0x00) |
			       ((buffer[i*8+2] & 0x01)?0x20:0x00) |
			       ((buffer[i*8+3] & 0x01)?0x10:0x00) |
			       ((buffer[i*8+4] & 0x01)?0x08:0x00) |
			       ((buffer[i*8+5] & 0x01)?0x04:0x00) |
			       ((buffer[i*8+6] & 0x01)?0x02:0x00) |
			       ((buffer[i*8+7] & 0x01)?0x01:0x00);
		}

		printf("P=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ", P[0], P[1], P[2], P[3], P[4], P[5], P[6], P[7], P[8], P[9], P[10], P[11]);
		printf("Q=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ", Q[0], Q[1], Q[2], Q[3], Q[4], Q[5], Q[6], Q[7], Q[8], Q[9], Q[10], Q[11]);
		printf("R=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ", R[0], R[1], R[2], R[3], R[4], R[5], R[6], R[7], R[8], R[9], R[10], R[11]);
		printf("S=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ", S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], S[8], S[9], S[10], S[11]);
		printf("T=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ", T[0], T[1], T[2], T[3], T[4], T[5], T[6], T[7], T[8], T[9], T[10], T[11]);
		printf("U=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ", U[0], U[1], U[2], U[3], U[4], U[5], U[6], U[7], U[8], U[9], U[10], U[11]);
		printf("V=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ", V[0], V[1], V[2], V[3], V[4], V[5], V[6], V[7], V[8], V[9], V[10], V[11]);
		printf("W=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ", W[0], W[1], W[2], W[3], W[4], W[5], W[6], W[7], W[8], W[9], W[10], W[11]);

		switch (Q[0] & 0xc0)
		{
			case 0x00: printf ("2-CH_CDDA NO_PRE-EMPH "); break;
			case 0x10: printf ("2-CH_CDDA PRE-EMPH "); break;
			case 0x80: printf ("4-CH_CDDA NO_PRE-EMPH "); break;
			case 0x90: printf ("4-CH_CDDA PRE-EMPH "); break;
			case 0x40: printf ("Data, recorded uninterrupted "); break;
			case 0x50: printf ("Data, recorded incremental "); break;
			default: printf ("Reserved Q-mode"); break;
		}
		if (Q[0] & 0x20) printf ("COPY "); else printf ("NO_COPY ");

		switch (Q[0] & 0x0f)
		{
			case 1: /* we assume we are not in the lead-in area, bytes changes meaning there */
				printf ("TIMING ");
				printf ("Track 0x%02x Index 0x%02x %02x:%02x.%02x    Absolute %02x:%02x.%02x",
					Q[1], Q[2],
					Q[3], Q[4], Q[5],
					/* Q[6] should be fixed ZERO */
					Q[7], Q[8], Q[9]);
				break; /* or TOC if in the lead-in area */
			case 2: printf ("MCN "); break;
			case 3: printf ("ISRC "); break;
		}

		putchar ('\n');	
		sector++;
	}
	printf("EOF\n");
	close (fd);
	return 0;
}
