/* $Id$ */

/*
 * Copyright (c) 2001-2004 Aaron Turner, Matt Bing.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright owners nor the names of its
 *    contributors may be used to endorse or promote products derived from 
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "defines.h"
#include "common.h"

#ifdef DEBUG
extern int debug;
#endif

/* 
 * this is wrapped up in a #define safe_malloc
 * This function, detects failures to malloc memory and zeros out the
 * memory before returning
 */

void *
_our_safe_malloc(size_t len, const char *funcname, const int line, const char *file)
{
    u_char *ptr;

    if ((ptr = malloc(len)) == NULL)
        errx(1, "safe_malloc() error: Unable to malloc %d bytes\n"
            "%s:%s() line %d\n", len, file, funcname, line);
    
    /* zero memory */
    memset(ptr, 0, len);
    
#ifdef DEBUG
    /* wrapped inside an #ifdef for better performance */
    dbg(4, "Malloc'd %d bytes in %s:%s() line %d", len, file, funcname, line);
#endif
    
    return (void *)ptr;
}

/* 
 * this is wrapped up in a #define safe_realloc
 * This function, detects failures to realloc memory and zeros
 * out the NEW memory if len > current len
 */
void *
_our_safe_realloc(void *ptr, size_t len, const char *funcname, const int line, const char *file)
{
    size_t oldlen;
    char *charptr = (char *)ptr;

    oldlen = sizeof(ptr);

    if ((ptr = realloc(ptr, len)) == NULL)
        errx(1, "safe_realloc() error: Unable to grow ptr from %d to %d bytes\n"
                "%s:%s() line %d\n", oldlen, len, file, funcname, line);

    if (oldlen < len)
        memset(&charptr[oldlen], 0, len - oldlen -1);

#ifdef DEBUG
    dbg(4, "Remalloc'd to %d bytes in %s:%s() line %d", len, file, funcname, line);
#endif

    return (void *)ptr;
}

/* 
 * this is wrapped up in a #define safe_strdup
 * This function, detects failures to realloc memory
 */
char *
_our_safe_strdup(const char *str, const char *funcname, const int line, const char *file)
{
    char *newstr;
    
    if ((newstr = (char *)malloc(strlen(str) + 1)) == NULL)
        errx(1, "safe_strdup() error: Unable to malloc %d bytes\n"
                "%s:%s() line %d\n", strlen(str), file, funcname, line);

    memcpy(newstr, str, strlen(str) + 1);
    
    return newstr;

}



void
packet_stats(struct timeval *begin, struct timeval *end, 
        COUNTER bytes_sent, COUNTER pkts_sent, COUNTER failed)
{
    float bytes_sec = 0.0, mb_sec = 0.0;
    int pkts_sec = 0;
    char bits[3];

    if (gettimeofday(end, NULL) < 0)
        err(1, "gettimeofday");

    timersub(end, begin, begin);
    if (timerisset(begin)) {
        if (bytes_sent) {
            bytes_sec =
                bytes_sent / (begin->tv_sec + (float)begin->tv_usec / 1000000);
            mb_sec = (bytes_sec * 8) / (1024 * 1024);
        }
        if (pkts_sent)
            pkts_sec =
                pkts_sent / (begin->tv_sec + (float)begin->tv_usec / 1000000);
    }

    snprintf(bits, sizeof(bits), "%d", begin->tv_usec);

    fprintf(stderr, " %llu packets (%llu bytes) sent in %d.%s seconds\n",
            pkts_sent, bytes_sent, begin->tv_sec, bits);
    fprintf(stderr, " %.1f bytes/sec %.2f megabits/sec %d packets/sec\n",
            bytes_sec, mb_sec, pkts_sec);

    if (failed)
        warnx(" %llu write attempts failed from full buffers and were repeated\n",
              failed);

}

int
read_hexstring(const char *l2string, char *hex, const int hexlen)
{
    int numbytes = 0;
    unsigned int value;
    char *l2byte;
    u_char databyte;
    char *token = NULL;
    char *string;

    string = safe_strdup(l2string);

    if (hexlen <= 0)
        errx(1, "Hex buffer must be > 0");

    memset(hex, '\0', hexlen);

    /* data is hex, comma seperated, byte by byte */

    /* get the first byte */
    l2byte = strtok_r(string, ",", &token);
    sscanf(l2byte, "%x", &value);
    if (value > 0xff)
        errx(1, "Invalid hex byte passed to -2: %s", l2byte);
    databyte = (u_char) value;
    memcpy(&hex[numbytes], &databyte, 1);

    /* get remaining bytes */
    while ((l2byte = strtok_r(NULL, ",", &token)) != NULL) {
        numbytes++;
        if (numbytes + 1 > hexlen) {
            warnx("Hex buffer too small for data- skipping data");
            return (++numbytes);
        }
        sscanf(l2byte, "%x", &value);
        if (value > 0xff)
            errx(1, "Invalid hex byte passed to -2: %s", l2byte);
        databyte = (u_char) value;
        memcpy(&hex[numbytes], &databyte, 1);
    }

    numbytes++;

    free(string);

    dbg(1, "Read %d bytes of layer 2 data", numbytes);
    return (numbytes);
}

/* whorishly appropriated from fragroute-1.2 */

int
argv_create(char *p, int argc, char *argv[])
{
    int i;

    for (i = 0; i < argc - 1; i++) {
        while (*p != '\0' && isspace((int)*p))
            *p++ = '\0';

        if (*p == '\0')
            break;
        argv[i] = p;

        while (*p != '\0' && !isspace((int)*p))
            p++;
    }
    p[0] = '\0';
    argv[i] = NULL;

    return (i);
}


/*
 * returns a pointer to the layer 4 header which is just beyond the IP header
 */
void *
get_layer4(ip_hdr_t * ip_hdr)
{
    void *ptr;
    ptr = (u_int32_t *) ip_hdr + ip_hdr->ip_hl;
    return ((void *)ptr);
}

/*
 Local Variables:
 mode:c
 indent-tabs-mode:nil
 c-basic-offset:4
 End:
*/
