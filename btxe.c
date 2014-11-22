#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <zlib.h>
#include <sys/stat.h>
#include <libgen.h>

#include "lodepng.h"
// #include "lodepng.c"

typedef unsigned char byte;

#define READ_FULLY(fd, target, size) { \
    size_t _eval_one = (size_t)(size); \
    /* printf("dbg size: %zu\n", _eval_one); */ \
    assert(read((fd), (target), _eval_one) == _eval_one); \
}

#define WRITE_FULLY(fd, source, size) { \
    size_t _eval_one = (size_t)(size); \
    /* printf("dbg size: %zu\n", _eval_one); */ \
    assert(write((fd), (source), _eval_one) == _eval_one); \
}

int repack(char *template_p, char *replacement_p, char *output_p) {
    int fd_intexb,
        fd_out;

    byte *image;
    unsigned int rbank_w, rbank_h;
    if (lodepng_decode32_file(&image, &rbank_w, &rbank_h, replacement_p))
        return 1;

    fd_intexb = open(template_p, O_RDONLY);
    fd_out = open(output_p, O_CREAT | O_WRONLY, 0644);

    if (fd_intexb == -1 || fd_out == -1) {
        free(image);
        return 2;
    }

    byte hdr[10];
    READ_FULLY(fd_intexb, hdr, 10);

    uint32_t magic = ntohl(*(uint32_t *)(hdr));
    uint32_t data_length = ntohl(*(uint32_t *)(hdr + 4));
    if (magic != 'TEXB') {
        puts("bad magic number!");
        printf("expected %u, got %u\n", 'TEXB', magic);

        free(image);
        close(fd_intexb);
        close(fd_out);
        return 3;
    }

    unsigned short intname_len = ntohs(*(unsigned short *)(hdr + 8));
    char intname[intname_len + 4];
    READ_FULLY(fd_intexb, intname, intname_len);
    printf("repacking as internal name: %s.\n", intname);

    unsigned short attrval[6], attrval_int[6];
    READ_FULLY(fd_intexb, attrval, 12);
    /* flip bytes of the 6 attrs. */
    for (int i = 0; i < 6; ++i)
        attrval_int[i] = ntohs(attrval[i]);

    if (rbank_w != attrval_int[0] || rbank_h != attrval_int[1]) {
        printf("invalid replacement bank size! expected %hu x %hu\n", attrval_int[0], attrval_int[1]);
        return 4;
    }

    unsigned short newflags = 0xCC;
    attrval[2] = htons(newflags);

    memset(hdr + 4, 0, 4);
    WRITE_FULLY(fd_out, hdr, 10);
    WRITE_FULLY(fd_out, intname, intname_len);
    WRITE_FULLY(fd_out, attrval, 12);

    printf("Images inside:   %hu\n", attrval_int[5]);
    for (int i = 0; i < attrval_int[5]; ++i) {
        byte hdr[6];
        READ_FULLY(fd_intexb, hdr, 6);
        unsigned short rest = ntohs(*(unsigned short *)(hdr + 4));

        byte *rest_data = malloc(rest);
        READ_FULLY(fd_intexb, rest_data, rest);

        WRITE_FULLY(fd_out, hdr, 6);
        WRITE_FULLY(fd_out, rest_data, rest);
    }

    byte compress_tag[4] = { 0 };
    WRITE_FULLY(fd_out, compress_tag, 4);

    z_stream state;
    memset(&state, 0, sizeof(z_stream));
    if (deflateInit(&state, Z_DEFAULT_COMPRESSION) != Z_OK) {
        puts("cannot initialize zlib");
        return 7;
    }

    byte *deflated = malloc(rbank_w * rbank_h * 4);

    state.avail_in = rbank_w * rbank_h * 4;
    state.next_in = image;
    state.avail_out = rbank_w * rbank_h * 4;
    state.next_out = deflated;

    deflate(&state, Z_FINISH);
    deflateEnd(&state);

    unsigned int cmpdatasize = (rbank_w * rbank_h * 4) - state.avail_out;
    printf("writing %u\n", cmpdatasize);

    WRITE_FULLY(fd_out, deflated, cmpdatasize);
    off_t file_size = lseek(fd_out, 0, SEEK_CUR);

    uint32_t fsbuf = htonl(file_size - 8);
    lseek(fd_out, 4, SEEK_SET);
    WRITE_FULLY(fd_out, &fsbuf, 4);

    free(deflated);
    free(image);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        puts("use: ./bxte <in-texb> <replace-bank> <out-texb>");
        puts("takes <in-texb> and replaces pixel backing with PNG from <replace-bank>.");
        puts("result is output to <out-texb>");
        return 1;
    }

    return repack(argv[1], argv[2], argv[3]);
}
