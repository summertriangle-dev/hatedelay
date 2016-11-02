#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <zlib.h>
#include <sys/stat.h>
#include <libgen.h>
#include <math.h>

#ifdef _WIN32
#include <io.h>
#ifndef _MSC_VER
#include <winsock2.h>
#endif /* _MSC_VER */
#else
#define O_BINARY 0
#include <unistd.h>
#include <arpa/inet.h>
#endif /* _WIN32 */

#include "lodepng.h"
// #include "lodepng.c"

typedef unsigned char byte;

#include "pixel.c"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define READ_FULLY(fd, target, size) do { \
    size_t _eval_one = (size_t)(size); \
    /* printf("dbg size: %zu\n", _eval_one); */ \
    size_t _eval_two = read((fd), (target), _eval_one); \
    assert(_eval_two == _eval_one); \
} while(0)

#define WRITE_FULLY(fd, source, size) do { \
    size_t _eval_one = (size_t)(size); \
    /* printf("dbg size: %zu\n", _eval_one); */ \
    size_t _eval_two = write((fd), (source), _eval_one); \
    assert(_eval_two == _eval_one); \
} while(0)

typedef enum {
    ALPHA    = 0,
    LUMA     = 1, /* possibly unused */
    LUMALPHA = 2, /* possibly unused */
    RGB      = 3,
    RGBA     = 4,
} szk_image_format_t;

typedef enum {
    RGB_565   = 0,
    RGBA_5551 = 1,
    RGBA_4444 = 2,
    RGBA_8888 = 3,
} szk_pixel_format_t;

typedef struct szk_opt_s {
    szk_image_format_t img_format;
    byte is_compressed;
    byte is_mipmapped;
    byte is_doublebuffered;
    szk_pixel_format_t pix_format;
} szk_type_t;

typedef struct szk_subimage_s {
    int width;
    int height;
    int xcenter;
    int ycenter;

    int vertexes_n;
    int indexes_n;
    double *vertexes;
    double *uv;
    int *indexes;
} *szk_subimage_t;

typedef struct szk_image_s {
    char *name;
    szk_subimage_t simgs;
    int simg_count;
} *szk_idescriptor_t;

int get_bpp(szk_pixel_format_t pf, szk_image_format_t iff) {
    switch (pf) {
        case RGB_565:
        case RGBA_5551:
        case RGBA_4444:
            return 2;
        case RGBA_8888:
            switch (iff) {
                case LUMA:
                case ALPHA:
                    return 1;
                case LUMALPHA:
                    return 2;
                case RGB:
                    return 3;
                case RGBA:
                default:
                    return 4;
            }
        default:
            return 0;
    }
}

/* flags reading */

void get_imgtype(unsigned short def, szk_type_t *ret) {
    byte flags = (byte)def;

    /* bits 1-3 */
    ret->img_format = (szk_image_format_t)(flags & 0x07);
    /* bit 4 */
    ret->is_compressed = (flags & 0x08) >> 3;
    /* bit 5 */
    ret->is_mipmapped = (flags & 0x10) >> 4;
    /* bit 6 */
    ret->is_doublebuffered = (flags & 0x20) >> 5;
    /* bit 7-8 */
    ret->pix_format = (szk_pixel_format_t)((flags & 0xC0) >> 6);
}

void print_imgtype(szk_type_t o) {
    printf("\tBank format:     %d\n", o.img_format);
    printf("\tBank attributes: ");
    if (o.is_compressed)
        printf("compressed, ");
    if (o.is_mipmapped)
        printf("mipmap, ");
    if (o.is_doublebuffered)
        printf("double-buffered, ");
    puts("");
    printf("\tPixel format:    %d\n", o.pix_format);
}

