#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <tgmath.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>

#define WIDTH 2048
#define HEIGHT 1024
#define REPS 10000
#define SEEDS 10
#define MIN_MEAN_COL 128
#define MIN_SD_COL 70
#define K_COEFF 0.1l
#define PATH "pretty.bmp"

typedef enum boolean {
    false, true
} bool;

#pragma pack(push, 1)
typedef struct bmp_file_header {
    unsigned short header;
    unsigned int file_size;
    unsigned short r1;
    unsigned short r2;
    unsigned int offset;
} file_header;
typedef struct bmp_info_header {
    unsigned int header_size;
    unsigned int width;
    unsigned int height;
    unsigned short panes;
    unsigned short bpp;
    unsigned int compression;
    unsigned int image_size;
    unsigned int hor_res;
    unsigned int ver_res;
    unsigned int num_col_palette;
    unsigned int num_imp_cols;
} info_header;
typedef struct colour {
    unsigned char b;
    unsigned char g;
    unsigned char r;
} clr;
typedef struct ld_colour {
    long double b;
    long double g;
    long double r;
} ld_clr;
typedef struct coordinate {
    unsigned int x;
    unsigned int y;
} coord;
typedef struct pixel {
    struct ld_colour col;
    long double left;
    long double up;
    long double right;
    long double down;
} pix;
#pragma pack(pop)

clr WHITE = {255, 255, 255};
clr BLACK = {0, 0, 0};
clr RED = {0, 0, 255};
clr GREEN = {0, 255, 0};
clr BLUE = {255, 0, 0};

int strcmp_c(const char *restrict s1, const char *restrict s2) {
    if (!s1 || !s2)
        return -128;
    while (*s1 && *s2 && *s1 == *s2) {
        ++s1;
        ++s2;
    }
    return *s1 - *s2;
}

void *zero(void *ptr, size_t _n) {
    if (!ptr)
        return NULL;
    char *_c = ptr;
    while (_n --> 0)
        *_c++ = 0;
    return ptr;
}

unsigned int to_uint(const char *str) {
    if (!str || !*str)
        return -1;
    unsigned int ret = 0;
    while (*str) {
        if (*str < 48 || *str > 57) {
            fprintf(stderr, "Non-numeric character provided.\n");
            exit(1);
        }
        ret *= 10;
        ret += *str++ - 48;
    }
    return ret;
}

unsigned int rand_uint(unsigned int _b, unsigned int _t) {
#ifdef _WIN32
    srand(time(NULL));
    return _b + round_ld((_t - _b)*(((long double) rand()) / RAND_MAX));
#else
    FILE *fp = fopen("/dev/urandom", "rb");
    if (!fp) {
        fprintf(stderr, "Random unsigned integer could not be generated (/dev/urandom failed to open).\n");
        exit(1);
    }
    unsigned int val = 0;
    fread(&val, sizeof(unsigned int), 1, fp);
    fclose(fp);
    return _b + (val % (_t - _b + 1));
#endif
}

void gen_seeds(unsigned int num, coord *_c, unsigned int w, unsigned int h) {
    if (!num || !_c)
        return;
    --w;
    --h;
    while (num --> 0) {
        _c->x = rand_uint(0, w);
        _c++->y = rand_uint(0, h);
    }
}

long double *mean_sd(unsigned long long num, ...) {
    if (!num)
        return NULL;
    static long double avg[2];
    static long double *mptr = avg;
    static long double *sptr = avg + 1;
    *mptr = 0;
    *sptr = 0;
    va_list ptr;
    va_start(ptr, num);
    unsigned long long cpy = num;
    long double arg;
    while (cpy --> 0) {
        *mptr += (arg = va_arg(ptr, long double));
        *sptr += arg*arg;
    }
    va_end(ptr);
    *mptr /= num;
    *sptr = sqrtl(*sptr/num - (*mptr)*(*mptr));
    return avg;
}

