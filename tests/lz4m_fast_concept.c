#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define BUFFER_SIZE (1024 * 1024)  // 1MB
#define PATTERN_SIZE 4  // 4-byte patterns

// Simplified 1-byte granularity compression (LZ4-like)
int compress_1byte_scan(const char* src, int srcSize, char* dst, int dstCapacity) {
    const char* ip = src;
    const char* const iend = src + srcSize;
    char* op = dst;
    const char* const oend = dst + dstCapacity;
    
    // Simple hash table for match finding
    const int hashTableSize = 4096;
    int hashTable[4096] = {0};
    
    while (ip < iend - 4) {  // Need at least 4 bytes for a match
        // Check for match
        uint32_t hash = (*(uint32_t*)ip * 2654435761U) >> 20;
        int matchPos = hashTable[hash & (hashTableSize-1)];
        hashTable[hash & (hashTableSize-1)] = (int)(ip - src);
        
        if (matchPos > 0 && (ip - src) - matchPos < 65536) {  // Valid match position
            const char* match = src + matchPos;
            
            // Check if match is valid (at least 4 bytes)
            if (memcmp(ip, match, 4) == 0) {
                // We found a match, encode it
                int matchLength = 4;
                while (ip + matchLength < iend && match + matchLength < ip && 
                       ip[matchLength] == match[matchLength] && matchLength < 255)
                    matchLength++;
                
                // Write match info (simplified format)
                if (op + 3 > oend) return 0;  // Not enough space
                *op++ = (char)matchLength;
                uint16_t offset = (uint16_t)((ip - src) - matchPos);
                memcpy(op, &offset, 2);
                op += 2;
                
                ip += matchLength;
                continue;
            }
        }
        
        // No match found - scan byte by byte
        ip += 1;
        
        // Simplified literal handling (just for demonstration)
        if (op >= oend) return 0;
        *op++ = *ip;
    }
    
    // Copy any remaining literals
    while (ip < iend && op < oend)
        *op++ = *ip++;
    
    return (int)(op - dst);
}

// Simplified 4-byte granularity compression with 1-byte scanning (our previous LZ4m)
int compress_1byte_scan_4byte_match(const char* src, int srcSize, char* dst, int dstCapacity) {
    const char* ip = src;
    const char* const iend = src + srcSize;
    char* op = dst;
    const char* const oend = dst + dstCapacity;
    
    // Simple hash table for match finding
    const int hashTableSize = 4096;
    int hashTable[4096] = {0};
    
    while (ip < iend - 4) {  // Need at least 4 bytes for a match
        // Check for match
        uint32_t hash = (*(uint32_t*)ip * 2654435761U) >> 20;
        int matchPos = hashTable[hash & (hashTableSize-1)];
        hashTable[hash & (hashTableSize-1)] = (int)(ip - src);
        
        if (matchPos > 0 && (ip - src) - matchPos < 65536) {  // Valid match position
            const char* match = src + matchPos;
            
            // Check if match is valid (at least 4 bytes)
            if (memcmp(ip, match, 4) == 0) {
                // We found a match, encode it
                int matchLength = 4;
                // Check matches in 4-byte increments
                while (ip + matchLength + 4 <= iend && match + matchLength + 4 <= ip && 
                       *(uint32_t*)(ip + matchLength) == *(uint32_t*)(match + matchLength) && 
                       matchLength < 252)
                    matchLength += 4;
                
                // Write match info (simplified format)
                if (op + 3 > oend) return 0;  // Not enough space
                *op++ = (char)matchLength;
                uint16_t offset = (uint16_t)((ip - src) - matchPos);
                memcpy(op, &offset, 2);
                op += 2;
                
                ip += matchLength;
                continue;
            }
        }
        
        // No match found - scan byte by byte
        ip += 1;
        
        // Simplified literal handling (just for demonstration)
        if (op >= oend) return 0;
        *op++ = *ip;
    }
    
    // Copy any remaining literals
    while (ip < iend && op < oend)
        *op++ = *ip++;
    
    return (int)(op - dst);
}

