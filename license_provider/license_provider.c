
#include <license.h>

#define ERDFILE 0x0001
#define EEXTRAC 0x0002


//global variables
const char *prov_pri_pem = "provider.pem";
const char *prov_pub_pem = "public_provider.pem";
const char *client_lic = "client.lic";
char *cli_pri_pem = NULL;
char *cli_pub_pem = NULL;

const char *begin_session = "---BEGIN SESSION KEY---";
const char *end_session = "---END SESSION KEY---";
const char *begin_public = "-----BEGIN RSA PUBLIC KEY-----";
const char *end_public = "-----END RSA PUBLIC KEY-----";
const char *begin_license = "---BEGIN LICENSE---";
const char *end_license = "---END LICENSE---";
const char *begin_license_sha_a = "---BEGIN SHA1 A---";
const char *end_license_sha_a = "---END SHA1 A---";
const char *begin_license_sha_b = "---BEGIN SHA1 B---";
const char *end_license_sha_b = "---END SHA1 B---";

PLICENSE_SHA_STRUCT license = NULL;
RSA *rsa_provider = NULL;
RSA *rsa_client = NULL;

unsigned char *session_key = NULL;
unsigned char *license_buffer = NULL;
unsigned char *license_buffer_ex = NULL;
unsigned char *message_buffer = NULL;
unsigned char *license_sha_a = NULL;
unsigned char *license_sha_b = NULL;

char license_day[10];

void program_usage() {
	printf("usage:\nlicense_manager [test | day_count]\n");
	exit(1);
}

void program_exit(int exit_code) {

	if (rsa_provider) RSA_free(rsa_provider);
	if (rsa_client) RSA_free(rsa_client);

	if (session_key) free(session_key);
	if (message_buffer) free(message_buffer);
	if (license_sha_a) free(license_sha_a);
	if (license_sha_b) free(license_sha_b);
	if (license_buffer) free(license_buffer);
	if (license_buffer_ex) free(license_buffer_ex);

	crypto_final();

	printf("program terminated with code (%d).\n", exit_code);

	exit(exit_code);
}

void provider_private_key_load() {
	rsa_provider = rsa_privatekey_load_from_file(prov_pri_pem);
	publickey_write_to_file(prov_pub_pem, rsa_provider);
}

int load_from_file(const char *fname, unsigned char **buffer) {
 
        if (!buffer)
                exit_on_error(-ERDFILE);
                
        FILE *file = fopen(fname, "r");
        if (!file)
                exit_on_error(-ERDFILE);
 
        if (fseek(file, 0, SEEK_END)) {
                fclose(file);
                exit_on_error(-ERDFILE);
        }

        int flen = ftell(file);
        rewind(file);

        reallocate(buffer, flen + 1); 
        fread(buffer, flen, 1, file); 
        fclose(file);
 
        buffer[flen] = 0;
 
        return flen;
}
 
void load_message_from_client() {
	load_from_file(client_lic, &message_buffer);
}

char *sub_value_extract_trim(const char *lic_client, 
                        const char *begin_title, const char *end_title) {

	char *p_begin_title = strstr(lic_client, begin_title);
	if (!p_begin_title)
		exit_on_error(-EEXTRAC);

	char *p_end_title = strstr(lic_client, end_title);
	if (!p_end_title)
		exit_on_error(-EEXTRAC);

	int i = 0,
		j = 0,
		pos = strlen(begin_title),
		len = p_end_title - p_begin_title - pos;
	char *key = malloc(len + 1);
	memset(key, 0, len + 1);
	while (i < len) {
		if (p_begin_title[pos + i] != '\n')
			key[j++] = p_begin_title[pos + i];
		i++;
	}
 
	return key;

}

char *sub_value_extract(const char *lic_client, 
                        const char *begin_title, const char *end_title) {
 
	char *p_begin_title = strstr(lic_client, begin_title);
	if (!p_begin_title)
	        exit_on_error(-EEXTRAC);
 
	char *p_end_title = strstr(lic_client, end_title);
	if (!p_end_title)
	        exit_on_error(-EEXTRAC);

	int i = 0,
		j = 0,
		pos = strlen(begin_title),
		len = p_end_title - p_begin_title - pos;
	char *key = malloc(len + 1);
	memset(key, 0, len + 1);
	while (i < len)
		key[j++] = p_begin_title[pos + i++];

	return key;

}

