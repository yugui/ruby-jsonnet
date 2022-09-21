require 'mkmf'
require 'fileutils'

def using_system_libraries?
  arg_config('--use-system-libraries', !!ENV['JSONNET_USE_SYSTEM_LIBRARIES'])
end

dir_config('jsonnet')

RbConfig::MAKEFILE_CONFIG['LDSHARED'] = '$(CXX) -shared'
unless using_system_libraries?
  message "Building jsonnet using packaged libraries.\n"
  require 'rubygems'
  require 'mini_portile2'
  message "Using mini_portile version #{MiniPortile::VERSION}\n"

  recipe = MiniPortile.new('jsonnet', 'v0.18.0')
  recipe.files = ['https://github.com/google/jsonnet/archive/v0.18.0.tar.gz']
  class << recipe
    CORE_OBJS = %w[
      desugarer.o formatter.o lexer.o libjsonnet.o parser.o pass.o static_analysis.o string_utils.o vm.o
    ].map {|name| File.join('core', name) }
    MD5_OBJS = %w[
      md5.o
    ].map {|name| File.join('third_party', 'md5', name) }
    C4_CORE_OBJS = %w[
      base64.o
      char_traits.o
      error.o
      format.o
      language.o
      memory_resource.o
      memory_util.o
      time.o
    ].map {|name| File.join('third_party', 'rapidyaml', 'rapidyaml', 'ext', 'c4core', 'src', 'c4', name) }
    RAPID_YAML_OBJS = %w[
      common.o parse.o preprocess.o tree.o
    ].map {|name| File.join('third_party', 'rapidyaml', 'rapidyaml', 'src', 'c4', 'yml', name) }

    def compile
      # We want to create a file a library we can link to. Jsonnet provides us
      # with the command `make libjsonnet.so` which creates a shared object
      # however that won't be bundled into the compiled output so instead
      # we compile the c into .o files and then create an archive that can
      # be linked to
      execute('compile', make_cmd)
      execute('archive', 'ar rcs libjsonnet.a ' + target_object_files.join(' '))
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
      FileUtils.cp(File.join(work_path, 'include', 'libjsonnet_fmt.h'), include_path)
    end

    private
    def target_object_files
      if version >= 'v0.18.0'
        CORE_OBJS + MD5_OBJS + C4_CORE_OBJS + RAPID_YAML_OBJS
      else
        CORE_OBJS + MD5_OBJS
      end
    end
  end

  recipe.cook
  # I tried using recipe.activate here but that caused this file to build ok
  # but the makefile to fail. These commands add the necessary paths to do both
  $LIBPATH = ["#{recipe.path}/lib"] | $LIBPATH
  $CPPFLAGS << " -I#{recipe.path}/include"
end

abort 'libjsonnet.h not found' unless have_header('libjsonnet.h')
abort 'libjsonnet not found' unless have_library('jsonnet')
have_header('libjsonnet_fmt.h')
create_makefile('jsonnet/jsonnet_wrap')
