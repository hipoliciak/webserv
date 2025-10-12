#!/usr/bin/python3

import os
import sys
import cgi

# Print HTTP headers
print("Content-Type: text/html; charset=utf-8")
print()  # Empty line required between headers and body

# Print HTML response
print("""<!DOCTYPE html>
<html>
<head>
    <title>Python CGI Test</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 40px; }}
        .info {{ background: #f0f0f0; padding: 15px; margin: 10px 0; border-radius: 5px; }}
        .env-var {{ margin: 5px 0; }}
    </style>
</head>
<body>
    <h1>&#x1F40D; Python CGI Test</h1>
    <div class="info">
        <h2>Request Information</h2>
        <div class="env-var"><strong>Method:</strong> {}</div>
        <div class="env-var"><strong>Query String:</strong> {}</div>
        <div class="env-var"><strong>Content Length:</strong> {}</div>
        <div class="env-var"><strong>Content Type:</strong> {}</div>
        <div class="env-var"><strong>Server Name:</strong> {}</div>
        <div class="env-var"><strong>Server Port:</strong> {}</div>
    </div>
    
    <div class="info">
        <h2>Environment Variables</h2>""".format(
    os.environ.get('REQUEST_METHOD', 'Unknown'),
    os.environ.get('QUERY_STRING', 'None'),
    os.environ.get('CONTENT_LENGTH', '0'),
    os.environ.get('CONTENT_TYPE', 'None'),
    os.environ.get('SERVER_NAME', 'Unknown'),
    os.environ.get('SERVER_PORT', 'Unknown')
))

# Print environment variables
for key, value in sorted(os.environ.items()):
    if key.startswith(('HTTP_', 'REQUEST_', 'SERVER_', 'GATEWAY_', 'SCRIPT_')):
        print(f'        <div class="env-var"><strong>{key}:</strong> {value}</div>')

print("""    </div>
    
    <div class="info">
        <h2>POST Data</h2>""")

# Handle POST data if present
if os.environ.get('REQUEST_METHOD') == 'POST':
    content_length = int(os.environ.get('CONTENT_LENGTH', '0'))
    if content_length > 0:
        post_data = sys.stdin.read(content_length)
        print(f"        <pre>{post_data}</pre>")
    else:
        print("        <p>No POST data received</p>")
else:
    print("        <p>Not a POST request</p>")

print("""    </div>
    
    <p><a href="/">&larr; Back to Home</a></p>
</body>
</html>""")