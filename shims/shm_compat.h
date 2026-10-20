/*
 * shm_compat.h - shm_open() / shm_unlink() for Android (Bionic)
 *
 * Bionic has no POSIX shared memory. These implement the two functions
 * on top of memfd_create(), which Bionic does have.
 *
 * SCOPE - read this before using it.
 *
 * A memfd has no name in the filesystem. Unrelated processes therefore
 * cannot open the same object by name. This is a drop-in replacement
 * ONLY for the common pattern of:
 *
 *     fd = shm_open(name, ...);  ftruncate(fd, n);  mmap(..., fd, 0);
 *
 * where one process wants anonymous shared memory for itself and its
 * children (the fd is inherited across fork, and can be passed over a
 * unix socket). It is NOT a replacement for name-based sharing between
 * unrelated processes.
 *
 * Because every call creates a fresh object, O_EXCL never collides and
 * O_CREAT is implied. Code that loops over candidate names to find a
 * free one will succeed on the first attempt.
 *
 * Usage: include it, or force-include it with -include shm_compat.h.
 * Define SHM_COMPAT_DISABLE to compile it out.
 *
 * Public domain / CC0. Use it however you like.
 */

#ifndef SHM_COMPAT_H_
#define SHM_COMPAT_H_

#if (defined(__ANDROID__) || defined(__OHOS__)) && !defined(SHM_COMPAT_DISABLE)

#include <errno.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * memfd_create() is reached through syscall() rather than called
 * directly: Bionic only declares the wrapper when building against
 * API 30 or newer, and this header should work at lower API levels.
 */
int pthread_getname_np(pthread_t, char *, size_t);
void *aligned_alloc(size_t, size_t);
int pthread_cond_clockwait(pthread_cond_t *, pthread_mutex_t *, clockid_t, const struct timespec *);
int pthread_mutex_clocklock(pthread_mutex_t *, clockid_t, const struct timespec *);

static inline int shm_compat_open_(const char *name, int oflag, mode_t mode)
{
	unsigned int flags = 0;
	long fd;

	(void)mode;                 /* memfd permissions come from umask */

	if (name == NULL) {
		errno = EINVAL;
		return -1;
	}
	/* POSIX names start with '/'; memfd names must not. */
	if (*name == '/')
		name++;
	if (*name == '\0') {
		errno = EINVAL;
		return -1;
	}

	if (oflag & O_CLOEXEC)
		flags |= MFD_CLOEXEC;

	fd = syscall(SYS_memfd_create, name, flags);
	if (fd < 0)
		return -1;          /* syscall() has set errno */

	return (int)fd;
}

static inline int shm_compat_unlink_(const char *name)
{
	if (name == NULL) {
		errno = EINVAL;
		return -1;
	}
	/* A memfd disappears when its last fd is closed. Nothing to do. */
	return 0;
}

#ifdef __cplusplus
}
#endif

#define shm_open  shm_compat_open_
#define shm_unlink shm_compat_unlink_

#endif /* __ANDROID__ && !SHM_COMPAT_DISABLE */
#endif /* SHM_COMPAT_H_ */
