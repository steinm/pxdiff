#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <libintl.h>
#include <sys/types.h>
#include <regex.h>
#include <libgen.h>
#include "config.h"

#define _(String) gettext(String)
#define max(a,b) ((a)>(b) ? (a) : (b))
#define min(a,b) ((a)<(b) ? (a) : (b))

#ifdef MEMORY_DEBUGGING
#include <paradox-mp.h>
#else
#include <paradox.h>
#endif

int qsort_len;

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

/* qsort_comp_records()
 */
int qsort_comp_records(const void *data1, const void *data2) {
	void **d1 = (void **) data1;
	void **d2 = (void **) data2;
	return(memcmp(*d1, *d2, qsort_len));
}
/* }}} */

/* recordcmp() {{{
 * Compares two records. Returns -1, 0, or 1 respectively
 * to be data1 found less, equal, or greater than data2.
 */
int recordcmp(void *data1, void *data2, size_t n) {
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
 * Calculates the length of the longest common sequence of two data fields.
 * Each data field is list of pointers to the data records. The data
 * comparision function datacmp() must take two pointers to the data record and
 * th esize of the record. It returns -1, 0, or 1 respectively to data1 being
 * less, equal or greater the data2. The parameter l has to been array of num1
 * pointers to strings of num2 length. Or, in other words, it is array of num1
 * strings all being num2 long.
 * This function can be used universally for any two list of pointers to
 * an kind of data as long as the compare function is provided.
 */
int lcs_length(int **l, void **data1, int num1, void **data2, int num2, int
		(datacmp)(void *data1, void *data2, size_t n), size_t n) { int i, j;
	for(i=num1; i>=0; i--) { for(j=num2; j>=0; j--) {
			/* FIXME: l[num1][0-num2] and l[1-num1][num2] should be filled with
			 * 0 before. 
			 */
			if(i==num1 || j==num2) l[i][j] = 0; else if(!datacmp(data1[i],
						data2[j], n)) l[i][j] = 1 + l[i+1][j+1]; else l[i][j] =
				max(l[i+1][j], l[i][j+1]); } } return l[0][0]; }
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
void show_record(FILE *outfp, pxdoc_t *pxdoc, pxhead_t *pxh, char *data, int *selectedfields) {
	pxfield_t *pxf;
	int offset, first, i;
	char delimiter = '\t';
	char enclosure = '\"';
	pxf = pxh->px_fields;
	offset = 0;
	first = 0;  // set to 1 when first field has been output
	for(i=0; i<pxh->px_numfields; i++) {
		if(!selectedfields || (selectedfields && selectedfields[i] >= 0)) {
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
						pxdoc->free(pxdoc, value);
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
			offset += pxf->px_flen; /* record data only contains the selected fields */
		}
		pxf++;
	}
	fprintf(outfp, "\n");
}
/* }}} */

/* show_record_diff() {{{
 * Outputs the difference of two records. The must have identical
 * size and structure.
 * The function can handle records with common fields being at different
 * positions in the table structure but not if the common fields have a
 * different order.
 * Example: The following is fine for a set of selected fields (--- stands
 * for: this field is not selected.)
 *    file1         file2
 *    ---------------------
 * 0. ---           field1
 * 1. field1        field2
 * 2. ---           ---
 * 3. field2        ---
 * 
 * In such a case the algorithm will compare the fields at position 0.
 * Since file1[0] is not set the index (i) will be incremented. The other
 * index (j) remains unchanged. Now file[1] and file2[0] will be compared.
 * They are equal and i and j will be incremented. file[2] and file[1]
 * are not equal. Incrementing i and comparing file1[3] and file2[1] will
 * again be successfull. Since i is not at its upper limit it will not be
 * incremented any more. If j ist also at its upper limit the while loop
 * will quit.
 */
void show_record_diff(FILE *outfp, pxdoc_t *pxdoc1, pxhead_t *pxh1, char *data1, int *selectedfields1, pxdoc_t *pxdoc2, pxhead_t *pxh2, char *data2, int *selectedfields2) {
	pxfield_t *pxf1, *pxf2;
	int i, j;
	int offset1, offset2;
	char delimiter = '\t';
	char enclosure = '\"';
	pxf1 = pxh1->px_fields;
	pxf2 = pxh2->px_fields;
	i = j = 0;
	offset1 = offset2 = 0;
	/* The following loop makes some assumptions:
	 * 1. The sequence of fields which are common to both files is equal.
	 *    It may be that equal fields are not at the same index position
	 *    in selectedfields[1|2] but one will never be before are after
	 *    the other.
	 * 2. Common fields are absolutely equal in type and size.
	 */
	while((i < pxh1->px_numfields) && (j < pxh2->px_numfields)) {
		/* There may be no selected fields at all. In such a case the whole
		 * record will be compared. If we have selected fields we need to make
		 * sure to access the right field which may be at different positions
		 * in the table structure.
		 */
		if(!selectedfields1 || !selectedfields2 || ((selectedfields1[i] >= 0) && (selectedfields2[j] >= 0))) {
//			fprintf(outfp, "Comparing '%s' which is of type %d\n", pxf1[i].px_fname, pxf1[i].px_ftype);
			switch(pxf1[i].px_ftype) {
				case pxfAlpha: {
					char *value1, *value2;
					if(PX_get_data_alpha(pxdoc1, &data1[offset1], pxf1[i].px_flen, &value1)) {
						if(PX_get_data_alpha(pxdoc2, &data2[offset2], pxf2[j].px_flen, &value2)) {
							if(strncmp(value1, value2, min(pxf1[i].px_flen, pxf2[j].px_flen))) {
								fprintf(outfp, "%s%c", pxf1[i].px_fname, delimiter);
								fprintf(outfp, "'%s'%c", value1, delimiter);
								fprintf(outfp, "'%s'\n", value2);
							}
							pxdoc1->free(pxdoc2, value2);
						}
						pxdoc2->free(pxdoc1, value1);
					}
					break;
				}
				case pxfDate: {
					long value1, value2;
					int year1, month1, day1;
					int year2, month2, day2;
					if(PX_get_data_long(pxdoc1, &data1[offset1], pxf1[i].px_flen, &value1)) {
						if(PX_get_data_long(pxdoc2, &data2[offset2], pxf2[j].px_flen, &value2)) {
							if(value1 != value2) {
								PX_SdnToGregorian(value1+1721425, &year1, &month1, &day1);
								PX_SdnToGregorian(value2+1721425, &year2, &month2, &day2);
								fprintf(outfp, "%s%c", pxf1[i].px_fname, delimiter);
								fprintf(outfp, "%02d.%02d.%04d%c", day1, month1, year1, delimiter);
								fprintf(outfp, "%02d.%02d.%04d\n", day2, month2, year2);
							}
						}
					}
					break;
					}
				case pxfShort: {
					short int value1, value2;
					if(PX_get_data_short(pxdoc1, &data1[offset1], pxf1[i].px_flen, &value1)) {
						if(PX_get_data_short(pxdoc2, &data2[offset2], pxf2[j].px_flen, &value2)) {
							if(value1 != value2) {
								fprintf(outfp, "%s%c", pxf1[i].px_fname, delimiter);
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
					if(PX_get_data_long(pxdoc1, &data1[offset1], pxf1[i].px_flen, &value1)) {
						if(PX_get_data_long(pxdoc2, &data2[offset2], pxf2[j].px_flen, &value2)) {
							if(value1 != value2) {
								fprintf(outfp, "%s%c", pxf1[i].px_fname, delimiter);
								fprintf(outfp, "%ld%c", value1, delimiter);
								fprintf(outfp, "%ld\n", value2);
							}
						}
					}
					break;
					}
				case pxfTime: {
					long value1, value2;
					if(PX_get_data_long(pxdoc1, &data1[offset1], pxf1[i].px_flen, &value1)) {
						if(PX_get_data_long(pxdoc2, &data2[offset2], pxf2[j].px_flen, &value2)) {
							if(value1 != value2) {
								fprintf(outfp, "%s%c", pxf1[i].px_fname, delimiter);
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
					if(PX_get_data_double(pxdoc1, &data1[offset1], pxf1[i].px_flen, &value1)) {
						if(PX_get_data_double(pxdoc2, &data2[offset2], pxf2[j].px_flen, &value2)) {
							if(value1 != value2) {
								fprintf(outfp, "%s%c", pxf1[i].px_fname, delimiter);
								fprintf(outfp, "%f%c", value1, delimiter);
								fprintf(outfp, "%f\n", value2);
							}
						}
					} 
					break;
					} 
				case pxfLogical: {
					char value1, value2;
					if(PX_get_data_byte(pxdoc1, &data1[offset1], pxf1[i].px_flen, &value1)) {
						if(PX_get_data_byte(pxdoc2, &data2[offset2], pxf2[j].px_flen, &value2)) {
							if(value1 != value2) {
								fprintf(outfp, "%s%c", pxf1[i].px_fname, delimiter);
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
					fprintf(outfp, _("Not supported field type."));
			}
			offset1 += pxf1[i].px_flen;
			offset2 += pxf2[j].px_flen;
			i++;
			j++;
		} else {
			/* One of the two fields or both at the current positions (i and j)
			 * are not selected. Forward the index to the next field and check
			 * again for equal fields.
			 */
			if((selectedfields1[i] < 0) && (i < pxh1->px_numfields)) {
				offset1 += pxf1[i].px_flen;
				i++;
			}
			if((selectedfields2[j] < 0) && (j < pxh2->px_numfields)) {
				offset2 += pxf2[j].px_flen;
				j++;
			}
		}
	}
	fprintf(outfp, "\n");
}
/* }}} */

/* errorhandler() {{{
 */
void errorhandler(pxdoc_t *p, int error, const char *str, void *data) {
	  fprintf(stderr, "pxdiff: PXLib: %s\n", str);
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
	printf(_("  -d, --data          compare data."));
	printf("\n");
	printf(_("  -t, --schema        compare schema."));
	printf("\n");
	printf(_("  -s, --sort          sort data before calculating difference."));
	printf("\n");
	printf(_("  --mode=MODE         set compare mode (schema, data)."));
	printf("\n");
	printf(_("  -o, --output-file=FILE output data into file instead of stdout."));
	printf("\n");
	printf(_("  -n, --primary-key=FIELD use field as primary key."));
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
	printf("\n");

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
	printf("\n");
	if(PX_is_bigendian())
		printf(_("libpx has been compiled for big endian architecture."));
	else
		printf(_("libpx has been compiled for little endian architecture."));
	printf("\n");
	printf(_("libpx has gsf support: %s"), PX_has_gsf_support() == 1 ? _("Yes") : _("No"));
	printf("\n");
	printf(_("libpx has version: %d.%d.%d"), PX_get_majorversion(), PX_get_minorversion(), PX_get_subminorversion());
	printf("\n");
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
	int *selectedfields1 = NULL;
	int *selectedfields2 = NULL;
	int j, c; // general counters
	int outputinfo = 0;
	int comparedata = 0;
	int compareschema = 0;
	int outputdebug = 0;
	int schemasdiffer = 0;
	int usegsf = 0;
	int verbose = 0;
	int sortdata = 0;
	char delimiter = '\t';
	char enclosure = '"';
	char *inputfile1 = NULL;
	char *inputfile2 = NULL;
	char *outputfile = NULL;
	char *pindexfile1 = NULL;
	char *pindexfile2 = NULL;
	char *pkey = NULL;
	int pkeystart1, pkeystart2;
	int pkeylen1, pkeylen2;
	char *fieldregex = NULL;
	char *targetencoding = NULL;
	FILE *outfp = NULL;

#ifdef MEMORY_DEBUGGING
	PX_mp_init();
#endif

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
			{"data", 0, 0, 'd'},
			{"schema", 0, 0, 't'},
			{"data", 0, 0, 'd'},
			{"verbose", 0, 0, 'v'},
			{"recode", 1, 0, 'r'},
			{"output-file", 1, 0, 'o'},
			{"help", 0, 0, 'h'},
			{"fields", 1, 0, 'f'},
			{"mode", 1, 0, 4},
			{"use-gsf", 0, 0, 8},
			{"primary-key", 1, 0, 'k'},
			{0, 0, 0, 0}
		};
		c = getopt_long (argc, argv, "ivtdsf:r:o:k:h",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
			case 4:
				if(!strcmp(optarg, "data")) {
					comparedata = 1;
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
			case 's':
				sortdata = 1;
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
			case 'k':
				pkey = strdup(optarg);
				break;
		}
	}


	if (optind < argc) {
		inputfile1 = strdup(argv[optind++]);
	}
	if (optind < argc) {
		inputfile2 = strdup(argv[optind++]);
	}
	if (optind < argc) {
		pindexfile1 = strdup(argv[optind++]);
	}
	if (optind < argc) {
		pindexfile2 = strdup(argv[optind++]);
	}

	/* compare schema is the default if none is selected */
	if(!compareschema && !comparedata)
		compareschema = 1;

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

#ifdef MEMORY_DEBUGGING
	if(NULL == (pxdoc1 = PX_new2(errorhandler, PX_mp_malloc, PX_mp_realloc, PX_mp_free))) {
#else
	if(NULL == (pxdoc1 = PX_new2(errorhandler, NULL, NULL, NULL))) {
#endif
		fprintf(stderr, _("Could not create new paradox instance."));
		fprintf(stderr, "\n");
		exit(1);
	}

#ifdef MEMORY_DEBUGGING
	if(NULL == (pxdoc2 = PX_new2(errorhandler, PX_mp_malloc, PX_mp_realloc, PX_mp_free))) {
#else
	if(NULL == (pxdoc2 = PX_new2(errorhandler, NULL, NULL, NULL))) {
#endif
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

	/* Check which fields shall be compared {{{
	 */
	if(fieldregex) {
		regex_t preg;
		pxfield_t *pxf;
		int i;
		int offset;
		if(regcomp(&preg, fieldregex, REG_NOSUB|REG_EXTENDED|REG_ICASE)) {
			fprintf(stderr, _("Could not compile regular expression to select fields."));
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		/* allocate memory for selected field array */
		if((selectedfields1 = (int *) pxdoc1->malloc(pxdoc1, pxh1->px_numfields*sizeof(int), _("Allocate memory for array of selected fields."))) == NULL) {
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		if((selectedfields2 = (int *) pxdoc2->malloc(pxdoc2, pxh2->px_numfields*sizeof(int), _("Allocate memory for array of selected fields."))) == NULL) {
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}

		/* loop through all fields and check if the name matches the regular
		 * expression. If it does match then selectfields[i] will be set to the
		 * offset of the field data within the record. If it does not match
		 * if will be set to -1
		 */
//		memset(selectedfields1, '\0', pxh1->px_numfields);
		offset = 0;
		pxf = pxh1->px_fields;
		for(i=0; i<pxh1->px_numfields; i++) {
			if(0 == regexec(&preg, pxf->px_fname, 0, NULL, 0)) {
				selectedfields1[i] = offset;
			} else {
				selectedfields1[i] = -1;
			}
			offset += pxf->px_flen;
			pxf++;
		}
//		memset(selectedfields2, '\0', pxh2->px_numfields);
		offset = 0;
		pxf = pxh2->px_fields;
		for(i=0; i<pxh2->px_numfields; i++) {
			if(0 == regexec(&preg, pxf->px_fname, 0, NULL, 0)) {
				selectedfields2[i] = offset;
			} else {
				selectedfields2[i] = -1;
			}
			offset += pxf->px_flen;
			pxf++;
		}
	}
	/* }}} */

	/* Check for primary key {{{
	 * The primary key ist used to match records in the two databases.
	 * The database must be sorted by the primary key. If two matching records
	 * are found the output will indicate that the record has changed and not
	 * a new record was added a an old one deleted.
	 */
	if(pkey) {
		pxfield_t *pxf;
		int i;

		pkeystart1 = 0;
		pkeylen1 = 0;
		pxf = pxh1->px_fields;
		for(i=0; i<pxh1->px_numfields; i++) {
			if(0 == strcasecmp(pkey, pxf->px_fname)) {
				pkeylen1 = pxf->px_flen;
				break;
			}
			if(fieldregex) {
				if(selectedfields1[i] >= 0) {
					pkeystart1 += pxf->px_flen;
				}
			} else {
				pkeystart1 += pxf->px_flen;
			}
			pxf++;
		}
		if(pkeylen1 == 0) {
			fprintf(stderr, _("Primary key could not be found in first database."));
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		if(selectedfields1 && selectedfields1[i] < 0) {
			fprintf(stderr, _("Primary key is not in list of selected fields of first file."));
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}

		pkeystart2 = 0;
		pkeylen2 = 0;
		pxf = pxh2->px_fields;
		for(i=0; i<pxh2->px_numfields; i++) {
			if(0 == strcasecmp(pkey, pxf->px_fname)) {
				pkeylen2 = pxf->px_flen;
				break;
			}
			if(fieldregex) {
				if(selectedfields2[i] >= 0) {
					pkeystart2 += pxf->px_flen;
				}
			} else {
				pkeystart2 += pxf->px_flen;
			}
			pxf++;
		}
		if(pkeylen2 == 0) {
			fprintf(stderr, _("Primary key could not be found in second database."));
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		if(selectedfields2 && selectedfields2[i] < 0) {
			fprintf(stderr, _("Primary key is not in list of selected fields of second file."));
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}

		/* Check if both keys are equally long */
		if(pkeylen1 != pkeylen2) {
			fprintf(stderr, _("Primary keys have different length in databases."));
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		fprintf(outfp, "primary key: %s: %d %d: %d %d\n", pkey, pkeystart1, pkeylen1, pkeystart2, pkeylen2);
	}
	/* }}} */

	/* Compare schema {{{
	 */
	if(compareschema) {
		int **l;
		int i, j, k, len;
		pxfield_t **lcs;
		pxfield_t **fields1, **fields2;
		int numfields1, numfields2;

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
			if(fieldregex == NULL)
				schemasdiffer = 1;
		}
		if(pxh1->px_doscodepage != pxh2->px_doscodepage) {
			fprintf(outfp, _("-Code Page: %d"), pxh1->px_doscodepage);
			fprintf(outfp, "\n");
			fprintf(outfp, _("+Code Page: %d"), pxh2->px_doscodepage);
			fprintf(outfp, "\n");
			schemasdiffer = 1;
		}

		pxf1 = pxh1->px_fields;
		pxf2 = pxh2->px_fields;

		/* Allocate memory for lcs matrix: n strings of length m
		 * Could be smaller if field selection was taken into account */
		if((l = (int **) pxdoc1->malloc(pxdoc1, (pxh1->px_numfields+1)*sizeof(int *), _("Allocate memory for lcs array."))) == NULL) {
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		for(i=0; i<pxh1->px_numfields+1; i++) {
			if((l[i] = (int *) pxdoc1->malloc(pxdoc1, (pxh2->px_numfields+1) * sizeof(int), _("Allocate memory for lines in lcs array."))) == NULL) {
				PX_close(pxdoc1);
				PX_delete(pxdoc1);
				PX_close(pxdoc2);
				PX_delete(pxdoc2);
				exit(1);
			}
		}

		/* Create an array of pxfield_t pointers for the first file,
		 * because lcs_length needs it. */
		if((fields1 = (pxfield_t **) pxdoc1->malloc(pxdoc1, (pxh1->px_numfields)*sizeof(pxfield_t *), _("Allocate memory for array of field pointers."))) == NULL) {
			for(i=0; i<pxh1->px_numfields+1; i++)
				pxdoc1->free(pxdoc1, l[i]);
			pxdoc1->free(pxdoc1, l);
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		numfields1 = 0;
		for(i=0; i<pxh1->px_numfields; i++) {
			if(fieldregex == NULL || selectedfields1[i] >= 0) {
				fields1[numfields1++] = &pxf1[i];
			}
		}
		/* Create an array of pxfield_t pointers for the second file,
		 * because lcs_length needs it. */
		if((fields2 = (pxfield_t **) pxdoc2->malloc(pxdoc2, (pxh2->px_numfields)*sizeof(pxfield_t *), _("Allocate memory for array of field pointers."))) == NULL) {
			for(i=0; i<pxh1->px_numfields+1; i++)
				pxdoc1->free(pxdoc1, l[i]);
			pxdoc1->free(pxdoc1, l);
			pxdoc1->free(pxdoc1, fields1);
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		numfields2 = 0;
		for(i=0; i<pxh2->px_numfields; i++) {
			if(fieldregex == NULL || selectedfields2[i] >= 0) {
				fields2[numfields2++] = &pxf2[i];
			}
		}

//		sort(fields1, pxh1->px_numfields, fieldcmp);
//		sort(fields2, pxh2->px_numfields, fieldcmp);

		/* Calculate the length of the common subsequence */
		len = lcs_length(l, (void **) fields1, numfields1,
		                    (void **) fields2, numfields2,
		                    fieldcmp, 0);

		/* Output the matrix for debugging
		lcs_output_matrix(l, numfields1, numfields2);
		*/

		/* get the sequence. lcs will be an array of pointers pointing to
		 * the specifications of fields which are in both database.
		 * The pointer is actually pointing to specification of the first
		 * database. */
		if((lcs = (pxfield_t **) pxdoc1->malloc(pxdoc1, len * sizeof(pxfield_t *), _("Allocate memory for lcs array."))) == NULL) {
			for(i=0; i<pxh1->px_numfields+1; i++)
				pxdoc1->free(pxdoc1, l[i]);
			pxdoc1->free(pxdoc1, l);
			pxdoc1->free(pxdoc1, fields1);
			pxdoc2->free(pxdoc2, fields2);
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		lcs_sequence(l, (void **) fields1, numfields1,
		                (void **) fields2, numfields2,
		                fieldcmp, 0, (void **) lcs);

		/* Output the difference */
		i = 0; j = 0; k = 0;
		while(i < numfields1 && j < numfields2) {
			if(fieldcmp(fields1[i], fields2[j], 0)) {
				if(k >= len || fieldcmp(fields1[i], lcs[k], 0)) {
					fprintf(outfp, "- ");
					show_field(outfp, fields1[i++]);
				}
				if(k >= len || fieldcmp(fields2[j], lcs[k], 0)) {
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

		for(i=0; i<pxh1->px_numfields+1; i++)
			pxdoc1->free(pxdoc1, l[i]);
		pxdoc1->free(pxdoc1, l);
		pxdoc1->free(pxdoc1, lcs);
		pxdoc1->free(pxdoc1, fields1);
		pxdoc2->free(pxdoc2, fields2);
	}
	/* }}} */

	/* Compare the data records only if the schemas has not differ {{{
	 */
	if(comparedata && !schemasdiffer) {
		int **l;
		char **lcs;
		char *data1, *data2, *dataptr;
		char **records1, **records2;
		pxfield_t *pxf;
		int recordsize, len, i, j, k;
		int notinlcs1, notinlcs2;
		int realrecsize1, realrecsize2; /* real record size of only selected fields */

		if((l = (int **) pxdoc1->malloc(pxdoc1, (pxh1->px_numrecords+1)*sizeof(int *), _("Allocate memory for lcs array."))) == NULL) {
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		for(i=0; i<pxh1->px_numrecords+1; i++) {
			if((l[i] = (int *) pxdoc2->malloc(pxdoc2, (pxh2->px_numrecords+1) * sizeof(int), _("Allocate memory for lines in lcs array."))) == NULL) {
				PX_close(pxdoc1);
				PX_delete(pxdoc1);
				PX_close(pxdoc2);
				PX_delete(pxdoc2);
				exit(1);
			}
		}
		if((data1 = (char *) pxdoc1->malloc(pxdoc1, pxh1->px_recordsize * pxh1->px_numrecords, _("Allocate memory for record."))) == NULL) {
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		if((records1 = (char **) pxdoc1->malloc(pxdoc1, pxh1->px_numrecords * sizeof(char *), _("Allocate memory for array of records."))) == NULL) {
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		if((data2 = (char *) pxdoc2->malloc(pxdoc2, pxh2->px_recordsize * pxh2->px_numrecords, _("Allocate memory for record."))) == NULL) {
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}
		if((records2 = (char **) pxdoc2->malloc(pxdoc2, pxh2->px_numrecords * sizeof(char *), _("Allocate memory for array of records."))) == NULL) {
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}

		/* Get all records of first database. records1 will contain pointers
		 * to all records. If fields are selected they will be copied to the
		 * beginning of the record one after the other realrecsize1 will
		 * contain the sum the lenght of all selected fields.
		 */
		dataptr = data1;
		for(j=0; j<pxh1->px_numrecords; j++) {
			int srcoffset;
			if(NULL != PX_get_record(pxdoc1, j, dataptr)) {
				records1[j] = dataptr;
				if(fieldregex) {
					realrecsize1 = 0;
					srcoffset = 0;
					pxf = pxh1->px_fields;
					for(i=0; i<pxh1->px_numfields; i++) {
						if(selectedfields1[i] >= 0) {
							memcpy(&dataptr[realrecsize1], &dataptr[srcoffset], pxf[i].px_flen);
							realrecsize1 += pxf[i].px_flen;
						}
						srcoffset += pxf[i].px_flen;
					} 
					memset(&dataptr[realrecsize1], 0, srcoffset-realrecsize1);
				} else {
					realrecsize1 = pxh1->px_recordsize;
				}
			} else {
				fprintf(stderr, _("Could not get record number %d\n"), j);
			}
			dataptr += pxh1->px_recordsize;
		}

		/* Get all records of second database. Do the same as above. */
		dataptr = data2;
		for(j=0; j<pxh2->px_numrecords; j++) {
			int srcoffset;
			if(NULL != PX_get_record(pxdoc2, j, dataptr)) {
				records2[j] = dataptr;
				if(fieldregex) {
					realrecsize2 = 0;
					srcoffset = 0;
					pxf = pxh2->px_fields;
					for(i=0; i<pxh2->px_numfields; i++) {
						if(selectedfields2[i] >= 0) {
							memcpy(&dataptr[realrecsize2], &dataptr[srcoffset], pxf[i].px_flen);
							realrecsize2 += pxf[i].px_flen;
						}
						srcoffset += pxf[i].px_flen;
					} /* FIXME */
					memset(&dataptr[realrecsize2], 0, srcoffset-realrecsize2);
				} else {
					realrecsize2 = pxh2->px_recordsize;
				}
			} else {
				fprintf(stderr, _("Could not get record number %d\n"), j);
			}
			dataptr += pxh2->px_recordsize;
		}

//		for(j=0; j<pxh1->px_numrecords; j++) {
//			fprintf(outfp, "%d-0x%X\n", j, records1[j]);
//			show_record(outfp, pxdoc1, pxh1, records1[j], selectedfields1);
//		}

		if(realrecsize1 != realrecsize2) {
			fprintf(outfp, "Record size differs: %d != %d", realrecsize1, realrecsize2);
			fprintf(outfp, "\n");
			for(i=0; i<pxh1->px_numfields+1; i++)
				pxdoc1->free(pxdoc1, l[i]);
			pxdoc1->free(pxdoc1, data1);
			pxdoc1->free(pxdoc1, records1);
			pxdoc2->free(pxdoc2, data2);
			pxdoc2->free(pxdoc2, records2);
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
			exit(1);
		}

		/* Sort to the data arrays */
		if(sortdata) {
			if(pkey)
				qsort_len = pkeylen1;
			else
				qsort_len = realrecsize1;
			qsort((void *)records1, pxh1->px_numrecords, sizeof(char *), qsort_comp_records);
			qsort((void *)records2, pxh2->px_numrecords, sizeof(char *), qsort_comp_records);
		}

		/* Calculate the length of the common subsequence */
		recordsize = realrecsize1;
		len = lcs_length(l, (void **) records1, pxh1->px_numrecords,
		                    (void **) records2, pxh2->px_numrecords,
		                    recordcmp, recordsize);

//		lcs_output_matrix(l, pxh1->px_numrecords, pxh2->px_numrecords);

		fprintf(outfp, "Longest common sequence has lenght %d", len);
		fprintf(outfp, "\n");

		/* get the sequence of all common records. lcs contains the pointers
		 * to common records. The pointer is actually pointing to the record
		 * in the first database. */
		if((lcs = (char **) pxdoc1->malloc(pxdoc1, len * sizeof(char *), _("Allocate memory for lcs array."))) == NULL) {
			PX_close(pxdoc1);
			PX_delete(pxdoc1);
			PX_close(pxdoc2);
			PX_delete(pxdoc2);
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
		while(i < pxh1->px_numrecords && j < pxh2->px_numrecords) {
			notinlcs1 = 0;
			notinlcs2 = 0;
			if(recordcmp(records1[i], records2[j], recordsize)) {
				/* We could compare just pointers in case of records1 because
				 * lcs contains the pointers to the records in records1. I'm
				 * not sure if there are any site effects if records1 contains
				 * several identical records which still would have different
				 * pointers. Speed wise its not that much of a difference.
				 */
//				if(k >= len || records1[i] != lcs[k]) {
				if(k >= len || recordcmp(records1[i], lcs[k], recordsize)) {
					notinlcs1 = 1;
				}
				if(k >= len || recordcmp(records2[j], lcs[k], recordsize)) {
					notinlcs2 = 1;
				}
				/* Two records are assumed to be the same when its primary
				 * key is identical.
				 */
				if(pkey && notinlcs1 == 1 && notinlcs2 == 1 &&
				   !recordcmp(&records1[i][pkeystart1], &records2[j][pkeystart2], pkeylen1)) {
//					hex_dump(outfp, records1[i], recordsize);
//					fprintf(outfp, "\n");
//					hex_dump(outfp, records2[j], recordsize);
//					fprintf(outfp, "\n");

					fprintf(outfp, "<\t");
					show_record(outfp, pxdoc1, pxh1, records1[i], selectedfields1);
					fprintf(outfp, ">\t");
					show_record(outfp, pxdoc2, pxh2, records2[j], selectedfields2);
					show_record_diff(outfp, pxdoc1, pxh1, records1[i], selectedfields1, pxdoc2, pxh2, records2[j], selectedfields2);
					i++; j++;
				} else {
					if(notinlcs1 == 1) {
						fprintf(outfp, "-\t");
						show_record(outfp, pxdoc1, pxh1, records1[i++], selectedfields1);
					}
					if(notinlcs2 == 1) {
						fprintf(outfp, "+\t");
						show_record(outfp, pxdoc2, pxh2, records2[j++], selectedfields2);
					}
				}
			} else {
				i++; j++; k++;
			}
		}

		/* Output all remaining records in first database */
		while(i < pxh1->px_numrecords) {
			fprintf(outfp, "-\t");
			show_record(outfp, pxdoc1, pxh1, records1[i++], selectedfields1);
		}

		/* Output all remaining records in second database */
		while(i < pxh2->px_numrecords) {
			fprintf(outfp, "+\t");
			show_record(outfp, pxdoc2, pxh2, records2[i++], selectedfields2);
		}

		for(i=0; i<pxh1->px_numrecords+1; i++)
			pxdoc1->free(pxdoc1, l[i]);
		pxdoc1->free(pxdoc1, l);
		pxdoc1->free(pxdoc1, lcs);
		pxdoc1->free(pxdoc1, data1);
		pxdoc1->free(pxdoc1, records1);
		pxdoc2->free(pxdoc2, data2);
		pxdoc2->free(pxdoc2, records2);
	} else {
		if(schemasdiffer) {
			fprintf(outfp, _("Schema already differs, will not compare records."));
			fprintf(outfp, "\n");
		}
	}
	/* }}} */

	if(selectedfields1)
		pxdoc1->free(pxdoc1, selectedfields1);
	if(selectedfields2)
		pxdoc2->free(pxdoc2, selectedfields2);

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

#ifdef MEMORY_DEBUGGING
	PX_mp_list_unfreed();
#endif
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
