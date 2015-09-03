require 'mkmf'

dir_config('jsonnet')
abort 'libjsonnet.h not found' unless have_header('libjsonnet.h')
abort 'libjsonnet not found' unless have_library('jsonnet')
create_makefile('jsonnet_wrap')
