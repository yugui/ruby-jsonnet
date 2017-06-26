module Jsonnet
  class Evaluator
    def self.from_snippet(snippet, options = {})
      snippet_check = ->(key, value) { key.to_s.match(/^filename|multi$/) }
      snippet_options = options.select &snippet_check
      vm_options = options.reject &snippet_check
      self.new(vm_options).snippet(snippet, snippet_options)
    end

    def self.from_file(filename, options = {})
      file_check = ->(key, value) { key.to_s.match(/^encoding|multi$/) }
      file_options = options.select &file_check
      vm_options = options.reject &file_check
      self.new(vm_options).file(filename, file_options)
    end

    def initialize(options = {})
      @vm = Jsonnet::VM.new
      apply_options(options)
    end

    def snippet(snippet, options = {})
      vm.evaluate(snippet, options)
    end

    def file(filename, options = {})
      vm.evaluate_file(filename, options)
    end

  private
    attr_reader :vm

    def apply_options(options)
      options.each do |key, value|
        method = "#{key}="
        if vm.respond_to?(method)
          vm.public_send(method, value)
        else
          raise UnsupportedOptionError.new("Jsonnet VM does not support #{key} option")
        end
      end
    end

    class UnsupportedOptionError < RuntimeError; end
  end
end
