require 'jsonnet'
require 'test/unit'

class TestVM < Test::Unit::TestCase
  test 'test vm' do
    vm = Jsonnet::VM.new
  end
end

