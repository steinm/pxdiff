#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <libintl.h>
#include <sys/types.h>
#include <regex.h>
#include <libgen.h>
#include <paradox.h>
#include "config.h"

#define _(String) gettext(String)
#define max(a,b) ((a)>(b) ? (a) : (b))
#define min(a,b) ((a)<(b) ? (a) : (b))

/* strrep() {{{
 * Replace occurences of character c1 by c2.
 */
void strrep(char *str, char c1, char c2) {
	char *ptr = str;

	while(*ptr != '\0') {
		if(*ptr == c1)
			*ptr = c2;
		ptr++;
	}
}
/* }}} */

/* sort() {{{
 */
int sort(void **data, int len, int (datacmp)(void *data1, void *data2, size_t n)) {
	int finished = 0;
	int i, j = 1;
	void *tmp;
	while(!finished) {
		finished = 1;
		for(i=0; i<len-j; i++) {
			if(datacmp(data[i], data[i+1], 0) > 1) {
				tmp = data[i];
				data[i] = data[i+1];
				data[i+1] = tmp;
				finished = 0;
			}
		}
		j++;
	}
}
/* }}} */

/* fieldcmp() {{{
 * Compares two records. Returns -1, 0, or 1 respectively
 * to be data1 found less, equal, or greater than data2.
 */
int recordcmp(void *data1, void *data2, size_t n) {
	int d;
	return(memcmp(data1, data2, n));
}
/* }}} */
 
/* fieldcmp() {{{
 * Compares two field specifications. Returns -1, 0, or 1 respectively
 * to be data1 found less, equal, or greater than data2.
 */
int fieldcmp(void *data1, void *data2, size_t n) {
	pxfield_t *pxf1 = data1;
	pxfield_t *pxf2 = data2;
	int d;
	d = strcasecmp(pxf1->px_fname, pxf2->px_fname);
	if(d != 0) return d;
	if(pxf1->px_ftype < pxf2->px_ftype) return -1;
	if(pxf1->px_ftype > pxf2->px_ftype) return 1;
	if(pxf1->px_flen < pxf2->px_flen) return -1;
	if(pxf1->px_flen > pxf2->px_flen) return 1;
	return 0;
}
/* }}} */

/* lcs_length() {{{
 * Calculates the length of longest common sequence of two data fields. Each
 * data field is list of pointers to the data. The data comparision function
 * datacmp() must take two pointers to the data and size of the data. It
 * returns -1, 0, or 1 respectively to data1 being less, equal or greater the
 * data2. The parameter l has to been array of num1 pointers to strings
 * of num2 length.
 */
int lcs_length(int **l, void **data1, int num1, void **data2, int num2, int
(datacmp)(void *data1, void *data2, size_t n), size_t n) {
	int i, j;
	for(i=num1; i>=0; i--) {
		for(j=num2; j>=0; j--) {
			/* FIXME: l[num1][0-num2] and l[1-num1][num2] should be filled
			 * with 0 before. 
			 */
			if(i==num1 || j==num2) l[i][j] = 0;
			else if(!datacmp(data1[i], data2[j], n)) l[i][j] = 1 + l[i+1][j+1];
			else l[i][j] = max(l[i+1][j], l[i][j+1]);
		}
	}
	return l[0][0];
}
/* }}} */

/* lcs_sequence() {{{
 * Returns the actual sequence by traversing the matrix l calculated in
 * lcs_length(). lcs is an array of len pointers where len is the return
 * value of lcs_length(). All other parameters are identical to lcs_length().
 * lcs contains pointers to data records in the first database.
 */
int lcs_sequence(int **l, void **data1, int num1, void **data2, int num2, int (datacmp)(void *data1, void *data2, size_t n), size_t n, void **lcs) {
	int i, j, k;
	i = 0; j = 0; k = 0;
	while(i < num1 && j < num2) {
		if(!datacmp(data1[i], data2[j], n)) {
			lcs[k++] = data1[i];
			i++; j++;
		} else if(l[i+1][j] >= l[i][j+1]) i++;
		else j++;
	}
	return k;
}
/* }}} */

/* lcs_output_matrix() {{{
 * Outputs the matrix l as calculated by lcs_length().
 */
void lcs_output_matrix(int **l, int num1, int num2) {
	int i, j;
	for(i=0; i<=num1; i++) {
		fprintf(stdout, "%d\t", i);
			for(j=0; j<=num2; j++) {
				fprintf(stdout, "%d\t", l[i][j]);
			}
			fprintf(stdout, "\n");
		}
}
/* }}} */

/* show_field() {{{
 * Output the field specification
 */
