/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define unless no X support found */
/* #undef HAVE_X11 */

/* Define when curses library found */
/* #undef HAVE_LIBCURSES */

/* Define when termcap library found */
/* #undef HAVE_LIBTERMCAP */

/* Define when termlib library found */
/* #undef HAVE_LIBTERMLIB */

/* Define when ncurses library found */
#define HAVE_LIBNCURSES 1

/* Define when terminfo support found */
#define TERMINFO 1

/* Define when termcap.h contains ospeed */
#define HAVE_OSPEED 1

/* Define when ospeed can be extern */
/* #undef OSPEED_EXTERN */

/* Define when termcap.h contains UP, BC and PC */
#define HAVE_UP_BC_PC 1

/* Define when UP, BC and PC can be extern */
/* #undef UP_BC_PC_EXTERN */

/* Define when termcap.h defines outfuntype */
/* #undef HAVE_OUTFUNTYPE */

/* Define when __DATE__ " " __TIME__ can be used */
#define HAVE_DATE_TIME 1

#define UNIX 1
#define SVR4 1

/* Defined to the size of an int */
#define SIZEOF_INT 4

/*
 * If we cannot trust one of the following from the libraries, we use our
 * own safe but probably slower vim_memmove().
 */
/* #undef USEBCOPY */
#define USEMEMMOVE 1
/* #undef USEMEMCPY */

/* Define when "man -s 2" is to be used */
#define USEMAN_S 1

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef ino_t */

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if you can safely include both <sys/time.h> and <sys/select.h>.  */
#define SYS_SELECT_WITH_SYS_TIME 1

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define as the command at the end of signal handlers ("" or "return 0;").  */
#define SIGRETURN return

/* Define if struct sigcontext is present */
#define HAVE_SIGCONTEXT 1

/* Define if touuper/tolower only work on lower/upercase characters */
/* #undef BROKEN_TOUPPER */

/* Define if tgetstr() has a second argument that is (char *) */
/* #undef TGETSTR_CHAR_P */

/* Define if you have the bcmp() function.  */
#define HAVE_BCMP 1

/* Define if you have the fchdir() function. */
#define HAVE_FCHDIR 1

/* Define if you have the fchown() function.  */
#define HAVE_FCHOWN 1

/* Define if you have the fseeko() function. */
#define HAVE_FSEEKO 1

/* Define if you have the fsync() function. */
#define HAVE_FSYNC 1

/* Define if you have the ftello() function. */
#define HAVE_FTELLO 1

/* Define if the getcwd() function should not be used.  */
/* #undef BAD_GETCWD */

/* Define if you have the getcwd() function.  */
#define HAVE_GETCWD 1

/* Define if you have the getpwnam() function. */
#define HAVE_GETPWNAM 1

/* Define if you have the getpwuid() function. */
#define HAVE_GETPWUID 1

/* Define if you have the gettimeofday() function. */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the getwd() function.  */
#define HAVE_GETWD 1

/* Define if you have the lstat() function. */
#define HAVE_LSTAT 1

/* Define if you have the memcmp() function.  */
#define HAVE_MEMCMP 1

/* Define if you have the memset() function.  */
#define HAVE_MEMSET 1

/* Define if you have the nanosleep() function.  */
#define HAVE_NANOSLEEP 1

/* Define if you have the opendir() function. */
#define HAVE_OPENDIR 1

/* Define if you have the putenv() function. */
#define HAVE_PUTENV 1

/* Define if you have the qsort() function. */
#define HAVE_QSORT 1

/* Define if you have the select() function.  */
#define HAVE_SELECT 1

/* Define if you have the setenv() function. */
#define HAVE_SETENV 1

/* Define if you have the setpgid() function.  */
#define HAVE_SETPGID 1

/* Define if you have the setsid() function.  */
#define HAVE_SETSID 1

/* Define if you have the sigset() function.  */
#define HAVE_SIGSET 1

