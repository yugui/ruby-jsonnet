require "jsonnet/jsonnet_wrap"

module Jsonnet
  class VM
    class << self
      ##
      # Convenient method to evaluate a Jsonnet snippet.
      #
      # It implicitly instantiates a VM and then evaluate Jsonnet with the VM.
      #
      # @param snippet [String]  Jsonnet source string.
      # @param options [Hash]  options to {.new} or options to {#evaluate}
      # @return [String]
      # @see #evaluate
      def evaluate(snippet, options = {})
        snippet_check = ->(key, value) { key.to_s.match(/^filename|multi$/) }
        snippet_options = options.select &snippet_check
        vm_options = options.reject &snippet_check
        new(vm_options).evaluate(snippet, snippet_options)
      end

      ##
      # Convenient method to evaluate a Jsonnet file.
      #
      # It implicitly instantiates a VM and then evaluates Jsonnet with the VM.
      #
      # @param filename [String]  Jsonnet source file.
      # @param options [Hash]  options to {.new} or options to {#evaluate_file}
      # @return [String]
      # @see #evaluate_file
      def evaluate_file(filename, options = {})
        file_check = ->(key, value) { key.to_s.match(/^encoding|multi$/) }
        file_options = options.select &file_check
        vm_options = options.reject &file_check
        new(vm_options).evaluate_file(filename, file_options)
      end
    end

    ##
    # initializes a new VM with the given configuration.
    #
    # @param [Hash] options  a mapping from option names to their values.
    #    It can have names of writable attributes in VM class as keys.
    # @return [VM] the VM.
    def initialize(options = {})
      options.each do |key, value|
        method = "#{key}="
        if respond_to?(method)
          public_send(method, value)
        else
          raise UnsupportedOptionError.new("Jsonnet VM does not support #{key} option")
        end
      end
      self
    end

    ##
    # Evaluates Jsonnet source.
    #
    # @param [String]  jsonnet  Jsonnet source string.
    #                  Must be encoded in an ASCII-compatible encoding.
    # @param [String]  filename filename of the source. Used in stacktrace.
    # @param [Boolean] multi    enables multi-mode
    # @return [String] a JSON representation of the evaluation result
    # @raise [EvaluationError] raised when the evaluation results an error.
    # @raise [UnsupportedEncodingError] raised when the encoding of jsonnet
    #        is not ASCII-compatible.
    # @note It is recommended to encode the source string in UTF-8 because
    #       Jsonnet expects it is ASCII-compatible, the result JSON string
    #       shall be UTF-{8,16,32} according to RFC 7159 thus the only
    #       intersection between the requirements is UTF-8.
    def evaluate(jsonnet, filename: "(jsonnet)", multi: false)
      eval_snippet(jsonnet, filename, multi)
    end

    ##
    # Evaluates Jsonnet file.
    #
    # @param [String]  filename filename of a Jsonnet source file.
    # @param [Boolean] multi    enables multi-mode
    # @return [String] a JSON representation of the evaluation result
    # @raise [EvaluationError] raised when the evaluation results an error.
    # @note It is recommended to encode the source file in UTF-8 because
    #       Jsonnet expects it is ASCII-compatible, the result JSON string
    #       shall be UTF-{8,16,32} according to RFC 7159 thus the only
    #       intersection between the requirements is UTF-8.
    def evaluate_file(filename, encoding: Encoding.default_external, multi: false)
      eval_file(filename, encoding, multi)
    end

    ##
    # Lets the given block handle "import" expression of Jsonnet.
    # @yieldparam  [String] base base path to resolve "rel" from.
    # @yieldparam  [String] rel  a relative or absolute path to the file to be imported
    # @yieldreturn [Array<String>] a pair of the content of the imported file and
    #                              its path.
    def handle_import
      self.import_callback = to_method(Proc.new)
      nil
    end

    ##
    # Define a function (native extension) in the VM and let the given block
    # handle the invocation of the function.
    #
    # @param name [Symbol|String] name of the function.
    #   Must be a valid identifier in Jsonnet.
    # @param body [#to_proc] body of the function.
    # @yield calls the given block instead of `body` if `body` is `nil`
    #
    # @note Currently it cannot define keyword or optional paramters in Jsonnet.
    #   Also all the positional optional parameters of the body are interpreted
    #   as required parameters. And the body cannot have keyword, rest or
    #   keyword rest paramters.
    def define_function(name, body = nil)
      body = body ? body.to_proc : Proc.new
      params = body.parameters.map.with_index do |(type, name), i|
        raise ArgumentError, "rest or keyword parameters are not allowed: #{type}" \
          unless [:req, :opt].include? type

        name || "p#{i}"
      end

      register_native_callback(name.to_sym, to_method(body), params);
    end

    private
    # Wraps the function body with a method so that `break` and `return`
    # behave like `return` as they do in a body of Module#define_method.
    def to_method(body)
      mod = Module.new {
        define_method(:dummy, body)
      }
      mod.instance_method(:dummy).bind(body.binding.receiver)
    end

    class UnsupportedOptionError < RuntimeError; end
  end
end
