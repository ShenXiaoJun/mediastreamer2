/*
tester - liblinphone test suite
Copyright (C) 2013  Belledonne Communications SARL

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define CONFIG_FILE "mediastreamer-config.h"
#define LOG_ERROR BELLE_SIP_LOG_ERROR
#define LOG_MESSAGE BELLE_SIP_LOG_MESSAGE

#ifdef HAVE_CONFIG_H
#include CONFIG_FILE
#endif

#include "tester_utils.h"

#include <stdlib.h>
#include "CUnit/Automated.h"
#include <belle-sip/utils.h>
#include <ortp/str_utils.h>

#if WINAPI_FAMILY_PHONE_APP
const char *bc_tester_read_dir_prefix="Assets";
#elif defined(__QNX__)
const char *bc_tester_read_dir_prefix="./app/native/assets/";
#else
const char *bc_tester_read_dir_prefix=".";
#endif

/* TODO: have the same "static" for QNX and windows as above? */
#ifdef ANDROID
const char *bc_tester_writable_dir_prefix = "/data/data/org.linphone.tester/cache";
#else
const char *bc_tester_writable_dir_prefix = ".";
#endif

static test_suite_t **test_suite = NULL;
static int nb_test_suites = 0;

#if HAVE_CU_CURSES
static unsigned char curses = 0;
#endif

int use_log_file = 0;
char* xml_file = NULL;
int   xml_enabled = 0;
char * suite_name;
char * test_name;
void (*tester_printf_va)(int level, const char *fmt, va_list args);

static void tester_printf(int level, const char *fmt, ...) {
	va_list args;
	va_start (args, fmt);
	tester_printf_va(level, fmt, args);
	va_end (args);
}

static int tester_run_suite(test_suite_t *suite) {
	int i;

	CU_pSuite pSuite = CU_add_suite(suite->name, suite->init_func, suite->cleanup_func);

	for (i = 0; i < suite->nb_tests; i++) {
		if (NULL == CU_add_test(pSuite, suite->tests[i].name, suite->tests[i].func)) {
			return CU_get_error();
		}
	}

	return 0;
}

const char * tester_test_suite_name(int suite_index) {
	if (suite_index >= nb_test_suites) return NULL;
	return test_suite[suite_index]->name;
}

static int tester_test_suite_index(const char *suite_name) {
	int i;

	for (i = 0; i < nb_test_suites; i++) {
		if ((strcmp(suite_name, test_suite[i]->name) == 0) && (strlen(suite_name) == strlen(test_suite[i]->name))) {
			return i;
		}
	}

	return -1;
}
const char * tester_test_name(const char *suite_name, int test_index) {
	int suite_index = tester_test_suite_index(suite_name);
	if ((suite_index < 0) || (suite_index >= nb_test_suites)) return NULL;
	if (test_index >= test_suite[suite_index]->nb_tests) return NULL;
	return test_suite[suite_index]->tests[test_index].name;
}

static int tester_nb_tests(const char *suite_name) {
	int i = tester_test_suite_index(suite_name);
	if (i < 0) return 0;
	return test_suite[i]->nb_tests;
}

static void tester_list_suites() {
	int j;
	for(j=0;j<nb_test_suites;j++) {
		tester_printf(BELLE_SIP_LOG_MESSAGE, "%s", tester_test_suite_name(j));
	}
}

static void tester_list_suite_tests(const char *suite_name) {
	int j;
	for( j = 0; j < tester_nb_tests(suite_name); j++) {
		const char *test_name = tester_test_name(suite_name, j);
		tester_printf(BELLE_SIP_LOG_MESSAGE, "%s", test_name);
	}
}

static void all_complete_message_handler(const CU_pFailureRecord pFailure) {
#ifdef HAVE_CU_GET_SUITE
	char * results = CU_get_run_results_string();
	tester_printf(BELLE_SIP_LOG_MESSAGE,"\n%s",results);
	free(results);
#endif
}

static void suite_init_failure_message_handler(const CU_pSuite pSuite) {
	tester_printf(LOG_ERROR,"Suite initialization failed for [%s].", pSuite->pName);
}

static void suite_cleanup_failure_message_handler(const CU_pSuite pSuite) {
	tester_printf(LOG_ERROR,"Suite cleanup failed for [%s].", pSuite->pName);
}