void session_key_parse() {
	unsigned char *base64_buffer = NULL;
	unsigned char *enc_buffer = NULL;
	int len;

	base64_buffer = sub_value_extract_trim((const char *)message_buffer, 
                                        begin_session, end_session);
	if (base64_buffer) {
		len = base64_decode(base64_buffer, &enc_buffer);
		if (len) {
			session_key = malloc(len);
			len = private_decrypt_base64_buffer(enc_buffer, 
                                                &session_key, rsa_provider);
			if (len)
				session_key[len] = 0;
			free(enc_buffer);
		}
		free(base64_buffer);
	}

	if (!session_key)
		program_exit(3);

}

void client_public_key_parse() {
        char *base64_buffer = NULL;
        char *pem_temp_client = "pub_temp_client.pem";
	
        base64_buffer = sub_value_extract((const char *)message_buffer, 
                                        begin_public, end_public);
        if (base64_buffer) {
                FILE *fd = fopen(pem_temp_client, "w");
                if (fd) {
			fputs(begin_public, fd);
			fputs(base64_buffer, fd);
			fputs(end_public, fd);
			fputs("\n", fd);
			fclose(fd);
			rsa_client = rsa_publickey_load_from_file(pem_temp_client);
		}
		free(base64_buffer);
	}

	if (!rsa_client)
		program_exit(4);

}

void license_message_parse() {
	char *b64_buffer = NULL;
	unsigned char *enc_buffer = NULL;

	b64_buffer = sub_value_extract_trim((const char *)message_buffer, begin_license, end_license);
	if (!b64_buffer)
		program_exit(5);

	int len = base64_decode((unsigned char *)b64_buffer, &enc_buffer);
	if (len) {
		len = decrypt(enc_buffer, len, &license_buffer, session_key);
		if (len)
			license_buffer[len] = 0;
		free(enc_buffer);
	}
	free(b64_buffer);

	if (!license_buffer)
		program_exit(5);

	char lbuffer[4096];

	if (strlen(license_day) > 0)
		sprintf(lbuffer, "{\"Type\":\"DEMO\",\"ValidFor\":%s,%s", license_day, &license_buffer[1]);
	else
		sprintf(lbuffer, "{\"Type\":\"LIFETIME\",%s", &license_buffer[1]);

	len = strlen(lbuffer);

	license_buffer_ex = malloc(len + 1);
	memset(license_buffer_ex, 0, len + 1);
	strcpy((char *)license_buffer_ex, lbuffer);

}

void sha_license_buffer() {
	unsigned char *sha_buffer = NULL;
	unsigned char *base64_provider_enc_buffer = NULL;
	unsigned char half_buffer[512];

	sha256(license_buffer_ex, strlen((char *)license_buffer_ex), 
                        &sha_buffer);
	
	int elen = private_encrypt_base64_buffer(
                        strlen((char *)sha_buffer), sha_buffer, 
                        &base64_provider_enc_buffer, rsa_provider);
	if (elen < 1) {
		print_last_error();
		free(sha_buffer);
		program_exit(7);
	}

	free(sha_buffer);

	int slen = strlen((char *)base64_provider_enc_buffer);
	int blen = slen / 2;

	memset(half_buffer, 0, 512);
	memcpy(half_buffer, base64_provider_enc_buffer, blen);

	elen = public_encrypt_base64_buffer(strlen((char *)half_buffer), 
                                half_buffer, &license_sha_a, rsa_client);
	if (elen < 1) {
		print_last_error();
		free(base64_provider_enc_buffer);
		program_exit(7);
	}

	memset(half_buffer, 0, 512);
	memcpy(half_buffer, base64_provider_enc_buffer + blen, slen - blen);

	elen = public_encrypt_base64_buffer(strlen((char *)half_buffer), 
                                half_buffer, &license_sha_b, rsa_client);

	free(base64_provider_enc_buffer);

	if (elen < 1) {
		print_last_error();
		program_exit(7);
	}

}