/* Define if you have the strcasecmp() function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strerror() function.  */
#define HAVE_STRERROR 1

/* Define if you have the strftime() function.  */
#define HAVE_STRFTIME 1

/* Define if you have the stricmp() function.  */
/* #undef HAVE_STRICMP */

/* Define if you have the strncasecmp() function.  */
#define HAVE_STRNCASECMP 1

/* Define if you have the strnicmp() function.  */
/* #undef HAVE_STRNICMP */

/* Define if you have the strpbrk() function.  */
#define HAVE_STRPBRK 1

/* Define if you have the strtol() function.  */
#define HAVE_STRTOL 1

/* Define if you have the tgetent() function.  */
#define HAVE_TGETENT 1

/* Define if you have st_blksize in struct stat. */
#define HAVE_ST_BLKSIZE 1

/* Define if you have the rename() function. */
#define HAVE_RENAME 1

/* Define if you have the usleep() function.  */
#define HAVE_USLEEP 1

/* Define if you have the utime() function. */
#define HAVE_UTIME 1

/* Define if you do not have utime(), but do have the utimes() function. */
#define HAVE_UTIMES 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have a <sys/wait.h> that is not POSIX.1 compatible. */
/* #undef HAVE_UNION_WAIT */

/* This is currently unused in vim: */
/* Define if you have the ANSI C header files. */
/* #undef STDC_HEADERS */

/* instead, we check a few STDC things ourselves */
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/utsname.h> header file.  */
#define HAVE_SYS_UTSNAME_H 1

/* Define if you have the <termcap.h> header file.  */
#define HAVE_TERMCAP_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <sgtty.h> header file.  */
#define HAVE_SGTTY_H 1

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <termio.h> header file.  */
#define HAVE_TERMIO_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <stropts.h> header file. */
#define HAVE_STROPTS_H 1

/* Define if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define if you have the <Xm/Xm.h> header file (only used by configure). */
/* #undef HAVE_XM_XM_H */

/* Define if strings.h cannot be included when strings.h already is */
/* #undef NO_STRINGS_WITH_STRING_H */

/* Define if you have the <sys/systeminfo.h> header file. */
/* #undef HAVE_SYS_SYSTEMINFO_H */

/* Define if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define if you have the <sys/stream.h> header file. */
/* #undef HAVE_SYS_STREAM_H */

/* Define if you have the <sys/ptem.h> header file. */
/* #undef HAVE_SYS_PTEM_H */

/* Define if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define if you have the <libc.h> header file. */
/* #undef HAVE_LIBC_H */

/* Define if you have the <sys/statfs.h> header file. */
#define HAVE_SYS_STATFS_H 1

/* Define if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define if you have the <pwd.h> header file. */
#define HAVE_PWD_H 1

/* Define if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define if you want maximal features. */
/* #undef MAX_FEAT */

/* Define if you want minimal features. */
/* #undef MIN_FEAT */

/* Define if you want to include the Perl interpreter. */
/* #undef HAVE_PERL_INTERP */

/* Define if you want to include the Python interpreter. */
/* #undef HAVE_PYTHON */

/* Define if you want to include the Tcl interpreter. */
/* #undef HAVE_TCL */

/* Define if you want to add support of GPM (Linux console mouse daemon) */
/* #undef HAVE_GPM */

/* Define if you want to include the Cscope interface. */
/* #undef USE_CSCOPE */

/* Define if you want to include multibyte support. */
/* #undef MULTI_BYTE */

/* Define if you want to include fontset support. */
/* #undef USE_FONTSET */

/* Define if you want to include XIM support. */
/* #undef USE_XIM */

/* Define if you want to include Hangul input support. */
/* #undef HANGUL_INPUT */

/* Define if your libc doesn't work POSIX.2 locale function correctly */
/* #undef BROKEN_LOCALE */

/* Define if your X has own locale library */
/* #undef X_LOCALE */
