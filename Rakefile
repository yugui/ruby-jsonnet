require 'bundler/gem_tasks'
require "rake/extensiontask"
require 'rake/testtask'

task :default => :compile

Rake::ExtensionTask.new do |t|
  t.name = 'jsonnet_wrap'
  t.ext_dir = 'ext/jsonnet'
  t.lib_dir = 'lib/jsonnet'
end

Rake::TestTask.new('test' => 'compile') do |t|
  t.libs << 'test'
  t.verbose = true
end
