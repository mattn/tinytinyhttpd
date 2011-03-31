WScript.Echo( "Content-Type: text/html;charset=Shift_JIS\n" );
WScript.Echo( "<html><head><title>test</title></head>" );
(function(){
     WScript.Echo( "<body>" );
     WScript.Echo( "Hello, tinytinyhttpd + JScript World" );
     WScript.Echo( "</body>" );
})();
WScript.Echo( "</html>" );
