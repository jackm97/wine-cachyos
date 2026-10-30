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

# 3. Configure. The PE side is cross-compiled by clang, which does not see CFLAGS,
#    so x86-64-v3 is passed through configure.ac's ${wine_arch}_CFLAGS. i386 keeps
#    its default, because x86-64-v3 is invalid for i386.
run-external ($env.SRC_DIR | path join configure) ...[
    'x86_64_CFLAGS=-g -O2 -march=x86-64-v3'
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
