# Jsonnet

Jsonnet processor library.  Wraps the official C++ implementation with a Ruby extension library.

## Installation

Install libjsonnet:

    $ git clone https://github.com/google/jsonnet.git
    $ cd jsonnet
    $ make libjsonnet.so
    $ sudo cp libjsonnet.so /usr/local/lib/libjsonnet.so
    $ sudo cp include/libjsonnet.h /usr/local/include/libjsonnet.h

Add this line to your application's Gemfile:

```ruby
gem 'jsonnet'
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install jsonnet

## Usage

TODO: Write usage instructions here

## Contributing

1. Fork it ( https://github.com/yugui/ruby-jsonnet/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request