ld_clr *gen_cols(unsigned int num, long double mean, long double sd) {
    if (!num)
        return NULL;
    ld_clr *cols = malloc(sizeof(ld_clr)*num);
    ld_clr *cpy = cols;
#ifndef _WIN32
    FILE *fp = fopen("/dev/urandom", "rb");
    if (!fp) {
        fprintf(stderr, "Random colours could not be generated (failed to open /dev/urandom).\n");
        exit(1);
    }
    unsigned long long _num = 0;
    long double *mu = NULL;
    mean /= 255;
    sd /= 255;
#endif
    while (num --> 0) {
        do {
#ifdef _WIN32
            cpy->b = rand() / RAND_MAX;
            cpy->g = rand() / RAND_MAX;
            cpy->r = rand() / RAND_MAX;
#else
            fread(&_num, sizeof(unsigned long long), 1, fp);
            cpy->b = ((long double) _num) / ULLONG_MAX;
            fread(&_num, sizeof(unsigned long long), 1, fp);
            cpy->g = ((long double) _num) / ULLONG_MAX;
            fread(&_num, sizeof(unsigned long long), 1, fp);
            cpy->r = ((long double) _num) / ULLONG_MAX;
#endif
            mu = mean_sd(3, cpy->b, cpy->g, cpy->r);
        } while (*mu < mean || *(mu + 1) < sd);
        ++cpy;
    }
    fclose(fp);
    return cols;
}