#ifdef HAVE_CU_GET_SUITE
static void suite_start_message_handler(const CU_pSuite pSuite) {
	tester_printf(BELLE_SIP_LOG_MESSAGE,"Suite [%s] started\n", pSuite->pName);
}
static void suite_complete_message_handler(const CU_pSuite pSuite, const CU_pFailureRecord pFailure) {
	tester_printf(BELLE_SIP_LOG_MESSAGE,"Suite [%s] ended\n", pSuite->pName);
}

static void test_start_message_handler(const CU_pTest pTest, const CU_pSuite pSuite) {
	tester_printf(BELLE_SIP_LOG_MESSAGE,"Suite [%s] Test [%s] started", pSuite->pName,pTest->pName);
}

/*derivated from cunit*/
static void test_complete_message_handler(const CU_pTest pTest,
	const CU_pSuite pSuite,
	const CU_pFailureRecord pFailureList) {
	int i;
	char * result = malloc(sizeof(char)*2048);//not very pretty but...
	sprintf(result, "Suite [%s] Test [%s]", pSuite->pName, pTest->pName);
	CU_pFailureRecord pFailure = pFailureList;
	if (pFailure) {
		strncat(result, " failed:", strlen(" failed:"));
		for (i = 1 ; (NULL != pFailure) ; pFailure = pFailure->pNext, i++) {
			sprintf(result, "%s\n    %d. %s:%u  - %s", result, i,
				(NULL != pFailure->strFileName) ? pFailure->strFileName : "",
				pFailure->uiLineNumber,
				(NULL != pFailure->strCondition) ? pFailure->strCondition : "");
		}
	} else {
		strncat(result, " passed", strlen(" passed"));
	}
	tester_printf(BELLE_SIP_LOG_MESSAGE,"%s\n", result);
	free(result);
}
#endif

static int tester_run_tests(const char *suite_name, const char *test_name) {
	int i;
	int ret;
	/* initialize the CUnit test registry */
	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	for (i = 0; i < nb_test_suites; i++) {
		tester_run_suite(test_suite[i]);
	}
#ifdef HAVE_CU_GET_SUITE
	CU_set_suite_start_handler(suite_start_message_handler);
	CU_set_suite_complete_handler(suite_complete_message_handler);
	CU_set_test_start_handler(test_start_message_handler);
	CU_set_test_complete_handler(test_complete_message_handler);
#endif
	CU_set_all_test_complete_handler(all_complete_message_handler);
	CU_set_suite_init_failure_handler(suite_init_failure_message_handler);
	CU_set_suite_cleanup_failure_handler(suite_cleanup_failure_message_handler);

	if( xml_enabled != 0 ){
		CU_automated_run_tests();
	} else {

#if !HAVE_CU_GET_SUITE
		if( suite_name ){
			tester_printf(BELLE_SIP_LOG_MESSAGE, "Tester compiled without CU_get_suite() function, running all tests instead of suite '%s'", suite_name);
		}
#else
		if (suite_name){
			CU_pSuite suite;
			suite=CU_get_suite(suite_name);
			if (!suite) {
				tester_printf(LOG_ERROR, "Could not find suite '%s'. Available suites are:", suite_name);
				tester_list_suites();
				return -1;
			} else if (test_name) {
				CU_pTest test=CU_get_test_by_name(test_name, suite);
				if (!test) {
					tester_printf(LOG_ERROR, "Could not find test '%s' in suite '%s'. Available tests are:", test_name, suite_name);
					// do not use suite_name here, since this method is case sensitive
					tester_list_suite_tests(suite->pName);
					return -2;
				} else {
					CU_ErrorCode err= CU_run_test(suite, test);
					if (err != CUE_SUCCESS) tester_printf(LOG_ERROR, "CU_basic_run_test error %d", err);
				}
			} else {
				CU_run_suite(suite);
			}
		}
		else
#endif
		{
#if HAVE_CU_CURSES
			if (curses) {
			/* Run tests using the CUnit curses interface */
				CU_curses_run_tests();
			}
			else
#endif
			{
			/* Run all tests using the CUnit Basic interface */
				CU_run_all_tests();
			}
		}

	}
	ret=CU_get_number_of_tests_failed()!=0;

/* Redisplay list of failed tests on end */
	if (CU_get_number_of_failure_records()){
		CU_basic_show_failures(CU_get_failure_list());
		tester_printf(BELLE_SIP_LOG_MESSAGE,"");
	}

	CU_cleanup_registry();

	return ret;
}


