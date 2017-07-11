require "jsonnet/version"
require "jsonnet/vm"
require "json"

module Jsonnet
  module_function

  ##
  # Evaluates a string of Jsonnet and returns a hash of the resulting JSON
  #
  # @param [String]  jsonnet  Jsonnet source string, ideally in UTF-8 encoding
  # @param [Hash]    jsonnet_options A hash of options to for Jsonnet::VM.
  #                                  Available options are: filename, multi,
  #                                  import_callback, gc_growth_triger,
  #                                  gc_min_objects, max_stack, max_trace
  # @param [Hash]    json_options Options supported by {JSON.parse}[http://www.rubydoc.info/github/flori/json/JSON#parse-class_method]
  # @return [Hash] The JSON representation as a hash
  # @raise [UnsupportedOptionError] Raised when an option passed is unsupported by Jsonnet::VM
  #
  # @note This method runs Jsonnet::VM#evaluate and runs the string
  #       output through {JSON.parse}[http://www.rubydoc.info/github/flori/json/JSON#parse-class_method]
  #       so those should be looked at for furhter details
  def evaluate(jsonnet, jsonnet_options: {}, json_options: {})
    output = VM.evaluate(jsonnet, jsonnet_options)
    JSON.parse(output, json_options)
  end

  ##
  # Loads a Jsonnet file and returns a hash of the resulting JSON
  #
  # @param [String]  path path to the jsonnet file
  # @param [Hash]    jsonnet_options A hash of options to for Jsonnet::VM.
  #                                  Available options are: encoding, multi,
  #                                  import_callback, gc_growth_triger,
  #                                  gc_min_objects, max_stack, max_trace
  # @param [Hash]    json_options Options supported by {JSON.parse}[http://www.rubydoc.info/github/flori/json/JSON#parse-class_method]
  # @return [Hash] The JSON representation as a hash
  # @raise [UnsupportedOptionError] Raised when an option passed is unsupported by Jsonnet::VM
  #
  # @note This method runs Jsonnet::VM#evaluate_file and runs the string
  #       output through {JSON.parse}[http://www.rubydoc.info/github/flori/json/JSON#parse-class_method]
  #       so those should be looked at for furhter details
  def load(path, jsonnet_options: {}, json_options: {})
    output = VM.evaluate_file(path, jsonnet_options)
    JSON.parse(output, json_options)
  end
end
