require 'jsonnet'
require 'test/unit'

class TestJsonnet < Test::Unit::TestCase
  test 'libversion returns a String' do
    assert_kind_of String, Jsonnet.libversion 
  end
end
