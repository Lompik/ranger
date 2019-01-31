
#define STATX

#define _GNU_SOURCE
#define _ATFILE_SOURCE
#include <sys/types.h>
#ifdef STATX
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/syscall.h>

#define AT_STATX_SYNC_TYPE	0x6000
#define AT_STATX_SYNC_AS_STAT	0x0000
#define AT_STATX_FORCE_SYNC	0x2000
#define AT_STATX_DONT_SYNC	0x4000

static __attribute__((unused))
ssize_t mystatx(int dfd, const char *filename, unsigned flags,
	      unsigned int mask, struct statx *buffer)
{
	return syscall(__NR_statx, dfd, filename, flags, mask, buffer);
}
#else
#include <sys/stat.h>
#include <sys/fcntl.h>
#endif

#include "fts_run.h"
#include <time.h>

// call this function to start a nanosecond-resolution timer
struct timespec get_time(){
    struct timespec start_time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
    return start_time;
}
// call this function to end a timer, returning nanoseconds elapsed as a long
long time_diff(struct timespec t1, struct timespec t2){
    long diffInNanos = (t2.tv_sec - t1.tv_sec) * (long)1e9 + (t2.tv_nsec - t1.tv_nsec);
    return diffInNanos;
}

int inosort(const struct dirent **a, const struct dirent **b){
    return (*a)->d_ino - (*b)->d_ino;
}


int par_dir_stat(char *dirpath,  struct dirent ***namelist, struct stat **sts){
    DIR *dirp;

    errno=0;
    dirp = opendir(dirpath);
    if (dirp == NULL) {
        printf("opendir failed on '%s'", dirpath);
        return 0;
    }
    int i=0;
    const int dirf = dirfd(dirp);
    int imax = scandir(dirpath, namelist, NULL, inosort);
    if(imax<=0) {
        closedir(dirp);
        return 0;
    }
    *sts= malloc((imax) * sizeof(struct stat));
    /* For each entry in this directory, print directory + filename */
    int max_threads = min(16, imax);

#pragma omp parallel for num_threads(max_threads) firstprivate(dirf, namelist) private(i) shared(sts) ordered
    for (i=0; i < imax; i++) {
        errno = 0;               /* To distinguish error from end-of-directory */
#ifdef STATX
        struct statx stx;
        int atflag = AT_SYMLINK_NOFOLLOW;
        atflag |= AT_STATX_DONT_SYNC;
        unsigned int mask = STATX_BASIC_STATS;
        int ret = mystatx(dirf, (*namelist)[i]->d_name, atflag, mask, &stx);
        if(ret!=0){
            printf("Error in statx(pardirstat): dir:%s name:%s %d\n", dirpath, (*namelist)[i]->d_name, errno);
            perror("pardirstat(statx)");
            continue;
        };
#define set(name) (*sts)[i].st_##name=stx.stx_##name;
        set(mode);
        set(size);
        set(ino);
        set(nlink);
        set(uid);
        set(gid);
        (*sts)[i].st_dev = (stx.stx_dev_major << 8) + stx.stx_dev_minor;
#undef set
#define set(name1,name2) (*sts)[i].st_##name1.tv_sec=stx.stx_##name2.tv_sec;(*sts)[i].st_##name1.tv_nsec=stx.stx_##name2.tv_nsec;
        set(atim,atime);
        set(ctim,ctime);
        set(mtim,mtime);
#undef set
#else
        if(fstatat(dirf, (*namelist)[i]->d_name, &(*sts)[i], 0)!=0){
            if(errno==ENOENT
               #ifdef HAVE_DIRENT_D_TYPE
               && ((*namelist)[i]->d_type==DT_LNK)
               #endif
                ){
                if(fstatat(dirf, (*namelist)[i]->d_name, &(*sts)[i], AT_SYMLINK_NOFOLLOW)==0)
                   continue;

            }
            printf("Error in statat(pardirstat): dir:%s name:%s %d", dirpath, (*namelist)[i]->d_name, errno);
        };
#endif
    }
    closedir(dirp);
    return imax;
}

