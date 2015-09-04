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

  test 'Jsonnet::VM#evaluate evaluates snippet' do
    vm = Jsonnet::VM.new
    result = vm.evaluate(<<-EOS, 'example.snippet')
      local myvar = 1;
      {
          ["foo" + myvar]: myvar,
      }
    EOS

    assert_equal JSON.parse(<<-EOS), JSON.parse(result)
      {"foo1": 1}
    EOS
  end

  test 'Jsonnet::VM#evaluate can be called without filename' do
    vm = Jsonnet::VM.new
    result = vm.evaluate(<<-EOS)
      local myvar = 1;
      {
          ["foo" + myvar]: myvar,
      }
    EOS

    assert_equal JSON.parse(<<-EOS), JSON.parse(result)
      {"foo1": 1}
    EOS
  end

  test 'Jsonnet::VM#evaluate raises an EvaluationError on error' do
    vm = Jsonnet::VM.new
    assert_raise(Jsonnet::EvaluationError) do
      vm.evaluate(<<-EOS, 'example.snippet')
        {
            // unbound variable
            ["foo" + myvar]: myvar,
        }
      EOS
    end
  end

  test 'Jsonnet::VM#evaluate_multi returns a JSON per filename' do
    vm = Jsonnet::VM.new
    [
      [ "{}", {} ],
      [
        %<
          local myvar = 1;
          { ["foo" + myvar]: [myvar] }
        >,
        {
          'foo1' => [1],
        }
      ],
      [
        %<
          local myvar = 1;
          {
            ["foo" + myvar]: [myvar],
            ["bar" + myvar]: {
              ["baz" + (myvar+1)]: myvar+1,
            },
          }
        >,
        {
          'foo1' => [1],
          'bar1' => {'baz2' => 2},
        }
      ],
    ].each do |jsonnet, expected|
      result = vm.evaluate_multi(jsonnet)
      assert_equal expected.keys.sort, result.keys.sort
      expected.each do |fname, value|
        assert_not_nil result[fname]
        assert_equal value, JSON.parse(result[fname])
      end
    end
  end

  test 'Jsonnet::VM#evaluate_file_multi returns a JSON per filename' do
    vm = Jsonnet::VM.new
    result = vm.evaluate_file_multi example_file("example_multi.jsonnet")
    expected = {
      'foo1' => [1],
      'bar1' => {'baz2' => 2},
    }
    assert_equal expected.keys.sort, result.keys.sort
    expected.each do |fname, value|
      assert_not_nil result[fname]
      assert_equal value, JSON.parse(result[fname])
    end
  end

  private
  def example_file(name)
    File.join(File.dirname(__FILE__), name)
  end
end

