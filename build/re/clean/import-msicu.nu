# import-msicu.nu: nu port of .github/import-msicu.sh (bottles/soda).
# Usage: nu import-msicu.nu <source_dir> <commits.json>
# commits.json is the list of {name, sha} parsed from the readonly *_commit lines of the shell script.
# Differences from the shell script: no remote add/fetch/promisor config and no blob downloads,
# because every commit and blob is verified to exist locally first (git cat-file -e).
# Same cherry-pick --no-commit semantics, asserted conflict lists, conflict resolutions and final checks.

def main [source_dir: string, commits_json: path, blobs_json: path] {
  $env.SRC = $source_dir
  let c = (open $commits_json | reduce -f {} {|it, acc| $acc | insert $it.name $it.sha })
  let blobs = (open $blobs_json)

  # everything must be local: nothing is fetched
  for e in ($c | transpose name sha) { ^git -C $source_dir cat-file -e $"($e.sha)^{commit}" }
  for b in $blobs { ^git -C $source_dir cat-file -e $b.sha }

  cp-nc $c.stdio_scan_commit $c.wide_scan_commit
  cp-nc $c.math_declarations_commit $c.float_types_commit $c.quick_exit_commit $c.div_pe_commit $c.div_declaration_commit $c.wide_functions_commit $c.long_double_math_commit $c.cxx_math_commit $c.inttypes_commit $c.number_format_commit $c.timezone_variables_commit $c.exception_macros_commit
  cp-nc $c.vcruntime_exception_commit $c.vcruntime_typeinfo_header_commit $c.vcruntime_new_header_commit
  cp-nc $c.nullptr_commit
  cp-nc $c.exception_classes_commit
  cp-nc $c.cxx_headers_commit
  cp-nc $c.vcruntime_new_commit

  # asm pointer: expected conflict in signal_x86_64.c, keep ours
  expect-conflict $c.asm_global_pointer_commit ["dlls/ntdll/unix/signal_x86_64.c"]
  ^git -C $env.SRC checkout --ours -- dlls/ntdll/unix/signal_x86_64.c
  ^git -C $env.SRC add include/wine/asm.h dlls/ntdll/unix/signal_x86_64.c
  ^git -C $env.SRC cherry-pick --quit

  cp-nc $c.gcc_ctors_commit $c.ucrt_atexit_commit $c.compiler_rt_builtins_commit

  # compiler-rt configure conflict: configure deleted in HEAD
  expect-conflict $c.compiler_rt_mingw_configure_commit ["configure"]
  ^git -C $env.SRC rm -- configure
  ^git -C $env.SRC cherry-pick --quit

  cp-nc $c.compiler_rt_mingw_winegcc_commit $c.winecrt_tls_commit $c.compiler_rt_emutls_commit $c.msvcrt_exception_swap_commit $c.msvcp_exception_swap_commit $c.compiler_rt_i386_commit $c.compiler_rt_arm64ec_commit $c.compiler_rt_aarch64_commit $c.vcruntime_typeinfo_root_commit $c.vcruntime_init_thread_commit $c.vcruntime_typeinfo_commit

  expect-conflict $c.assembly_sources_commit ["tools/make_makefiles"]
  ^git -C $env.SRC checkout --ours -- tools/make_makefiles
  ^git -C $env.SRC add tools/make_makefiles tools/makedep.c
  ^git -C $env.SRC cherry-pick --quit

  expect-conflict $c.assembly_subdir_commit ["tools/makedep.c"]
  ^git -C $env.SRC checkout --ours -- tools/makedep.c
  let patch = 'diff --git a/tools/makedep.c b/tools/makedep.c
--- a/tools/makedep.c
+++ b/tools/makedep.c
@@ -618,0 +619 @@ static int is_subdir_other_arch( const char *name, unsigned int arch )
+    int cpu;
@@ -623,4 +624,3 @@ static int is_subdir_other_arch( const char *name, unsigned int arch )
-    if (!strcmp( dir, "arm64" )) dir = "aarch64";
-    if (!strcmp( dir, "amd64" )) dir = "x86_64";
-    if (native_archs[arch] && !strcmp( dir, archs.str[native_archs[arch]] )) return 0;
-    return strcmp( dir, archs.str[arch] );
+    if ((cpu = get_cpu_from_name( dir )) == -1) return 0;
+    if (native_archs[arch] && cpu == get_cpu_from_name( archs.str[native_archs[arch]] )) return 0;
+    return cpu != get_cpu_from_name( archs.str[arch] );
'
  $patch | ^git -C $source_dir apply --unidiff-zero -
  ^git -C $env.SRC add tools/makedep.c
  ^git -C $env.SRC cherry-pick --quit

  cp-nc $c.apisetcconv_commit $c.rtlsupport_cxx_commit

  # libc++: configure + configure.ac conflicts, drop the markers keeping both sides
  expect-conflict $c.libcxx_commit ["configure" "configure.ac"]
  ^git -C $env.SRC rm -- configure
  let ac = ($source_dir | path join configure.ac)
  open --raw $ac | lines
    | where {|l| not ($l == '<<<<<<< HEAD' or $l == '=======' or ($l =~ '^>>>>>>> .* \(libs: Import libc\+\+ from upstream LLVM version 8.0.1.\)$')) }
    | str join "\n" | $in + "\n" | save --force --raw $ac
  if (open --raw $ac | lines | any {|l| $l =~ '^(<<<<<<<|=======|>>>>>>>)' }) { error make {msg: "conflict markers left in configure.ac"} }
  assert-has $ac 'WINE_EXTLIB_FLAGS(COMPILER_RT, compiler-rt, "$wine_compiler_rt_libs")'
  assert-has $ac 'WINE_EXTLIB_FLAGS(CXX, c++, "c++ msvcp140 vcruntime140")'
  ^git -C $env.SRC add configure.ac
  ^git -C $env.SRC cherry-pick --quit

  expect-conflict $c.libunwind_commit ["configure"]
  ^git -C $env.SRC rm -- configure
  ^git -C $env.SRC cherry-pick --quit

  # libc++abi: configure + configure.ac conflicts, configure.ac takes ours plus seds
  expect-conflict $c.libcxxabi_commit ["configure" "configure.ac"]
  ^git -C $env.SRC rm -- configure
  ^git -C $env.SRC checkout --ours -- configure.ac
  mut t = (open --raw $ac)
  # sed 1: append after every line containing wine_compiler_rt_libs="compiler-rt"
  let tail1 = ['    case "$target" in' '      *mingw*|*-windows-gnu) enable_cppabi="$enable_cppabi,$wine_arch" ;;' '    esac']
  $t = ($t | lines | each {|l| if ($l | str contains '    wine_compiler_rt_libs="compiler-rt"') { [$l] ++ $tail1 } else { [$l] } } | flatten | str join "\n") + "\n"
  # sed 2: split the CXX WINE_EXTLIB_FLAGS line in two
  let old2 = 'WINE_EXTLIB_FLAGS(CXX, c++, "c++ msvcp140 vcruntime140")'
  let new2 = 'WINE_EXTLIB_FLAGS(CXX, c++, "c++ \$(CXXABI_PE_LIBS) msvcp140 vcruntime140")' + "\n" + 'WINE_EXTLIB_FLAGS(CXXABI, c++abi, "c++abi \$(UNWIND_PE_LIBS) c++", "-I\$(top_srcdir)/libs/c++abi/include")'
  $t = ($t | str replace --all $old2 $new2)
  # sed 3: insert before every line starting with enable_vcruntime140_1=
  let head3 = ['enable_cppabi=${enable_cppabi:-no}' 'enable_unwind=${enable_unwind:-$enable_cppabi}' 'if test "$enable_cppabi" = no; then' '  CXX_PE_LIBS="c++ msvcp140 vcruntime140"' 'fi']
  $t = ($t | lines | each {|l| if ($l | str starts-with 'enable_vcruntime140_1=') { $head3 ++ [$l] } else { [$l] } } | flatten | str join "\n") + "\n"
  # sed 4: append after every line containing WINE_CONFIG_MAKEFILE(libs/c++)
  $t = ($t | lines | each {|l| if ($l | str contains 'WINE_CONFIG_MAKEFILE(libs/c++)') { [$l 'WINE_CONFIG_MAKEFILE(libs/c++abi)'] } else { [$l] } } | flatten | str join "\n") + "\n"
  $t | save --force --raw $ac
  ^git -C $env.SRC add configure.ac
  ^git -C $env.SRC cherry-pick --quit

  for commit in [$c.icucommon_commit $c.icui18n_commit] {
    expect-conflict $commit ["configure"]
    ^git -C $env.SRC rm -- configure
    ^git -C $env.SRC cherry-pick --quit
  }

  cp-nc $c.icudata_commit

  # ICU integration
  expect-conflict $c.icu_commit ["configure" "configure.ac" "dlls/icu/Makefile.in" "dlls/icu/icu.spec"]
  ^git -C $env.SRC rm -- configure
  ^git -C $env.SRC checkout --theirs -- dlls/icu/Makefile.in dlls/icu/icu.spec
  ^git -C $env.SRC checkout --ours -- configure.ac
  let anchor = '    enable_compiler_rt=${enable_compiler_rt:-no}'
  open --raw $ac | lines
    | each {|l| if ($l | str contains $anchor) { [$l '    enable_icu=${enable_icu:-no}'] } else { [$l] } } | flatten
    | str join "\n" | $in + "\n" | save --force --raw $ac
  ^git -C $env.SRC add configure.ac dlls/icu/Makefile.in dlls/icu/icu.spec
  ^git -C $env.SRC cherry-pick --quit

  cp-nc $c.icucommon_cleanup_commit $c.icui18n_cleanup_commit

  for mf in [libs/c++/Makefile.in libs/c++abi/Makefile.in libs/unwind/Makefile.in libs/icucommon/Makefile.in libs/icui18n/Makefile.in] {
    let p = ($source_dir | path join $mf)
    open --raw $p | lines | each {|l| if ($l | str starts-with 'EXTRADEFS = ') { $l + ' -D_UCRT' } else { $l } }
      | str join "\n" | $in + "\n" | save --force --raw $p
    if not (open --raw $p | lines | any {|l| $l =~ '^EXTRADEFS = .* -D_UCRT$' }) { error make {msg: $"EXTRADEFS -D_UCRT missing in ($mf)"} }
  }

  let th = ($source_dir | path join include/msvcrt/time.h)
  open --raw $th | lines
    | each {|l| if $l == '#if _MSVCR_VER < 120 && defined(_USE_32BIT_TIME_T)' { '#if _MSVCR_VER < 120 && defined(_USE_32BIT_TIME_T) && !defined(_UCRT)' } else { $l } }
    | str join "\n" | $in + "\n" | save --force --raw $th
  assert-has $th '#if _MSVCR_VER < 120 && defined(_USE_32BIT_TIME_T) && !defined(_UCRT)'

  # final assertions (same as the shell script)
  assert-has ($source_dir | path join dlls/icu/Makefile.in) 'IMPORTS   = $(ICUI18N_PE_LIBS) $(ICUCOMMON_PE_LIBS) $(CXX_PE_LIBS) advapi32'
  assert-has $ac 'enable_icu=${enable_icu:-no}'
  assert-has $ac 'WINE_CONFIG_MAKEFILE(dlls/icu)'
  assert-absent $source_dir libs/icucommon/icuplug.cpp
  assert-absent $source_dir libs/icui18n/vtzone.cpp
  for f in [nls/icudtl.dat include/msvcrt/cstddef include/msvcrt/string libs/c++/Makefile.in libs/c++abi/Makefile.in libs/unwind/Makefile.in] { assert-nonempty ($source_dir | path join $f) }
  assert-has ($source_dir | path join libs/unwind/Makefile.in) 'src/UnwindRegistersSave.S'
  assert-has ($source_dir | path join include/rtlsupportapi.h) 'extern "C" {'
  assert-has ($source_dir | path join tools/makedep.c) 'strendswith( source->name, ".S" )'
  assert-has ($source_dir | path join tools/makedep.c) 'if ((cpu = get_cpu_from_name( dir )) == -1) return 0;'
  for f in [libs/compiler-rt/lib/builtins/emutls.c dlls/winecrt0/tls.c dlls/ucrtbase/atexit.c] { assert-nonempty ($source_dir | path join $f) }
  assert-has ($source_dir | path join dlls/msvcp140/msvcp140.spec) '@ cdecl -arch=win64 ?__ExceptionPtrSwap@@YAXPEAX0@Z(ptr ptr) __ExceptionPtrSwap'
  print "import-msicu.nu: all assertions passed"
}

