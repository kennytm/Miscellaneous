# The Homebrew formula for install gcc 4.6.
#
# Why this formula is here? Because the one from homebrew-alt doesn't compile.
#  The autotools is broken. Macros are used without appropriate headers. Types
#  are declared without respecting the result of configure. The GNU code are
#  generally not portable when building with another compiler, etc., and the one
#  from homebrew-alt doesn't address any of these problems. 
#
# Please check the class below for the patched. The build time on a Core 2 Duo
#  is 43.5 minutes, not including download time.

require 'formula'

class Gcc46 < Formula
    homepage 'http://gcc.gnu.org/gcc-4.6/'
    url 'http://ftp.tsukuba.wide.ad.jp/software/gcc/releases/gcc-4.6.1/gcc-4.6.1.tar.bz2'
    md5 'c57a9170c677bf795bdc04ed796ca491'

    #url 'http://ftp.tsukuba.wide.ad.jp/software/gcc/releases/gcc-4.6.1/gcc-4.6.1.tar.gz'
    #md5 '981feda5657e030fc48676045d55bb9d'
    ##decompressing the 'gz' takes 1.5 minutes, decompressing the 'bz2' takes 2 minutes
    ##md5 for gzip-ing with --fast.
    #md5 '92a5e0e46518cad2fec5e255e490a6a7'
    
    depends_on 'gmp'
    depends_on 'libmpc'
    depends_on 'mpfr'
    
    def install
        ohai 'Remember to install with the --use-gcc flag!'

        gmp = Formula.factory 'gmp'
        mpfr = Formula.factory 'mpfr'
        libmpc = Formula.factory 'libmpc'
        
        ENV.delete 'LD'
        ENV.delete 'GREP_OPTIONS'   # avoid the too many parameters to `-version-info' error because 'grep' is used.
        
        gcc_prefix = prefix + 'gcc'
        
        args = [
            "--prefix=#{gcc_prefix}",
            "--datarootdir=#{share}",
            "--bindir=#{bin}",
            "--program-suffix=-#{version.slice(/\d\.\d/)}",
            "--with-gmp=#{gmp.prefix}",
            "--with-mpfr=#{mpfr.prefix}",
            "--with-mpc=#{libmpc.prefix}",
            "--with-system-zlib",
            "--disable-bootstrap",
            "--enable-plugin",
            "--enable-shared",
            "--disable-nls",
            "--enable-languages=c++",
            "CFLAGS=#{ENV.cflags} " +
                " -DGCC_VERSION=4002" + # I can't find a way to include -DGCC_VERSION=(__GNUC__*1000+__GNUC_MINOR__) as a flag without breaking the shell.
                ' -UUSED_FOR_TARGET' +  # this and the -imacros below ensure all config macros are defined.
                " -imacros #{Dir.pwd}/build/gcc/auto-host.h" +
                ' -imacros limits.h' +  # some modules used CHAR_BIT and INT_MAX without including this. 
                ' -include sys/mman.h'  # some modules used the mmap functions without including this.
        ]

        inreplace 'gcc/system.h', 'extern const char *strsignal (int);', '#include <string.h>'  # Darwin's strsignal returns a 'char*', not 'const char*'.
        inreplace 'gcc/timevar.c', 'typedef int clock_t;', '#include <sys/types.h>'             # Darwin's clock_t is not an 'int'.

        Dir.mkdir 'build'
        Dir.chdir 'build' do
            Dir.mkdir 'gcc'
            FileUtils.touch 'gcc/auto-host.h'
            system '../configure', *args
            system 'make'
            system 'make install'
        end # chdir
    end # install
end # Gcc46

