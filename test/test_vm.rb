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

  test "Jsonnet::VM#ext_var binds a variable to a string value" do
    vm = Jsonnet::VM.new
    vm.ext_var("var1", "foo")
    result = vm.evaluate('[std.extVar("var1")]')
    assert_equal JSON.parse('["foo"]'), JSON.parse(result)
  end

  test "Jsonnet::VM#ext_code binds a variable to a code fragment" do
    vm = Jsonnet::VM.new
    vm.ext_code("var1", "{a:1}")
    result = vm.evaluate('[std.extVar("var1")]')
    assert_equal JSON.parse(<<-EOS), JSON.parse(result)
      [
        {
          "a": 1
        }
      ]
    EOS
  end

  test "Jsonnet::VM#tla_var binds a top-level variable to a string value" do
    vm = Jsonnet::VM.new
    vm.tla_var("var1", "foo")
    result = vm.evaluate('function(var1) [var1, var1]')
    assert_equal JSON.parse('["foo", "foo"]'), JSON.parse(result)
  end

  test "Jsonnet::VM#tla_var binds a top-level argument to a string value" do
    vm = Jsonnet::VM.new
    vm.tla_var("var1", "foo")
    result = vm.evaluate('function(var1) [var1, var1]')
    assert_equal JSON.parse('["foo", "foo"]'), JSON.parse(result)
  end

  test "Jsonnet::VM#tla_code binds a top-level argument to a code fragment" do
    vm = Jsonnet::VM.new
    vm.tla_code("var1", "{a:1}")
    result = vm.evaluate('function(var1) [var1, var1]')
    assert_equal JSON.parse(<<-EOS), JSON.parse(result)
      [
        {
          "a": 1
        },
        {
          "a": 1
        }
      ]
    EOS
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

  test "Jsonnet::VM responds to max_stack=" do
    Jsonnet::VM.new.max_stack = 1
  end

  test "Jsonnet::VM responds to gc_min_objects=" do
    Jsonnet::VM.new.gc_min_objects = 1
  end

  test "Jsonnet::VM responds to gc_growth_trigger=" do
    Jsonnet::VM.new.gc_growth_trigger = 1.5
  end

  test "Jsonnet::VM responds to max_trace=" do
    Jsonnet::VM.new.max_trace = 1
  end

  test "Jsonnet::VM#string_output lets the VM output a raw string" do
    vm = Jsonnet::VM.new
    vm.string_output = true
    assert_equal "foo\n", vm.evaluate(%q[ "foo" ])
    vm.string_output = false
    assert_equal ["foo"], JSON.parse(vm.evaluate(%q[ ["foo"] ]))
  end

  test "Jsonnet::VM#import_callback customizes import file resolution" do
    vm = Jsonnet::VM.new
    vm.import_callback = ->(base, rel) {
      case [base, rel]
      when ['/path/to/base/', 'imported1.jsonnet']
        return <<-EOS, '/path/to/imported1/imported1.jsonnet'
          (import "imported2.jsonnet") + {
            b: 2,
          }
        EOS
      when ['/path/to/imported1/', "imported2.jsonnet"]
        return <<-EOS, '/path/to/imported2/imported2.jsonnet'
          { a: 1 }
        EOS
      else
        raise Errno::ENOENT, "#{rel} at #{base}"
      end
    }
    result = vm.evaluate(<<-EOS, filename: "/path/to/base/example.jsonnet")
      (import "imported1.jsonnet") + { c: 3 }
    EOS

    expected = {"a" => 1, "b" => 2, "c" => 3}
    assert_equal expected, JSON.parse(result)
  end

  test "Jsonnet::VM#evaluate returns an error if customized import callback raises an exception" do
    vm = Jsonnet::VM.new
    called = false
    vm.import_callback = ->(base, rel) { called = true; raise }
    assert_raise(Jsonnet::EvaluationError) {
      vm.evaluate(<<-EOS)
        (import "a.jsonnet") + {}
      EOS
    }
    assert_true called
  end

  test "Jsonnet::VM#handle_import treats global escapes as define_method does" do
    num_eval = 0
    begin
      bodies = [
        proc   {|rel, base| return 'null', '/x.libsonnet' },
        lambda {|rel, base| return 'null', '/x.libsonnet' },
        proc   {|rel, base| next 'null', '/x.libsonnet' },
        lambda {|rel, base| next 'null', '/x.libsonnet' },
        proc   {|rel, base| break 'null', '/x.libsonnet' },
        lambda {|rel, base| break 'null', '/x.libsonnet' },
      ]
      bodies.each do |prc|
        vm = Jsonnet::VM.new
        vm.handle_import(&prc)

        result = vm.evaluate('import "a.jsonnet"')
        assert_nil JSON.load(result)

        num_eval += 1
      end
    ensure
      assert_equal bodies.size, num_eval
    end
  end

  test "Jsonnet::VM#handle_import is safe on throw" do
    [
      proc   {|rel, base| throw :dummy },
      lambda {|rel, base| throw :dummy },
    ].each do |prc|
      vm = Jsonnet::VM.new
      vm.handle_import(&prc)

      catch(:dummy) {
        vm.evaluate('import "a.jsonnet"')
        flunk "never reach here"
      }
    end
  end

  test "Jsonnet::VM#jpath_add adds a library search path" do
    vm = Jsonnet::VM.new
    snippet = "(import 'jpath.libsonnet') {b: 2}"
    assert_raise(Jsonnet::EvaluationError) {
      vm.evaluate(snippet)
    }

    vm.jpath_add(File.join(__dir__, 'fixtures'))
    result = vm.evaluate(snippet)
    assert_equal JSON.parse(<<-EOS), JSON.parse(result)
      {
        "a": 1,
        "b": 2
      }
    EOS
  end

  test "Jsonnet::VM#define_function adds a new native extension" do
    vm = Jsonnet::VM.new
    called = false

    vm.define_function("myPow") do |x, y|
      called = true
      x ** y
    end

    result = vm.evaluate("std.native('myPow')(3, 4)")
    assert_equal 3**4, JSON.load(result)
    assert_true called
  end

  test "Jsonnet::VM#define_function passes various types of arguments" do
    [
      [%q(null), nil],
      [%q("abc"), "abc"],
      [%q(1), 1.0],
      [%q(1.25), 1.25],
      [%q(true), true],
      [%q(false), false],
    ].each do |expr, value|
      vm = Jsonnet::VM.new
      vm.define_function("myFunc") do |x|
        assert_equal value, x
        next nil
      end
      vm.evaluate("std.native('myFunc')(#{expr})")
    end
  end

  test "Jsonnet::VM#define_function returns various types of values" do
    [
      [nil, nil],
      ["abc", "abc"],
      [1, 1.0],
      [1.25, 1.25],
      [true, true],
      [false, false],
    ].each do |retval, expected|
      vm = Jsonnet::VM.new
      vm.define_function("myFunc") { retval }

      result = vm.evaluate("std.native('myFunc')()")
      assert_equal expected, JSON.load(result)
    end
  end

  test "Jsonnet::VM#define_function translates an exception in a native function into an error" do
    vm = Jsonnet::VM.new
    vm.define_function("myFunc") do |x|
      raise "something wrong"
    end
    assert_raise(Jsonnet::EvaluationError) {
      vm.evaluate("std.native('myFunc')(1)")
    }
  end

  test "Jsonnet::VM#define_function let the function return a compound object" do
    vm = Jsonnet::VM.new
    vm.define_function("myCompound") do |x, y|
      {
        x => y,
        y => [x, y, y, x],
      }
    end

    result = vm.evaluate("std.native('myCompound')('abc', 'def')")
    assert_equal JSON.parse(<<-EOS), JSON.parse(result)
      {
        "abc": "def",
        "def": ["abc", "def", "def", "abc"]
      }
    EOS
  end

  test "Jsonnet::VM#define_function treats global escapes as define_method does" do
    num_eval = 0
    begin
      bodies = [
        proc   {|x| return x },
        lambda {|x| return x },
        proc   {|x| next x },
        lambda {|x| next x },
        proc   {|x| break x },
        lambda {|x| break x },
      ]
      bodies.each do |prc|
        vm = Jsonnet::VM.new
        vm.define_function(:myFunc, prc)

        result = vm.evaluate('std.native("myFunc")(1.25) + 0.25')
        assert_equal 1.25 + 0.25, JSON.load(result)

        num_eval += 1
      end
    ensure
      assert_equal bodies.size, num_eval
    end
  end

  test "Jsonnet::VM#define_function is safe on throw" do
    [
      proc   {|x| throw :dummy },
      lambda {|x| throw :dummy },
    ].each do |prc|
      vm = Jsonnet::VM.new
      vm.define_function(:myFunc, prc)

      catch(:dummy) {
        vm.evaluate('std.native("myFunc")(1.234)')
        flunk "never reach here"
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

