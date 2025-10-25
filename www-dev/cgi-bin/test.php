<?php
header("Content-Type: text/html; charset=utf-8");
?>
<!DOCTYPE html>
<html>
<head>
    <title>PHP CGI Test</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .info { background: #f0f8ff; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #007bff; }
        .php-var { margin: 5px 0; }
        h1 { color: #007bff; }
        .highlight { background: #fff3cd; padding: 10px; border-radius: 5px; }
    </style>
</head>
<body>
    <h1>&#x1F418; PHP CGI Test</h1>
    
    <div class="info">
        <h2>PHP Information</h2>
        <div class="php-var"><strong>PHP Version:</strong> <?php echo phpversion(); ?></div>
        <div class="php-var"><strong>Server Time:</strong> <?php echo date('Y-m-d H:i:s'); ?></div>
        <div class="php-var"><strong>Server Software:</strong> <?php echo $_SERVER['SERVER_SOFTWARE'] ?? 'Unknown'; ?></div>
    </div>
    
    <div class="info">
        <h2>Request Information</h2>
        <div class="php-var"><strong>Method:</strong> <?php echo $_SERVER['REQUEST_METHOD'] ?? 'Unknown'; ?></div>
        <div class="php-var"><strong>URI:</strong> <?php echo $_SERVER['REQUEST_URI'] ?? 'Unknown'; ?></div>
        <div class="php-var"><strong>Query String:</strong> <?php echo $_SERVER['QUERY_STRING'] ?? 'None'; ?></div>
        <div class="php-var"><strong>User Agent:</strong> <?php echo $_SERVER['HTTP_USER_AGENT'] ?? 'Unknown'; ?></div>
    </div>
    
    <div class="info">
        <h2>Environment Variables</h2>
        <?php
        $cgi_vars = array();
        foreach ($_SERVER as $key => $value) {
            if (strpos($key, 'HTTP_') === 0 || strpos($key, 'SERVER_') === 0 || strpos($key, 'REQUEST_') === 0 || strpos($key, 'GATEWAY_') === 0) {
                $cgi_vars[$key] = $value;
            }
        }
        ksort($cgi_vars);
        foreach ($cgi_vars as $key => $value) {
            echo "<div class=\"php-var\"><strong>$key:</strong> " . htmlspecialchars($value) . "</div>";
        }
        ?>
    </div>
    
    <div class="highlight">
        <p><strong>Dynamic Content:</strong> Current timestamp: <?php echo time(); ?></p>
        <p><strong>Random Number:</strong> <?php echo rand(1, 1000); ?></p>
    </div>
    
    <p><a href="/">&larr; Back to Home</a></p>
</body>
</html>