#include <dirent.h>
#include "tlpi_hdr.h"
#include <sys/stat.h>
#include "khash.h"

struct stats {
    char *filename;
    unsigned long st_dev;
    unsigned long st_ino;
    uint st_uid;
    uint st_gid;
    ushort st_mode;
    uint st_nlink;
    unsigned long long st_size;
    double s_atime;
    double s_ctime;
    double s_mtime;
    /* unsigned long st_rdev; */
    /* long long st_atime; */
    /* long st_atimendesc; */
    /* long long st_mtime; */
    /* long st_mtimendesc; */
    /* long long st_ctime; */
    /* long st_ctimendesc; */
    /* int64 st_blocks; */
    /* uint32 st_blksize; */
    /* uint32 st_flags; */
    /* uint32 st_gen; */
};

char * join_path_filenane(const char *path, const char *filename){
    size_t path_len = strlen(path) +1;
    size_t filename_len = strlen(filename) +1;
    size_t size =  path_len + filename_len;
    char * res = malloc(size);
    strcpy(res, path);

    if (path_len > 0 && res[path_len - 2] != '/'){
        res[path_len-1] = '/';
        path_len++;
    }

    strcpy(res + path_len - 1, filename);
    return res;
}

int listFiles( char *dirpath, struct stats **res) { /* List all files in directory 'dirPath' */
    DIR *dirp;
    struct dirent *dp_n;
    struct dirent **dps= malloc(50000* sizeof(struct dirent*));
    Boolean isCurrent;          /* True if 'dirpath' is "." */

    isCurrent = strcmp(dirpath, ".") == 0;

    dirp = opendir(dirpath);
    if (dirp == NULL) {
        errMsg("opendir failed on '%s'", dirpath);
        free(dps);
        return 1;
    }

    int i=0;
    while((dp_n=readdir(dirp)) != NULL){
        if (strcmp(dp_n->d_name, "..") == 0)
            continue;           /* Skip . and .. */
        dps[i] = dp_n;
    }
    int imax=i;
    struct stats *sts;
    sts = malloc((imax)* sizeof(struct stats));
    /* For each entry in this directory, print directory + filename */
#pragma omp parallel for num_threads(16) private(i) shared(dps, sts)
    for (i=0; i<imax; i++) {
        errno = 0;               /* To distinguish error from end-of-directory */
        struct dirent *dp = dps[i];

        /* if (!isCurrent) */
        /*     printf("%s/", dirpath); */
        /* printf("%s\n", dp->d_name); */
        struct stat statbuf;
        char *filename = join_path_filenane(dirpath, dp->d_name);
        stat(filename, &statbuf);
        struct stats st;
        st.filename = filename;
        st.st_ino = statbuf.st_ino;
        st.st_size = statbuf.st_size;
        st.st_dev = statbuf.st_dev;
        st.st_ino = statbuf.st_ino;
        st.st_uid = statbuf.st_uid;
        st.st_gid = statbuf.st_gid;
        st.st_mode = statbuf.st_mode;
        st.st_nlink = statbuf.st_nlink;
        st.st_size = statbuf.st_size;
        st.s_atime = statbuf.st_atim.tv_sec + statbuf.st_atim.tv_nsec*1e-9;
        st.s_mtime = statbuf.st_mtim.tv_sec + statbuf.st_mtim.tv_nsec*1e-9;
        st.s_ctime = statbuf.st_ctim.tv_sec + statbuf.st_ctim.tv_nsec*1e-9;
        //#pragma omp atomic write
        sts[i] = st;
        //free(st);
        //free(filename);
        //printf("inode: %lu mode:%x size:%lu\n", kh_value(h, k).st_ino, kh_value(h, k).st_mode, kh_value(h, k).st_size);
    }

    /* for(int i=0; i<imax; i++) */
    /*         printf("%s inode: %lu mode:%x size:%llu\n",  sts[i].filename, sts[i].st_ino, sts[i].st_mode, sts[i].st_size); */
    closedir(dirp);
    free(dps);
    *res = sts;
    //printf("%lu %lu %p res:%p\n", sizeof(sts), sizeof(*sts), sts, res);
    return imax;
    /* if (errno != 0) */
    /*     errExit("readdir"); */

    /* if (closedir(dirp) == -1) */
    /*     errMsg("closedir"); */
}

void release(struct stats **res, int imax){
    for(int i=0; i<imax; i++)
        free((*res)[i].filename);
    free(*res);
}
/* int */
/* main(int argc, char *argv[]) */
/* { */
/*     printf("DT_REG: %d\n", DT_REG); */
/*     printf("DT_REG: %d\n", DT_UNKNOWN); */
/*     int imax=0; */
/*     struct stats *sts; */
/*     if (argc > 1 && strcmp(argv[1], "--help") == 0) */
/*         usageErr("%s [dir...]\n", argv[0]); */

/*     if (argc == 1)               /\* No arguments - use current directory *\/ */
/*         imax=listFiles("/mnt/localDev/Kimsufi/Musics", &sts); */
/*         //listFiles("/usr/lib"); */
/*     else */
/*         for (argv++; *argv; argv++) */
/*             imax=listFiles(*argv, &sts); */
/*     release(&sts, imax); */
/*     exit(EXIT_SUCCESS); */
/* } */