void show_field(FILE *outfp, pxfield_t *pxf) {
	fprintf(outfp, "%18s | ", pxf->px_fname);
	switch(pxf->px_ftype) {
		case pxfAlpha:
			fprintf(outfp, "char(%d)\n", pxf->px_flen);
			break;
		case pxfDate:
			fprintf(outfp, "date(%d)\n", pxf->px_flen);
			break;
		case pxfShort:
			fprintf(outfp, "int(%d)\n", pxf->px_flen);
			break;
		case pxfLong:
			fprintf(outfp, "int(%d)\n", pxf->px_flen);
			break;
		case pxfCurrency:
			fprintf(outfp, "currency(%d)\n", pxf->px_flen);
			break;
		case pxfNumber:
			fprintf(outfp, "double(%d)\n", pxf->px_flen);
			break;
		case pxfLogical:
			fprintf(outfp, "boolean(%d)\n", pxf->px_flen);
			break;
		case pxfMemoBLOb:
			fprintf(outfp, "blob(%d)\n", pxf->px_flen);
			break;
		case pxfBLOb:
			fprintf(outfp, "blob(%d)\n", pxf->px_flen);
			break;
		case pxfFmtMemoBLOb:
			fprintf(outfp, "blob(%d)\n", pxf->px_flen);
			break;
		case pxfOLE:
			fprintf(outfp, "ole(%d)\n", pxf->px_flen);
			break;
		case pxfGraphic:
			fprintf(outfp, "graphic(%d)\n", pxf->px_flen);
			break;
		case pxfTime:
			fprintf(outfp, "time(%d)\n", pxf->px_flen);
			break;
		case pxfTimestamp:
			fprintf(outfp, "timestamp(%d)\n", pxf->px_flen);
			break;
		case pxfAutoInc:
			fprintf(outfp, "autoinc(%d)\n", pxf->px_flen);
			break;
		case pxfBCD:
			fprintf(outfp, "decimal(17,%d)\n", pxf->px_flen);
			break;
		case pxfBytes:
			fprintf(outfp, "bytes(%d)\n", pxf->px_flen);
			break;
		default:
			fprintf(outfp, "%c(%d)\n", pxf->px_ftype, pxf->px_flen);
	}
}
/* }}} */

/* show_record() {{{
 * Outputs a record as csv
 */
void show_record(FILE *outfp, pxdoc_t *pxdoc, pxhead_t *pxh, char *data) {
	pxfield_t *pxf;
	int offset, first, i;
	char delimiter = '\t';
	char enclosure = '\"';
	pxf = pxh->px_fields;
	offset = 0;
	first = 0;  // set to 1 when first field has been output
	for(i=0; i<pxh->px_numfields; i++) {
//		if(fieldregex == NULL || selectedfields[i]) {
			if(first == 1)
				fprintf(outfp, "%c", delimiter);
			switch(pxf->px_ftype) {
				case pxfAlpha: {
					char *value;
					if(PX_get_data_alpha(pxdoc, &data[offset], pxf->px_flen, &value)) {
						if(enclosure && strchr(value, delimiter))
							fprintf(outfp, "%c%s%c", enclosure, value, enclosure);
						else
							fprintf(outfp, "%s", value);
					}
					first = 1;
					break;
				}
				case pxfDate: {
					long value;
					int year, month, day;
					if(PX_get_data_long(pxdoc, &data[offset], pxf->px_flen, &value)) {
						PX_SdnToGregorian(value+1721425, &year, &month, &day);
						fprintf(outfp, "%02d.%02d.%04d", day, month, year);
					}
					first = 1;
					break;
					}
				case pxfShort: {
					short int value;
					if(PX_get_data_short(pxdoc, &data[offset], pxf->px_flen, &value)) {
						fprintf(outfp, "%d", value);
					}
					first = 1;
					break;
					}
				case pxfAutoInc:
				case pxfTimestamp:
				case pxfLong: {
					long value;
					if(PX_get_data_long(pxdoc, &data[offset], pxf->px_flen, &value)) {
						fprintf(outfp, "%ld", value);
					}
					first = 1;
					break;
					}
				case pxfTime: {
					long value;
					if(PX_get_data_long(pxdoc, &data[offset], pxf->px_flen, &value)) {
						fprintf(outfp, "'%02d:%02d:%02.3f'", value/3600000, value/60000%60, value%60000/1000.0);
					}
					first = 1;
					break;
					}
				case pxfCurrency:
				case pxfNumber: {
					double value;
					if(PX_get_data_double(pxdoc, &data[offset], pxf->px_flen, &value)) {
						fprintf(outfp, "%f", value);
					} 
					first = 1;
					break;
					} 
				case pxfLogical: {
					char value;
					if(PX_get_data_byte(pxdoc, &data[offset], pxf->px_flen, &value)) {
						if(value)
							fprintf(outfp, "1");
						else
							fprintf(outfp, "0");
					}
					first = 1;
					break;
					}
				case pxfGraphic:
				case pxfBLOb:
					fprintf(outfp, "offset=%ld ", get_long_le(&data[offset]) & 0xffffff00);
					fprintf(outfp, "size=%ld ", get_long_le(&data[offset+4]));
					fprintf(outfp, "mod_nr=%d ", get_short_le(&data[offset+8]));
					hex_dump(outfp, &data[offset], pxf->px_flen);
					first = 1;
					break;
				default:
					fprintf(outfp, "");
			}
//		}
		offset += pxf->px_flen;
		pxf++;
	}
	fprintf(outfp, "\n");
}
/* }}} */

