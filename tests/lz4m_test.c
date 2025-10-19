/*
 * Simple test program for LZ4M
 * Copyright (C) 2023
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/lz4m.h"

#define TEST_DATA_SIZE (10 * 1024 * 1024)  /* 10 MB */
#define COMPRESS_BOUND LZ4M_compressBound(TEST_DATA_SIZE)

/* Function to generate test data with 4-byte patterns */
static void generate_test_data(char* buffer, size_t size) {
    size_t i;
    uint32_t* ptr = (uint32_t*)buffer;
    
    /* Fill with repeating 4-byte patterns to test 4-byte granularity */
    for (i = 0; i < size / 4; i++) {
        if (i % 100 < 80) {
            /* 80% of the data is repeating patterns */
            ptr[i] = (i / 100) % 256;
        } else {
            /* 20% is random data */
            ptr[i] = rand();
        }
    }
}

/* Function to verify compression/decompression */
static int verify_compression(const char* source, int sourceSize) {
    char* compressed = (char*)malloc(COMPRESS_BOUND);
    char* decompressed = (char*)malloc(sourceSize);
    int compressedSize, decompressedSize;
    int result = 0;
    
    if (!compressed || !decompressed) {
        printf("Memory allocation error\n");
        result = 1;
        goto cleanup;
    }
    
    /* Compress data */
    compressedSize = LZ4M_compress_default(source, compressed, sourceSize, COMPRESS_BOUND);
    if (compressedSize <= 0) {
        printf("Compression failed: %d\n", compressedSize);
        result = 1;
        goto cleanup;
    }
    
    printf("Original size: %d bytes, Compressed size: %d bytes, Ratio: %.2f%%\n", 
           sourceSize, compressedSize, (compressedSize * 100.0) / sourceSize);
    
    /* Decompress data */
    decompressedSize = LZ4M_decompress_safe(compressed, decompressed, compressedSize, sourceSize);
    if (decompressedSize <= 0) {
        printf("Decompression failed: %d\n", decompressedSize);
        result = 1;
        goto cleanup;
    }
    
    /* Verify decompressed data matches original */
    if (decompressedSize != sourceSize) {
        printf("Decompressed size doesn't match original: %d != %d\n", decompressedSize, sourceSize);
        result = 1;
        goto cleanup;
    }
    
    if (memcmp(source, decompressed, sourceSize) != 0) {
        printf("Decompressed data doesn't match original\n");
        result = 1;
        goto cleanup;
    }
    
    printf("Verification successful: data correctly compressed and decompressed\n");
    
cleanup:
    free(compressed);
    free(decompressed);
    return result;
}

/* Compare LZ4M with original LZ4 */
static void compare_with_lz4(const char* source, int sourceSize) {
    /* This function would require linking with both LZ4 and LZ4M libraries */
    /* For simplicity, we'll just print a message */
    printf("Note: To compare LZ4M with LZ4, compile and link with both libraries\n");
}

int main(void) {
    char* testData;
    int result;
    
    /* Allocate and generate test data */
    testData = (char*)malloc(TEST_DATA_SIZE);
    if (!testData) {
        printf("Memory allocation error\n");
        return 1;
    }
    
    printf("Generating test data with 4-byte patterns...\n");
    generate_test_data(testData, TEST_DATA_SIZE);
    
    /* Test LZ4M compression/decompression */
    printf("\n=== Testing LZ4M compression and decompression ===\n");
    result = verify_compression(testData, TEST_DATA_SIZE);
    
    /* Compare with original LZ4 (placeholder) */
    printf("\n=== Comparison with original LZ4 ===\n");
    compare_with_lz4(testData, TEST_DATA_SIZE);
    
    /* Cleanup */
    free(testData);
    return result;
}