void client_license_write() {

	FILE *fd = fopen("client.license", "w");

	if (fd) {
		fputs((const char *)license_buffer_ex, fd);
		fputs("\n", fd);
		fputs(begin_license_sha_a, fd);
		fputs("\n", fd);
		base64_write_to_file(license_sha_a, fd);
		fputs(end_license_sha_a, fd);
		fputs("\n", fd);
		fputs(begin_license_sha_b, fd);
		fputs("\n", fd);
		base64_write_to_file(license_sha_b, fd);
		fputs(end_license_sha_b, fd);
		fputs("\n", fd);

		fclose(fd);
	}

}

void newline_trim(char *p) {
	int i = strlen(p);
	while ('\n' == p[i - 1] || '\r' == p[i - 1])
		p[--i] = 0;
}

void client_license_test() {

	FILE *fd = fopen("client.license", "r");
	if (fd) {

		unsigned char license[4096];
		unsigned char line[96];
		unsigned char sha_a[1024];
		unsigned char sha_b[1024];

		memset(license, 0, 4096);
		memset(sha_a, 0, 1024);
		memset(sha_b, 0, 1024);

		if (fgets((char *)license, 4096, fd)) {

			newline_trim((char *)license);

			short isSha_a = 0;
			short isSha_b = 0;
			short bsha_alen = strlen(begin_license_sha_a);
			short bsha_blen = strlen(begin_license_sha_b);
			short esha_alen = strlen(end_license_sha_a);
			short esha_blen = strlen(end_license_sha_b);

			while (!feof(fd) && fgets((char *)line, 96, fd)) {

				if (!memcmp(line, begin_license_sha_a, bsha_alen)) {
					isSha_a = 1;
					isSha_b = 0;
					continue;
				}
				else if (!memcmp(line, begin_license_sha_b, bsha_blen)) {
					isSha_a = 0;
					isSha_b = 1;
					continue;
				}
				else if (!memcmp(line, end_license_sha_a, esha_alen)) {
					isSha_a = 0;
					isSha_b = 0;
					continue;
				}
				else if (!memcmp(line, end_license_sha_b, esha_blen)) {
					isSha_a = 0;
					isSha_b = 0;
					continue;
				}

				newline_trim((char *)line);

				if (isSha_a)
					strcat((char *)sha_a, (const char *)line);
				else if (isSha_b)
					strcat((char *)sha_b, (const char *)line);

			}

		}

		fclose(fd);

		license_sha(license, sha_a, sha_b);
		printf("TEST => OK\n");

	}




}

int main(int argc, char **argv)
{

        crypto_init();

        if (argc == 2) {
                if (!memcmp("test", argv[1], 4)) {
                	        client_license_test();
                	        program_exit(0);
                }

                int test_day = atoi(argv[1]);
                if ( test_day <= 0 || test_day > 90 ) {
                        program_usage();
                        program_exit(0);
                }
                snprintf(license_day, 10, "%d", test_day);
        }

        provider_private_key_load();

        load_message_from_client();

        session_key_parse();

        client_public_key_parse();

        license_message_parse();

        sha_license_buffer();

        client_license_write();

        client_license_test();

        program_exit(0);
}



