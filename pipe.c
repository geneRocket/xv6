#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

#define PIPESIZE 512

struct pipe{
	struct spinlock lock;
	char data[PIPESIZE];
	uint nread;// number of bytes read
	uint nwrite;// number of bytes written
	int readopen;// read fd is still open
	int writeopen;// write fd is still open
};



int pipewrite(struct pipe *p,char *addr, int n)
{
	acquire(&p->lock);
	for(int i=0;i<n;i++)
	{
		while(p->nwrite==p->nread+PIPESIZE)//pipewrite-full
		{
			if(p->readopen==0||proc->killed)//if no check readopen,may sleep forever
			{
				release(&p->lock);
				return -1;
			}
			wakeup(&p->nread);
			sleep(&p->nwrite,&p->lock);
		}
		p->data[p->nwrite++%PIPESIZE]=addr[i];

	}
	wakeup(&p->nread);//pipewrite-wakeup1
	release(&p->lock);
	return n;
}

int piperead(struct pipe *p,char *addr,int n)
{
	int i;
	acquire(&p->lock);
	while(p->nread==p->nwrite && p->writeopen)//pipe-empty
	{
		if(proc->killed)
		{
			realease(&p->lock);
			return -1;
		}
		sleep(&p->nread,&p->lock);//piperead-sleep
	}
	for(int i=0;i<n;i++){//piperead-copy
		if(p->nread==p->nwrite)
			break;
		addr[i]=p->data[p->nread++%PIPESIZE];
	}
	wakeup(&p->nwrite);//piperead-wakeup
	release(&p->lock);
	return i;

}
