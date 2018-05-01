[![Build Status](https://travis-ci.org/yugui/ruby-jsonnet.svg?branch=master)](https://travis-ci.org/yugui/ruby-jsonnet)

# Jsonnet

[Jsonnet][] processor library.  Wraps the official C++ implementation with a Ruby extension library.

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'jsonnet'
```

And then execute:

```shell
$ bundle install
```

Or install it yourself as:

```shell
$ gem install jsonnet
```

By default this gem will compile and install Jsonnet (v0.10.0) as part of
installation. However you can use the system version of Jsonnet if you prefer.
This would be the recommended route if you want to use a different version
of Jsonnet or are having problems installing this.

To install libjsonnet:

```shell
$ git clone https://github.com/google/jsonnet.git
$ cd jsonnet
$ make libjsonnet.so
$ sudo cp libjsonnet.so /usr/local/lib/libjsonnet.so
$ sudo cp include/libjsonnet.h /usr/local/include/libjsonnet.h
```

Note: /usr/local/lib and /usr/local/include are used as they are library lookup
locations. You may need to adjust these for your system if you have errors
running this gem saying it can't open libjsonnet.so - on Ubuntu for instance
I found /lib worked when /usr/local/lib did not.

To install this gem without jsonnet:

Use `JSONNET_USE_SYSTEM_LIBRARIES` ENV var:

```shell
$ JSONNET_USE_SYSTEM_LIBRARIES=1 bundle install
```

or, the `--use-system-libraries` option:


```shell
gem install jsonnet -- --use-system-libraries
```

## Usage

Load the library with `require "jsonnet"`

You can evaluate a string of Jsonnet using `Jsonnet.parse`

```
irb(main):002:0> Jsonnet.evaluate('{ foo: "bar" }')
=> {"foo"=>"bar"}
```
Or load a file using `Jsonnet.load`

```
irb(main):002:0> Jsonnet.load('example.jsonnet')
=> {"baz"=>1}
```

To get closer to the C++ interface you can create an instance of `Jsonnet::VM`

```
irb(main):002:0> vm = Jsonnet::VM.new
=> #<Jsonnet::VM:0x007fd29aa1e568>
irb(main):003:0> vm.evaluate('{ foo: "bar" }')
=> "{\n   \"foo\": \"bar\"\n}\n"
irb(main):004:0> vm.evaluate_file('example.jsonnet')
=> "{\n   \"baz\": 1\n}\n"
```

## Contributing

1. Fork it ( https://github.com/yugui/ruby-jsonnet/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

[Jsonnet]: https://github.com/google/jsonnet
