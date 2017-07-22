#ifndef OPENSSL_SIGN_H
#define OPENSSL_SIGN_H

#include <openssl/evp.h>
#include "openssl_sign.h"
#include "define.h"

#define SENDER_MODE 0
#define RECEIVER_MODE 1

struct open_ssl_keys {
	char path_To_Verifying_Key[KEY_PATHLENGTH_MAX+1];
	char path_To_Signing_Key[KEY_PATHLENGTH_MAX+1];
	char pcszPassphrase[1024];
	int generate_keys;
	int sender_or_receiver_mode; 
};

/* Returns 0 for success, non-0 otherwise */
int make_keys(EVP_PKEY** skey, EVP_PKEY** vkey, struct open_ssl_keys *lanbeacon_keys);

/* Prints a buffer to stdout. Label is optional */
void print_it(const char* label, const unsigned char* buff, size_t len);

int passwd_callback(char *pcszBuff,int size,int rwflag, void *pPass);

int signlanbeacon(	unsigned char** sig, size_t* slen, const unsigned char* msg, 
					size_t qqlen, struct open_ssl_keys *lanbeacon_keys);

int read_keys(EVP_PKEY** skey, EVP_PKEY** vkey, struct open_ssl_keys *lanbeacon_keys);

int verifylanbeacon(const unsigned char* msg, size_t mlen, 
					struct open_ssl_keys *lanbeacon_keys);

#endif
