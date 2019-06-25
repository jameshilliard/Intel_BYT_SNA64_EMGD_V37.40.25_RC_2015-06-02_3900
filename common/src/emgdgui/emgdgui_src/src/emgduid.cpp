/* Copyright (c) 2002-2014, Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "emgduid.h"

struct MHD_Daemon *my_daemon;
extern map_t map;

const char *page_index  = 
	/* REPLACE_INDEX_HTML  */

int helper_send_HTTP_response_and_free_malloc(char* mstr, struct MHD_Connection *connection)
{
	struct MHD_Response *response;
	int ret;
	response = MHD_create_response_from_buffer (strlen (mstr), mstr,
                                              MHD_RESPMEM_MUST_FREE);
	if (response == NULL) {
		free (mstr);
		return MHD_NO; 
	}
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	return ret;
}

int helper_send_HTTP_response_cpp_string(string cppstr, struct MHD_Connection *connection)
{
	struct MHD_Response *response;
	int ret;
	response = MHD_create_response_from_buffer (strlen (cppstr.c_str()), (char*) cppstr.c_str(),
                                              MHD_RESPMEM_MUST_COPY);
	if (response == NULL) {
		return MHD_NO;
	}
	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);
	return ret;
}

/* str must be a string representation of a non-negative int 
 * Returns -1 otherwise
 */
long strtoindex(char* str, double optionalMultiplier)
{
	char *end;
	long ret = -1;
	if (str != NULL && strlen(str) > 0) {
		errno = 0;    /* To distinguish success/failure after call */
		if (optionalMultiplier != 0 && optionalMultiplier != 1) {
			ret = (optionalMultiplier*strtod(str, &end)) + 0.5;
		} else {
			ret = strtoul(str, &end, 0);
		}
		if (errno == 0 && *end == '\0') {
			//success
		} else { 
			ret = -1;
		}
	}
	return ret;	
}

long strtolong(const char* str, long default_val) {
	char *end;
	unsigned long ret;	
	if (str != NULL && strlen(str) > 0) {
		errno = 0;    /* To distinguish success/failure after call */
		ret = strtoul(str, &end, 0);
		if (errno == 0 && *end == '\0') {
			return ret;	
		} 
	}
	return default_val;
}

double strtointeger(const char* str, int default_val) {
	if(str != NULL && strlen(str) > 0) {
		return atof(str);
	}
	return default_val;
}


string getLineFromFile(const char *filePath) {
	string line;
	ifstream myfile (filePath);
	if (myfile.is_open()) {
		getline (myfile,line);
		myfile.close();
	}
	return line;
}

int invalidKeyHTML(struct MHD_Connection *connection) {
	string s;
	s = "<HTML><p>Invalid key</p>\n";
	s +="<a href='/default'>Return</a></HTML>";

	return helper_send_HTTP_response_cpp_string(s, connection);
}

static int f_default(struct MHD_Connection *connection, void *cls) {
	char *key = (char*) cls;
	string s;

	if (key == NULL)
		return f_main(connection, cls);

	s = "<HTML><p>Secure key required. Please enter key phrase<p>\n";
	s += "<form method='GET' action='main'>\n";
	s += "<input type='text' name='key'/><br>\n";
	s += "<input type='submit' value='Submit'/>\n";
	s += "</form></HTML>";

	return helper_send_HTTP_response_cpp_string(s, connection);
}

static const URL_TABLE_STRUCT urlTable[] = {
	{"/main", &f_main},
	{"/crtc", &f_crtc},
	{"/color", &f_color},
	{"/plane", &f_plane},
	{"/xr", &f_xrandr}
};

static ssize_t read_file_block(void *file_ptr, uint64_t pos, char *buffer, size_t max)
{
	fseek((FILE*)file_ptr, pos, SEEK_SET);
	return fread(buffer, 1, max, (FILE*)file_ptr);
}

static void callback_close_file(void *file_ptr)
{
	fclose ((FILE*) file_ptr);
}

static int established_connection(void *cls,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data, size_t *upload_data_size, void **ptr)
{
	static int aptr;
	struct MHD_Response *response;
	int ret, i,pageCount;
	FILE *file;
	struct stat buf;

	if (0 != strcmp (method, MHD_HTTP_METHOD_GET)) {
		//printf("Will ignore any %s data\n", method);
		//return MHD_NO;              /* unexpected method */
	}
	if (&aptr != *ptr) {
		/* do never respond on first call */
		*ptr = &aptr;
		return MHD_YES;
	}
	*ptr = NULL;                  /* reset when done */

	//First see if need to serve a file
	if (0 == stat (&url[1], &buf))
		file = fopen (&url[1], "rb");
	else
		file = NULL;
	if (file != NULL) {
		response = MHD_create_response_from_callback (buf.st_size, 32 * 1024,     /* 32k page size */
                                        &read_file_block,
                                        file,
                                        &callback_close_file);
		if (response == NULL) {
			fclose (file);
			return MHD_NO;
		}
		ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
		MHD_destroy_response (response);
		return ret;
	}

	// See if URL matches internal function
	pageCount = sizeof(urlTable) / sizeof(URL_TABLE_STRUCT);  
	for (i=0; i<pageCount; i++) {
		if (strcmp(url,urlTable[i].functionname) == 0)
			return urlTable[i].urlFunction(connection, cls);
	} 

	// default when no match
	return f_default(connection, cls);
}

void signal_exit(int num) {
	printf("\nCleaning up and exiting. (Caught signal %d)\n", num);
	MHD_stop_daemon (my_daemon);
	exit(num);
}

int main (int argc, char *const *argv) {
	char *key = NULL;
	string input_line;

	if (argc > 2) {
		printf("Only 1 argument expected. Syntax:\n");
		printf("  %s <optional key phrase>\n     or\n", argv[0]);
		printf("  %s --no-key\n", argv[0]);
		return 0;
	} else if (argc == 2) {
		if (strstr(argv[1], "--no-key") == NULL) {
			key = argv[1];
		}
	} else {
		printf("Please enter a security key (or just press enter for no key)\n");
		getline(cin, input_line);
		key =(char*) input_line.c_str();
		if (key !=NULL && key[0] == '\0') {
			key = NULL;
			printf("Security warning:\n\tNo key provided. Any user can connect to the daemon\n");
		}
	}
	printf("Daemon started. Open a browser to localhost:%d\n", PORT);
	
	/* Set up structure to only allow local incoming connections */
	struct sockaddr_in ip_addr;
	memset (&ip_addr, 0, sizeof (struct sockaddr_in));
	ip_addr.sin_family = AF_INET;
	ip_addr.sin_port = htons (PORT);
	inet_pton (AF_INET, "127.0.0.1", &ip_addr.sin_addr);

	my_daemon = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG,
			PORT, NULL, NULL, 
			&established_connection, key, 
			MHD_OPTION_SOCK_ADDR, &ip_addr, /* comment out this line to allow non-local connections */
			MHD_OPTION_END);
	if (my_daemon == NULL)
		return 1;

	//TODO evaluate sigprocmask
	signal (SIGINT, signal_exit);
	signal (SIGTERM, signal_exit);
	//system("firefox localhost:8888");

	while (1) {
		sleep(10);
	}

	return 0;
}

