// webserv JavaScript - Simple and functional

document.addEventListener('DOMContentLoaded', function() {
    console.log('webserv client-side script loaded');
    
    // Add click animation to test links
    const testLinks = document.querySelectorAll('.test-link');
    testLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            this.style.transform = 'scale(0.95)';
            setTimeout(() => {
                this.style.transform = '';
            }, 150);
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
            
            // Show upload progress
            const submitBtn = this.querySelector('input[type="submit"]');
            if (submitBtn) {
                submitBtn.value = 'Uploading...';
                submitBtn.disabled = true;
            }
        });
    }
});