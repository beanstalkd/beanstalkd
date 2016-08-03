#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dat.h"

Max_job   glob_max_job={.name="",
	.max_ct=0,
	.next=NULL,};
enum Max_job_action glob_job_action=ACTION_DROP;
uint glob_max_bury_job=0;

void set_max_job_to_tube()
{
	Max_job *p_job=NULL;
	p_job=&glob_max_job;
	tube t=NULL;
	while (NULL!=p_job){
		if(strlen(p_job->name)>0){
			t=tube_find_or_make(p_job->name);
      if(NULL!=t) t->stat.max_ct = p_job->max_ct;
			TUBE_ASSIGN(p_job->pre_tube, t);

		}
		p_job=p_job->next;
	}
	global_stat.max_ct=glob_max_job.max_ct;
}

uint get_max_job_by_tube(char *name)
{
	Max_job *p_job=NULL;
	p_job=&glob_max_job;
	uint max_ct=0;
	while ((NULL!=name) && (NULL!=p_job)){
		if(strcmp(p_job->name,name)==0){
			max_ct=p_job->max_ct;
			break;
		}
		p_job=p_job->next;
	}
	return max_ct;
}


static Max_job *max_job_add_data(Max_job* cur_job,char *pname,int name_len,uint ct)
{
	int len=MAX_TUBE_NAME_LEN;
	if (len>name_len) len=name_len;
	if(pname&&cur_job){
		cur_job->next=(Max_job*)calloc(1,sizeof(Max_job));
			if(NULL!=cur_job->next){
				strncpy(cur_job->next->name,pname,len);
				cur_job->next->max_ct=ct;
			}else{
				twarnx("max_job_add_data()");
				exit(112);
			}
	}
	return cur_job->next;
}

int max_job_init(char *max_arg)
{
	char *s_pmd=NULL;
	char *e_pmd=NULL;
	char *s_arg=NULL;
	char *e_arg=NULL;
	Max_job *p_job=NULL;
	p_job=&glob_max_job;
	if(max_arg){
		s_arg=max_arg;
		while(s_arg){
			s_pmd=s_arg;
			if((e_pmd=strchr(s_pmd,':'))!=NULL){
				if((e_arg=strchr(e_pmd,','))){
					if ((e_arg-e_pmd)>1){
						*e_arg='\0';
						e_arg++;
						if(e_pmd==s_pmd){
							glob_max_job.max_ct=(uint)atoi(e_pmd+1);
						}else{
							max_job_add_data(p_job,s_pmd,e_pmd-s_pmd,(uint)atoi(e_pmd+1));
							p_job=p_job->next;
						}
					}
					s_arg=e_arg;
				}else{
					if ((e_pmd+1)){
						if(e_pmd==s_pmd){
							glob_max_job.max_ct=(uint)atoi(e_pmd+1);
						}else{
							max_job_add_data(p_job,s_pmd,e_pmd-s_pmd,(uint)atoi(e_pmd+1));
						}
					}
					s_arg=NULL;
				}
			}
		}
	}
	return 0;
}

uint get_tube_default_max_job()
{
	return glob_max_job.max_ct;
}
void max_job_uninit()
{

        Max_job *p1=NULL;
        Max_job *p2=NULL;
	p1=glob_max_job.next;
	while(p1){
		p2=p1->next;
		free(p1);
		p1=p2;
	}
}