int read_descriptor(int fd, szk_idescriptor_t result) {
    byte hdr[8];
    READ_FULLY(fd, hdr, 8);

    uint32_t magic = ntohl(*(uint32_t *)(hdr));
    if (magic != 'TIMG') {
        puts("bad magic number!");
        printf("expected %u, got %u\n", 'TIMG', magic);
        return 3;
    }

    unsigned short intname_len = ntohs(*(unsigned short *)(hdr + 6));
    char *intname = (char *)calloc(intname_len, 1);
    READ_FULLY(fd, intname, intname_len);

    // printf("%s\n", intname);
    result->name = strdup(intname + 1);
    free(intname);

    unsigned short subs_attrsk;
    READ_FULLY(fd, &subs_attrsk, 2);

    /* Not documented, but if count is FFFF the image have attributes. */
    /* And not shown here in code, but when count is FEFF uvs become 2 bytes.
     * It looks like a bug in the engine bc real count is never updated.
     * so unlikely it will show up in real resouces. */
    if (subs_attrsk == 0xFFFF) {
        unsigned short acs;
        READ_FULLY(fd, &acs, 2);
        acs = ntohs(acs);

        // printf("%hu attribute to read. skipping until implemented.\n", acs);
        for (int i = 0; i < acs; ++i) {
            byte key_and_type[2];
            READ_FULLY(fd, &key_and_type, 2);
            switch (key_and_type[1]) {
                case 0:
                case 1:
                    lseek(fd, 4, SEEK_CUR);
                    break;
                case 2: {
                    unsigned short len;
                    READ_FULLY(fd, &len, 2);
                    len = ntohs(len);
                    lseek(fd, len, SEEK_CUR);
                    break;
                }
                default:
                    assert(!"unknown attribute type.");
            }
        }
        READ_FULLY(fd, &subs_attrsk, 2);
    }

    subs_attrsk = ntohs(subs_attrsk);
    // printf("%u sections in this image.\n", subs_attrsk);
    result->simg_count = subs_attrsk;
    result->simgs = (szk_subimage_t)malloc(subs_attrsk * sizeof(struct szk_subimage_s));

    for (int i = 0; i < subs_attrsk; ++i) {
        unsigned short attrval[5];
        READ_FULLY(fd, attrval, 10);
        for (int j = 1; j < 5; ++j)
            attrval[j] = ntohs(attrval[j]);

        byte *short_vals = (byte *) attrval;
        result->simgs[i].vertexes_n = short_vals[0];
        result->simgs[i].indexes_n =  short_vals[1];

        result->simgs[i].width   = attrval[1];
        result->simgs[i].height  = attrval[2];
        result->simgs[i].xcenter = attrval[3];
        result->simgs[i].ycenter = attrval[4];

        /* printf("V: %d I: %d %d x %d %d %d\n", short_vals[0], short_vals[1],
                 attrval[1], attrval[2], attrval[3], attrval[4]); */

        int blks_vertex = sizeof(uint32_t) * short_vals[0] * 4;
        int blks_index = short_vals[1];

        byte *vibuf = (byte *) malloc(blks_vertex + blks_index);
        READ_FULLY(fd, vibuf, blks_vertex + blks_index);

        uint32_t *lcoords = (uint32_t *) vibuf;
        result->simgs[i].vertexes = (double *) malloc(sizeof(double) * short_vals[0] * 2);
        result->simgs[i].uv = (double *) malloc(sizeof(double) * short_vals[0] * 2);

        for (int j = 0; j < short_vals[0]; ++j) {
            /* XY in screen scale */
            double vertex_x = ntohl(lcoords[0]) / 65536.0;
            double vertex_y = ntohl(lcoords[1]) / 65536.0;
            result->simgs[i].vertexes[j * 2] = vertex_x;
            result->simgs[i].vertexes[(j * 2) + 1] = vertex_y;

            /* UV = OpenGL texture coordinates from 0..1 */
            double uv_x = ntohl(lcoords[2]) / 65536.0;
            double uv_y = ntohl(lcoords[3]) / 65536.0;
            result->simgs[i].uv[j * 2] = uv_x;
            result->simgs[i].uv[(j * 2) + 1] = uv_y;

            lcoords += 4;
        }

        result->simgs[i].indexes = (int *) malloc(blks_index);
        memcpy(vibuf + blks_vertex, result->simgs[i].indexes, blks_index);
        free(vibuf);
    }
    return 0;
}

void print_descriptor(szk_idescriptor_t img) {
    printf("%s, %d subimages.\n", img->name, img->simg_count);

    for (int i = 0; i < img->simg_count; ++i) {
        szk_subimage_t s = &(img->simgs[i]);
        printf("\tsubimage %d: %d x %d center (%d, %d)\n",
               i, s->width, s->height, s->xcenter, s->ycenter);

        for (int j = 0; j < s->vertexes_n * 2; j += 2) {
            printf("\t\tvertex %d at: (%f, %f)\n", j / 2, s->vertexes[j],
                   s->vertexes[j + 1]);
            printf("\t\tUV coordinate: (%f, %f)\n", s->uv[j], s->uv[j + 1]);
        }
    }
}

