require 'rubygems'
require 'sinatra'

set :run, true

get '/?' do
  "hello<br /><a href=\"foo\">foo</a>"
end

get '/foo' do
  "hello<br /><a href=\"bar\">bar</a>"
end

get '/bar' do
  "hello<br /><a href=\".\">index</a>"
end
