#include <stdlib.h>
#include <immintrin.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>



/*write blazing fast checksum function-MUST outpace fastest nvme io speeds*/
#pragma once

void cpuid(int info[4], int infoType);
int is_avx_supported(void);
void init_crc32_table(void);
uint32_t crc32(const void *data, size_t length);
uint32_t crc32_avx2(const uint8_t *data, size_t length);
bool verify_data(const uint8_t *data, size_t len, uint32_t checksum);
