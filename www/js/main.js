// webserv JavaScript

document.addEventListener('DOMContentLoaded', function() {
    console.log('webserv client-side script loaded');
    
    // Add some interactivity
    const testLinks = document.querySelectorAll('.test-link');
    
    testLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            // Add click animation
            this.style.transform = 'scale(0.95)';
            setTimeout(() => {
                this.style.transform = '';
            }, 150);
        });
    });
    
    // Display current time
    function updateTime() {
        const now = new Date();
        const timeString = now.toLocaleTimeString();
        
        // Create or update time display
        let timeDisplay = document.getElementById('current-time');
        if (!timeDisplay) {
            timeDisplay = document.createElement('div');
            timeDisplay.id = 'current-time';
            timeDisplay.style.cssText = `
                position: fixed;
                top: 20px;
                right: 20px;
                background: rgba(0, 0, 0, 0.8);
                color: white;
                padding: 10px 15px;
                border-radius: 5px;
                font-family: monospace;
                font-size: 14px;
                z-index: 1000;
            `;
            document.body.appendChild(timeDisplay);
        }
        
        timeDisplay.textContent = timeString;
    }
    
    // Update time every second
    updateTime();
    setInterval(updateTime, 1000);
    
    // Add smooth scrolling for internal links
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', function(e) {
            e.preventDefault();
            const target = document.querySelector(this.getAttribute('href'));
            if (target) {
                target.scrollIntoView({
                    behavior: 'smooth',
                    block: 'start'
                });
            }
        });
    });
    
    // Simple form validation for upload form
    const uploadForm = document.getElementById('upload-form');
    if (uploadForm) {
        uploadForm.addEventListener('submit', function(e) {
            const fileInput = this.querySelector('input[type="file"]');
            if (fileInput && !fileInput.files.length) {
                e.preventDefault();
                alert('Please select a file to upload.');
                return false;
            }
            
            // Show upload progress (simulated)
            const submitBtn = this.querySelector('input[type="submit"]');
            if (submitBtn) {
                submitBtn.value = 'Uploading...';
                submitBtn.disabled = true;
            }
        });
    }
    
    // Add easter egg
    let clickCount = 0;
    const header = document.querySelector('h1');
    if (header) {
        header.addEventListener('click', function() {
            clickCount++;
            if (clickCount === 5) {
                this.style.animation = 'rainbow 2s infinite';
                setTimeout(() => {
                    this.style.animation = '';
                    clickCount = 0;
                }, 5000);
            }
        });
    }
});

// Add rainbow animation
const style = document.createElement('style');
style.textContent = `
    @keyframes rainbow {
        0% { color: #ff0000; }
        16% { color: #ff8000; }
        33% { color: #ffff00; }
        50% { color: #00ff00; }
        66% { color: #0080ff; }
        83% { color: #8000ff; }
        100% { color: #ff0000; }
    }
`;
document.head.appendChild(style);