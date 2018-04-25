require 'mkmf'
require 'fileutils'

def using_system_libraries?
  arg_config('--use-system-libraries', !!ENV['JSONNET_USE_SYSTEM_LIBRARIES'])
end

dir_config('jsonnet')

unless using_system_libraries?
  message "Building jsonnet using packaged libraries.\n"
  require 'rubygems'
  require 'mini_portile2'
  message "Using mini_portile version #{MiniPortile::VERSION}\n"

  recipe = MiniPortile.new('jsonnet', 'v0.10.0')
  recipe.files = ['https://github.com/google/jsonnet/archive/v0.10.0.tar.gz']
  class << recipe

    def compile
      # We want to create a file a library we can link to. Jsonnet provides us
      # with the command `make libjsonnet.so` which creates a shared object
      # however that won't be bundled into the compiled output so instead
      # we compile the c into .o files and then create an archive that can
      # be linked to
      execute('compile', make_cmd)
      execute('archive', 'ar rcs libjsonnet.a core/desugarer.o core/formatter.o core/lexer.o core/libjsonnet.o core/parser.o core/pass.o core/static_analysis.o core/string_utils.o core/vm.o third_party/md5/md5.o')
    end

    def configured?
      true
    end

    def install
      lib_path = File.join(port_path, 'lib')
      include_path = File.join(port_path, 'include')

      FileUtils.mkdir_p([lib_path, include_path])

      FileUtils.cp(File.join(work_path, 'libjsonnet.a'), lib_path)
      FileUtils.cp(File.join(work_path, 'include', 'libjsonnet.h'), include_path)
    end
  end

  recipe.cook
  # I tried using recipe.activate here but that caused this file to build ok
  # but the makefile to fail. These commands add the necessary paths to do both
  $LIBPATH = ["#{recipe.path}/lib"] | $LIBPATH
  $CPPFLAGS << " -I#{recipe.path}/include"

  # This resolves an issue where you can get improper linkage when compiling
  # and get an error like "undefined symbol: _ZTVN10__cxxabiv121__vmi_class_type_infoE"
  # experienced on ubuntu.
  # See: https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=193950
  $LIBS << " -lstdc++"
end

abort 'libjsonnet.h not found' unless have_header('libjsonnet.h')
abort 'libjsonnet not found' unless have_library('jsonnet')
create_makefile('jsonnet/jsonnet_wrap')
