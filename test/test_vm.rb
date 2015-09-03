require 'jsonnet'

require 'json'
require 'test/unit'

class TestVM < Test::Unit::TestCase
  test 'Jsonnet::VM#evaluate_file evaluates file' do
    vm = Jsonnet::VM.new
    result = vm.evaluate_file example_file('example.jsonnet')

    assert_equal JSON.parse(<<-EOS), JSON.parse(result)
      {"foo1": 1}
    EOS
  end

  test 'Jsonnet::VM#evaluate_file raises an EvaluationError on error' do
    vm = Jsonnet::VM.new
    assert_raise(Jsonnet::EvaluationError) do
      vm.evaluate_file example_file('error.jsonnet')
    end
  end

  private
  def example_file(name)
    File.join(File.dirname(__FILE__), name)
  end
end