/* show_record_diff() {{{
 * Outputs the difference of two records. The must have identical
 * size and structure.
 */
void show_record_diff(FILE *outfp, pxdoc_t *pxdoc1, pxhead_t *pxh1, char *data1, pxdoc_t *pxdoc2, pxhead_t *pxh2, char *data2) {
	pxfield_t *pxf;
	int offset, i;
	char delimiter = '\t';
	char enclosure = '\"';
	pxf = pxh1->px_fields; /* By definition the structure must be equal */
	offset = 0;
	for(i=0; i<pxh1->px_numfields; i++) {
//		if(fieldregex == NULL || selectedfields[i]) {
//			fprintf(outfp, "Comparing '%s'\n", pxf->px_fname);
			switch(pxf->px_ftype) {
				case pxfAlpha: {
					char *value1, *value2;
					if(PX_get_data_alpha(pxdoc1, &data1[offset], pxf->px_flen, &value1)) {
						if(PX_get_data_alpha(pxdoc2, &data2[offset], pxf->px_flen, &value2)) {
							if(strncmp(value1, value2, pxf->px_flen)) {
								fprintf(outfp, "%s%c", pxf->px_fname, delimiter);
								fprintf(outfp, "%s%c", value1, delimiter);
								fprintf(outfp, "%s\n", value2);
							}
						}
					}
					break;
				}
				case pxfDate: {
					long value1, value2;
					int year1, month1, day1;
					int year2, month2, day2;
					if(PX_get_data_long(pxdoc1, &data1[offset], pxf->px_flen, &value1)) {
						if(PX_get_data_long(pxdoc2, &data2[offset], pxf->px_flen, &value2)) {
							if(value1 != value2) {
								PX_SdnToGregorian(value1+1721425, &year1, &month1, &day1);
								PX_SdnToGregorian(value2+1721425, &year2, &month2, &day2);
								fprintf(outfp, "%s%c", pxf->px_fname, delimiter);
								fprintf(outfp, "%02d.%02d.%04d%c", day1, month1, year1, delimiter);
								fprintf(outfp, "%02d.%02d.%04d\n", day2, month2, year2);
							}
						}
					}
					break;
					}
				case pxfShort: {
					short int value1, value2;
					if(PX_get_data_short(pxdoc1, &data1[offset], pxf->px_flen, &value1)) {
						if(PX_get_data_short(pxdoc2, &data2[offset], pxf->px_flen, &value2)) {
							if(value1 != value2) {
								fprintf(outfp, "%s%c", pxf->px_fname, delimiter);
								fprintf(outfp, "%d%c", value1, delimiter);
								fprintf(outfp, "%d\n", value2);
							}
						}
					}
					break;
					}
				case pxfAutoInc:
				case pxfTimestamp:
				case pxfLong: {
					long value1, value2;
					if(PX_get_data_long(pxdoc1, &data1[offset], pxf->px_flen, &value1)) {
						if(PX_get_data_long(pxdoc2, &data2[offset], pxf->px_flen, &value2)) {
							if(value1 != value2) {
								fprintf(outfp, "%s%c", pxf->px_fname, delimiter);
								fprintf(outfp, "%ld%c", value1, delimiter);
								fprintf(outfp, "%ld\n", value2);
							}
						}
					}
					break;
					}
				case pxfTime: {
					long value1, value2;
					if(PX_get_data_long(pxdoc1, &data1[offset], pxf->px_flen, &value1)) {
						if(PX_get_data_long(pxdoc2, &data2[offset], pxf->px_flen, &value2)) {
							if(value1 != value2) {
								fprintf(outfp, "%s%c", pxf->px_fname, delimiter);
								fprintf(outfp, "'%02d:%02d:%02.3f'%c", value1/3600000, value1/60000%60, value1%60000/1000.0, delimiter);
								fprintf(outfp, "'%02d:%02d:%02.3f'\n", value2/3600000, value2/60000%60, value2%60000/1000.0);
							}
						}
					}
					break;
					}
				case pxfCurrency:
				case pxfNumber: {
					double value1, value2;
					if(PX_get_data_double(pxdoc1, &data1[offset], pxf->px_flen, &value1)) {
						if(PX_get_data_double(pxdoc2, &data2[offset], pxf->px_flen, &value2)) {
							if(value1 != value2) {
								fprintf(outfp, "%s%c", pxf->px_fname, delimiter);
								fprintf(outfp, "%f%c", value1, delimiter);
								fprintf(outfp, "%f\n", value2);
							}
						}
					} 
					break;
					} 
				case pxfLogical: {
					char value1, value2;
					if(PX_get_data_byte(pxdoc1, &data1[offset], pxf->px_flen, &value1)) {
						if(PX_get_data_byte(pxdoc2, &data2[offset], pxf->px_flen, &value2)) {
							if(value1 != value2) {
								fprintf(outfp, "%s%c", pxf->px_fname, delimiter);
								if(value1)
									fprintf(outfp, "1%c", delimiter);
								else
									fprintf(outfp, "0%c", delimiter);
								if(value2)
									fprintf(outfp, "1\n");
								else
									fprintf(outfp, "0\n");
							}
						}
					}
					break;
					}
				default:
					fprintf(outfp, "");
			}
//		}
		offset += pxf->px_flen;
		pxf++;
	}
	fprintf(outfp, "\n");
}
/* }}} */
/* errorhandler() {{{
 */