void convert_map(byte *raw, int w, int h, szk_type_t type, byte *output) {
    if (type.pix_format == RGBA_8888) {
        switch (type.img_format) {
            case LUMA:
                // 1 bpp
                copy_1bpp_luma(raw, w * h, output);
                break;
            case ALPHA:
                copy_1bpp_alpha(raw, w * h, output);
                break;
            case LUMALPHA:
                copy_2bpp_lumalpha(raw, w * h, output);
                break;
            case RGB:
                copy_3bpp_rgb(raw, w * h, output);
                break;
            default:
                memcpy(output, raw, w * h * 4);
                break;
        }
    } else {
        switch(type.pix_format) {
            case RGB_565:
                copy_2bpp_rgb565(raw, w * h, output);
                break;
            case RGBA_4444:
                copy_2bpp_rgba4444(raw, w * h, output);
                break;
            case RGBA_5551:
                copy_2bpp_rgba5551(raw, w * h, output);
                break;
            default:
                break;
        }
    }

}

void write_png(byte *buf, unsigned long len, int w, int h, const char *path) {
    lodepng_encode_file(path, buf, w, h, LCT_RGBA, 8);
}

void sample_and_write_image(byte *buf, int bank_width, int bank_height,
                            szk_idescriptor_t des, const char *prefix) {

    int tl, tr, bl, br;
    int low_x = 99999, high_x = -99999, low_y = 99999, high_y = -99999;

    double *vertices = des->simgs[0].vertexes;
    double *uvs = des->simgs[0].uv;

    for (int i = 0; i < des->simgs[0].vertexes_n; ++i) {
        double *x = &vertices[i * 2];
        double *y = &vertices[(i * 2) + 1];

        low_x = MIN(low_x, *x);
        high_x = MAX(high_x, *x);

        low_y = MIN(low_y, *y);
        high_y = MAX(high_y, *y);
    }

    for (int i = 0; i < des->simgs[0].vertexes_n; ++i) {
        double x = vertices[i * 2];
        double y = vertices[(i * 2) + 1];

        if (x == low_x && y == low_y)    tl = i;
        if (x == low_x && y == high_y)   bl = i;
        if (x == high_x && y == low_y)   tr = i;
        if (x == high_x && y == high_y)  br = i;
    }

    int swap_xy = (uvs[tl * 2] == uvs[tr * 2])? 1 : 0;
    double x_near = uvs[tl * 2];
    double x_far  = uvs[br * 2];
    double y_near = uvs[(tl * 2) + 1];
    double y_far  = uvs[(br * 2) + 1];

#define LERP(near, far, pct) (near + ((far - near) * pct))
#define SAMPLE(x, y, stride) ((( (uint32_t)y * stride) + (uint32_t)x ) * 4)

    // compile with -Ofast!!

    byte *img = malloc(des->simgs[0].width * des->simgs[0].height * 4);
    if (swap_xy) {
        for (int tgt_y = 0; tgt_y < des->simgs[0].height; tgt_y++) {
            double lerp_x = round((LERP(x_near, x_far, (double)tgt_y / des->simgs[0].height) * bank_width));

            for (int tgt_x = 0; tgt_x < des->simgs[0].width; tgt_x++) {
                double lerp_y = round((LERP(y_near, y_far, (double)tgt_x / des->simgs[0].width) * bank_height));
                memcpy(img + SAMPLE(tgt_x, tgt_y, des->simgs[0].width), buf + SAMPLE(lerp_x, lerp_y, bank_width), 4);
            }
        }
    } else {
        for (int tgt_y = 0; tgt_y < des->simgs[0].height; tgt_y++) {
            double lerp_y = round((LERP(y_near, y_far, (double)tgt_y / des->simgs[0].height) * bank_height));

            for (int tgt_x = 0; tgt_x < des->simgs[0].width; tgt_x++) {
                double lerp_x = round((LERP(x_near, x_far, (double)tgt_x / des->simgs[0].width) * bank_width));
                memcpy(img + SAMPLE(tgt_x, tgt_y, des->simgs[0].width), buf + SAMPLE(lerp_x, lerp_y, bank_width), 4);
            }
        }
    }

#undef LERP
#undef SAMPLE

    char *tmp = strdup(des->name);
    char *base = basename(tmp);

    int staticpart = strlen(prefix) + strlen(base) + 1;
    char *filename = (char *) calloc(staticpart + 8, 1);
    strcpy(filename, prefix);
    strcat(filename, "/");
    strcat(filename, base);

    free(tmp);

    if (!strncmp(filename + staticpart - 5, ".imag", 5)) {
        filename[staticpart - 5] = '\0';
    }
    if (strncmp(filename + staticpart - 9, ".png", 4)) {
        strcat(filename, ".png");
    }

    write_png(img, 0, des->simgs[0].width, des->simgs[0].height, filename);
    printf("SAVED: %s to %s\n", des->name, filename);

    free(filename);
    free(img);
}

