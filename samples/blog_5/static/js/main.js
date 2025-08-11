// Main JavaScript file for Qwen ChatBot

// Global utility functions
function showToast(message, type = 'info', duration = 5000) {
    const toastContainer = document.querySelector('.toast-container');
    const toast = document.getElementById('toast');
    const toastBody = document.getElementById('toast-body');
    
    if (!toast) return;
    
    // Set toast type styling
    toast.className = `toast show`;
    const header = toast.querySelector('.toast-header');
    
    // Remove existing type classes
    header.classList.remove('bg-success', 'bg-danger', 'bg-warning', 'bg-info', 'text-white');
    
    // Add appropriate styling based on type
    switch (type) {
        case 'success':
            header.classList.add('bg-success', 'text-white');
            header.querySelector('i').className = 'fas fa-check-circle me-2';
            break;
        case 'error':
        case 'danger':
            header.classList.add('bg-danger', 'text-white');
            header.querySelector('i').className = 'fas fa-exclamation-triangle me-2';
            break;
        case 'warning':
            header.classList.add('bg-warning', 'text-white');
            header.querySelector('i').className = 'fas fa-exclamation-circle me-2';
            break;
        default:
            header.classList.add('bg-info', 'text-white');
            header.querySelector('i').className = 'fas fa-info-circle me-2';
    }
    
    // Set message
    toastBody.textContent = message;
    
    // Show toast
    const bsToast = new bootstrap.Toast(toast, {
        autohide: true,
        delay: duration
    });
    bsToast.show();
}

// Loading indicator utilities
function showLoading(element, text = 'Loading...') {
    if (element) {
        element.disabled = true;
        const originalContent = element.innerHTML;
        element.setAttribute('data-original-content', originalContent);
        element.innerHTML = `
            <span class="spinner-border spinner-border-sm me-2" role="status" aria-hidden="true"></span>
            ${text}
        `;
    }
}

function hideLoading(element) {
    if (element) {
        element.disabled = false;
        const originalContent = element.getAttribute('data-original-content');
        if (originalContent) {
            element.innerHTML = originalContent;
            element.removeAttribute('data-original-content');
        }
    }
}

// API utilities
async function makeApiCall(url, options = {}) {
    try {
        const response = await fetch(url, {
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            },
            ...options
        });
        
        if (!response.ok) {
            const errorData = await response.json().catch(() => ({ error: 'Network error' }));
            throw new Error(errorData.error || `HTTP error! status: ${response.status}`);
        }
        
        return await response.json();
    } catch (error) {
        console.error('API call failed:', error);
        throw error;
    }
}

// Form validation utilities
function validateMessage(message) {
    const errors = [];
    
    if (!message || !message.trim()) {
        errors.push('Message cannot be empty');
    }
    
    if (message.length > 2000) {
        errors.push('Message too long (max 2000 characters)');
    }
    
    return {
        isValid: errors.length === 0,
        errors: errors
    };
}

// Text utilities
function truncateText(text, maxLength = 100) {
    if (text.length <= maxLength) return text;
    return text.substring(0, maxLength) + '...';
}

function formatTimestamp(timestamp) {
    if (!timestamp) return '';
    const date = new Date(timestamp);
    return date.toLocaleString();
}

// Local storage utilities
function saveToLocalStorage(key, value) {
    try {
        localStorage.setItem(key, JSON.stringify(value));
    } catch (error) {
        console.warn('Failed to save to localStorage:', error);
    }
}

function loadFromLocalStorage(key, defaultValue = null) {
    try {
        const item = localStorage.getItem(key);
        return item ? JSON.parse(item) : defaultValue;
    } catch (error) {
        console.warn('Failed to load from localStorage:', error);
        return defaultValue;
    }
}

// Animation utilities
function animateElement(element, animationClass = 'fade-in') {
    if (element) {
        element.classList.add(animationClass);
        element.addEventListener('animationend', () => {
            element.classList.remove(animationClass);
        }, { once: true });
    }
}

// Error handling utilities
function handleApiError(error, context = '') {
    console.error(`API Error${context ? ` in ${context}` : ''}:`, error);
    
    let message = 'An unexpected error occurred';
    
    if (error.message) {
        if (error.message.includes('network') || error.message.includes('fetch')) {
            message = 'Network error. Please check your connection.';
        } else if (error.message.includes('401') || error.message.includes('unauthorized')) {
            message = 'Authentication failed. Please check your API key.';
        } else if (error.message.includes('403') || error.message.includes('forbidden')) {
            message = 'Access denied. Please check your permissions.';
        } else if (error.message.includes('429') || error.message.includes('rate limit')) {
            message = 'Rate limit exceeded. Please try again later.';
        } else {
            message = error.message;
        }
    }
    
    showToast(message, 'error');
}

// Initialize common functionality
document.addEventListener('DOMContentLoaded', function() {
    // Initialize tooltips
    const tooltipTriggerList = [].slice.call(document.querySelectorAll('[data-bs-toggle="tooltip"]'));
    tooltipTriggerList.map(function (tooltipTriggerEl) {
        return new bootstrap.Tooltip(tooltipTriggerEl);
    });
    
    // Initialize popovers
    const popoverTriggerList = [].slice.call(document.querySelectorAll('[data-bs-toggle="popover"]'));
    popoverTriggerList.map(function (popoverTriggerEl) {
        return new bootstrap.Popover(popoverTriggerEl);
    });
    
    // Auto-hide alerts after 5 seconds
    const alerts = document.querySelectorAll('.alert[data-auto-dismiss="true"]');
    alerts.forEach(alert => {
        setTimeout(() => {
            const bsAlert = new bootstrap.Alert(alert);
            bsAlert.close();
        }, 5000);
    });
    
    // Add smooth scrolling to anchor links
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', function (e) {
            const target = document.querySelector(this.getAttribute('href'));
            if (target) {
                e.preventDefault();
                target.scrollIntoView({
                    behavior: 'smooth'
                });
            }
        });
    });
    
    // Add loading states to buttons
    document.querySelectorAll('button[data-loading-text]').forEach(button => {
        button.addEventListener('click', function() {
            const loadingText = this.getAttribute('data-loading-text');
            if (loadingText) {
                showLoading(this, loadingText);
            }
        });
    });
});

// Keyboard shortcuts
document.addEventListener('keydown', function(e) {
    // Escape key to close modals/dropdowns
    if (e.key === 'Escape') {
        const openModals = document.querySelectorAll('.modal.show');
        openModals.forEach(modal => {
            const bsModal = bootstrap.Modal.getInstance(modal);
            if (bsModal) bsModal.hide();
        });
        
        const openDropdowns = document.querySelectorAll('.dropdown-menu.show');
        openDropdowns.forEach(dropdown => {
            const toggle = dropdown.previousElementSibling;
            if (toggle) {
                const bsDropdown = bootstrap.Dropdown.getInstance(toggle);
                if (bsDropdown) bsDropdown.hide();
            }
        });
    }
});

// Export utilities for use in other scripts
window.ChatBotUtils = {
    showToast,
    showLoading,
    hideLoading,
    makeApiCall,
    validateMessage,
    truncateText,
    formatTimestamp,
    saveToLocalStorage,
    loadFromLocalStorage,
    animateElement,
    handleApiError
};