void errorhandler(pxdoc_t *p, int error, const char *str, void *data) {
	  fprintf(stderr, "pxdif: PXLib: %s\n", str);
}
/* }}} */

/* usage() {{{
 */
void usage(char *progname) {
	int recode;

	printf(_("Version: %s %s http://sourceforge.net/projects/pxlib"), progname, VERSION);
	printf("\n");
	printf(_("Copyright: Copyright (C) 2003 Uwe Steinmann <uwe@steinmann.cx>"));
	printf("\n\n");
	printf(_("%s compares two paradox databases."), progname);
	printf("\n\n");
	printf(_("Usage: %s [OPTIONS] FILE1 FILE2"), progname);
	printf("\n\n");
	printf(_("Options:"));
	printf("\n\n");
	printf(_("  -h, --help          this usage information."));
	printf("\n");
	printf(_("  -v, --verbose       be more verbose."));
	printf("\n");
	printf(_("  --mode=MODE         set output mode (csv, sql, or schema)."));
	printf("\n");
	printf(_("  -o, --output-file=FILE output data into file instead of stdout."));
	printf("\n");
	printf(_("  -n, --primary-index-file=FILE read primary index from file."));
	printf("\n");
	printf(_("  -r, --recode=ENCODING sets the target encoding."));
	printf("\n");
	printf(_("  --fields=REGEX      extended regular expression to select fields."));
	printf("\n");
#ifdef HAVE_GSF
	if(PX_has_gsf_support()) {
		printf(_("  --use-gsf           use gsf library to read input file."));
		printf("\n");
	}
#endif

	recode = PX_has_recode_support();
	switch(recode) {
		case 1:
			printf(_("libpx uses librecode for recoding."));
			break;
		case 2:
			printf(_("libpx uses iconv for recoding."));
			break;
		case 0:
			printf(_("libpx has no support for recoding."));
			break;
	}
	printf("\n\n");
	if(PX_is_bigendian())
		printf(_("libpx has been compiled for big endian architecture."));
	else
		printf(_("libpx has been compiled for little endian architecture."));
	printf("\n\n");
	printf(_("libpx has gsf support: %s"), PX_has_gsf_support() == 1 ? _("Yes") : _("No"));
	printf("\n\n");
	printf(_("libpx has version: %d.%d.%d"), PX_get_majorversion(), PX_get_minorversion(), PX_get_subminorversion());
	printf("\n\n");
}
/* }}} */

/* main() {{{
 */
