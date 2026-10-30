# Does not set CC, CXX, CFLAGS, LDFLAGS or PKG_CONFIG_PATH: the compiler
# activation and the host environment provide them.

# 1. Generate the sources with the fork's autogen.sh. Its scripts find perl and
#    python3 through env, so they resolve to the build env's; autogen.sh also
#    removes autom4te.cache.
cd $env.SRC_DIR
^./autogen.sh

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
