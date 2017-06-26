require 'jsonnet'

require 'tempfile'
require 'test/unit'

class TestJsonnet < Test::Unit::TestCase
  test 'libversion returns a String' do
    assert_kind_of String, Jsonnet.libversion
  end

  test 'Jsonnet.evaluate returns a JSON parsed result' do
    result = Jsonnet.evaluate('{ foo: "bar" }')
    assert_equal result, { "foo" => "bar" }
  end

  test 'Jsonnet.evaluate can accept options for JSON' do
    result = Jsonnet.evaluate('{ foo: "bar" }', json_options: { symbolize_names: true })
    assert_equal result, { foo: "bar" }
  end

  test 'Jsonnet.evaluate can accept options for Jsonnet VM' do
    result = Jsonnet.evaluate(
      'import "imported.jsonnet"',
      jsonnet_options: {
        import_callback: ->(_base, _rel) do
          return ['{ foo: "bar" }', 'imported']
        end
      }
    )
    assert_equal result, { "foo" => "bar" }
  end

  test 'Jsonnet.load returns a JSON parsed result' do
    result = Jsonnet.load(example_jsonnet_file.path)
    assert_equal result, { "foo1" => 1 }
  end

  private

  def example_jsonnet_file
    example = Tempfile.open("example.jsonnet") do |f|
      f.write %<
        local myvar = 1;
        {
          ["foo" + myvar]: myvar,
        }
      >
      f
    end
  end
end
