module Jsonnet
  class VM
    ##
    # Evaluates Jsonnet source.
    #
    # @param [String]  jsonnet  Jsonnet source string
    # @param [String]  filename filename of the source. Used in stacktrace.
    # @param [Boolean] multi    enables multi-mode
    # @return [String] a JSON representation of the evaluation result
    # @raise [EvaluationError] raised when the evaluation results an error.
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
    def evaluate_file(filename, multi: false)
      eval_file(filename, multi)
    end
  end
end
