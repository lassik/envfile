/*
 * Copyright (c) 2018 Lassi Kortela
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

static char buf[4096];
static char *buflim;
static char *pos;
static char *envfilename;
static FILE *envinput;
static int initflag;
static int verbosity;

static void
usage(void)
{
	fprintf(stderr, "usage: envfile env.ini prog\n");
	exit(1);
}

static int
is_horz_white(int ch)
{
	return ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f';
}

static int
is_vert_white(int ch)
{
	return ch == '\n' || ch == '\r' || ch == '\0';
}

static int
is_white(int ch)
{
	return is_horz_white(ch) || is_vert_white(ch);
}

static int
is_horz(int ch)
{
	return !is_vert_white(ch);
}

static int
is_env_name(int ch)
{
	return isalnum(ch) || ch == '_';
}

static int
is_env_value(int ch)
{
	return is_horz(ch) && ch != '#';
}

static int
scan_end(void)
{
	return pos >= buflim;
}

static void
scan_while(int (*predicate)(int))
{
	while (!scan_end()) {
		if (!predicate(*pos))
			break;
		pos++;
	}
}

static void
skip_line(void)
{
	scan_while(is_horz);
}

static void
parsed(const char *name, const char *value)
{
	if (verbosity >= 2) {
		fprintf(stderr, "%s=%s\n", name, value);
	} else if (verbosity >= 1) {
		fprintf(stderr, "%s\n", name);
	}
	if (setenv(name, value, 1) == -1) {
		fprintf(stderr, "cannot set env\n");
		exit(1);
	}
}

static void
parse(void)
{
	for (;;) {
		scan_while(is_white);
		if (scan_end()) {
			break;
		}
		if (*pos == '#') {
			skip_line();
			continue;
		}
		char *name = pos;
		scan_while(is_env_name);
		char *name_lim = pos;
		scan_while(is_horz_white);
		if (*pos != '=') {
			skip_line();
			continue;
		}
		pos++;
		scan_while(is_horz_white);
		char *value = pos;
		scan_while(is_env_value);
		while (is_horz_white(pos[-1]))
			pos--;
		char *value_lim = pos;
		skip_line();
		*name_lim = 0;
		*value_lim = 0;
		parsed(name, value);
	}
}

int
main(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, "iv")) != -1) {
		switch (ch) {
		case 'i':
			initflag = 1;
			break;
		case 'v':
			verbosity++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (!*argv) {
		usage();
	}
	envfilename = *argv++;
	if (!strcmp(envfilename, "-")) {
		envinput = stdin;
	} else if (!(envinput = fopen(envfilename, "rb"))) {
		fprintf(stderr, "cannot open env file\n");
		exit(1);
	}
	if (initflag) {
		if (!(environ = calloc(1, sizeof(*environ)))) {
			fprintf(stderr, "out of memory\n");
			exit(1);
		}
	}
	size_t len = fread(buf, 1, sizeof(buf), envinput);
	if (ferror(envinput)) {
		fprintf(stderr, "read error");
	}
	if (!feof(envinput)) {
		fprintf(stderr, "too long");
	}
	pos = buf;
	buflim = buf + len;
	parse();
	if (*argv) {
		execvp(*argv, argv);
		fprintf(stderr, "cannot run %s: %s\n", *argv, strerror(errno));
	}
	return 0;
}