// Simplified 4-byte granularity compression with 4-byte scanning (paper's LZ4m)
int compress_4byte_scan_4byte_match(const char* src, int srcSize, char* dst, int dstCapacity) {
    const char* ip = src;
    const char* const iend = src + srcSize;
    char* op = dst;
    const char* const oend = dst + dstCapacity;
    
    // Simple hash table for match finding
    const int hashTableSize = 4096;
    int hashTable[4096] = {0};
    
    while (ip < iend - 4) {  // Need at least 4 bytes for a match
        // Check for match
        uint32_t hash = (*(uint32_t*)ip * 2654435761U) >> 20;
        int matchPos = hashTable[hash & (hashTableSize-1)];
        hashTable[hash & (hashTableSize-1)] = (int)(ip - src);
        
        if (matchPos > 0 && (ip - src) - matchPos < 65536) {  // Valid match position
            const char* match = src + matchPos;
            
            // Check if match is valid (at least 4 bytes)
            if (memcmp(ip, match, 4) == 0) {
                // We found a match, encode it
                int matchLength = 4;
                // Check matches in 4-byte increments
                while (ip + matchLength + 4 <= iend && match + matchLength + 4 <= ip && 
                       *(uint32_t*)(ip + matchLength) == *(uint32_t*)(match + matchLength) && 
                       matchLength < 252)
                    matchLength += 4;
                
                // Write match info (simplified format)
                if (op + 3 > oend) return 0;  // Not enough space
                *op++ = (char)matchLength;
                uint16_t offset = (uint16_t)((ip - src) - matchPos);
                memcpy(op, &offset, 2);
                op += 2;
                
                ip += matchLength;
                continue;
            }
        }
        
        // No match found - skip forward 4 bytes (paper's approach)
        ip += 4;
        
        // Simplified literal handling (just for demonstration)
        if (op + 4 > oend) return 0;
        memcpy(op, ip - 4, 4);
        op += 4;
    }
    
    // Copy any remaining literals
    while (ip < iend && op < oend)
        *op++ = *ip++;
    
    return (int)(op - dst);
}

// Function to generate test data with 4-byte patterns
void generate_test_data(char* buffer, int size) {
    srand(time(NULL));
    
    // Create data with repeating 4-byte patterns
    for (int i = 0; i < size; i += 4) {
        if (rand() % 10 < 8) {  // 80% chance of repeating a previous pattern
            int offset = (rand() % 100) * 4;  // Pick a pattern from recent history
            if (i >= offset) {
                memcpy(buffer + i, buffer + i - offset, 4);
            } else {
                // Generate a new random 4-byte pattern
                for (int j = 0; j < 4 && i + j < size; j++) {
                    buffer[i + j] = (char)(rand() % 256);
                }
            }
        } else {
            // Generate a new random 4-byte pattern
            for (int j = 0; j < 4 && i + j < size; j++) {
                buffer[i + j] = (char)(rand() % 256);
            }
        }
    }
}

int main() {
    char* srcBuffer = (char*)malloc(BUFFER_SIZE);
    char* dstBuffer1 = (char*)malloc(BUFFER_SIZE);
    char* dstBuffer2 = (char*)malloc(BUFFER_SIZE);
    char* dstBuffer3 = (char*)malloc(BUFFER_SIZE);
    
    if (!srcBuffer || !dstBuffer1 || !dstBuffer2 || !dstBuffer3) {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    // Generate test data with 4-byte patterns
    generate_test_data(srcBuffer, BUFFER_SIZE);
    
    // Compress with different approaches
    clock_t start, end;
    double time_used;
    
    // 1-byte scan (LZ4-like)
    start = clock();
    int compSize1 = compress_1byte_scan(srcBuffer, BUFFER_SIZE, dstBuffer1, BUFFER_SIZE);
    end = clock();
    time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("1-byte scan (LZ4-like):\n");
    printf("  Compressed size: %d bytes (%.2f%%)\n", compSize1, (float)compSize1 * 100 / BUFFER_SIZE);
    printf("  Compression time: %.6f seconds\n\n", time_used);
    
    // 1-byte scan, 4-byte match (our previous LZ4m)
    start = clock();
    int compSize2 = compress_1byte_scan_4byte_match(srcBuffer, BUFFER_SIZE, dstBuffer2, BUFFER_SIZE);
    end = clock();
    time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("1-byte scan, 4-byte match (our previous LZ4m):\n");
    printf("  Compressed size: %d bytes (%.2f%%)\n", compSize2, (float)compSize2 * 100 / BUFFER_SIZE);
    printf("  Compression time: %.6f seconds\n\n", time_used);
    
    // 4-byte scan, 4-byte match (paper's LZ4m)
    start = clock();
    int compSize3 = compress_4byte_scan_4byte_match(srcBuffer, BUFFER_SIZE, dstBuffer3, BUFFER_SIZE);
    end = clock();
    time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("4-byte scan, 4-byte match (paper's LZ4m):\n");
    printf("  Compressed size: %d bytes (%.2f%%)\n", compSize3, (float)compSize3 * 100 / BUFFER_SIZE);
    printf("  Compression time: %.6f seconds\n\n", time_used);
    
    // Compare results
    printf("Compression ratio comparison:\n");
    printf("  1-byte scan vs 1-byte scan, 4-byte match: %.2f%%\n", 
           (float)(compSize1 - compSize2) * 100 / compSize1);
    printf("  1-byte scan vs 4-byte scan, 4-byte match: %.2f%%\n", 
           (float)(compSize1 - compSize3) * 100 / compSize1);
    printf("  1-byte scan, 4-byte match vs 4-byte scan, 4-byte match: %.2f%%\n", 
           (float)(compSize2 - compSize3) * 100 / compSize2);
    
    free(srcBuffer);
    free(dstBuffer1);
    free(dstBuffer2);
    free(dstBuffer3);
    
    return 0;
}