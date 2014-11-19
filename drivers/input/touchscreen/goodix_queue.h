/*
 * include/linux/goodix_queue.h
 *
 * Copyright (C) 2010 Goodix, Inc. All rights reserved.  
 * 
 * Author: Eltonny
 * Date: 2011.03.08
 */
/* 使用头文件方式仅为分离操作主流程与操作函数，在goodix_touch.c文件中使用
 * 用于管理手指序列的伪队列操作函数，
 * 调整手指上报顺序以避免出现手指ID交换现象(主要适用于双指)
 * 调整缓冲区管理方式以加快删除手指的速度
 *
 * 注意事项：使用该文件可能影响上报坐标的速率
 */ 
#ifndef _LINUX_GOODIX_QUEUE_H
#define	_LINUX_GOODIX_QUEUE_H

struct point_data   //包含四项基本信息
{
	bool used;
	bool state;
	unsigned int x;
	unsigned int y;
	unsigned char pressure;
	unsigned char id; /*在触摸屏原始数据中的位置 */
	struct point_data * next;
};

struct point_queue   //管理当前按键伪队列
{
	struct point_data * head;   //链表头
	struct point_data * tail;   //链表尾，用于加入元素
	unsigned char length;
};

static struct point_data buffer[MAX_FINGER_NUM];   //缓冲区数组，请勿直接使用

/*******************************************************	
功能：
	在队列尾中加入新增的手指
参数：
	point_list
	num：手指标号
return：
	是否成功增加手指
********************************************************/
static bool add_point(struct point_queue * list, unsigned char id)
{
	int i;
	struct point_data * node;

	if(id >= MAX_FINGER_NUM)
		return false;
	for(i = 0; i<MAX_FINGER_NUM; i++)
	{
		if(!buffer[i].used)
			break;
	}
	if(i >= MAX_FINGER_NUM)    //没有找到合适缓冲区
		return false;
	
	node = &buffer[i];
	node->id = id;
	node->state = node->used = true;//按下状态
	node->next = NULL;
	if(list->head == NULL)
		list->head = node;
	else
		list->tail->next = node;
	list->tail = node;
	list->length++;
	return true;
}

/*******************************************************	
功能：
	查找松键的手指并设置标志位为FLAG_UP
参数：
	list
	num：手指标号
return：
	是否成功设置手指标志位
********************************************************/
static bool set_up_point(struct point_queue * list, unsigned char id)
{
	struct point_data * p;

	if(id >= MAX_FINGER_NUM)
		return false;

	for(p = list->head; p!= NULL; p = p->next)
	{
		if(p->id == id)
		{
			p->state = false;
			return true;
		}
	}
	return false;
}

/*******************************************************	
功能：
	分析上次和本次手指按键情况调整队列
参数：
	list
	finger_bit:上次按键的Index
	finger:标识本次手指变动情况的Index
********************************************************/
static void analyse_points(struct point_queue * list, unsigned char finger_bit, unsigned char finger)
{
	#define POINT_DOWN_MASK	0x01
	unsigned char count;
	for(count = 0; (finger !=0) && (count < MAX_FINGER_NUM);  count++)
	{
		if((finger&POINT_DOWN_MASK) != 0)			//current bit is 1, so NO.postion finger is changed
		{							
			if((finger_bit&POINT_DOWN_MASK) != 0)	//NO.postion finger is UP
				set_up_point(list, count);
			else 
				add_point(list, count);
		}
		finger>>=1;
		finger_bit>>=1;
	}
}

/*******************************************************	
功能：
	删除手指队列中松键的手指
参数：
	list
********************************************************/
static void delete_points(struct point_queue * list)
{
	struct point_data *p, *temp;

	for(p = list->head; p != NULL && p->next != NULL;)
	{	//准备删除p->next
		if(!p->next->state)
		{	
			temp = p->next->next;
			if(temp == NULL)
				list->tail = p;	//尾结点将被删除	
			p->next->used = false;  /*将使用标志复位 */
			p->next = temp;
			list->length--;
		}
		else //没有删除动作时，当前结点后移
			p = p->next;
	}
	if(list->head!= NULL && !list->head->state)
	{
		list->head->used = false;
		list->head = list->head->next;
		if(list->head == NULL)	//唯一1个节点被删除
			list->tail = NULL;
		list->length--;
	}
}

#endif /* _LINUX_GOODIX_QUEUE_H */
