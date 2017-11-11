#include <stdlib.h>
#include <string.h>

#include "license.h"

int crypto_check(byte *source, int slen, byte **target, byte *session_key) {
        if (!source || !slen)
		return 0;

	if (!target)
		return 0;

	if (!session_key)
		return 0;

        return 1;
}

int __stdcall encrypt(byte *source, int slen, byte **target, byte *session_key) {
	EVP_CIPHER_CTX *ctx;
	int rc, tlen, flen;

	if (!crypto_check(source, slen, target, session_key))
		return 0;

	ctx = EVP_CIPHER_CTX_new();

	rc = EVP_EncryptInit_ex(ctx, EVP_bf_cbc(), NULL, session_key, NULL);
	if (!rc) {
		print_last_error();
                EVP_CIPHER_CTX_free(ctx);
		return 0;
	}

	tlen = slen + slen % EVP_CIPHER_block_size(ctx) + 1;
	reallocate(target, tlen + 1);

	rc = EVP_EncryptUpdate(ctx, *target, &tlen, source, slen);
	if (rc)
                rc = EVP_EncryptFinal_ex(ctx, (*target) + tlen, &flen);

	if (!rc) {
		print_last_error();
		EVP_CIPHER_CTX_free(ctx);
		return 0;
	}

	tlen += flen;

	EVP_CIPHER_CTX_free(ctx);

	return tlen;
}

int __stdcall decrypt(byte *source, int slen, byte **target, byte *session_key) {
	EVP_CIPHER_CTX *ctx;
	int rc, tlen, flen;

	if (!crypto_check(source, slen, target, session_key))
		return 0;

	ctx = EVP_CIPHER_CTX_new();

	rc = EVP_DecryptInit_ex(ctx, EVP_bf_cbc(), NULL, session_key, NULL);
	if (!rc) {
		print_last_error();
                EVP_CIPHER_CTX_free(ctx);
		return 0;
	}

	tlen = slen + slen % EVP_CIPHER_block_size(ctx) + 1;
	reallocate(target, tlen + 1);

	rc = EVP_DecryptUpdate(ctx, *target, &tlen, source, slen);
	if (rc)
	       rc = EVP_DecryptFinal_ex(ctx, (*target) + tlen, &flen);

	if (!rc) {
		print_last_error();
		EVP_CIPHER_CTX_free(ctx);
		return 0;
	}

	tlen += flen;

	EVP_CIPHER_CTX_free(ctx);

	memset((*target) + tlen, 0, 1);

	return tlen;
}