// Shell.

#include"types.h"
#include"user.h"
#include"fcntl.h"

// Parsed command representation
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5

#define MAXARGS 10

struct cmd {
	int type;
};

struct execcmd {
	int type;
	char *argv[MAXARGS];
	char *eargv[MAXARGS]; //end of argv
};

struct redircmd {
	int type;
	struct cmd *cmd;
	char *file;
	char *efile;
	int mode;
	int fd; //被重定向的文件描述符
};

//管道命令可以看成两条命令合并
struct pipecmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

/*
 0	stdin	Standard input
 1	stdout	Standard output
 2	stderr	Standard error
 */

//同一个行两条指令;隔开,顺序执行
struct listcmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

//两条指令 & 隔开 ，异步执行
struct backcmd {
	int type;
	struct cmd *cmd;
};

int fork1();
void panic(char *);

struct cmd* parsecmd(char *);

// Execute cmd.  Never returns.
void runcmd(struct cmd *cmd) {
	int p[2];
	struct backcmd *bcmd;
	struct execcmd *ecmd;
	struct listcmd *lcmd;
	struct pipecmd *pcmd;
	struct redircmd *rcmd;

	if (cmd == 0)
		exit();
	switch (cmd->type) {
	default:
		panic("runcmd");
	case EXEC:
		ecmd = (struct execcmd*) cmd;
		if (ecmd->argv[0] == 0)
			exit();
		exec(ecmd->argv[0], ecmd->argv);
		printf(2, "exec %s failed\n", ecmd->argv[0]);
		/*The exec system call replaces the calling process’s memory with a new memory
		 image loaded from a file stored in the file system.
		 When exec succeeds, it does not return to the calling program; instead,
		 the instructions loaded from the file start executing at the entry point declared in the
		 ELF header.
		 */
		break;
	case REDIR:
		rcmd = (struct redircmd*) cmd;
		close(rcmd->fd);
		if (open(rcmd->file, rcmd->mode) < 0) {
			printf(2, "open %s failed\n", rcmd->file);
			exit();
		}
		//关了，打开就行就行了？
		/*A newly allocated file descriptor is al-
		 ways the lowest-numbered unused descriptor of the current process.

		 Fork copies the parent’s file descriptor table along with its memory, so that the child
		 starts with exactly the same open files as the parent. The system call exec replaces the
		 calling process’s memory but preserves its file table.
		 */
		runcmd(rcmd->cmd);
		break;
	case LIST:
		lcmd = (struct listcmd*) cmd;
		if (fork1() == 0)
			runcmd(lcmd->left);
		wait();
		//Although fork copies the file descriptor table, each underlying file offset is shared between parent and child.
		runcmd(lcmd->right);
		break;
	case PIPE:

		pcmd = (struct pipecmd*) cmd;
		//p[0]输入，p[1]输出
		if (pipe(p) < 0)
			panic("pipe");
		if (fork1() == 0) {
			close(1);
			dup(p[1]);
			close(p[0]);
			close(p[1]);
			/*
			 If no data is available,a read on a pipe waits for either data to be written or all
			 file descriptors referring to the write end to be closed;
			 The fact that read blocks
			 until it is impossible for new data to arrive is one reason that it’s important for the
			 child to close the write end of the pipe before executing wc above: if one of wc’s file
			 descriptors referred to the write end of the pipe, wc would never see end-of-file
			 */
			runcmd(pcmd->left);
		}
		if (fork1() == 0) {
			close(0);
			dup(p[0]);
			close(p[0]);
			close(p[1]);

			runcmd(pcmd->right);
		}
		close(p[0]);
		close(p[1]);
		wait();
		wait();
		break;
	case BACK: //????????
		bcmd = (struct backcmd*) cmd;
		if (fork1() == 0)
			runcmd(bcmd->cmd);
		break;
	}
	exit();
}

int getcmd(char *buf, int nbuf) {
	printf(2, "$ ");
	memset(buf, 0, nbuf);
	gets(buf, nbuf);
	if (buf[0] == 0)
		return -1;
	return 0;
}

int main() {
	static char buf[100];
	int fd;

	// Ensure that three file descriptors are open.
	while ((fd = open("console", O_RDWR)) >= 0) {
		if (fd >= 3) {
			close(fd);
			break;
		}
	}

	// Read and run input commands.
	while (getcmd(buf, sizeof(buf)) >= 0) {
		if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
			buf[strlen(buf) - 1] = 0;
			if (chdir(buf + 3) < 0)
				printf(2, "cannot cd %s\n", buf + 3);
			continue;
		}
		if (fork1() == 0)
			runcmd(parsecmd(buf));
		wait();
	}
	exit();
}

void panic(char *s) {
	printf(2, "%s\n", s);
	exit();
}

int fork1() {
	int pid;

	pid = fork();
	if (pid == -1)
		panic("fork");
	return pid;
}

struct cmd*execcmd(void) {
	struct execcmd *cmd;
	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = EXEC;
	return (struct cmd*) cmd;
}

struct cmd* redircmd(struct cmd *subcmd, char *file, char *efile, int mode,
		int fd) {
	struct redircmd *cmd;
	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = REDIR;
	cmd->cmd = subcmd;
	cmd->file = file;
	cmd->efile = efile;
	cmd->mode = mode;
	cmd->fd = fd;
	return (struct cmd*) cmd;
}

