#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dirent.h>
#include <GLFW/glfw3.h>

#ifdef __APPLE__
#include <OpenGL/glext.h>
#else
#include <GL/glext.h>
#endif

#include "lodepng.h"

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

void read_verts(int fd, int *nverts, double **vertices, double **tcoords) {
    uint32_t hot = 0;
    READ_FULLY(fd, &hot, sizeof(uint32_t));

    *nverts = hot;
    *vertices = malloc(hot * 2 * sizeof(double));
    *tcoords = malloc(hot * 2 * sizeof(double));

    READ_FULLY(fd, *vertices, hot * 2 * sizeof(double));
    READ_FULLY(fd, *tcoords, hot * 2 * sizeof(double));
}

void render(GLFWwindow *win, int fd, char *origname) {
    char *output_name = strdup(origname);
    size_t len = strlen(output_name);
    if (strncmp(output_name + len - 7, ".extbvt", 7)) {
        free(output_name);
        return;
    }

    *(output_name + len - 7) = '\0';

    uint32_t bank_len = 0;
    READ_FULLY(fd, &bank_len, sizeof(uint32_t));
    char *bank_name = calloc(bank_len + 1, 1);
    READ_FULLY(fd, bank_name, bank_len);
    char *bank_base_name = basename(bank_name);

    uint32_t wh[2] = { 0 };
    READ_FULLY(fd, wh, sizeof(uint32_t) * 2);

    char *base = strdup(origname);
    char *dn = dirname(base);

    char *bank_full_name = calloc(strlen(dn) + strlen(bank_base_name) + 2, 1);
    strcat(bank_full_name, dn);
    if (strlen(dn))
        strcat(bank_full_name, "/");
    strcat(bank_full_name, bank_base_name);

    free(base);
    free(bank_name);

    int nverts;
    double *vertices,
           *tcoords;
    read_verts(fd, &nverts, &vertices, &tcoords);

    byte *image;
    uint32_t bank_w,
             bank_h;
    int ret = lodepng_decode32_file(&image, &bank_w, &bank_h, bank_full_name);
    assert(ret == 0);
    free(bank_full_name);

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    GLuint textures[2];
    glGenTextures(2, textures);

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    //glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    //glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bank_w,
                 bank_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glVertexPointer(2, GL_DOUBLE, 0, vertices);
    glTexCoordPointer(2, GL_DOUBLE, 0, tcoords);

    /*
    glfwSetWindowSize(win, wh[0], wh[1]);
    glDrawBuffer(GL_BACK);

    glViewport(0, 0, wh[0], wh[1]);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, wh[0], wh[1], 0, 0, 1);
    glMatrixMode(GL_MODELVIEW);

    glDrawArrays(GL_QUADS, 0, nverts * 2);
    glFinish();
    */

    // draw to fb and dump to disk.

    GLuint fb = 0;
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bank_w,
                 bank_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1], 0);

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, wh[0], wh[1]);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, wh[0], 0, wh[1], 0, 1);
    glMatrixMode(GL_MODELVIEW);

    glDrawArrays(GL_QUADS, 0, nverts * 2);
    glFinish();

    byte *image_cut = malloc(wh[0] * wh[1] * 4);
    glReadPixels(0, 0, wh[0], wh[1], GL_RGBA, GL_UNSIGNED_BYTE, image_cut);
    lodepng_encode32_file(output_name, image_cut, wh[0], wh[1]);
    printf("%s\n", output_name);
    free(image_cut);

    if (unlink(origname) == -1) {
        perror(origname);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(2, textures);
    free(vertices);
    free(tcoords);
    free(image);
    free(output_name);

    //glfwSwapBuffers(win);
    //glfwPollEvents();
}

void render_all_in_root(GLFWwindow *win, char *root) {
    DIR *directory = opendir(root);
    if (!directory)
        return;

    struct dirent *record = readdir(directory);
    while (record != NULL) {
        if (!strcmp(record->d_name, ".") || !strcmp(record->d_name, "..")) {
            record = readdir(directory);
            continue;
        }
        char *pth = calloc(strlen(root) + strlen(record->d_name) + 2, 1);
        strcat(pth, root);
        strcat(pth, "/");
        strcat(pth, record->d_name);

        struct stat attrs;
        if (stat(pth, &attrs) == 0) {
            if (attrs.st_mode & S_IFDIR) {
                render_all_in_root(win, pth);
            } else if (attrs.st_mode & S_IFREG) {
                int fd = open(pth, O_RDONLY);
                if (fd < 0) {
                    perror(pth);
                    free(pth);
                    continue;
                }

                render(win, fd, pth);
                close(fd);
            }
        } else {
            perror(record->d_name);
        }

        free(pth);
        record = readdir(directory);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("use: $exe <root>");
        puts("scans <root> for extbvt files and renders them.");
        return 1;
    }

    GLFWwindow* window;
    if (!glfwInit())
        return -1;

    window = glfwCreateWindow(640, 480, "ayy lmao", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    char *root = realpath(argv[1], NULL);
    render_all_in_root(window, root);
    free(root);

    glfwTerminate();
    return 0;
}
