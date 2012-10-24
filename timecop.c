#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <dlfcn.h> /* RTLD_NEXT, dlsym(2) */

/**
 This is a time traveling library in C which is inspired by php-timecop.
 Currenty this is concept of proof for being able to replace PHP code using php-timecop.
 Thererfore i will target the following functions.

 m@e1:~/wrk/c-timecop$ nm /usr/local/stow/php-5.4.7/bin/php  | egrep '(day|time|date)' | grep GLIBC
                 U asctime@@GLIBC_2.2.5
                 U asctime_r@@GLIBC_2.2.5
                 U gettimeofday@@GLIBC_2.2.5
                 U gmtime@@GLIBC_2.2.5
                 U gmtime_r@@GLIBC_2.2.5
                 U localtime@@GLIBC_2.2.5
                 U localtime_r@@GLIBC_2.2.5
                 U mktime@@GLIBC_2.2.5
                 U setitimer@@GLIBC_2.2.5
                 U strftime@@GLIBC_2.2.5
                 U strptime@@GLIBC_2.2.5
                 U time@@GLIBC_2.2.5
  so, replace time and gettimeofday only.
  and I also reffer from http://superuser.com/questions/226033/how-to-freeze-the-clock-for-a-specific-task.
  add timeb(2).

  I expect the php following functions will replace above inner function (ref. php-timecop and thanks @hnw)
	time()
	mktime()
	gmmktime()
	date()
	gmdate()
	idate()
	getdate()
	localtime()
	strtotime()
	strftime()
	gmstrftime()
	unixtojd() 
 */

#define DBGOUT(...)		fprintf(stderr, __VA_ARGS__)

#include <time.h> 
  typedef time_t (* tc_time_t)(time_t *t); 
  typedef int (* tc_clock_gettime_t)(clockid_t clk_id, struct timespec *tp); 

#include <sys/time.h>
  typedef int (* tc_gettimeofday_t)(struct timeval *tv, struct timezone *tz);
  typedef int (* tc_settimeofday_t)(const struct timeval *tv, const struct timezone *tz);

#include <sys/timeb.h>
  typedef int (* tc_ftime_t)(struct timeb *tp);

struct hook_funcs {
  tc_time_t tc_time;
  tc_clock_gettime_t tc_clock_gettime;
  tc_gettimeofday_t tc_gettimeofday;
  tc_settimeofday_t tc_settimeofday;
  tc_ftime_t tc_ftime;
};

static struct tc_conf_t {
  int is_initialize;    
#define FALSE 0
#define TRUE  !FALSE
  time_t freeze;
  struct hook_funcs * funcs;
} tc_conf = { 
  FALSE, 
  (time_t) 0, 
  NULL 
}; 

#define	default_func(name)	tc_conf.funcs->tc_##name
static void __attribute__((constructor))
tc_constructor() {
  if (tc_conf.is_initialize) return;
  tc_conf.funcs = (struct hook_funcs *) malloc( sizeof(*tc_conf.funcs) );
  if (!tc_conf.funcs) {
    fprintf(stderr, "%s: Cannnot allocation memory\n", __FUNCTION__);
    return;
  }
#define	register_func(name)	do { \
       const char * nouse = "no_" #name; \
       if (getenv(nouse) ) break; \
       default_func(name) = (tc_##name##_t) dlsym(RTLD_NEXT, #name); \
       if ( !default_func(name) ) { \
          fprintf(stderr,"%s: Cannot resolve the symbol %s - %s \n", __FUNCTION__, #name, dlerror() ); \
          fflush(NULL); \
          exit(EXIT_FAILURE); \
       } \
     } while (0);

  /* time.h */
  register_func(time);
  register_func(clock_gettime);
  /* sys/time.h */
  register_func(gettimeofday)
  register_func(settimeofday)
  /* sys/timeb.h */
  register_func(ftime)

  /* freeze */
  tc_conf.freeze = time(NULL);
  static const char * FREEZE_TIME = "TC_FREEZE_TIME";
  if (getenv(FREEZE_TIME)) {
    time_t afreeze = atoi( getenv(FREEZE_TIME) );
    if (afreeze) tc_conf.freeze = afreeze;
  }
  tc_conf.is_initialize = TRUE;
}

static void __attribute__((destructor))
tc_destructor() {
  if (!tc_conf.is_initialize) return;
  free(tc_conf.funcs);
}

/**
 * Each hook functions can enable and disable on the fly.
 * Use `enable_XXX' env.
 */
#define prologue(f, ...)  do { \
    const char * is_enable = "enable_" #f; \
    if ( NULL == default_func(f) || NULL == getenv(is_enable) ) return default_func(f)(__VA_ARGS__); \
  } while(0)

#define epilogue(f,x)	x

time_t time(time_t *t) {
  prologue(time, t);
  time_t ret;
  ret = tc_conf.freeze;
  if (t) *t = tc_conf.freeze;
  return epilogue(time, ret);
}

int 
gettimeofday(struct timeval *tv, struct timezone *tz) {
  prologue(gettimeofday, tv, tz);
  int ret = 0;
  if (tz) {
  /**  
    The  use  of  the  timezone  structure  is  obsolete; the tz argument should normally be specified as NULL.  The
    tz_dsttime field has never been used under Linux; it has not been and will not be supported by  libc  or  glibc.
    Each  and  every occurrence of this field in the kernel source (other than the declaration) is a bug.  Thus, the
    following is purely of historic interest.
   */
   assert(tz == NULL);
   return default_func(gettimeofday)(tv, tz);
  }
  if (tv) {
    tv->tv_sec = tc_conf.freeze;
    tv->tv_usec = 0;
  } else 
    return default_func(gettimeofday)(tv, tz);
  return epilogue(getitimer, ret);
}

int 
settimeofday(const struct timeval *tv, const struct timezone *tz) {
  prologue(settimeofday, tv, tz);
  int ret = 0;;
  static const char * REWRITE = "TC_REWRITE";
  if (!getenv(REWRITE) || !tv) return default_func(settimeofday)(tv, tz);
  tc_conf.freeze = tv->tv_sec;
  return epilogue(settimeofday, ret);
}

int 
clock_gettime(clockid_t clk_id, struct timespec *tp) {
  prologue(clock_gettime, clk_id, tp);
  int ret = 0;
  if (tp) {
    tp->tv_sec = tc_conf.freeze;
    tp->tv_nsec = 0;
  } else
    ret = -1;
  return epilogue(clock_gettime, ret);
}

int 
ftime(struct timeb *tp) {
  prologue(ftime, tp);
  int ret = 0;
  if (tp) {
    // ftime(2): 
    // POSIX.1-2001 says that the contents of the timezone and dstflag fields are unspecified; avoid relying on them.
    // but these values setting by gettimeofday 
    tp->time = tc_conf.freeze;
    tp->millitm = 0;
    struct timezone tz;
    int res = default_func(gettimeofday)(NULL, &tz);
    assert(res == 0);
    tp->timezone = tz.tz_minuteswest;
    tp->dstflag = tz.tz_dsttime;
  } else
    ret = -1;
  return epilogue(ftime, ret);
}

