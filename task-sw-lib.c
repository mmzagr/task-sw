#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "task-sw.h"


int is_valid_ipv4(const char *ipv4, const char *msg) {
    struct in_addr addr;

    if (inet_pton(AF_INET, ipv4, &addr) != 1) {
        fprintf(stderr, "IPV4 address: %s is not valid", msg);
        return 0;
    }
    return 1;
}

unsigned short str_to_u_short(const char *str) {
    char *endptr;
    long ul_value = strtol(str, &endptr, 10);

    // Check if conversion was successful
    if (*endptr != '\0' || ul_value <= 0 || ul_value > USHRT_MAX) {
        return 0;
    }

    return (unsigned short)ul_value;
}

//return the context of the buffer in HEX format (in an allocated memory)
char *buf_to_hex_str(const char *buffer, size_t length) {
    char *dst = calloc(length * 3 + 2, sizeof(char));

    strcpy(dst, "[");
    for (size_t i = 0; i < length; ++i) {
        sprintf(dst + strlen(dst), "%02X%c", buffer[i], i == length - 1 ? '\0' : ' ');
    }
    strcpy(dst + strlen(dst), "]");
    return dst;
}