void ranger_cumsize(FTS *ftsp, FTSENT *entry, struct fts_data *data){
    if((entry->fts_info != FTS_NSOK))
        return;

#ifdef STATX
    struct statx stx;
    int atflag = AT_SYMLINK_NOFOLLOW;
    atflag |= AT_STATX_DONT_SYNC;
    unsigned int mask = STATX_SIZE;
    int ret = statx(AT_FDCWD, entry->fts_path, atflag, mask, &stx);
    if(ret == 0)
        data->size += (stx.stx_size);
#else
    struct stat pstat;
    if(stat(entry->fts_path, &pstat) == 0)
        data->size += (pstat.st_size);
#endif // STATX

}

void ranger_maxmtime(FTS *ftsp, FTSENT *entry, struct fts_data *data){

    if(entry->fts_info != FTS_D)
        return;
    int level = data->level;
    if((level>0) & (entry->fts_level==level)) {
        fts_set(ftsp, entry, FTS_SKIP);
    }
#ifdef STATX
    struct statx stx;
    int atflag = AT_SYMLINK_NOFOLLOW;
    atflag |= AT_STATX_DONT_SYNC;
    unsigned int mask = STATX_MTIME ;
    int ret = statx(AT_FDCWD, entry->fts_path, atflag, mask, &stx);
    if(ret == 0)
        data->max_mtime = max(data->max_mtime, (stx.stx_mtime.tv_sec+1e-9*stx.stx_mtime.tv_nsec));
#else
    struct stat pstat;
    if(stat(entry->fts_path, &pstat) == 0)
        data->max_mtime = max(data->max_mtime, (pstat.st_mtim.tv_sec+1e-9*pstat.st_mtim.tv_nsec));
#endif
}

int fts_run(char * const * path, int level, fts_f func, struct fts_data *res){

    FTS *ftsp = fts_open(path, FTS_PHYSICAL | FTS_NOSTAT | FTS_NOCHDIR, NULL);

    if (ftsp == NULL) {
        perror("fts_open");
        return 1;
    }
    FTSENT *entry = fts_read(ftsp);
    do{
        func(ftsp, entry, res);
    } while((entry = fts_read(ftsp)) != NULL);


    if ((fts_close(ftsp)) == -1) {
        perror("fts_close");
        return 1;
    }

    return(0);
}

/* int main(int argc, char *argv[]) */
/* { */
/*     const char *top[2]; */
/*     struct timespec t1,t2; */
/*     //top[0] = "/mnt/localDev/Kimsufi"; */
/*     //top[0] = "/usr"; */
/*     top[0] = "/home/lompik"; */
/*     top[1] = (char * ) NULL; */
/*     struct fts_data res = {0, 0, 0}; */
/*     t1 = get_time(); */
/*     fts_run((char * const *)top, 2, ranger_maxmtime, &res); */
/*     t2 = get_time(); */
/*     printf("maxtime: %f %fms\n", res.max_mtime, time_diff(t1, t2) * 1e-6); */

/*     t1 = get_time(); */
/*     struct fts_data res2 = {-1, 0, 0}; */
/*     fts_run((char * const *)top, -1, ranger_cumsize, &res2); */
/*     t2 = get_time(); */
/*     printf("cumsize: %ld %fms\n", res2.size, time_diff(t1, t2) * 1e-6); */
/*     return 0; */
/* } */


/* int main(int argc, char *argv[]) */
/* { */
/*     char *top="/tmp"; */
/*     struct dirent **namelist=NULL; */
/*     struct stat *sts=NULL; */
/*     int imax = par_dir_stat(top, &namelist, &sts); */
/*     if(imax==0) return 1; */
/*     free(sts); */
/*     for(int i=0; i< imax; i++) free(namelist[i]); */
/*     free(namelist); */
/*     return 0; */
/* } */
