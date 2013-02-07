CC=gcc
CFLAGS=$(RPM_OPT_FLAGS)

nvidia-detect:	nvidia-detect.h nvidia-detect.c
	$(CC) $(CFLAGS) nvidia-detect.c -o nvidia-detect -lpci -lresolv

.PHONEY:	clean

clean:
	-rm -f nvidia-detect
