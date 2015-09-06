# -*- coding: UTF-8 -*-
require 'jsonnet'

require 'json'
require 'tempfile'
require 'test/unit'

class TestVM < Test::Unit::TestCase
  test 'Jsonnet::VM#evaluate_file evaluates file' do
    vm = Jsonnet::VM.new
    with_example_file(%<
      local myvar = 1;
      {
        ["foo" + myvar]: myvar,
      }
    >) {|fname|
      result = vm.evaluate_file(fname)
      assert_equal JSON.parse(<<-EOS), JSON.parse(result)
      {"foo1": 1}
      EOS
    }
  end

  test 'Jsonnet::VM#evaluate_file raises an EvaluationError on error' do
    vm = Jsonnet::VM.new
    with_example_file(%<
      {
        // unbound variable
        ["foo" + myvar]: myvar,
      }
    >) {|fname|
      assert_raise(Jsonnet::EvaluationError) do
        vm.evaluate_file(fname)
      end
    }
  end

  test "Jsonnet::VM#evaluate_file returns the same encoding as source" do
    vm = Jsonnet::VM.new
    with_example_file(%Q{ ["テスト"] }.encode(Encoding::EUC_JP)) {|fname|
      result = vm.evaluate_file(fname, encoding: Encoding::EUC_JP)
      assert_equal Encoding::EUC_JP, result.encoding
    }
  end

  test "Jsonnet::VM#evaluate_file raises an error in the encoding of filename" do
    vm = Jsonnet::VM.new
    begin
      with_example_file(%q{ ["unterminated string }) {|fname|
        vm.evaluate_file(fname.encode(Encoding::SJIS)) 
      }
    rescue Jsonnet::EvaluationError => e
      assert_equal Encoding::SJIS, e.message.encoding
    end
  end


  test 'Jsonnet::VM#evaluate evaluates snippet' do
    vm = Jsonnet::VM.new
    result = vm.evaluate(<<-EOS, filename: 'example.snippet')
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
      vm.evaluate(<<-EOS, filename: 'example.snippet')
        {
            // unbound variable
            ["foo" + myvar]: myvar,
        }
      EOS
    end
  end

  test "Jsonnet::VM#evaluate returns the same encoding as source" do
    vm = Jsonnet::VM.new
    result = vm.evaluate(%Q{ ["テスト"] }.encode(Encoding::EUC_JP))
    assert_equal Encoding::EUC_JP, result.encoding
  end

  test "Jsonnet::VM#evaluate raises an error in the encoding of filename" do
    vm = Jsonnet::VM.new
    begin
      vm.evaluate(%Q{ ["unterminated string }, filename: "テスト.json".encode(Encoding::SJIS)) 
    rescue Jsonnet::EvaluationError => e
      assert_equal Encoding::SJIS, e.message.encoding
    end
  end

  test "Jsonnet::VM#ext_var binds a variable" do
    vm = Jsonnet::VM.new
    vm.ext_var("var1", "foo")
    result = vm.evaluate('[std.extVar("var1")]')
    assert_equal JSON.parse('["foo"]'), JSON.parse(result)
  end

  test 'Jsonnet::VM#evaluate returns a JSON per filename on multi mode' do
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
      result = vm.evaluate(jsonnet, multi: true)
      assert_equal expected.keys.sort, result.keys.sort
      expected.each do |fname, value|
        assert_not_nil result[fname]
        assert_equal value, JSON.parse(result[fname])
      end
    end
  end

  test 'Jsonnet::VM#evaluate_file returns a JSON per filename on multi mode' do
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
      with_example_file(jsonnet) {|fname|
        result = vm.evaluate_file(fname, multi: true)
        assert_equal expected.keys.sort, result.keys.sort
        expected.each do |fname, value|
          assert_not_nil result[fname]
          assert_equal value, JSON.parse(result[fname])
        end
      }
    end
  end

  private
  def with_example_file(content)
    Tempfile.open("example.jsonnet") {|f|
      f.print content
      f.flush
      f.rewind
      yield f.path
    }
  end
end