/*


-#include <stdlib.h>
-#include <stdio.h>
-#include <sys/stat.h>
-#include <memory.h>
-#include <string.h>
-
 #include "license.h"
 
-void __stdcall lib_initialize() {
-        ERR_load_crypto_strings();
-        OpenSSL_add_all_algorithms();
-        OPENSSL_config(NULL);
-        srand(time(0));
-}
-
-void __stdcall lib_finalize() {
-        EVP_cleanup();
-        CRYPTO_cleanup_all_ex_data();
-        ERR_free_strings();
+extern const char *prov_pri_pem;
+extern const char *prov_pub_pem;
+extern const char *cli_pri_pem;
+extern const char *cli_pub_pem;
+
+unsigned char *sha256(const unsigned char *source, const int slen, 
+                        unsigned char **target) {
+	
+        unsigned int md_len = 0;
+        EVP_MD_CTX *mdctx;
+        const EVP_MD *md;
+        unsigned char sha_buffer[EVP_MAX_MD_SIZE];
+         
+        if (!source || !target || slen < 1)
+	        exit_on_error(ESHAFAIL);
+
+        md = EVP_get_digestbyname("SHA256");
+
+        if(!md) exit_on_error(ESHAFAIL);
+
+        mdctx = EVP_MD_CTX_new();
+        EVP_DigestInit_ex(mdctx, md, NULL);
+        EVP_DigestUpdate(mdctx, source, slen);
+        EVP_DigestFinal_ex(mdctx, sha_buffer, &md_len);
+        EVP_MD_CTX_free(mdctx);
+ 
+	return base64_encode(sha_buffer, md_len, target);
 }
 
-void __stdcall exit_on_error_m(const char *file_name, const char *function_name, const int line_number) {
-        printf("vvv program is stopped on error vvv\n(%s(%d):%s)\n", file_name, line_number, function_name);
-        program_exit(-0x1000);
-}
-
-void __stdcall print_last_error_m(const char *file_name, const char *function_name, const int line_number) {
-        char last_error[4000];
-        ERR_error_string(ERR_get_error(), last_error);
-        printf("vvv crypto last error vvv\n(%s(%d):%s)\n\t%s\n", file_name, line_number, function_name, last_error);
-}
-
-PLICENSE_STRUCT __stdcall license_init(int argc, const char **argv) {
-        PLICENSE_STRUCT license;
-        PSERVICE_STRUCT nipps;
-
-        if (!argc || !argv)
-                exit_on_error();
-
-        nipps = malloc(sizeof(SERVICE_STRUCT) * argc - 3);
-
-        int i = 3, k = 0;
-        char *p = NULL;
-        for (; i < argc; i++) {
-                p = strstr(argv[i], ":");
-                if (!p)
-                        continue;
-
-                nipps[i - 3].service_name = malloc(p - argv[i] + 1);
-                memset(nipps[i - 3].service_name, 0, p - argv[i] + 1);
-                strncpy(nipps[i - 3].service_name, argv[i], p - argv[i]);
+void license_sha_initialize(PLICENSE_SHA_STRUCT plsha) {
 
-                nipps[i - 3].service_version = malloc(strlen(p));
-                memset(nipps[i - 3].service_version, 0, strlen(p));
-                strcpy(nipps[i - 3].service_version, p + 1);
+	if (!plsha)
+		exit_on_error(ELICINIT);
 
-                k++;
-        }
+	memset(plsha, 0, sizeof(LICENSE_SHA_STRUCT));
 
-        license = malloc(sizeof(LICENSE_STRUCT));
+	plsha->pub_provider = rsa_publickey_read_from_file(prov_pub_pem);
+	plsha->pri_client = rsa_privatekey_read_from_file(cli_pri_pem);
 
-        license->license_acquirer = "CLIENT";
-        license->license_service_size = k;
-        license->license_version = malloc(strlen(argv[2]) + 1);
-        memset(license->license_version, 0, strlen(argv[2]) + 1);
-        strcpy(license->license_version, argv[2]);
-        license->license_issuer = "TECHNICIAN";
-        license->license_services = nipps;
-
-        return license;
-}
-
-void __stdcall license_free(PLICENSE_STRUCT plicense) {
-
-	if (!plicense)
-	        exit_on_error();
-
-	free(plicense->license_services);
-	free(plicense);
 }
 
-int __stdcall license_size(PLICENSE_STRUCT plicense) {
-	int len = 0;
-	if (!plicense)
-	        exit_on_error();
+void license_sha_finalize(PLICENSE_SHA_STRUCT plsha) {
 
-	char *license_json = "{'Version':'','LicensedTo':'','IssuedBy':'','Services':[]}";
-	char *license_services_json = "{'Name':'','Version':''},";
+	if (!plsha)
+	        exit_on_error(ELICFINL);
 
-	len += strlen(plicense->license_issuer);
-	len += strlen(plicense->license_acquirer);
-	len += strlen(plicense->license_version);
+	if (plsha->source_sha) free(plsha->source_sha);
+	if (plsha->sha) free(plsha->sha);
 
-	len += 2; //plicense->Service_Count
+	if (plsha->pub_provider) RSA_free(plsha->pub_provider);
+	if (plsha->pri_client) RSA_free(plsha->pri_client);
 
-	int i = 0;
-	for(; i < plicense->license_service_size; i++) {
-		len += strlen(plicense->license_services[i].service_name);
-		len += strlen(plicense->license_services[i].service_version);
-	}
 
-	return len + strlen(license_json) + plicense->license_service_size * strlen(license_services_json);
 }
 
-char *__stdcall license_to_json_string(PLICENSE_STRUCT plicense, char **slicense) {
-
-	if (!plicense || !slicense)
-		exit_on_error();
-
-	int slen = license_size(plicense);
-	if (!(*slicense))
-		*slicense = malloc((slen));
-	memset(*slicense, 0, slen);
-
-	sprintf(*slicense,
-		"{\"Version\":\"%s\",\"LicensedTo\":\"%s\",\"IssuedBy\":\"%s\",\"Services\":["
-		, plicense->license_version
-		, plicense->license_acquirer
-		, plicense->license_issuer
-		);
-
-	int i, pos = strlen(*slicense);
-
-	for (i = 0; i < plicense->license_service_size; i++)
-	{
-		sprintf(*slicense + pos
-			, "{\"Name\":\"%s\",\"Version\":\"%s\"}"
-			, plicense->license_services[i].service_name
-			, plicense->license_services[i].service_version);
-		pos = strlen(*slicense) ;
-		if (i < plicense->license_service_size - 1)
-			sprintf(*slicense + pos++, ",");
-	}
+void license_sha_decrypt(PLICENSE_SHA_STRUCT plsha, unsigned char *sha_a, 
+                        unsigned char *sha_b) {
 
-	pos = strlen(*slicense);
-	sprintf(*slicense + pos, "]}");
+	if (!plsha)
+		exit_on_error(ELICUPDT);
 
-	return *slicense;
-}
+	unsigned char *dec_shaa = NULL;
+	int alen = private_decrypt_base64_buffer(sha_a, &dec_shaa, 
+                                                plsha->pri_client);
+	
+	unsigned char *dec_shab = NULL;
+	int blen = private_decrypt_base64_buffer(sha_b, &dec_shab, 
+                                                plsha->pri_client);
 
-void __stdcall license_print(PLICENSE_STRUCT plicense) {
- 	if (!plicense)
-		exit_on_error();
-	char *buffer = NULL;
-	printf("\n%s\n", license_to_json_string(plicense, &buffer));
-	free(buffer);
-}
+	unsigned char *sha = malloc(alen + blen + 1);
+	memset(sha, 0, alen + blen + 1);
+	memcpy(sha, dec_shaa, alen);
+	memcpy(sha + alen, dec_shab, blen);
 
-byte *__stdcall reallocate(byte **b, int blen) {
-	if (!b)
-		exit_on_error();
-
-	if(*b)
-		free(*b);
-	*b = malloc(blen);
-	memset(*b, 0, blen);
-	return *b;
-}
+	free(dec_shaa);
+	free(dec_shab);
 
-byte *__stdcall load_from_file(const char *fname) {
+	public_decrypt_base64_buffer(sha, &plsha->sha, plsha->pub_provider);
+	free(sha);
 
-	FILE *file = fopen(fname, "r");
-	if (!file)
-		exit_on_error();
-
-	if (fseek(file, 0, SEEK_END)) {
-		fclose(file);
-		exit_on_error();
-	}
-
-	int flen = ftell(file);
-	rewind(file);
-
-	byte *message = malloc(flen + 1);
-
-	fread(message, flen, 1, file);
-
-	fclose(file);
-
-	message[flen] = 0;
-
-	return message;
 }
 
-
+void license_sha(unsigned char *source, unsigned char *sha_a, 
+                unsigned char *sha_b) {
 
-
 * 

*/