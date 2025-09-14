#!/bin/bash

echo "Content-Type: text/html"
echo ""

cat << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Bash CGI Test</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .info { background: #f0f0f0; padding: 15px; margin: 10px 0; border-radius: 5px; }
        .env-var { margin: 5px 0; }
    </style>
</head>
<body>
    <h1>🐚 Bash CGI Test</h1>
    
    <div class="info">
        <h2>System Information</h2>
EOF

echo "        <div class=\"env-var\"><strong>Date:</strong> $(date)</div>"
echo "        <div class=\"env-var\"><strong>Server:</strong> $(uname -a)</div>"
echo "        <div class=\"env-var\"><strong>Working Directory:</strong> $(pwd)</div>"
echo "        <div class=\"env-var\"><strong>User:</strong> $(whoami)</div>"

cat << 'EOF'
    </div>
    
    <div class="info">
        <h2>Request Information</h2>
EOF

echo "        <div class=\"env-var\"><strong>Method:</strong> ${REQUEST_METHOD:-Unknown}</div>"
echo "        <div class=\"env-var\"><strong>Query String:</strong> ${QUERY_STRING:-None}</div>"
echo "        <div class=\"env-var\"><strong>Server Name:</strong> ${SERVER_NAME:-Unknown}</div>"
echo "        <div class=\"env-var\"><strong>Server Port:</strong> ${SERVER_PORT:-Unknown}</div>"

cat << 'EOF'
    </div>
    
    <div class="info">
        <h2>CGI Environment</h2>
EOF

# Print CGI-related environment variables
for var in $(env | grep -E '^(HTTP_|REQUEST_|SERVER_|GATEWAY_|SCRIPT_)' | sort); do
    echo "        <div class=\"env-var\"><strong>$(echo "$var" | cut -d= -f1):</strong> $(echo "$var" | cut -d= -f2-)</div>"
done

cat << 'EOF'
    </div>
    
    <p><a href="/">← Back to Home</a></p>
</body>
</html>
EOF