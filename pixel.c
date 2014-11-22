/* implementations dubious? it is not care either way. */
void copy_1bpp_luma(byte *raw, int len, byte *output) {
    memset(output, 255, len * 4);
    for (int i = 0, ctr = 0; i < len; ctr = (++i) * 4) {
        output[ctr] = raw[i];
        output[ctr + 1] = raw[i];
        output[ctr + 2] = raw[i];
    }
}

void copy_1bpp_alpha(byte *raw, int len, byte *output) {
    memset(output, 0, len * 4);
    for (int i = 0, ctr = 0; i < len; ctr = (++i) * 4) {
        output[ctr + 3] = raw[i];
    }
}

void copy_2bpp_lumalpha(byte *raw, int len, byte *output) {
    memset(output, 0, len * 4);
    for (int i = 0, ctr = 0; i < len; ctr = (i += 2) * 4) {
        output[ctr] = raw[i];
        output[ctr + 1] = raw[i];
        output[ctr + 2] = raw[i];
        output[ctr + 3] = raw[i + 1];
    }
}

void copy_2bpp_rgb565(byte *raw, int len, byte *output) {
    memset(output, 255, len * 4);
    unsigned short *pixels = (unsigned short *) raw;
    for (int i = 0, ctr = 0; i < len; ctr = (++i) * 4) {
        unsigned short pixel = pixels[i];
        byte shift = (pixel & 0xF800) >> 8;
        output[ctr] = shift | (shift >> 5);
        shift = (pixel & 0x07E0) >> 3;
        output[ctr + 1] = shift | (shift >> 6);
        shift = (pixel & 0x001F) << 3;
        output[ctr + 2] = shift | (shift >> 5);
    }
}

void copy_2bpp_rgba5551(byte *raw, int len, byte *output) {
    unsigned short *pixels = (unsigned short *) raw;
    for (int i = 0, ctr = 0; i < len; ctr = (++i) * 4) {
        unsigned short pixel = pixels[i];
        byte shift = (pixel & 0xF800) >> 8;
        output[ctr] = shift | (shift >> 5);
        shift = (pixel & 0x07C0) >> 3;
        output[ctr + 1] = shift | (shift >> 5);
        shift = (pixel & 0x003E) << 3;
        output[ctr + 2] = shift | (shift >> 5);
        output[ctr + 3] = (pixel % 2)? 255 : 0;
    }
}

void copy_2bpp_rgba4444(byte *raw, int len, byte *output) {
    unsigned short *pixels = (unsigned short *) raw;
    for (int i = 0, ctr = 0; i < len; ctr = (++i) * 4) {
        unsigned short pixel = pixels[i];
        byte shift = (pixel & 0xF000) >> 8;
        output[ctr] = shift | (shift >> 4);
        shift = (pixel & 0x0F00) >> 4;
        output[ctr + 1] = shift | (shift >> 4);
        shift = pixel & 0x00F0;
        output[ctr + 2] = shift | (shift >> 4);
        shift = pixel & 0x000F;
        output[ctr + 3] = shift | (shift << 4);
    }
}

void copy_3bpp_rgb(byte *raw, int len, byte *output) {
    memset(output, 255, len * 4);
    for (int i = 0, ctr = 0; i < len; ctr = (i += 3) * 4) {
        output[ctr] = raw[i];
        output[ctr + 1] = raw[i + 1];
        output[ctr + 2] = raw[i + 2];
    }
}
