#define _POSIX_C_SOURCE 200112L // POSIX.1-2001

#include <stdio.h> /* IO */
#include <stdlib.h> /* rand */
#include <sys/types.h> /* ssize_t */
#include <stdint.h> /* other int types */
#include <ctype.h> /* tolower */
#include <string.h> /* memcpy */

#ifndef VERSION
#define VERSION "Something went wrong with VERSION"
#endif

#define BUFFER_SIZE BUFSIZ
/* This controls how often emojis are displayed. */
#define EMOJI_BALANCE 1
#define STUTTER_BALANCE 2
/* This is the max len of the emoji strings. */
#define EMOJI_MAX_LEN 20

static char *progname = NULL;
static uint8_t emojis[][EMOJI_MAX_LEN] = {
	"(ʘωʘ) ",
	"( °꒳° ) ",
	"(„ᵕᴗᵕ„) ",
	"(U ﹏ U) "
};
static size_t emojis_len = (sizeof(emojis)/sizeof(emojis[0]));

/* Custom faster isalpha. */
static inline int fast_isalpha(int c) {
    return ((unsigned int)(c | 32) - 97) < 26U;
}

int cnv_ascii(FILE *in, FILE *out);
/* Little endian version. */
int cnv_ascii_le(FILE *in, FILE *out);
/* Big endian version. */
int cnv_ascii_be(FILE *in, FILE *out);

int main(int argc, char *argv[]){
	// FILE *in = fopen("./tests/test_10k.txt", "r");
	FILE *in = stdin;
	FILE *out = stdout;

	progname = argv[0];
	fprintf(stderr, "%s: Version: %s\n", progname, VERSION);

	return cnv_ascii(in, out);
}

int cnv_ascii(FILE *in, FILE *out) {
	uint_least16_t i = 1;
	char *c = (char *)&i;
	if (*c) {
		fprintf(stderr, "%s: Little endian\n", progname);
		return cnv_ascii_le(in, out);
	} else {
		fprintf(stderr, "%s: Big endian\n", progname);
		return cnv_ascii_be(in, out);
	}
	return 0;
}

#define WBUFFER_WRITE(_ADDED_CHAR) { \
	if (write_buffer_free >= buffer_size) { \
		fwrite(write_buffer, buffer_size, 1, out); \
		write_buffer_free = 0; \
	} \
	write_buffer[write_buffer_free++] = _ADDED_CHAR; \
}
#define LCCC_ACCESS(_lccc, _i) (((uint8_t *)&_lccc)[_i])

/* Little endian:
 * [0x1, 0x2] -> 0x0201
 */
int cnv_ascii_le(FILE *in, FILE *out) {
	/* Buffers */
	const size_t buffer_size = BUFFER_SIZE;
	uint8_t read_buffer[BUFFER_SIZE];
	int _padding = 0; /* We need this because we will underflow the write_buffer */
	uint8_t write_buffer[BUFFER_SIZE];
	size_t write_buffer_free = 0;
	/* Generic variables */
	ssize_t bytes_read = 0;
	size_t n = 0, m = 0;
	//uint_fast8_t cc = 0, lc = 0; /* Current char, last char. */
	uint16_t lccc = 0x0; /* cc and lc combined. */
	uint32_t emoji_rand = 0;
	uint8_t *emoji_s = NULL;
	/* State counters */
	uint32_t sentence_state = 1; /* 0 - at start of sentence, 1 - normal */
	uint32_t stutter_state = 1; /* 0 - stutter, 0+ - don't */
	uint32_t emoji_state = 1; /* 0 - draw random emoji, 0+ - don't */

	/* Master loop, loop through buffers. */
	while ((bytes_read = fread(read_buffer, 1, buffer_size, in)) > 0) {
		/* Loop through charectors. */
		for (n = 0; n < bytes_read; ++n) {
			LCCC_ACCESS(lccc, 1) = read_buffer[n];
			/* Is char from alphabet */
			if (fast_isalpha(LCCC_ACCESS(lccc, 1))) {
				/* Everything must be lowercase. */
				LCCC_ACCESS(lccc, 1) = tolower(LCCC_ACCESS(lccc, 1));
				/* Remove double 'l','r' and convert into 'w'. BE & LE */
				if (lccc == 0x6C6C || lccc == 0x7272) {
					LCCC_ACCESS(lccc, 1) = '\0';
				} else if (LCCC_ACCESS(lccc, 1) == 'l' || LCCC_ACCESS(lccc, 1) == 'r') {
					LCCC_ACCESS(lccc, 1) = 'w';
				}
				/* Add 'y' in between 'n' and 'a'. LE */
				if (lccc == 0x616e) {
					WBUFFER_WRITE('y');
				}
			}
			/* Sentence state. */
			if (sentence_state == 1) {
				/* Check if cc == ' ' and lc = '!' or '.'. LE */
				if (lccc == 0x202e || lccc == 0x2120) {
					sentence_state = 0;
				}
			} else {
				/* Draw emoji if state says so. */
				if (emoji_state == 0) {
					/* Get random emoji. */
					emoji_rand = rand() % emojis_len;
					emoji_s = emojis[emoji_rand];
					/* Write and flush write buffer. */
					do {
						WBUFFER_WRITE(*(emoji_s++));
					} while (*emoji_s != 0);
					emoji_state = 1;
				} else {
					if (emoji_state++ >= EMOJI_BALANCE) {
						emoji_state = 0;
					}
				}
				if (fast_isalpha(LCCC_ACCESS(lccc, 1))) {
					/* Stutter logic. */
					if (stutter_state == 0) {
						WBUFFER_WRITE(LCCC_ACCESS(lccc, 1));
						WBUFFER_WRITE('-');
						stutter_state = 1;
					} else {
						if (stutter_state++ >= STUTTER_BALANCE) {
							stutter_state = 0;
						}
					}
				}
				sentence_state = 1;
			}
			/* Write current char if it isn't `NULL`. */
			LCCC_ACCESS(lccc, 0) = read_buffer[n];
			if (LCCC_ACCESS(lccc, 1) != '\0') {
				WBUFFER_WRITE(LCCC_ACCESS(lccc, 1));
			}
		}
	}
	if (write_buffer_free > 0) {
		fwrite(write_buffer, write_buffer_free, 1, out);
		write_buffer_free = 0;
	}

	return 0;
}