int main(int argc, char *argv[]) {
	pxhead_t *pxh1, *pxh2;
	pxfield_t *pxf1, *pxf2;
	pxdoc_t *pxdoc1 = NULL, *pxdoc2 = NULL;
	pxdoc_t *pindexdoc1 = NULL, *pindexdoc2 = NULL;
	char *progname = NULL;
	char *selectedfields = NULL;
	int i1, i2, j, c; // general counters
	int first; // used to indicate if output has started or not
	int outputcsv = 0;
	int outputhtml = 0;
	int outputinfo = 0;
	int outputsql = 0;
	int comparedata = 0;
	int compareschema = 0;
	int outputdebug = 0;
	int schemasdiffer = 0;
	int usegsf = 0;
	int verbose = 0;
	char delimiter = '\t';
	char enclosure = '"';
	char *inputfile1 = NULL;
	char *inputfile2 = NULL;
	char *outputfile = NULL;
	char *pindexfile1 = NULL;
	char *pindexfile2 = NULL;
	char *fieldregex = NULL;
	char *targetencoding = NULL;
	FILE *outfp = NULL;

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	setlocale (LC_NUMERIC, "C");
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	textdomain (GETTEXT_PACKAGE);
#endif

	/* handling of program options {{{
	 */
	progname = basename(strdup(argv[0]));
	while(1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"info", 0, 0, 'i'},
			{"csv", 0, 0, 'c'},
			{"sql", 0, 0, 's'},
			{"html", 0, 0, 'x'},
			{"schema", 0, 0, 't'},
			{"data", 0, 0, 'd'},
			{"verbose", 0, 0, 'v'},
			{"recode", 1, 0, 'r'},
			{"output-file", 1, 0, 'o'},
			{"help", 0, 0, 'h'},
			{"fields", 1, 0, 'f'},
			{"mode", 1, 0, 4},
			{"use-gsf", 0, 0, 8},
			{"primary-index-file1", 1, 0, 'n'},
			{0, 0, 0, 0}
		};
		c = getopt_long (argc, argv, "icsxvtdf:r:o:n:h",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
			case 4:
				if(!strcmp(optarg, "csv")) {
					outputcsv = 1;
				} else if(!strcmp(optarg, "sql")) {
					outputsql = 1;
				} else if(!strcmp(optarg, "html")) {
					outputhtml = 1;
				} else if(!strcmp(optarg, "schema")) {
					compareschema = 1;
				} else if(!strcmp(optarg, "debug")) {
					outputdebug = 1;
				}
				break;
			case 8:
				usegsf = 1;
				break;
			case 'h':
				usage(progname);
				exit(0);
				break;
			case 'v':
				verbose = 1;
				break;
			case 't':
				compareschema = 1;
				break;
			case 'd':
				comparedata = 1;
				break;
			case 'r':
				targetencoding = strdup(optarg);
				break;
			case 'f':
				fieldregex = strdup(optarg);
				break;
			case 'o':
				outputfile = strdup(optarg);
				break;
			case 'i':
				outputinfo = 1;
				break;
			case 'c':
				outputcsv = 1;
				break;
			case 's':
				outputsql = 1;
				break;
			case 'x':
				outputhtml = 1;
				break;
			case 'n':
				pindexfile1 = strdup(optarg);
				break;
		}
	}


	if (optind < argc) {
		inputfile1 = strdup(argv[optind++]);
	}
	if (optind < argc) {
		inputfile2 = strdup(argv[optind]);
	}

	if(!inputfile1 || !inputfile2) {
		fprintf(stderr, _("You must at least specify the files to compare."));
		fprintf(stderr, "\n");
		fprintf(stderr, "\n");
		usage(progname);
		exit(1);
	}
	/* }}} */

	/* Open input and output files {{{
	 */
	if((outputfile == NULL) || !strcmp(outputfile, "-")) {
		outfp = stdout;
	} else {
		outfp = fopen(outputfile, "w");
		if(outfp == NULL) {
			fprintf(stderr, _("Could not open output file."));
			fprintf(stderr, "\n");
			exit(1);
		}
	}

	if(NULL == (pxdoc1 = PX_new2(errorhandler, NULL, NULL, NULL))) {
		fprintf(stderr, _("Could not create new paradox instance."));
		fprintf(stderr, "\n");
		exit(1);
	}

	if(NULL == (pxdoc2 = PX_new2(errorhandler, NULL, NULL, NULL))) {
		fprintf(stderr, _("Could not create new paradox instance."));
		fprintf(stderr, "\n");
		exit(1);
	}

#ifdef HAVE_GSF
	if(PX_has_gsf_support() && usegsf) {
		GsfInput *input = NULL;
		GsfInputStdio  *in_stdio;
		GsfInputMemory *in_mem;
		GError *gerr = NULL;
		fprintf(stderr, "Inputfile:  %s\n", inputfile);
		gsf_init ();
		in_mem = gsf_input_mmap_new (inputfile, NULL);
		if (in_mem == NULL) {
			in_stdio = gsf_input_stdio_new(inputfile, &gerr);
			if(in_stdio != NULL)
				input = GSF_INPUT (in_stdio);
			else {
				fprintf(stderr, _("Could not open gsf input file."));
				fprintf(stderr, "\n");
				g_object_unref (G_OBJECT (input));
				exit(1);
			}
		} else {
			input = GSF_INPUT (in_mem);
		}
		if(0 > PX_open_gsf(pxdoc, input)) {
			fprintf(stderr, _("Could not open input file."));
			fprintf(stderr, "\n");
			exit(1);
		}
	} else {
#endif
		if(0 > PX_open_file(pxdoc1, inputfile1)) {
			fprintf(stderr, _("Could not open first input file."));
			fprintf(stderr, "\n");
			exit(1);
		}
		if(0 > PX_open_file(pxdoc2, inputfile2)) {
			fprintf(stderr, _("Could not open second input file."));
			fprintf(stderr, "\n");
			exit(1);
		}
#ifdef HAVE_GSF
	}
