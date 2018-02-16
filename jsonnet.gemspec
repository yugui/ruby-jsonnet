# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'jsonnet/version'

Gem::Specification.new do |spec|
  spec.name          = "jsonnet"
  spec.version       = Jsonnet::VERSION
  spec.authors       = ["Yuki Yugui Sonoda"]
  spec.email         = ["yugui@yugui.jp"]
  spec.summary       = %q{Jsonnet library}
  spec.description   = %q{Wraps the official C++ implementation of Jsonnet}
  spec.homepage      = ""
  spec.license       = "MIT"

  spec.files         = `git ls-files -z`.split("\x0")
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.extensions    = ['ext/jsonnet/extconf.rb']
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["lib"]

  spec.add_runtime_dependency "mini_portile2", ">= 2.2.0"

  spec.add_development_dependency "bundler", "~> 1.7"
  spec.add_development_dependency "rake", "~> 10.0"
  spec.add_development_dependency "test-unit", "~> 3.1.3"
  spec.add_development_dependency "rake-compiler", "~> 0.9.5"
end
