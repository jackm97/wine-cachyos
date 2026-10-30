# Does not set CC, CXX, CFLAGS, LDFLAGS or PKG_CONFIG_PATH: the compiler
# activation and the host environment provide them.

# 1. Generate the sources (the steps of autogen.sh, run through the build env's
#    perl/python because the scripts' shebangs are /usr/bin/perl).
cd $env.SRC_DIR
^perl tools/make_requests
^perl tools/make_specfiles
^python dlls/winevulkan/make_vulkan -x vk.xml -X video.xml
^autoreconf -ifv
rm -rf autom4te.cache

# 2. Out-of-tree build directory; kept between runs so later builds are incremental.
let build = ($env.SRC_DIR | path dirname | path join build)
mkdir $build
cd $build

# 3. Configure.
run-external ($env.SRC_DIR | path join configure) ...[
    '--enable-archs=i386,x86_64'
    '--disable-tests'
    '--with-x'
    '--with-wayland'
    '--with-vulkan'
    '--with-opencl'
    '--with-gstreamer'
    '--with-ffmpeg'
    '--with-pulse'
    '--with-pipewire'
    '--with-freetype'
    '--with-fontconfig'
    '--with-gnutls'
    '--without-oss'
    $'--prefix=($env.PREFIX)'
]

# 4. Build and install.
^make $'-j($env.CPU_COUNT)'
^make install