int cnv_ascii_be(FILE *in, FILE *out) {
	/* Buffers */
	const size_t buffer_size = BUFFER_SIZE;
	uint8_t read_buffer[BUFFER_SIZE];
	int _padding = 0; /* We need this because we will underflow the write_buffer */
	uint8_t write_buffer[BUFFER_SIZE];
	size_t write_buffer_free = 0;
	/* Generic variables */
	ssize_t bytes_read = 0;
	size_t n = 0, m = 0;
	//uint_fast8_t cc = 0, lc = 0; /* Current char, last char. */
	uint16_t lccc = 0x0; /* cc and lc combined. */
	uint32_t emoji_rand = 0;
	uint8_t *emoji_s = NULL;
	/* State counters */
	uint32_t sentence_state = 1; /* 0 - at start of sentence, 1 - normal */
	uint32_t stutter_state = 1; /* 0 - stutter, 0+ - don't */
	uint32_t emoji_state = 1; /* 0 - draw random emoji, 0+ - don't */

	/* Master loop, loop through buffers. */
	while ((bytes_read = fread(read_buffer, 1, buffer_size, in)) > 0) {
		/* Loop through charectors. */
		for (n = 0; n < bytes_read; ++n) {
			LCCC_ACCESS(lccc, 1) = read_buffer[n];
			/* Is char from alphabet */
			if (fast_isalpha(LCCC_ACCESS(lccc, 1))) {
				/* Everything must be lowercase. */
				LCCC_ACCESS(lccc, 1) = tolower(LCCC_ACCESS(lccc, 1));
				/* Remove double 'l','r' and convert into 'w'. BE & LE */
				if (lccc == 0x6C6C || lccc == 0x7272) {
					LCCC_ACCESS(lccc, 1) = '\0';
				} else if (LCCC_ACCESS(lccc, 1) == 'l' || LCCC_ACCESS(lccc, 1) == 'r') {
					LCCC_ACCESS(lccc, 1) = 'w';
				}
				/* Add 'y' in between 'n' and 'a'. BE */
				if (lccc == 0x6e61) {
					WBUFFER_WRITE('y');
				}
			}
			/* Sentence state. */
			if (sentence_state == 1) {
				/* Check if cc == ' ' and lc = '!' or '.'. LE */
				if (lccc == 0x2e20 || lccc == 0x2021) {
					sentence_state = 0;
				}
			} else {
				/* Draw emoji if state says so. */
				if (emoji_state == 0) {
					/* Get random emoji. */
					emoji_rand = rand() % emojis_len;
					emoji_s = emojis[emoji_rand];
					/* Write and flush write buffer. */
					do {
						WBUFFER_WRITE(*(emoji_s++));
					} while (*emoji_s != 0);
					emoji_state = 1;
				} else {
					if (emoji_state++ >= EMOJI_BALANCE) {
						emoji_state = 0;
					}
				}
				if (fast_isalpha(LCCC_ACCESS(lccc, 1))) {
					/* Stutter logic. */
					if (stutter_state == 0) {
						WBUFFER_WRITE(LCCC_ACCESS(lccc, 1));
						WBUFFER_WRITE('-');
						stutter_state = 1;
					} else {
						if (stutter_state++ >= STUTTER_BALANCE) {
							stutter_state = 0;
						}
					}
				}
				sentence_state = 1;
			}
			/* Write current char if it isn't `NULL`. */
			LCCC_ACCESS(lccc, 0) = read_buffer[n];
			if (LCCC_ACCESS(lccc, 1) != '\0') {
				WBUFFER_WRITE(LCCC_ACCESS(lccc, 1));
			}
		}
	}
	if (write_buffer_free > 0) {
		fwrite(write_buffer, write_buffer_free, 1, out);
		write_buffer_free = 0;
	}

	return 0;
	return 0;
}
