#ifndef FTS_RUN_H
#define FTS_RUN_H

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

struct fts_data{
    int level;
    double max_mtime;
    intmax_t size;
};

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _b : _a; })

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })


typedef void fts_f(FTS *, FTSENT *, struct fts_data *) ;

void ranger_cumsize(FTS *ftsp, FTSENT *entry, struct fts_data *data);
void ranger_maxmtime(FTS *ftsp, FTSENT *entry, struct fts_data *data);
int fts_run(char * const * path, int level, fts_f func, struct fts_data *res);
int par_dir_stat(char *dirpath, struct dirent ***namelist_ret, struct stat **sts);

#endif
