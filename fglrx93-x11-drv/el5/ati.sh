ARCH="$(uname -m)"

case "$ARCH" in
        x86_64) export LIBGL_DRIVERS_PATH="/usr/lib64/dri:/usr/lib/dri";;
                #export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/lib64/fglrx:/usr/lib/fglrx";;
    i[3-6\d]86) export LIBGL_DRIVERS_PATH="/usr/lib/dri";;
                #export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/lib/fglrx";;
esac
