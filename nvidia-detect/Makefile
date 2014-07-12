CC     := gcc
CFLAGS += $(RPM_OPT_FLAGS)
LDLIBS += -lresolv -lpci

nvidia-detect:	nvidia-detect.h nvidia-detect.c
	$(CC) $(CFLAGS) $(LDLIBS) nvidia-detect.c -o nvidia-detect

.PHONEY:	clean

clean:
	-rm -f nvidia-detect