struct cmd* pipecmd(struct cmd *left, struct cmd *right) {
	struct pipecmd *cmd;

	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = PIPE;
	cmd->left = left;
	cmd->right = right;
	return (struct cmd*) cmd;
}

struct cmd* listcmd(struct cmd*left, struct cmd *right) {
	struct listcmd *cmd;
	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = LIST;
	cmd->left = left;
	cmd->right = right;
	return (struct cmd*) cmd;
}

struct cmd* backcmd(struct cmd *subcmd) {
	struct backcmd *cmd;
	cmd = malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = BACK;
	cmd->cmd = subcmd;
	return (struct cmd*) cmd;
}

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

//获取一个词或者一个连接符
int gettoken(char **ps, char *es, char **q, char **eq) {
	char *s;
	int ret;

	s = *ps;
	while (s < es && strchr(whitespace, *s))
		s++;
	if (q)
		*q = s;
	ret = *s;
	switch (*s) {
	case 0:
		break;
	case '|':
	case '(':
	case ')':
	case ';':
	case '&':
	case '<':
		s++;
		break;
	case '>':
		s++;
		if (*s == '>') { //>>
			ret = '+';
			s++;
		}
		break;
	default:
		ret = 'a';
		while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
			s++;
		break;
	}
	if (eq)
		*eq = s;
	while (s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return ret;
}

//获取一个词的首字符是否在toks中
int peek(char **ps, char *es, char *toks) {
	char *s;
	s = *ps;
	while (s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd* parsecmd(char *s) {
	char *es;
	struct cmd *cmd;
	es = s + strlen(s);
	cmd = parseline(&s, es);
	peek(&s, es, "");
	if (s != es) {
		printf(2, "leftvoers:%s\n", s);
		panic("syntax");
	}
	nulterminate(cmd);
	return cmd;
}

struct cmd*parseline(char **ps, char *es) {
	struct cmd *cmd;
	cmd = parsepipe(ps, es);
	while (peek(ps, es, "&")) {
		gettoken(ps, es, 0, 0);
		cmd = backcmd(cmd); //连接
	}
	if (peek(ps, es, ";")) {
		gettoken(ps, es, 0, 0);
		cmd = listcmd(cmd, parseline(ps, es));
	}
	return cmd;
}

struct cmd* parsepipe(char **ps, char *es) {
	struct cmd *cmd;
	cmd = parseexec(ps, es);
	if (peek(ps, es, "|")) {
		gettoken(ps, es, 0, 0);
		cmd = pipecmd(cmd, parsepipe(ps, es));
	}
	return cmd;
}

struct cmd*parseredirs(struct cmd *cmd, char **ps, char *es) {
	int tok;
	char *q, *eq;
	/*mkdir creates a
	 new directory, open with the O_CREATE flag creates a new data file*/
	while (peek(ps, es, "<>")) {
		tok = gettoken(ps, es, 0, 0);
		if (gettoken(ps, es, &q, &eq) != 'a')
			panic("missing file for redirection");
		switch (tok) {
		case '<':
			cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
			break;
		case '>':
			cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
			break;
		case '+':
			cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
			break;
		}
	}
	return cmd;
}

struct cmd* parseblock(char **ps, char *es) {
	struct cmd *cmd;
	if (!peek(ps, es, ("(")))
		panic("parseblock");
	gettoken(ps, es, 0, 0);
	cmd = parseline(ps, es);
	if (!peek(ps, es, (")")))
		panic("syntax - missing )");
	gettoken(ps, es, 0, 0);
	cmd = parseredirs(cmd, ps, es);
	return cmd;
}

struct cmd*parseexec(char **ps, char *es) {
	char *q, *eq;
	int tok, argc;
	struct execcmd* cmd;
	struct cmd *ret;
	if (peek(ps, es, "("))
		return parseblock(ps, es);
	ret = execcmd();
	cmd = (struct execcmd*) ret;

	argc = 0;
	ret = parseredirs(ret, ps, es);
	while (!peek(ps, es, "|)&;")) {
		if ((tok = gettoken(ps, es, &q, &eq)) == 0)
			break;
		if (tok != 'a')
			panic("syntax");
		cmd->argv[argc] = q;
		cmd->eargv[argc] = eq;
		argc++;
		if (argc >= MAXARGS)
			panic("too many args");
		ret = parseredirs(ret, ps, es);
	}
	cmd->argv[argc] = 0;
	cmd->eargv[argc] = 0;
	return ret;
}

struct cmd* nulterminate(struct cmd *cmd) {
	struct backcmd *bcmd;
	struct execcmd *ecmd;
	struct listcmd *lcmd;
	struct pipecmd *pcmd;
	struct redircmd *rcmd;
	if (cmd == 0)
		return 0;
	switch (cmd->type) {
	case EXEC:
		ecmd = (struct execcmd*) cmd;
		for (int i = 0; ecmd->argv[i]; i++)
			*ecmd->eargv[i] = 0;
		break;
	case REDIR:
		rcmd = (struct redircmd*) cmd;
		nulterminate(rcmd->cmd);
		*rcmd->efile = 0;
		break;
	case PIPE:
		pcmd = (struct pipecmd*) cmd;
		nulterminate(pcmd->left);
		nulterminate(pcmd->right);
		break;
	case LIST:
		lcmd = (struct listcmd*) cmd;
		nulterminate(lcmd->left);
		nulterminate(lcmd->right);
		break;
	case BACK:
		bcmd = (struct backcmd*) cmd;
		nulterminate(bcmd->cmd);
		break;
	}
	return cmd;
}