void parse_argv(int argc, char **argv, unsigned int *w, unsigned int *h, unsigned long long *reps, unsigned int *s,
                coord **coords, long double *k, ld_clr **clrs, char **path, bool *verbose) {
    if (!w || !h)
        abort();
    *w = WIDTH;
    *h = HEIGHT;
    *reps = REPS;
    *s = SEEDS;
    long double mu = nanl("");
    long double sd = mu;
    *coords = NULL;
    *k = K_COEFF;
    *clrs = NULL;
    *path = PATH;
    *verbose = false;
    ++argv;
    char *clrs_path = NULL;
    while (argc > 1) {
        if (!strcmp_c(*argv, "-w")) {
            if (argc == 2) {
                fprintf(stderr, "Width flag \"-w\" provided without subsequent width argument.\n");
                exit(1);
            }
            *w = to_uint(*(argv + 1));
        }
        else if (!strcmp_c(*argv, "-h")) {
            if (argc == 2) {
                fprintf(stderr, "Height flag \"-h\" provided without subsequent height argument.\n");
                exit(1);
            }
            *h = to_uint(*(argv + 1));
        }
        else if (!strcmp_c(*argv, "-r")) {
            if (argc == 2) {
                fprintf(stderr, "Iterations flag \"-r\" provided without subsequent argument.\n");
                exit(1);
            }
            *reps = to_uint(*(argv + 1));
        }
        else if (!strcmp_c(*argv, "-n")) {
            if (argc == 2) {
                fprintf(stderr, "Number of seeds flag \"-n\" provided without subsequent argument.\n");
                exit(1);
            }
            if (!(*s = to_uint(*(argv + 1)))) {
                fprintf(stderr, "Error: the number of seeds cannot be zero.\n");
                exit(1);
            }
        }
        else if (!strcmp_c(*argv, "-x")) {
            if (argc < 2*(*s) + 2) {
                fprintf(stderr, "Coordinates flag \"-x\" provided without sufficient subsequent arguments "
                                "(%u pairs of coordinates expected).\n", *s);
                exit(1);
            }
            coord *cpy = *coords = malloc(sizeof(coord)*(*s));
            unsigned int scpy = *s;
            while (scpy --> 0) {
                cpy->x = to_uint(*++argv);
                cpy++->y = to_uint(*++argv);
            }
            ++argv;
            argc -= 2*(*s) + 1;
            continue;
        }
        else if (!strcmp_c(*argv, "-k")) {
            if (argc == 2) {
                fprintf(stderr, "k-coefficient flag provided without subsequent argument.\n");
                exit(1);
            }
            char *endptr = NULL;
            if ((*k = strtold(*(argv + 1), &endptr)) == 0.0l && endptr == *(argv + 1)) {
                fprintf(stderr, "Error: k-coefficient could not be successfully converted to floating point value.\n");
                exit(1);
            }
            if (*k < 0.0l || *k > 1.0l) {
                fprintf(stderr, "Error: k-coefficient must be between 0 and 1.\n");
                exit(1);
            }
        }
        else if (!strcmp_c(*argv, "-m")) {
            if (argc == 2) {
                fprintf(stderr, "Minimum mean colour flag provided without subsequent argument.\n");
                exit(1);
            }
            char *endptr;
            if ((mu = strtold(*(argv + 1), NULL)) == 0 && *(argv + 1) == endptr) {
                fprintf(stderr, "Could not convert minimum mean colour to floating point value.\n");
                exit(1);
            }
        }
        else if (!strcmp_c(*argv, "-s")) {
            if (argc == 2) {
                fprintf(stderr, "Minimum colour standard deviation flag provided without subsequent argument.\n");
                exit(1);
            }
            char *endptr;
            if ((sd = strtold(*(argv + 1), NULL)) == 0 && *(argv + 1) == endptr) {
                fprintf(stderr, "Could not convert minimum colour standard deviation to floating point value.\n");
                exit(1);
            }
        }
        else if (!strcmp_c(*argv, "-c")) {
            if (argc == 2) {
                fprintf(stderr, "Color file flag provided without subsequent argument.\n");
                exit(1);
            }
            clrs_path = *(argv + 1);
        }
        else if (!strcmp_c(*argv, "-o")) {
            if (argc == 2) {
                fprintf(stderr, "BMP path flag \"-o\" provided without subsequent path argument.\n");
                exit(1);
            }
            *path = *(argv + 1);
        }
        else if (!strcmp_c(*argv, "-v")) {
            *verbose = true;
            --argc;
            ++argv;
            continue;
        }
        else {
            fprintf(stderr, "Unrecognised command-line argument: \"%s\"\n", *argv);
            exit(1);
        }
        argc -= 2;
        argv += 2;
    }
    if (!*coords) {
        *coords = malloc(sizeof(coord)*(*s));
        gen_seeds(*s, *coords, *w, *h);
    }
    if (clrs_path) { // allows the -n flag optionally to be specified after the -c flag
        if (!isnan(mu) || !isnan(sd)) {
            fprintf(stderr, "Minimum mean colour and/or standard deviation flags cannot be set if a path for colours "
                            "is also given.\n");
            exit(1);
        }
        struct stat buff;
        if (stat(clrs_path, &buff) == -1) {
            fprintf(stderr, "Error obtaining \"%s\" file information.\n", clrs_path);
            exit(1);
        }
        char *str = malloc(sizeof(char)*buff.st_size);
        if (!str) {
            fprintf(stderr, "Error allocating memory for contents of colour file.\n");
            exit(1);
        }
        if (!(*clrs = malloc(sizeof(ld_clr)*(*s)))) {
            fprintf(stderr, "Error allocating memory for colours found in file.\n");
            exit(1);
        }
        FILE *fp = fopen(clrs_path, "r");
        if (!fp) {
            fprintf(stderr, "Error opening file \"%s\"\n", clrs_path);
            exit(1);
        }
        fread(str, sizeof(char), buff.st_size, fp);
        fclose(fp);
        unsigned int _c = 0;
        ld_clr *cpy = *clrs;
        char *scpy = str;
        char *endptr;
        while (_c++ < *s) {
            cpy->b = strtold(scpy, &endptr);
            if (cpy->b == 0 && scpy == endptr) {
                fprintf(stderr, "Error converting blue channel to long double in line %u of colour file.\n", _c);
                exit(1);
            }
            scpy = endptr + 1;
            cpy->g = strtold(scpy, &endptr);
            if (cpy->g == 0 && scpy == endptr) {
                fprintf(stderr, "Error converting blue channel to long double in line %u of colour file.\n", _c);
                exit(1);
            }
            scpy = endptr + 1;
            cpy->r = strtold(scpy, &endptr);
            if (cpy++->r == 0 && scpy == endptr) {
                fprintf(stderr, "Error converting blue channel to long double in line %u of colour file.\n", _c);
                exit(1);
            }
            scpy = endptr + 1;
        }
        free(str);
    }
    else
        *clrs = gen_cols(*s, isnan(mu) ? MIN_MEAN_COL : mu, isnan(sd) ? MIN_SD_COL : sd);
}

