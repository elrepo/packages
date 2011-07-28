set arch = `uname -m`

if ($arch == x86_64) then
    setenv LIBGL_DRIVERS_PATH /usr/lib64/dri:/usr/lib/dri
    #setenv LD_LIBRARY_PATH /usr/lib64/fglrx:/usr/lib/fglrx:$LD_LIBRARY_PATH
else if ($arch =~ i[3-6]86) then
    setenv LIBGL_DRIVERS_PATH /usr/lib/dri
    #setenv LD_LIBRARY_PATH /usr/lib/fglrx:$LD_LIBRARY_PATH
endif
