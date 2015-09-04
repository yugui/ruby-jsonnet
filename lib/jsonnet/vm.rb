module Jsonnet
  class VM
    def evaluate(jsonnet, filename: "(jsonnet)", multi: false)
      eval_snippet(jsonnet, filename, multi)
    end

    def evaluate_file(filename, multi: false)
      eval_file(filename, multi)
    end
  end
end
