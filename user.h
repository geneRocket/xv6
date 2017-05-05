struct stat;
struct rtcdate;

//system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
/*pipe() creates a pipe, a unidirectional data channel that can be used for interprocess communication. The array pipefd is used to return two file descriptors referring to the ends of the pipe. pipefd[0] refers to the read end of the pipe. pipefd[1] refers to the write end of the pipe. Data written to the write end of the pipe is buffered by the kernel until it is read from the read end of the pipe. */
int pipe(int *);
//ssize_t write(int fildes, const void *buf, size_t nbytes);
int write(int, void*, int);
int read(int, void *, int);
int close(int);
int kill(int);
int exec(char*, char **);
int open(char *, int);
int mknod(char*, short, short);
/*unlink() deletes a name from the filesystem.  If that name was the
 last link to a file and no processes have the file open, the file is
 deleted and the space it was using is made available for reuse.*/
int unlink(char *);
//int fstat(int fildes, struct stat *buf);
/*int fildes 	The file descriptor of the file that is being inquired.
 struct stat *buf 	A structure where data about the file will be stored. A detailed look at all of the fields in this structure can be found in the struct stat page.
 return value 	Returns a negative value on failure.*/
int fstat(int fd, struct stat*);
//make a new name
int link(char*,char *);
int mkdir(char*);
int chdir(char*);
//copy file descriptor
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
//uptime tells you how long the system has been running.
int uptime(void);


//ulib.c
int stat(char*,struct stat*);
char* strcpy(char*, char*);
void *memmove(void*, void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, char*, ...);char* gets(char*, int max);
uint strlen(char*);
void* memset(void*, int, uint);
void *malloc(uint);
void free(void*);
int atoi(const char *);