static inline unsigned int round_ld(long double num) {
    if (num < 0)
        return -1;
    return (unsigned int) (num + 0.5l);
}

pix **alloc_pixels(unsigned int w, unsigned int h) {
    if (!w || !h)
        return NULL;
    pix **ptr = malloc(sizeof(pix*)*h);
    if (!ptr)
        return NULL;
    pix **cpy = ptr;
    size_t _s = sizeof(pix)*w; // probably resolved at compile-time but whatever
    unsigned int h_c = 0;
    while (h_c++ < h) {
        if (!(*cpy = malloc(_s))) {
            while (h_c --> 0)
                free(*--cpy);
            free(ptr);
            return NULL;
        }
        zero(*cpy, _s);
        ++cpy;
    }
    return ptr;
}

clr **alloc_bmp(unsigned int w, unsigned int h) {
    if (!w || !h)
        return NULL;
    clr **ptr = malloc(sizeof(clr*)*h);
    if (!ptr)
        return NULL;
    clr **cpy = ptr;
    size_t _s = sizeof(clr)*w; // probably resolved at compile-time but whatever
    unsigned int h_c = 0;
    while (h_c++ < h) {
        if (!(*cpy = malloc(_s))) {
            while (h_c --> 0)
                free(*--cpy);
            free(ptr);
            return NULL;
        }
        ++cpy;
    }
    return ptr;
}

void write_bmp(clr **bmp, unsigned int w, unsigned int h, const char *path,
               unsigned char padding, unsigned int file_size) {
    if (!bmp || !w || !h)
        return;
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "File \"%s\" could not be opened.\n", path);
        abort();
    }
    file_header fh = {};
    fh.header = ('M' << 8) + 'B'; // assumes little-endian
    fh.file_size = file_size;
    fh.offset = 54;
    info_header ih = {};
    ih.header_size = 40;
    ih.width = w;
    ih.height = h;
    ih.panes = 1;
    ih.bpp = 24;
    fwrite(&fh, sizeof(file_header), 1, fp);
    fwrite(&ih, sizeof(info_header), 1, fp);
    unsigned char pad[3] = {};
    if (padding)
        while (h --> 0) {
            fwrite(*bmp++, sizeof(clr), w, fp);
            fwrite(pad, sizeof(unsigned char), padding, fp);
        }
    else
        while (h --> 0)
            fwrite(*bmp++, sizeof(clr), w, fp);
    fclose(fp);
}

unsigned long long *full_time(time_t start, time_t end) {
    if (end < start)
        return NULL;
    static unsigned long long vals[4];
    static unsigned long long *ptr = vals;
    unsigned long long range = end - start;
    *ptr = range / 86400; // get days
    range -= (*ptr++)*86400;
    *ptr = range / 3600; // get hours
    range -= (*ptr++)*3600;
    *ptr = range / 60; // get minutes
    range -= (*ptr++)*60;
    *ptr = range; // get seconds
    return vals;
}

void free_pixels(pix **pixels, unsigned int h) {
    if (!pixels || !h)
        abort();
    pix **cpy = pixels;
    while (h --> 0)
        free(*cpy++);
    free(pixels);
}

_Noreturn void free_bmp(clr **bmp, unsigned int h) {
    if (!bmp || !h)
        abort();
    clr **cpy = bmp;
    while (h --> 0)
        free(*cpy++);
    free(bmp);
    exit(0);
}
