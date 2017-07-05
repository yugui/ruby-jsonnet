module Jsonnet
  class VM
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
      self.import_callback = Proc.new
      nil
    end
  end
end