def g [...a: string] { ^git -C $env.SRC ...$a }

# git cherry-pick --no-commit <shas...>; must succeed
def cp-nc [...shas: string] { ^git -C $env.SRC cherry-pick --no-commit ...$shas }

# cherry-pick --no-commit that must conflict in exactly the given (sorted) file list
def expect-conflict [sha: string, files: list<string>] {
  let r = (do { ^git -C $env.SRC cherry-pick --no-commit $sha } | complete)
  if $r.exit_code == 0 { error make {msg: $"Expected conflicts for ($sha) but cherry-pick succeeded"} }
  let got = (^git -C $env.SRC diff --name-only --diff-filter=U | lines | sort)
  if $got != ($files | sort) { error make {msg: $"Unexpected conflict list for ($sha): ($got | str join ', ') vs expected ($files | str join ', ')"} }
}

def assert-has [file: string, text: string] {
  if not ((open --raw $file) | str contains $text) { error make {msg: $"missing in ($file): ($text)"} }
}
def assert-absent [src: string, rel: string] {
  if ($src | path join $rel | path exists) { error make {msg: $"should not exist: ($rel)"} }
}
def assert-nonempty [file: string] {
  if not ($file | path exists) or (ls $file | get 0.size) == 0b { error make {msg: $"missing or empty: ($file)"} }
}