#endif

	if(pindexfile1) {
		pindexdoc1 = PX_new2(errorhandler, NULL, NULL, NULL);
		if(0 > PX_open_file(pindexdoc1, pindexfile2)) {
			fprintf(stderr, _("Could not open primary index to first file."));
			fprintf(stderr, "\n");
			exit(1);
		}
		if(0 > PX_read_primary_index(pindexdoc1)) {
			fprintf(stderr, _("Could not read primary index to first file."));
			fprintf(stderr, "\n");
			exit(1);
		}
		if(0 > PX_add_primary_index(pxdoc1, pindexdoc1)) {
			fprintf(stderr, _("Could not add primary index to first file."));
			fprintf(stderr, "\n");
			exit(1);
		}
	}

	if(pindexfile2) {
		pindexdoc2 = PX_new2(errorhandler, NULL, NULL, NULL);
		if(0 > PX_open_file(pindexdoc2, pindexfile2)) {
			fprintf(stderr, _("Could not open primary index to second file."));
			fprintf(stderr, "\n");
			exit(1);
		}
		if(0 > PX_read_primary_index(pindexdoc2)) {
			fprintf(stderr, _("Could not read primary index to second file."));
			fprintf(stderr, "\n");
			exit(1);
		}
		if(0 > PX_add_primary_index(pxdoc2, pindexdoc2)) {
			fprintf(stderr, _("Could not add primary index to second file."));
			fprintf(stderr, "\n");
			exit(1);
		}
	}
	/* }}} */

	pxh1 = pxdoc1->px_head;
	pxh2 = pxdoc2->px_head;
	if(targetencoding != NULL) {
		PX_set_targetencoding(pxdoc1, targetencoding);
		PX_set_targetencoding(pxdoc2, targetencoding);
	}

	/* Compare schema {{{
	 */
	if(compareschema) {
		int **l;
		int i, j, k, len;
		pxfield_t **lcs;
		pxfield_t **fields1, **fields2;
		if(pxh1->px_fileversion != pxh2->px_fileversion) {
			fprintf(outfp, _("-File Version: %1.1f"), (float) pxh1->px_fileversion/10.0);
			fprintf(outfp, "\n");
			fprintf(outfp, _("+File Version: %1.1f"), (float) pxh2->px_fileversion/10.0);
			fprintf(outfp, "\n");
		}
		if(pxh1->px_filetype != pxh2->px_filetype) {
			fprintf(outfp, _("-File Type: %d"), pxh1->px_filetype);
			fprintf(outfp, "\n");
			fprintf(outfp, _("+File Type: %d"), pxh2->px_filetype);
			fprintf(outfp, "\n");
		}
		if(strcmp(pxh1->px_tablename, pxh2->px_tablename)) {
			fprintf(outfp, _("-Tablename: %s"), pxh1->px_tablename);
			fprintf(outfp, "\n");
			fprintf(outfp, _("+Tablename: %s"), pxh2->px_tablename);
			fprintf(outfp, "\n");
		}
		if(pxh1->px_numrecords != pxh2->px_numrecords) {
			fprintf(outfp, _("-Num. of Records: %d"), pxh1->px_numrecords);
			fprintf(outfp, "\n");
			fprintf(outfp, _("+Num. of Records: %d"), pxh2->px_numrecords);
			fprintf(outfp, "\n");
		}
		if(pxh1->px_numfields != pxh2->px_numfields) {
			fprintf(outfp, _("-Num. of Fields: %d"), pxh1->px_numfields);
			fprintf(outfp, "\n");
			fprintf(outfp, _("+Num. of Fields: %d"), pxh2->px_numfields);
			fprintf(outfp, "\n");
		}
		if(pxh1->px_doscodepage != pxh2->px_doscodepage) {
			fprintf(outfp, _("-Code Page: %d"), pxh1->px_doscodepage);
			fprintf(outfp, "\n");
			fprintf(outfp, _("+Code Page: %d"), pxh2->px_doscodepage);
			fprintf(outfp, "\n");
		}


		pxf1 = pxh1->px_fields;
		pxf2 = pxh2->px_fields;
		i1 = i2 = 0;
		if((l = (int **) pxdoc1->malloc(pxdoc1, (pxh1->px_numfields+1)*sizeof(int *), _("Could not allocate memory lcs array."))) == NULL) {
			PX_close(pxdoc1);
			PX_close(pxdoc2);
			exit(1);
		}
		for(i1=0; i1<pxh1->px_numfields+1; i1++) {
			if((l[i1] = (int *) pxdoc1->malloc(pxdoc1, (pxh2->px_numfields+1) * sizeof(int), _("Could not allocate memory lcs array."))) == NULL) {
				PX_close(pxdoc1);
				PX_close(pxdoc2);
				exit(1);
			}
		}

		/* Create an array of pxfield_t pointers because lcs_length needs it. */
		if((fields1 = (pxfield_t **) pxdoc1->malloc(pxdoc1, (pxh1->px_numfields)*sizeof(pxfield_t *), _("Could not allocate memory lcs array."))) == NULL) {
			PX_close(pxdoc1);
			PX_close(pxdoc2);
			exit(1);
		}
		for(i=0; i<pxh1->px_numfields; i++) {
			fields1[i] = &pxf1[i];
		}
		if((fields2 = (pxfield_t **) pxdoc1->malloc(pxdoc1, (pxh2->px_numfields)*sizeof(pxfield_t *), _("Could not allocate memory lcs array."))) == NULL) {
			PX_close(pxdoc1);
			PX_close(pxdoc2);
			exit(1);
		}
		for(i=0; i<pxh2->px_numfields; i++) {
			fields2[i] = &pxf2[i];
		}

//		sort(fields1, pxh1->px_numfields, fieldcmp);
//		sort(fields2, pxh2->px_numfields, fieldcmp);

		/* Calculate the length of the commen subsequence */
		len = lcs_length(l, (void **) fields1, pxh1->px_numfields,
		                    (void **) fields2, pxh2->px_numfields,
		                    fieldcmp, 0);

		/* Output the matrix for debugging
		lcs_output_matrix(l, pxh1->px_numfields, pxh2->px_numfields);
		*/

		/* get the sequence */
		if((lcs = (pxfield_t **) pxdoc1->malloc(pxdoc1, len * sizeof(pxfield_t *), _("Could not allocate memory lcs array."))) == NULL) {
			PX_close(pxdoc1);
			PX_close(pxdoc2);
			exit(1);
		}
		lcs_sequence(l, (void **) fields1, pxh1->px_numfields,
		                (void **) fields2, pxh2->px_numfields,
		                fieldcmp, 0, (void **) lcs);

		/* Output the difference */
		i = 0; j = 0; k = 0;
		schemasdiffer = 0;
		while(i < pxh1->px_numfields && j < pxh2->px_numfields) {
			if(fieldcmp(fields1[i], fields2[j], 0)) {
				if(fieldcmp(fields1[i], lcs[k], 0)) {
					fprintf(outfp, "- ");
					show_field(outfp, fields1[i++]);
				}
				if(fieldcmp(fields2[j], lcs[k], 0)) {
					fprintf(outfp, "+ ");
					show_field(outfp, fields2[j++]);
				}
				schemasdiffer = 1;
			} else {
				fprintf(outfp, "= ");
				show_field(outfp, fields2[j]);
				i++; j++; k++;
			}
		}
	}
	/* }}} */

