#CC=arm-generic-linux-gnueabi-gcc
#CC=arm-none-eabi-gcc
#CC=arm-linux-gnueabi-gcc
#CC=/usr/local/CodeSourcery/Sourcery_G++_Lite/bin/arm-none-eabi-gcc
#CC=arm-linux-gcc


TARGET		= testmtv

INCLUDES	= -I. -I./..
SUBLIBS		=  -lpthread
CFLAGS		= -g -Wall -D_REENTRANT 
LDFLAGS		= 

SRCS    = test.c test_freq_tbl.c test_isdbt.c
OBJS	= $(SRCS:.c=.o)

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $<

$(TARGET):	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(SUBLIBS) -lm -static
#	cp testapp /home/RootFS/v210/linux/home/root/


all:	$(TARGET)

clean:
	rm -f $(TARGET) *.o *.a *~

distclean:
	rm -f $(TARGET) *.o *.a *.bak *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend


#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