void bc_tester_helper(const char *name, const char* additionnal_helper) {
	fprintf(stderr,"%s --help\n"
		"\t\t\t--list-suites\n"
		"\t\t\t--list-tests <suite>\n"
		"\t\t\t--suite <suite name>\n"
		"\t\t\t--test <test name>\n"
		"\t\t\t--log-file <output log file path>\n"
#if HAVE_CU_CURSES
		"\t\t\t--curses\n"
#endif
		"\t\t\t--xml\n"
		"\t\t\t--xml-file <xml file prefix (will be suffixed by '-Results.xml')>\n"
		"And additionally:\n"
		"%s"
		, name
		, additionnal_helper);
}

void bc_tester_init(void (*ftester_printf)(int level, const char *fmt, va_list args)) {
	tester_printf_va = ftester_printf;
}

int bc_tester_parse_args(int argc, char **argv, int argid)
{
	int i = argid;

	if (strcmp(argv[i],"--help")==0){
		return -1;
	} else if (strcmp(argv[i],"--test")==0){
		CHECK_ARG("--test", ++i, argc);
		test_name=argv[i];
	}else if (strcmp(argv[i],"--suite")==0){
		CHECK_ARG("--suite", ++i, argc);
		suite_name=argv[i];
	} else if (strcmp(argv[i],"--list-suites")==0){
		tester_list_suites();
		return -1;
	} else if (strcmp(argv[i],"--list-tests")==0){
		CHECK_ARG("--list-tests", ++i, argc);
		suite_name = argv[i];
		tester_list_suite_tests(suite_name);
		return -1;
	} else if (strcmp(argv[i], "--xml-file") == 0){
		CHECK_ARG("--xml-file", ++i, argc);
		xml_file = argv[i];
		xml_enabled = 1;
	} else if (strcmp(argv[i], "--xml") == 0){
		xml_enabled = 1;
	} else if (strcmp(argv[i],"--log-file")==0){
		CHECK_ARG("--log-file", ++i, argc);
		FILE *log_file=fopen(argv[i],"w");
		if (!log_file) {
			fprintf(stderr, "Cannot open file [%s] for writing logs because [%s]",argv[i],strerror(errno));
			return -2;
		} else {
			use_log_file=1;
			tester_printf(BELLE_SIP_LOG_MESSAGE,"Redirecting traces to file [%s]",argv[i]);
			// linphone_core_set_log_file(log_file);
		}
	}else {
		fprintf(stderr, "Unknown option \"%s\"\n", argv[i]);
		return -2;
	}

	if( xml_enabled && (suite_name || test_name) ){
		fprintf(stderr, "Cannot use both XML and specific test suite\n");
		return -2;
	}

	/* returns number of arguments read */
	return i - argid;
}

int bc_tester_start() {
	int ret;
	if( xml_enabled ){
		char * xml_tmp_file = malloc(sizeof(char) * (strlen(xml_file) + strlen(".tmp") + 1));
		snprintf(xml_tmp_file, sizeof(xml_tmp_file), "%s.tmp", xml_file);
		CU_set_output_filename(xml_tmp_file);
		free(xml_tmp_file);
	}

	ret = tester_run_tests(suite_name, test_name);

	return ret;
}
void bc_tester_add_suite(test_suite_t *suite) {
	if (test_suite == NULL) {
		test_suite = (test_suite_t **)malloc(10 * sizeof(test_suite_t *));
	}
	test_suite[nb_test_suites] = suite;
	nb_test_suites++;
	if ((nb_test_suites % 10) == 0) {
		test_suite = (test_suite_t **)realloc(test_suite, (nb_test_suites + 10) * sizeof(test_suite_t *));
	}
}

void bc_tester_uninit() {
	if( xml_enabled ){
		/*create real xml file only if tester did not crash*/
		char * xml_tmp_file = malloc(sizeof(char) * (strlen(xml_file) + strlen(".tmp") + 1));
		snprintf(xml_tmp_file, sizeof(xml_tmp_file), "%s.tmp", xml_file);
		rename(xml_tmp_file, xml_file);
		free(xml_tmp_file);
	}

	if (test_suite != NULL) {
		free(test_suite);
		test_suite = NULL;
		nb_test_suites = 0;
	}
}
