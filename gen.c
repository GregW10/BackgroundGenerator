#include "gen.h"

int main(int argc, char **argv) {
    time_t start = time(NULL);
    unsigned int w;
    unsigned int h;
    unsigned long long reps;
    unsigned int seeds;
    coord *coords = NULL;
    long double k;
    ld_clr *clrs = NULL;
    char *path = NULL;
    bool verbose;
    parse_argv(argc, argv, &w, &h, &reps, &seeds, &coords, &k, &clrs, &path, &verbose);
    if (!w || !h) {
        fprintf(stderr, "Error: BMP cannot have zero size.\n");
        return 1;
    }
    unsigned long long w3 = w*3;
    unsigned char rem = w3 % 4;
    size_t padding = rem ? 4 - rem : 0;
    unsigned int file_size = (w3 + padding)*h + 54;
    coord *cpy;
    ld_clr *ccpy;
    unsigned int counter;
    if (verbose) {
        printf("BMP width: %u pixels\nBMP height: %u pixels\nNumber of iterations: %llu\nNumber of seeds: %u\nk-value: "
               "%Lf\nOutput path: %s\nOutput file size: %u bytes\n\nCoordinates and colours of seeds:\n",
               w, h, reps, seeds, k, path, file_size);
        cpy = coords;
        ccpy = clrs;
        for (counter = 0; counter < seeds; ++cpy, ++ccpy) {
            printf("%u. (%u,%u), {b:%u,g:%u,r:%u}\n", ++counter, cpy->x, cpy->y,
                   round_ld(255*ccpy->b), round_ld(255*ccpy->g), round_ld(255*ccpy->r));
            if (cpy->x >= w || cpy->y >= h) {
                fprintf(stderr, "Error: coordinate pair %u does not fall within image bounds.\n", counter);
                return 1;
            }
        }
    }
    if (verbose)
        printf("\nAllocating floating point pixel array...\n");
    pix **pixels = alloc_pixels(w, h);
    if (!pixels) {
        fprintf(stderr, "Memory allocation error for pixel propagation array.\n");
        exit(1);
    }
    if (verbose)
        printf("Allocating colour array...\n");
    clr **bmp = alloc_bmp(w, h);
    if (!bmp) {
        fprintf(stderr, "Memory allocation error for colour array.\n");
        exit(1);
    }
    unsigned int i;
    unsigned int j;
    pix **y;
    pix **yd; // down
    pix *x;
    pix *xl; // left
    pix *xd; // down
    unsigned int wm1 = w - 1;
    unsigned int hm1 = h - 1;
    unsigned long long _c = 0;
    if (verbose)
        printf("Starting main colour-spreading loop...\n");
    while (_c++ < reps) {
        cpy = coords;
        ccpy = clrs;
        for (counter = 0; counter < seeds; ++counter, ++cpy, ++ccpy) // faster than skipping seeds in above loop
            pixels[cpy->y][cpy->x].col = *ccpy;
#define COL_REP(tok) \
        y = pixels; \
        yd = y + 1; \
        for (j = 0; j < hm1; ++j, ++y, ++yd) { \
            x = *y; \
            xl = x + 1; \
            xd = *yd; \
            for (i = 0; i < wm1; ++i, ++x, ++xl, ++xd) { \
                xl->left = xl->col.tok - x->col.tok; \
                xd->down = xd->col.tok - x->col.tok; \
                x->col.tok += k*(xl->left + xd->down - x->left - x->down); \
            } \
            xd->down = xd->col.tok - x->col.tok; \
            x->col.tok += k*(xd->down - x->left - x->down); \
        } \
        x = *y; \
        xl = x + 1; \
        for (i = 0; i < wm1; ++i, ++x, ++xl, ++xd) { \
            xl->left = xl->col.tok - x->col.tok; \
            x->col.tok += k*(xl->left - x->left - x->down); \
        } \
        x->col.tok += k*(-x->left - x->down);
        COL_REP(b)
        COL_REP(g)
        COL_REP(r)
        if (verbose) {
            printf("Completed: %llu/%llu iterations\r", _c, reps);
            fflush(stdout);
        }
    }
    cpy = coords;
    ccpy = clrs;
    for (counter = 0; counter < seeds; ++counter, ++cpy, ++ccpy) // faster than skipping seeds in above loop
        pixels[cpy->y][cpy->x].col = *ccpy;
    if (verbose)
        printf("\nCopying floating point colours array to final BGR .bmp array...\n");
    y = pixels;
    clr **yc = bmp;
    clr *xc;
    for (j = 0; j < h; ++j, ++y, ++yc) {
        x = *y;
        xc = *yc;
        for (i = 0; i < w; ++i, ++x, ++xc) {
            xc->b = round_ld(255*x->col.b);
            xc->g = round_ld(255*x->col.g);
            xc->r = round_ld(255*x->col.r);
        }
    }
    if (verbose)
        printf("Writing BMP...\n");
    write_bmp(bmp, w, h, path, padding, file_size);
    if (verbose) {
        time_t end = time(NULL);
        unsigned long long tot_sec = end - start;
        printf("Done!\n");
        unsigned long long *full = full_time(start, end);
        printf("--------------------\nTime elapsed:\n"
               "%llu day%c, %llu hour%c, %llu minute%c & %llu second%c\n(%llu second%c)\n",
               *full, "s"[*full == 1], *(full + 1), "s"[*(full + 1) == 1],
               *(full + 2), "s"[*(full + 2) == 1], *(full + 3), "s"[*(full + 3) == 1], tot_sec, "s"[tot_sec == 1]);
    }
    free(coords);
    free(clrs);
    free_pixels(pixels, h);
    free_bmp(bmp, h);
}
