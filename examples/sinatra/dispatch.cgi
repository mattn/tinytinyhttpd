#!/usr/bin/ruby
require 'rubygems'

load "foo.rb"

set :run, false

Rack::Handler::CGI.run Sinatra::Application
