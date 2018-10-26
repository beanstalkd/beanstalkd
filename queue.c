#include<stdio.h>
#include<stdlib.h>
struct plane{
		int id;
		struct plane* next;
}*temp,*front=NULL,*rear;

void ins()
{
	temp=(struct plane*)malloc(sizeof(struct plane));
	printf("Enter ID\n");
	scanf("%d",&temp->id);
	temp->next=NULL;
	if(front==NULL)
		front=rear=temp;
	else
	{
		rear->next=temp;
		rear=temp;
		temp->next=front;
	}
}

void del()
{
	if(front==rear)
		front=rear=NULL;
	if(front==NULL)
		printf("UNDERFLOW\n");
	else
	{
		rear->next=front->next;
		front=front->next;
	}
}
void disp()
{
	temp=front;
	do
	{
		printf("ID: %d\n",temp->id);
		printf("\n");
		temp=temp->next;
	}while(temp!=front);
}
int main()
{
	int choice=0;
	while(choice!=4)
	{
		printf("1.Insert\n");
		printf("2.Delete\n");
		printf("3.Display\n");
		printf("4.Exit\n");
		scanf("%d",&choice);
		switch(choice)
		{
			case 1:ins();break;
			case 2:del();break;
			case 3:disp();break;
		}
	}
	return 0;
}