#if 0
	/* Check which fields shall be compared */
	if(fieldregex) {
		regex_t preg;
		if(regcomp(&preg, fieldregex, REG_NOSUB|REG_EXTENDED|REG_ICASE)) {
			fprintf(stderr, _("Could not compile regular expression to select fields."));
			PX_close(pxdoc);
			exit(1);
		}
		/* allocate memory for selected field array */
		if((selectedfields = (char *) pxdoc->malloc(pxdoc, pxh->px_numfields, _("Could not allocate memory for array of selected fields."))) == NULL) {
			PX_close(pxdoc);
			exit(1);
		}
		memset(selectedfields, '\0', pxh->px_numfields);
		pxf = pxh->px_fields;
		for(i=0; i<pxh->px_numfields; i++) {
			if(0 == regexec(&preg, pxf->px_fname, 0, NULL, 0)) {
				selectedfields[i] = 1;
			}
			pxf++;
		}
	}
#endif

	/* Compare the data records only if the schemas has not differ {{{
	 */
	if(comparedata && !schemasdiffer) {
		int **l;
		char **lcs;
		char *data1, *data2;
		char **records1, **records2;
		int recordsize, len, i, j, k;

		if((l = (int **) pxdoc1->malloc(pxdoc1, (pxh1->px_numrecords+1)*sizeof(char *), _("Could not allocate memory lcs array."))) == NULL) {
			PX_close(pxdoc1);
			exit(1);
		}
		for(i=0; i<pxh1->px_numrecords+1; i++) {
			if((l[i] = (int *) pxdoc2->malloc(pxdoc2, (pxh2->px_numrecords+1) * sizeof(int), _("Could not allocate memory lcs array."))) == NULL) {
				PX_close(pxdoc1);
				PX_close(pxdoc2);
				exit(1);
			}
		}
		if((data1 = (char *) pxdoc1->malloc(pxdoc1, pxh1->px_recordsize * pxh1->px_numrecords, _("Could not allocate memory for record."))) == NULL) {
			PX_close(pxdoc1);
			exit(1);
		}
		if((records1 = (char **) pxdoc1->malloc(pxdoc1, pxh1->px_numrecords * sizeof(char *), _("Could not allocate memory for record."))) == NULL) {
			PX_close(pxdoc1);
			exit(1);
		}
		if((data2 = (char *) pxdoc2->malloc(pxdoc2, pxh2->px_recordsize * pxh2->px_numrecords, _("Could not allocate memory for record."))) == NULL) {
			PX_close(pxdoc1);
			PX_close(pxdoc2);
			exit(1);
		}
		if((records2 = (char **) pxdoc2->malloc(pxdoc2, pxh2->px_numrecords * sizeof(char *), _("Could not allocate memory for record."))) == NULL) {
			PX_close(pxdoc1);
			PX_close(pxdoc2);
			exit(1);
		}

		/* Get all records of first database */
		for(j=0; j<pxh1->px_numrecords; j++) {
			if(NULL != PX_get_record(pxdoc1, j, data1)) {
				records1[j] = data1;
			} else {
				fprintf(stderr, _("Couldn't get record number %d\n"), j);
			}
			data1 += pxh1->px_recordsize;
		}

		/* Get all records of second database */
		for(j=0; j<pxh2->px_numrecords; j++) {
			if(NULL != PX_get_record(pxdoc2, j, data2)) {
				records2[j] = data2;
			} else {
				fprintf(stderr, _("Couldn't get record number %d\n"), j);
			}
			data2 += pxh2->px_recordsize;
		}

//		for(j=0; j<pxh1->px_numrecords; j++) {
//			fprintf(outfp, "%d-0x%X\n", j, records1[j]);
//			show_record(outfp, pxdoc1, pxh1, records1[j]);
//		}

		if(pxh1->px_recordsize != pxh2->px_recordsize) {
			fprintf(outfp, "Record size differs!!!");
			fprintf(outfp, "\n");
		}
		/* Calculate the length of the common subsequence */
		recordsize = min(pxh1->px_recordsize, pxh2->px_recordsize);
		len = lcs_length(l, (void **) records1, pxh1->px_numrecords,
		                    (void **) records2, pxh2->px_numrecords,
		                    recordcmp, recordsize);

//		lcs_output_matrix(l, pxh1->px_numrecords, pxh2->px_numrecords);

		fprintf(outfp, "Longest common sequence has lenght %d", len);
		fprintf(outfp, "\n");

		/* get the sequence */
		if((lcs = (char **) pxdoc1->malloc(pxdoc1, len * sizeof(char *), _("Could not allocate memory lcs array."))) == NULL) {
			PX_close(pxdoc1);
			PX_close(pxdoc2);
			exit(1);
		}
		lcs_sequence(l, (void **) records1, pxh1->px_numrecords,
		                (void **) records2, pxh2->px_numrecords,
		                recordcmp, recordsize, (void **) lcs);

//		for(j=0; j<len; j++) {
//			fprintf(outfp, "%d-0x%X-", j, lcs[j]);
//			show_record(outfp, pxdoc1, pxh1, lcs[j]);
//		}

		/* Output the difference */
		i = 0; j = 0; k = 0;
		int notinlcs1, notinlcs2;
		while(i < pxh1->px_numrecords && j < pxh2->px_numrecords) {
			notinlcs1 = 0;
			notinlcs2 = 0;
			if(recordcmp(records1[i], records2[j], recordsize)) {
				if(k >= len || recordcmp(records1[i], lcs[k], recordsize)) {
					notinlcs1 = 1;
				}
				if(k >= len || recordcmp(records2[j], lcs[k], recordsize)) {
					notinlcs2 = 1;
				}
				if(notinlcs1 == 1 && notinlcs2 == 1 &&
				   !recordcmp(records1[i], records2[j], 16)) {
					fprintf(outfp, "<\t");
					show_record(outfp, pxdoc1, pxh1, records1[i]);
					fprintf(outfp, ">\t");
					show_record(outfp, pxdoc2, pxh2, records2[j]);
//					show_record_diff(outfp, pxdoc1, pxh1, records1[i], pxdoc2, pxh2, records2[j]);
					i++; j++;
				} else if(notinlcs1 == 1) {
					fprintf(outfp, "-\t");
					show_record(outfp, pxdoc1, pxh1, records1[i++]);
				} else if(notinlcs2 == 1) {
					fprintf(outfp, "+\t");
					show_record(outfp, pxdoc2, pxh2, records2[j++]);
				}
			} else {
				i++; j++; k++;
			}
		}

		px_free(pxdoc1, data1);
		px_free(pxdoc1, records1);
		px_free(pxdoc2, data2);
		px_free(pxdoc2, records2);
	}
	/* }}} */

	/* Close all files and free memory {{{
	 */
	if(pindexfile1) {
		PX_close(pindexdoc1);
		PX_delete(pindexdoc1);
	}

	if(pindexfile2) {
		PX_close(pindexdoc2);
		PX_delete(pindexdoc2);
	}

	PX_close(pxdoc1);
	PX_delete(pxdoc1);

	PX_close(pxdoc2);
	PX_delete(pxdoc2);

#ifdef HAVE_GSF
	if(PX_has_gsf_support() && usegsf) {
		gsf_shutdown();
	}
#endif
	/* }}} */

	exit(0);
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
