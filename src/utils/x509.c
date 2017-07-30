/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "asinine/x509.h"

int
dump_certificates(const uint8_t *contents, size_t length) {
	x509_cert_t *cert = calloc(1, sizeof(x509_cert_t));
	if (cert == NULL) {
		fprintf(stderr, "Failed to allocate memory\n");
		return 0;
	}

	int res = 0;
	char buf[256];

	asn1_parser_t parser;
	asn1_init(&parser, contents, length);

	while (!asn1_end(&parser)) {
		asinine_err_t err;
		if ((err = x509_parse(&parser, cert)) != ASININE_OK) {
			fprintf(stderr, "Invalid certificate: %s\n", asinine_strerror(err));
			res = 1;
			goto exit;
		}

		printf("---\n");
		printf("Version: %d, Algo: %d\n", cert->version, cert->algorithm);

		if (asn1_time_to_string(buf, sizeof(buf), &cert->valid_from) >=
		    sizeof(buf)) {
			fprintf(stderr, "Couldn't format time\n");
			res = 1;
			goto exit;
		}
		printf("Valid from: %s", buf);

		if (asn1_time_to_string(buf, sizeof(buf), &cert->valid_to) >=
		    sizeof(buf)) {
			fprintf(stderr, "Couldn't format time\n");
			res = 1;
			goto exit;
		}
		printf(", to: %s\n", buf);

		printf("Issuer:\n");
		for (size_t i = 0; i < cert->issuer.num; i++) {
			const x509_rdn_t *rdn = &cert->issuer.rdns[i];

			if (asn1_oid_to_string(buf, sizeof(buf), &rdn->oid) >=
			    sizeof(buf)) {
				printf("  %s...: ", buf);
			} else {
				printf("  %s: ", buf);
			}

			if ((err = asn1_string(&rdn->value, buf, sizeof(buf))) !=
			    ASININE_OK) {
				printf("%s\n", asinine_strerror(err));
			} else {
				printf("%s\n", buf);
			}
		}
	}

exit:
	free(cert);
	return res;
}

static uint8_t *load(int fd, size_t *length);

int
main(int argc, const char *argv[]) {
	if (argc < 2) {
		printf("%s [<file>|-]\n", argv[0]);
		return 1;
	}

	int fd = STDIN_FILENO;
	if (strcmp(argv[1], "-") != 0) {
		fd = open(argv[1], O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "Could not open source\n");
			return 1;
		}
	}

	size_t length;
	uint8_t *contents = load(fd, &length);
	close(fd);

	if (contents == NULL) {
		return 1;
	}

	int res = dump_certificates(contents, length);
	free(contents);
	return res;
}

static uint8_t *
load(int fd, size_t *length) {
#define BUF_SIZE (1024 * 1024)
	uint8_t *contents = calloc(1, BUF_SIZE);
	if (contents == NULL) {
		printf("Could not allocate memory\n");
		return NULL;
	}

	*length = 0;
	while (*length < BUF_SIZE) {
		ssize_t n = read(fd, contents + *length, BUF_SIZE - *length);
		if (n == 0) {
			return contents;
		} else if (n < 0) {
			perror("Could not read full file");
			return NULL;
		}
		*length += (size_t)n;
	}

	fprintf(stderr, "Input too large\n");
	free(contents);
	return NULL;
#undef BUF_SIZE
}