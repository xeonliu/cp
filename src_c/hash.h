#ifndef HASH_H
#define HASH_H

#include "common.h"

// MD5 context structure
typedef struct {
    unsigned int state[4];
    unsigned int count[2];
    unsigned char buffer[64];
} MD5_CTX;

// MD5 functions
void MD5Init(MD5_CTX *context);
void MD5Update(MD5_CTX *context, const unsigned char *input, unsigned int inputLen);
void MD5Final(unsigned char digest[16], MD5_CTX *context);

// High-level hash function using memory-mapped files for performance
bool ComputeFileHashFast(const wchar_t* filepath, unsigned char hash[HASH_SIZE]);

// Compare two hash values
bool HashesEqual(const unsigned char hash1[HASH_SIZE], const unsigned char hash2[HASH_SIZE]);

#endif // HASH_H