int read_file(int fd, char *out) {
    int final_status = 0;

    byte hdr[10];
    READ_FULLY(fd, hdr, 10);

    uint32_t magic = ntohl(*(uint32_t *)(hdr));
    uint32_t data_length = ntohl(*(uint32_t *)(hdr + 4));
    if (magic != 'TEXB') {
        puts("bad magic number!");
        printf("expected %u, got %u\n", 'TEXB', magic);
        return 3;
    }

    unsigned short intname_len = ntohs(*(unsigned short *)(hdr + 8));
    char *intname = (char *) calloc(intname_len + 4, 1);
    READ_FULLY(fd, intname, intname_len);
    printf("File header for %s.\n", intname + 1);

    unsigned short attrval[6];
    READ_FULLY(fd, attrval, 12);
    /* flip bytes of the 6 attrs. */
    for (int i = 0; i < 6; ++i)
        attrval[i] = ntohs(attrval[i]);

    printf("Bank resolution: %hu x %hu\n", attrval[0], attrval[1]);
    puts("Bank flags:");
    szk_type_t bflags;
    get_imgtype(attrval[2], &bflags);
    print_imgtype(bflags);

    printf("Vertexes:        %hu\n", attrval[3]);
    printf("Indexes:         %hu\n", attrval[4]);
    printf("Images inside:   %hu\n", attrval[5]);

    if (attrval[3] == 0xFFFF || attrval[4] == 0xFFFF) {
        puts("Image has too many vertexes/indexes! Not supported now.");
        return 4;
    }

    szk_idescriptor_t images = (szk_idescriptor_t) calloc(attrval[5], sizeof(struct szk_image_s));
    for (int i = 0; i < attrval[5]; ++i) {
        int ret = read_descriptor(fd, &images[i]);
        print_descriptor(&images[i]);
        if (ret != 0) {
            final_status = ret;
            goto cleanup;
        } else {
            continue;
        }
    }

    unsigned long have = lseek(fd, 0, SEEK_CUR);
    /* Need compensate for magic & size (8byte). */
    unsigned long toread = data_length - have + 8;
    byte *raw = (byte *) malloc(toread);
    READ_FULLY(fd, raw, toread);

    /* all are RGBA */
    byte *bitmap = (byte *) calloc(attrval[0] * attrval[1] * 4, 1);

    if (bflags.is_compressed) {
        /* according to playground, read 4 more bytes to find out type of
         * compression */
        uint32_t cmp_kind = ntohl(*(uint32_t *) raw);

        if (cmp_kind != 0) {
            printf("unsupported compression type %u", cmp_kind);
            final_status = 5;
            free(bitmap);
            free(raw);
            goto cleanup;
        } else {
            z_stream state;
            memset(&state, 0, sizeof(z_stream));
            int ret = inflateInit(&state);

            if (ret != Z_OK) {
                puts("cannot initialize zlib");
                final_status = 7;
                free(bitmap);
                free(raw);
                goto cleanup;
            }

            size_t infsize = attrval[0] * attrval[1] *
                             get_bpp(bflags.pix_format, bflags.img_format);
            byte *inf = (byte *) calloc(infsize, 1);

            state.avail_in = toread;
            state.next_in = raw + 4;
            state.avail_out = infsize;
            state.next_out = inf;

            ret = inflate(&state, Z_NO_FLUSH);

            if ((ret != Z_OK) && (ret != Z_STREAM_END)) {
                puts("cannot initialize zlib");
                final_status = 7;
                free(bitmap);
                free(raw);
                goto cleanup;
            }
            ret = inflateEnd(&state);

            free(raw);
            raw = inf;
            toread = infsize;
        }
    }

    convert_map(raw, attrval[0], attrval[1], bflags, bitmap);

    for (int i = 0; i < attrval[5]; ++i) {
        sample_and_write_image(bitmap, attrval[0], attrval[1], &images[i], out);
    }

  late_cleanup:
    free(raw);
    free(bitmap);

  cleanup:
    for (int i = 0; i < attrval[5]; ++i) {
        free(images[i].name);
        for (int j = 0; j < images[i].simg_count; ++j) {
            free(images[i].simgs[j].vertexes);
            free(images[i].simgs[j].uv);
            free(images[i].simgs[j].indexes);
        }
        free(images[i].simgs);
    }
    free(images);
    free(intname);
    return final_status;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("use: ./extb <in-file> <dest-root>");
        puts("writes bank and cutting support files to name under dest-root.");
        puts("to be invoked by doit.sh");
        return 1;
    }

    int fd = open(argv[1], O_RDONLY | O_BINARY);
    if (fd < 0) {
        perror("open failed");
        return 2;
    }

    int ret = read_file(fd, argv[2]);
    close(fd);

    return ret;
}
