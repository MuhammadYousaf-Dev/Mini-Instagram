// ============================================
// SOCIAL MEDIA APP 
// ============================================

console.log(" SocialVerse App v2.0 - Professional Edition");

// ========== CONFIGURATION ==========
const API_BASE = 'http://localhost:8080/api';
let currentUser = null;
let feedPosts = [];
let currentFeedIndex = -1;
let userPosts = [];
let currentPostIndex = -1;
let stories = {
    my: [],
    followed: []
};
let currentStoryIndex = -1;
let currentStoryType = 'my';

// ========== INITIALIZATION ==========
window.onload = function() {
    console.log(" Page loaded");
    
    // Check for saved session
    const savedUser = localStorage.getItem('socialverse_user');
    if (savedUser) {
        try {
            currentUser = JSON.parse(savedUser);
            console.log(" Session restored:", currentUser);
            
            // Update UI
            updateUserDisplay();
            
            // Show main app
            document.getElementById('authContainer').style.display = 'none';
            document.getElementById('appContainer').style.display = 'flex';
            
            // Load all data
            loadAllData();
        } catch (e) {
            console.error("Failed to restore session:", e);
            localStorage.removeItem('socialverse_user');
        }
    }
    
    checkServerConnection();
    setupEventListeners();
};

function setupEventListeners() {
    // Search input with debounce
    const searchInput = document.getElementById('searchInput');
    if (searchInput) {
        let searchTimeout;
        searchInput.addEventListener('keyup', function() {
            clearTimeout(searchTimeout);
            searchTimeout = setTimeout(() => {
                if (this.value.length >= 2) {
                    searchUsers();
                } else if (this.value.length === 0) {
                    document.getElementById('searchResults').innerHTML = '';
                }
            }, 300);
        });
    }
    
    // Modal close on outside click
    window.onclick = function(event) {
        if (event.target.classList.contains('modal')) {
            event.target.classList.remove('show');
        }
    };
    
    // Enter key for login
    document.getElementById('loginPassword').addEventListener('keypress', function(e) {
        if (e.key === 'Enter') login();
    });
}

function updateUserDisplay() {
    if (!currentUser) return;
    
    document.getElementById('displayUsername').textContent = currentUser.username;
    document.getElementById('displayUserId').textContent = currentUser.id;
    document.getElementById('profileUsername').textContent = currentUser.username;
    document.getElementById('profileUserId').textContent = currentUser.id;
}

async function checkServerConnection() {
    try {
        const response = await fetch(`${API_BASE}/status`);
        const data = await response.json();
        console.log(" Server connected:", data);
    } catch (error) {
        console.error(" Server not connected:", error);
        showToast("Cannot connect to server. Please start server.exe", "error", 5000);
    }
}

function loadAllData() {
    if (!currentUser) return;
    
    showLoading(true);
    
    Promise.all([
        loadFeed().catch(e => console.error("Feed error:", e)),
        loadUserPosts().catch(e => console.error("User posts error:", e)),
        loadProfile().catch(e => console.error("Profile error:", e)),
        loadFollowing().catch(e => console.error("Following error:", e)),
        loadStories().catch(e => console.error("Stories error:", e)),
        loadLikedPosts().catch(e => console.error("Liked posts error:", e))
    ]).finally(() => {
        showLoading(false);
    });
}

function showLoading(show) {
    const loader = document.getElementById('globalLoader') || createGlobalLoader();
    loader.style.display = show ? 'flex' : 'none';
}

function createGlobalLoader() {
    const loader = document.createElement('div');
    loader.id = 'globalLoader';
    loader.innerHTML = '<div class="spinner"></div><p>Loading...</p>';
    loader.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        background: rgba(0,0,0,0.5);
        display: none;
        justify-content: center;
        align-items: center;
        flex-direction: column;
        z-index: 9999;
        color: white;
    `;
    document.body.appendChild(loader);
    return loader;
}

// ========== TOAST NOTIFICATION ==========
function showToast(message, type = 'info', duration = 3000) {
    const toast = document.getElementById('toast');
    if (!toast) {
        alert(message);
        return;
    }
    
    toast.textContent = message;
    toast.className = `toast show ${type}`;
    
    setTimeout(() => {
        toast.classList.remove('show');
    }, duration);
}

// ========== AUTHENTICATION ==========
function showLogin() {
    document.getElementById('loginForm').classList.add('active');
    document.getElementById('registerForm').classList.remove('active');
    
    // Clear fields
    document.getElementById('loginUserId').value = '';
    document.getElementById('loginPassword').value = '';
}

function showRegister() {
    document.getElementById('loginForm').classList.remove('active');
    document.getElementById('registerForm').classList.add('active');
    
    // Clear fields
    document.getElementById('regUserId').value = '';
    document.getElementById('regUsername').value = '';
    document.getElementById('regPassword').value = '';
    document.getElementById('regConfirmPassword').value = '';
}

async function login() {
    const userId = document.getElementById('loginUserId').value.trim();
    const password = document.getElementById('loginPassword').value.trim();
    
    if (!userId || !password) {
        showToast('Please enter User ID and Password', 'error');
        return;
    }
    
    const loginBtn = document.querySelector('#loginForm .btn-primary');
    const originalText = loginBtn.innerHTML;
    loginBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Logging in...';
    loginBtn.disabled = true;
    
    try {
        const response = await fetch(`${API_BASE}/login`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                user_id: parseInt(userId),
                password: password
            })
        });
        
        const data = await response.json();
        
        if (data.success) {
            currentUser = {
                id: data.user_id,
                username: data.username
            };
            
            // Save session
            localStorage.setItem('socialverse_user', JSON.stringify(currentUser));
            
            // Update UI
            updateUserDisplay();
            
            // Show main app
            document.getElementById('authContainer').style.display = 'none';
            document.getElementById('appContainer').style.display = 'flex';
            
            showToast(`Welcome back, ${currentUser.username}!`, 'success');
            
            // Load all data
            loadAllData();
        } else {
            showToast(data.message || 'Login failed', 'error');
        }
    } catch (error) {
        console.error('Login error:', error);
        showToast('Server error. Please try again.', 'error');
    } finally {
        loginBtn.innerHTML = originalText;
        loginBtn.disabled = false;
    }
}

async function register() {
    const userId = document.getElementById('regUserId').value.trim();
    const username = document.getElementById('regUsername').value.trim();
    const password = document.getElementById('regPassword').value.trim();
    const confirmPass = document.getElementById('regConfirmPassword').value.trim();
    
    // Validation
    if (!userId || !username || !password || !confirmPass) {
        showToast('Please fill all fields', 'error');
        return;
    }
    
    if (password !== confirmPass) {
        showToast('Passwords do not match', 'error');
        return;
    }
    
    if (password.length < 4) {
        showToast('Password must be at least 4 characters', 'error');
        return;
    }
    
    const registerBtn = document.querySelector('#registerForm .btn-primary');
    const originalText = registerBtn.innerHTML;
    registerBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Registering...';
    registerBtn.disabled = true;
    
    try {
        const response = await fetch(`${API_BASE}/register`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                user_id: parseInt(userId),
                username: username,
                password: password
            })
        });
        
        const data = await response.json();
        
        if (data.success) {
            showToast('Registration successful! Please login.', 'success');
            showLogin();
        } else {
            showToast(data.message || 'Registration failed', 'error');
        }
    } catch (error) {
        console.error('Register error:', error);
        showToast('Server error', 'error');
    } finally {
        registerBtn.innerHTML = originalText;
        registerBtn.disabled = false;
    }
}

function logout() {
    if (!confirm('Are you sure you want to logout?')) return;
    
    currentUser = null;
    feedPosts = [];
    currentFeedIndex = -1;
    
    localStorage.removeItem('socialverse_user');
    
    document.getElementById('authContainer').style.display = 'flex';
    document.getElementById('appContainer').style.display = 'none';
    
    showToast('Logged out successfully', 'info');
    
    // Clear forms
    showLogin();
}

// ========== SECTION NAVIGATION ==========
function showSection(section, event) {
    if (event) {
        document.querySelectorAll('.nav-item').forEach(item => {
            item.classList.remove('active');
        });
        event.currentTarget.classList.add('active');
    }
    
    document.querySelectorAll('.content-section').forEach(s => {
        s.classList.remove('active');
    });
    document.getElementById(section + 'Section').classList.add('active');
    
    // Refresh data when switching to certain sections
    switch(section) {
        case 'feed':
            if (feedPosts.length === 0) loadFeed();
            break;
        case 'posts':
            if (userPosts.length === 0) loadUserPosts();
            break;
        case 'stories':
            loadStories();
            break;
        case 'profile':
            loadProfile();
            break;
        case 'following':
            loadFollowing();
            break;
        case 'liked':
            loadLikedPosts();
            break;
        case 'search':
            document.getElementById('searchResults').innerHTML = '';
            document.getElementById('searchInput').value = '';
            document.getElementById('searchInput').focus();
            break;
    }
}

// ========== FEED FUNCTIONS ==========
async function loadFeed() {
    if (!currentUser) return;
    
    const feedContainer = document.getElementById('feedContainer');
    feedContainer.innerHTML = '<div class="loading-spinner"><i class="fas fa-spinner fa-spin"></i> Loading your feed...</div>';
    
    try {
        const response = await fetch(`${API_BASE}/feed/${currentUser.id}`);
        const posts = await response.json();
        
        feedPosts = posts.map(post => ({
            id: post.postId,
            userId: post.userId,
            username: post.username,
            caption: post.caption,
            likes: post.likes || 0,
            liked: post.liked || false,
            comments: post.commentCount || 0
        }));
        
        currentFeedIndex = feedPosts.length > 0 ? 0 : -1;
        displayCurrentFeedPost();
    } catch (error) {
        console.error('Feed error:', error);
        feedContainer.innerHTML = '<div class="error-message">Failed to load feed. <button onclick="loadFeed()">Try again</button></div>';
    }
}

function displayCurrentFeedPost() {
    const container = document.getElementById('feedContainer');
    const counter = document.getElementById('feedCounter');
    
    if (feedPosts.length === 0 || currentFeedIndex === -1) {
        container.innerHTML = `
            <div class="empty-state">
                <i class="fas fa-images"></i>
                <h3>No posts in your feed</h3>
                <p>Follow users to see their posts here!</p>
                <button class="btn-primary" onclick="showSection('search', {currentTarget: document.querySelector('[onclick*=\"search\"]')})">
                    <i class="fas fa-search"></i> Find Users
                </button>
            </div>
        `;
        if (counter) counter.textContent = '0 posts';
        return;
    }
    
    const post = feedPosts[currentFeedIndex];
    
    container.innerHTML = `
        <div class="feed-post" data-post-id="${post.id}">
            <div class="post-header">
                <div class="post-avatar">
                    <i class="fas fa-user-circle"></i>
                </div>
                <div class="post-user-info">
                    <h4>${post.username}</h4>
                    <span class="post-user-id">@user_${post.userId}</span>
                </div>
                ${post.userId === currentUser.id ? 
                    `<button class="btn-icon btn-delete" onclick="deletePost(${post.id}, event)" title="Delete post">
                        <i class="fas fa-trash"></i>
                    </button>` : ''}
            </div>
            <div class="post-content">
                <p>${post.caption}</p>
            </div>
            <div class="post-stats">
                <span><i class="fas fa-heart"></i> ${post.likes} likes</span>
                <span><i class="fas fa-comment"></i> ${post.comments} comments</span>
            </div>
            <div class="post-actions">
                <button class="action-btn ${post.liked ? 'liked' : ''}" onclick="likePost(${post.id})">
                    <i class="fas ${post.liked ? 'fa-heart' : 'fa-heart'}"></i> 
                    <span>Like</span>
                </button>
                <button class="action-btn" onclick="showComments(${post.id})">
                    <i class="fas fa-comment"></i> Comment
                </button>
            </div>
        </div>
    `;
    
    if (counter) {
        counter.textContent = `Post ${currentFeedIndex + 1} of ${feedPosts.length}`;
    }
    
    // Update navigation buttons
    document.getElementById('prevFeedBtn').disabled = currentFeedIndex === 0;
    document.getElementById('nextFeedBtn').disabled = currentFeedIndex === feedPosts.length - 1;
}

async function likePost(postId) {
    if (!currentUser) return;
    
    try {
        const response = await fetch(`${API_BASE}/like`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                post_id: postId,
                user_id: currentUser.id
            })
        });
        
        const result = await response.json();
        
        const post = feedPosts.find(p => p.id === postId);
        if (post) {
            post.liked = result.liked;
            post.likes = result.count;
            displayCurrentFeedPost();
        }
    } catch (error) {
        console.error('Like error:', error);
        showToast('Failed to like post', 'error');
    }
}

async function deletePost(postId, event) {
    if (event) event.stopPropagation();
    
    if (!confirm('Delete this post permanently?')) return;
    
    try {
        const response = await fetch(`${API_BASE}/posts/${postId}`, {
            method: 'DELETE',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ user_id: currentUser.id })
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('Post deleted', 'success');
            
            // Remove from feed
            feedPosts = feedPosts.filter(p => p.id !== postId);
            if (currentFeedIndex >= feedPosts.length) {
                currentFeedIndex = feedPosts.length - 1;
            }
            displayCurrentFeedPost();
            
            // Refresh user posts
            loadUserPosts();
        } else {
            showToast(result.message || 'Failed to delete post', 'error');
        }
    } catch (error) {
        console.error('Delete error:', error);
        showToast('Server error', 'error');
    }
}

function nextFeedPost() {
    if (currentFeedIndex < feedPosts.length - 1) {
        currentFeedIndex++;
        displayCurrentFeedPost();
    }
}

function previousFeedPost() {
    if (currentFeedIndex > 0) {
        currentFeedIndex--;
        displayCurrentFeedPost();
    }
}

function refreshFeed() {
    loadFeed();
    showToast('Feed refreshed', 'success');
}

// ========== POST FUNCTIONS ==========
function showCreatePostModal() {
    document.getElementById('newPostCaption').value = '';
    document.getElementById('createPostModal').classList.add('show');
    setTimeout(() => document.getElementById('newPostCaption').focus(), 100);
}

async function createPost() {
    const caption = document.getElementById('newPostCaption').value.trim();
    
    if (!caption) {
        showToast('Please enter a caption', 'error');
        return;
    }
    
    const createBtn = document.querySelector('#createPostModal .btn-primary');
    const originalText = createBtn.innerHTML;
    createBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Posting...';
    createBtn.disabled = true;
    
    try {
        const response = await fetch(`${API_BASE}/posts`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                user_id: currentUser.id,
                caption: caption
            })
        });
        
        const result = await response.json();
        
        if (result.success) {
            closeModal('createPostModal');
            showToast('Post created successfully!', 'success');
            loadFeed();
            loadUserPosts();
        } else {
            showToast(result.message || 'Failed to create post', 'error');
        }
    } catch (error) {
        console.error('Create post error:', error);
        showToast('Server error', 'error');
    } finally {
        createBtn.innerHTML = originalText;
        createBtn.disabled = false;
    }
}

async function loadUserPosts() {
    if (!currentUser) return;
    
    try {
        const response = await fetch(`${API_BASE}/user/${currentUser.id}/posts`);
        const posts = await response.json();
        userPosts = posts;
        displayUserPosts();
    } catch (error) {
        console.error('Load user posts error:', error);
    }
}

function displayUserPosts() {
    const grid = document.getElementById('postsGrid');
    
    if (userPosts.length === 0) {
        grid.innerHTML = `
            <div class="empty-state">
                <i class="fas fa-images"></i>
                <h3>No posts yet</h3>
                <p>Share your first post with the community!</p>
                <button class="btn-primary" onclick="showCreatePostModal()">
                    <i class="fas fa-plus"></i> Create Post
                </button>
            </div>
        `;
        return;
    }
    
    grid.innerHTML = userPosts.map(post => `
        <div class="post-card" data-post-id="${post.postId}">
            <div class="post-card-header">
                <span class="post-date">Post #${post.postId}</span>
                <button class="btn-icon btn-delete" onclick="deletePost(${post.postId}, event)" title="Delete">
                    <i class="fas fa-trash"></i>
                </button>
            </div>
            <div class="post-card-body">
                <p>${post.caption}</p>
            </div>
            <div class="post-card-footer">
                <span><i class="fas fa-heart"></i> ${post.likes}</span>
                <span><i class="fas fa-comment"></i> 0</span>
            </div>
        </div>
    `).join('');
}

// ========== COMMENT FUNCTIONS ==========
async function showComments(postId) {
    showLoading(true);
    
    try {
        const response = await fetch(`${API_BASE}/posts/${postId}/comments`);
        const comments = await response.json();
        
        const commentsList = document.getElementById('commentsList');
        
        if (comments.length === 0) {
            commentsList.innerHTML = '<p class="empty-comments">No comments yet. Be the first to comment!</p>';
        } else {
            commentsList.innerHTML = comments.map(comment => `
                <div class="comment-item">
                    <div class="comment-header">
                        <span class="comment-author">
                            <i class="fas fa-user-circle"></i> ${comment.username}
                        </span>
                        <span class="comment-time">${formatTime(comment.timestamp)}</span>
                    </div>
                    <div class="comment-text">${comment.text}</div>
                </div>
            `).join('');
        }
        
        document.getElementById('commentModal').setAttribute('data-post-id', postId);
        document.getElementById('commentText').value = '';
        document.getElementById('commentModal').classList.add('show');
    } catch (error) {
        console.error('Load comments error:', error);
        showToast('Failed to load comments', 'error');
    } finally {
        showLoading(false);
    }
}

async function addComment() {
    const commentText = document.getElementById('commentText').value.trim();
    const postId = parseInt(document.getElementById('commentModal').getAttribute('data-post-id'));
    
    if (!commentText) {
        showToast('Please enter a comment', 'error');
        return;
    }
    
    const commentBtn = document.querySelector('#commentModal .btn-primary');
    const originalText = commentBtn.innerHTML;
    commentBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Posting...';
    commentBtn.disabled = true;
    
    try {
        const response = await fetch(`${API_BASE}/comments`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                post_id: postId,
                user_id: currentUser.id,
                text: commentText
            })
        });
        
        const result = await response.json();
        
        if (result.success) {
            document.getElementById('commentText').value = '';
            showToast('Comment added!', 'success');
            
            // Refresh comments
            await showComments(postId);
            
            // Update feed if needed
            const post = feedPosts.find(p => p.id === postId);
            if (post) {
                post.comments++;
                if (currentFeedIndex >= 0 && feedPosts[currentFeedIndex]?.id === postId) {
                    displayCurrentFeedPost();
                }
            }
        } else {
            showToast(result.message || 'Failed to add comment', 'error');
        }
    } catch (error) {
        console.error('Add comment error:', error);
        showToast('Server error', 'error');
    } finally {
        commentBtn.innerHTML = originalText;
        commentBtn.disabled = false;
    }
}

function formatTime(timestamp) {
    const now = Math.floor(Date.now() / 1000);
    const diff = now - timestamp;
    
    if (diff < 60) return 'just now';
    if (diff < 3600) return Math.floor(diff / 60) + 'm ago';
    if (diff < 86400) return Math.floor(diff / 3600) + 'h ago';
    if (diff < 604800) return Math.floor(diff / 86400) + 'd ago';
    return new Date(timestamp * 1000).toLocaleDateString();
}

// ========== STORIES FUNCTIONS ==========
function showCreateStoryModal() {
    document.getElementById('storyCaption').value = '';
    document.getElementById('createStoryModal').classList.add('show');
    setTimeout(() => document.getElementById('storyCaption').focus(), 100);
}

async function createStory() {
    const caption = document.getElementById('storyCaption').value.trim();
    
    if (!caption) {
        showToast('Please enter a caption', 'error');
        return;
    }
    
    const createBtn = document.querySelector('#createStoryModal .btn-primary');
    const originalText = createBtn.innerHTML;
    createBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Adding...';
    createBtn.disabled = true;
    
    try {
        const response = await fetch(`${API_BASE}/stories`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                user_id: currentUser.id,
                caption: caption
            })
        });
        
        const result = await response.json();
        
        if (result.success) {
            closeModal('createStoryModal');
            showToast('Story added! It will expire in 24 hours', 'success');
            loadStories();
        } else {
            showToast(result.message || 'Failed to add story', 'error');
        }
    } catch (error) {
        console.error('Create story error:', error);
        showToast('Server error', 'error');
    } finally {
        createBtn.innerHTML = originalText;
        createBtn.disabled = false;
    }
}

async function loadStories() {
    if (!currentUser) return;
    
    console.log("📖 Loading stories for user:", currentUser.id);
    showToast('Loading stories...', 'info');
    
    try {
        // Load my stories
        const myResponse = await fetch(`${API_BASE}/user/${currentUser.id}/my-stories`);
        const myStories = await myResponse.json();
        console.log("My stories response:", myStories);
        
        // Load followed stories
        const followedResponse = await fetch(`${API_BASE}/user/${currentUser.id}/followed-stories`);
        const followedStories = await followedResponse.json();
        console.log("Followed stories response:", followedStories);
        
        // Store in global variable
        stories.my = Array.isArray(myStories) ? myStories : [];
        stories.followed = Array.isArray(followedStories) ? followedStories : [];
        
        displayStories();
    } catch (error) {
        console.error('Load stories error:', error);
        showToast('Failed to load stories', 'error');
    }
}

function displayStories() {
    console.log("📝 Displaying stories");
    const myStoriesList = document.getElementById('myStoriesList');
    const followedStoriesList = document.getElementById('followedStoriesList');
    
    // My stories
    if (!stories.my || stories.my.length === 0) {
        myStoriesList.innerHTML = `
            <div class="empty-mini">
                <i class="fas fa-story"></i>
                <p>No stories yet</p>
                <button class="btn-primary btn-small" onclick="showCreateStoryModal()">
                    <i class="fas fa-plus"></i> Add Your First Story
                </button>
            </div>
        `;
    } else {
        let html = '';
        stories.my.forEach(story => {
            const timeAgo = getTimeAgo(story.timestamp);
            html += `
                <div class="story-item">
                    <div class="story-info">
                        <i class="fas fa-quote-right story-icon"></i>
                        <div class="story-details">
                            <span class="story-caption">${story.caption || 'My story'}</span>
                            <span class="story-time">${timeAgo}</span>
                            ${!story.is_seen ? '<span class="badge-new">New</span>' : ''}
                        </div>
                    </div>
                    <button class="btn-delete-small" onclick="deleteStory(${story.storyId})" title="Delete story">
                        <i class="fas fa-trash"></i>
                    </button>
                </div>
            `;
        });
        myStoriesList.innerHTML = html;
    }
    
    // Followed stories
    if (!stories.followed || stories.followed.length === 0) {
        followedStoriesList.innerHTML = `
            <div class="empty-mini">
                <i class="fas fa-story"></i>
                <p>No stories from people you follow</p>
                <button class="btn-primary btn-small" onclick="showSection('search', {currentTarget: document.querySelector('[onclick*=\"search\"]')})">
                    <i class="fas fa-search"></i> Find People to Follow
                </button>
            </div>
        `;
    } else {
        let html = '';
        stories.followed.forEach(story => {
            const timeAgo = getTimeAgo(story.timestamp);
            html += `
                <div class="story-item">
                    <div class="story-info">
                        <i class="fas fa-quote-right story-icon"></i>
                        <div class="story-details">
                            <span class="story-username">${story.username}</span>
                            <span class="story-caption">${story.caption || 'New story'}</span>
                            <span class="story-time">${timeAgo}</span>
                        </div>
                    </div>
                    ${!story.is_seen ? 
                        `<button class="btn-primary btn-small" onclick="markStorySeen(${story.storyId}, ${story.userId})">
                            <i class="fas fa-check"></i> Mark Seen
                        </button>` : 
                        '<span class="badge-seen"><i class="fas fa-check-circle"></i> Seen</span>'
                    }
                </div>
            `;
        });
        followedStoriesList.innerHTML = html;
    }
}

function getTimeAgo(timestamp) {
    const now = Math.floor(Date.now() / 1000);
    const diff = now - timestamp;
    
    if (diff < 60) return 'just now';
    if (diff < 3600) return Math.floor(diff / 60) + ' minutes ago';
    if (diff < 86400) return Math.floor(diff / 3600) + ' hours ago';
    return Math.floor(diff / 86400) + ' days ago';
}

async function markStorySeen(storyId, storyUserId) {
    try {
        const response = await fetch(`${API_BASE}/stories/${storyId}/seen`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ user_id: currentUser.id })
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('Story marked as seen', 'success');
            await loadStories();
        }
    } catch (error) {
        console.error('Mark story seen error:', error);
        showToast('Server error', 'error');
    }
}

async function deleteStory(storyId) {
    if (!confirm('Delete this story?')) return;
    
    try {
        const response = await fetch(`${API_BASE}/stories/${storyId}`, {
            method: 'DELETE',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ user_id: currentUser.id })
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('Story deleted', 'success');
            await loadStories();
        }
    } catch (error) {
        console.error('Delete story error:', error);
        showToast('Server error', 'error');
    }
}

// ========== SEARCH FUNCTIONS ==========
async function searchUsers() {
    const query = document.getElementById('searchInput').value.trim();
    
    if (query.length < 2) {
        document.getElementById('searchResults').innerHTML = '';
        return;
    }
    
    const resultsDiv = document.getElementById('searchResults');
    resultsDiv.innerHTML = '<div class="loading-spinner"><i class="fas fa-spinner fa-spin"></i> Searching...</div>';
    
    try {
        const response = await fetch(`${API_BASE}/search/${encodeURIComponent(query)}`);
        const users = await response.json();
        displaySearchResults(users);
    } catch (error) {
        console.error('Search error:', error);
        resultsDiv.innerHTML = '<div class="error-message">Search failed</div>';
    }
}

function displaySearchResults(users) {
    const resultsDiv = document.getElementById('searchResults');
    
    if (users.length === 0) {
        resultsDiv.innerHTML = '<div class="empty-state"><i class="fas fa-search"></i><p>No users found</p></div>';
        return;
    }
    
    resultsDiv.innerHTML = users.map(user => `
        <div class="search-result-item">
            <div class="user-info-compact">
                <i class="fas fa-user-circle"></i>
                <div class="user-details-compact">
                    <h4>${user.username}</h4>
                    <p>ID: ${user.userId}</p>
                </div>
            </div>
            ${user.userId !== currentUser.id ? 
                `<button class="btn-primary" onclick="followUser(${user.userId})">
                    <i class="fas fa-user-plus"></i> Follow
                </button>` : 
                '<span class="badge-current">You</span>'
            }
        </div>
    `).join('');
}

// ========== FOLLOW FUNCTIONS ==========
function showFollowModal() {
    document.getElementById('followUserId').value = '';
    document.getElementById('followModal').classList.add('show');
    setTimeout(() => document.getElementById('followUserId').focus(), 100);
}

async function followUser(userId) {
    if (!currentUser) return;
    
    if (userId === currentUser.id) {
        showToast('You cannot follow yourself', 'error');
        return;
    }
    
    closeModal('followModal');
    
    try {
        const response = await fetch(`${API_BASE}/follow`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                follower_id: currentUser.id,
                following_id: userId
            })
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('Now following user!', 'success');
            loadFollowing();
            loadFeed();
            
            const searchQuery = document.getElementById('searchInput').value;
            if (searchQuery) searchUsers();
        } else {
            showToast(result.message || 'Failed to follow user', 'error');
        }
    } catch (error) {
        console.error('Follow error:', error);
        showToast('Server error', 'error');
    }
}

async function followUserById() {
    const userId = parseInt(document.getElementById('followUserId').value);
    
    if (isNaN(userId)) {
        showToast('Please enter a valid User ID', 'error');
        return;
    }
    
    await followUser(userId);
}

async function unfollowUser(userId) {
    if (!currentUser) return;
    
    if (!confirm('Unfollow this user?')) return;
    
    try {
        const response = await fetch(`${API_BASE}/unfollow`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                follower_id: currentUser.id,
                following_id: userId
            })
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('Unfollowed user', 'success');
            loadFollowing();
            loadFeed();
        } else {
            showToast(result.message || 'Failed to unfollow', 'error');
        }
    } catch (error) {
        console.error('Unfollow error:', error);
        showToast('Server error', 'error');
    }
}

async function loadFollowing() {
    if (!currentUser) return;
    
    try {
        const [followingResponse, followersResponse] = await Promise.all([
            fetch(`${API_BASE}/user/${currentUser.id}/following`),
            fetch(`${API_BASE}/user/${currentUser.id}/followers`)
        ]);
        
        const following = await followingResponse.json();
        const followers = await followersResponse.json();
        
        displayFollowing(following, followers);
    } catch (error) {
        console.error('Load following error:', error);
    }
}

function displayFollowing(following, followers) {
    const followingList = document.getElementById('followingList');
    const followersList = document.getElementById('followersList');
    
    // Following
    if (following.length === 0) {
        followingList.innerHTML = `
            <div class="empty-state">
                <i class="fas fa-users"></i>
                <h3>Not following anyone</h3>
                <p>Find users to follow and see their posts!</p>
                <button class="btn-primary" onclick="showSection('search', {currentTarget: document.querySelector('[onclick*=\"search\"]')})">
                    <i class="fas fa-search"></i> Search Users
                </button>
            </div>
        `;
    } else {
        followingList.innerHTML = following.map(user => `
            <div class="user-item">
                <div class="user-info-compact">
                    <i class="fas fa-user-circle"></i>
                    <div>
                        <h4>${user.username}</h4>
                        <p>ID: ${user.userId}</p>
                    </div>
                </div>
                <button class="btn-delete" onclick="unfollowUser(${user.userId})">
                    <i class="fas fa-user-minus"></i> Unfollow
                </button>
            </div>
        `).join('');
    }
    
    // Followers
    if (followers.length === 0) {
        followersList.innerHTML = `
            <div class="empty-state">
                <i class="fas fa-users"></i>
                <h3>No followers yet</h3>
                <p>Share your posts to attract followers!</p>
                <button class="btn-primary" onclick="showCreatePostModal()">
                    <i class="fas fa-plus"></i> Create Post
                </button>
            </div>
        `;
    } else {
        followersList.innerHTML = followers.map(user => `
            <div class="user-item">
                <div class="user-info-compact">
                    <i class="fas fa-user-circle"></i>
                    <div>
                        <h4>${user.username}</h4>
                        <p>ID: ${user.userId}</p>
                    </div>
                </div>
                ${!following.some(f => f.userId === user.userId) ? 
                    `<button class="btn-primary" onclick="followUser(${user.userId})">
                        <i class="fas fa-user-plus"></i> Follow Back
                    </button>` : 
                    '<span class="badge-following">Following</span>'
                }
            </div>
        `).join('');
    }
}

function switchTab(tab, event) {
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    event.currentTarget.classList.add('active');
    
    document.getElementById('followingList').style.display = tab === 'following' ? 'block' : 'none';
    document.getElementById('followersList').style.display = tab === 'followers' ? 'block' : 'none';
}

// ========== PROFILE FUNCTIONS ==========
async function loadProfile() {
    if (!currentUser) return;
    
    try {
        const response = await fetch(`${API_BASE}/user/${currentUser.id}/stats`);
        const stats = await response.json();
        
        document.getElementById('statPosts').textContent = stats.posts || 0;
        document.getElementById('statFollowers').textContent = stats.followers || 0;
        document.getElementById('statFollowing').textContent = stats.following || 0;
        document.getElementById('statLikes').textContent = stats.totalLikes || 0;
    } catch (error) {
        console.error('Load profile error:', error);
    }
}

// ========== LIKED POSTS ==========
async function loadLikedPosts() {
    if (!currentUser) return;
    
    const container = document.getElementById('likedPostsList');
    container.innerHTML = '<div class="loading-spinner"><i class="fas fa-spinner fa-spin"></i> Loading...</div>';
    
    try {
        const response = await fetch(`${API_BASE}/user/${currentUser.id}/liked-posts`);
        const posts = await response.json();
        displayLikedPosts(posts);
    } catch (error) {
        console.error('Load liked posts error:', error);
        container.innerHTML = '<div class="error-message">Failed to load liked posts</div>';
    }
}

function displayLikedPosts(posts) {
    const container = document.getElementById('likedPostsList');
    
    if (posts.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <i class="fas fa-heart"></i>
                <h3>No liked posts yet</h3>
                <p>Like posts to see them here!</p>
                <button class="btn-primary" onclick="showSection('feed', {currentTarget: document.querySelector('[onclick*=\"feed\"]')})">
                    <i class="fas fa-home"></i> Go to Feed
                </button>
            </div>
        `;
        return;
    }
    
    container.innerHTML = posts.map(post => `
        <div class="liked-post-item">
            <div class="liked-post-header">
                <span class="post-author">
                    <i class="fas fa-user"></i> ${post.username}
                </span>
                <span class="post-likes">
                    <i class="fas fa-heart"></i> ${post.likes}
                </span>
            </div>
            <div class="liked-post-caption">
                ${post.caption}
            </div>
            <div class="liked-post-footer">
                <button class="btn-primary btn-small" onclick="showComments(${post.postId})">
                    <i class="fas fa-comment"></i> Comment
                </button>
            </div>
        </div>
    `).join('');
}

// ========== MODAL FUNCTIONS ==========
function closeModal(modalId) {
    document.getElementById(modalId).classList.remove('show');
}

// ========== STORIES VIEWER WITH NEXT/PREV BUTTONS ==========
let currentStories = [];

async function showStoriesViewer() {
    if (!currentUser) return;
    
    showToast('Loading stories into circular queue...', 'info');
    
    try {
        const response = await fetch(`${API_BASE}/stories/queue`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ user_id: currentUser.id })
        });
        
        currentStories = await response.json();
        console.log("📚 Stories loaded:", currentStories);
        
        if (currentStories.length === 0) {
            showToast('No stories to show!', 'error');
            return;
        }
        
        currentStoryIndex = 0;
        showDSAExplanation();
        
    } catch (error) {
        console.error('Error loading stories:', error);
        showToast('Failed to load stories', 'error');
    }
}

function showDSAExplanation() {
    const modal = document.getElementById('dsaModal');
    modal.innerHTML = `
        <div class="modal-content" style="max-width: 500px;">
            <div class="modal-header">
                <h3><i class="fas fa-project-diagram"></i> Double Circular Queue</h3>
                <button class="close-btn" onclick="document.getElementById('dsaModal').classList.remove('show')">&times;</button>
            </div>
            <div class="modal-body">
                <p>Stories are stored in a <strong>Double Circular Linked List</strong>:</p>
                <ul style="margin: 15px 0 15px 20px;">
                    <li>Each story = Node with next/prev pointers</li>
                    <li>Last node → First node (circular)</li>
                    <li>First node ← Last node (double linked)</li>
                    <li>Navigation: O(1) time complexity</li>
                </ul>
                <div style="text-align: center; margin: 20px 0;">
                    <div style="display: flex; justify-content: center; gap: 5px; flex-wrap: wrap;">
                        ${currentStories.map((_, i) => `
                            <div style="width: 40px; height: 40px; background: ${i === 0 ? '#1877f2' : '#e4e6eb'}; 
                                 color: ${i === 0 ? 'white' : 'black'}; border-radius: 50%; display: flex; 
                                 align-items: center; justify-content: center; font-weight: bold;">
                                ${i+1}
                            </div>
                        `).join('')}
                    </div>
                    <div style="margin-top: 10px; color: #65676b;">
                        ⟲ Last connects to First (Circular)
                    </div>
                </div>
                <button class="btn-primary" onclick="startStoryViewer(); document.getElementById('dsaModal').classList.remove('show')">
                    Start Stories <i class="fas fa-play"></i>
                </button>
            </div>
        </div>
    `;
    modal.classList.add('show');
}

function startStoryViewer() {
    showStory(currentStoryIndex);
}

function showStory(index) {
    if (index >= currentStories.length) {
        showToast('✨ All stories viewed! Back to stories.', 'success');
        showSection('stories');
        return;
    }
    
    const story = currentStories[index];
    const viewer = document.getElementById('storyViewer');
    
    // Calculate if prev/next buttons should be enabled
    const hasPrev = index > 0;
    const hasNext = index < currentStories.length - 1;
    
    viewer.innerHTML = `
        <div class="modal-content" style="max-width: 500px; background: linear-gradient(135deg, #1877f2, #9b59b6); color: white;">
            <div class="modal-header" style="border-bottom-color: rgba(255,255,255,0.2);">
                <div style="display: flex; align-items: center; gap: 10px;">
                    <i class="fas fa-user-circle" style="font-size: 2rem;"></i>
                    <div>
                        <h3 style="color: white; margin:0;">${story.username || 'User'}</h3>
                        <small>${getTimeAgo(story.timestamp)}</small>
                    </div>
                </div>
                <button class="close-btn" style="color: white;" onclick="this.closest('.modal').classList.remove('show'); showSection('stories')">&times;</button>
            </div>
            
            <div class="modal-body" style="min-height: 200px; display: flex; align-items: center; justify-content: center; flex-direction: column;">
                <p style="font-size: 1.5rem; text-align: center; margin-bottom: 20px;">${story.caption || 'No caption'}</p>
                
                <!-- Circular Queue Visualization -->
                <div style="background: rgba(255,255,255,0.1); padding: 10px; border-radius: 10px; width: 100%; margin-top: 10px;">
                    <div style="display: flex; justify-content: space-between; align-items: center; font-size: 0.9rem;">
                        <span>← Prev</span>
                        <span>Node ${index + 1}/${currentStories.length}</span>
                        <span>Next →</span>
                    </div>
                    <div style="display: flex; justify-content: center; gap: 5px; margin-top: 10px;">
                        ${currentStories.map((_, i) => `
                            <div style="width: 30px; height: 30px; background: ${i === index ? '#ffc107' : 'rgba(255,255,255,0.3)'}; 
                                 border-radius: 50%; display: flex; align-items: center; justify-content: center; 
                                 font-weight: bold; font-size: 0.8rem;">
                                ${i+1}
                            </div>
                        `).join('')}
                    </div>
                    <div style="text-align: center; margin-top: 5px; font-size: 0.8rem; opacity: 0.8;">
                        ⭕ Circular Queue (Last → First)
                    </div>
                </div>
            </div>
            
            <div class="modal-footer" style="border-top-color: rgba(255,255,255,0.2); justify-content: space-between; display: flex; gap: 10px;">
                <button class="nav-btn" style="flex: 1; background: rgba(255,255,255,0.2); color: white; border: none; padding: 12px; border-radius: 30px; cursor: pointer; font-weight: 600;" 
                        onclick="navigateStory('prev')" ${!hasPrev ? 'disabled' : ''}>
                    <i class="fas fa-chevron-left"></i> Previous
                </button>
                <button class="nav-btn" style="flex: 1; background: rgba(255,255,255,0.2); color: white; border: none; padding: 12px; border-radius: 30px; cursor: pointer; font-weight: 600;" 
                        onclick="navigateStory('next')">
                    ${hasNext ? 'Next <i class="fas fa-chevron-right"></i>' : 'Finish <i class="fas fa-check"></i>'}
                </button>
            </div>
            
            <!-- DSA Note -->
            <div style="padding: 10px; text-align: center; font-size: 0.8rem; background: rgba(0,0,0,0.1); border-radius: 0 0 12px 12px;">
                <i class="fas fa-project-diagram"></i> Double Circular Queue: next & prev pointers O(1)
            </div>
        </div>
    `;
    
    viewer.classList.add('show');
}

async function navigateStory(direction) {
    const currentId = currentStories[currentStoryIndex].storyId;
    
    if (direction === 'next') {
        if (currentStoryIndex === currentStories.length - 1) {
            document.getElementById('storyViewer').classList.remove('show');
            showToast('🎉 Circular queue complete! Back to stories.', 'success');
            showSection('stories');
            return;
        }
        
        try {
            const response = await fetch(`${API_BASE}/stories/next`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ current_story_id: currentId })
            });
            
            const nextStory = await response.json();
            if (!nextStory.error) {
                currentStoryIndex++;
                showStory(currentStoryIndex);
            }
        } catch (error) {
            currentStoryIndex++;
            showStory(currentStoryIndex);
        }
        
    } else if (direction === 'prev' && currentStoryIndex > 0) {
        try {
            const response = await fetch(`${API_BASE}/stories/prev`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ current_story_id: currentId })
            });
            
            const prevStory = await response.json();
            if (!prevStory.error) {
                currentStoryIndex--;
                showStory(currentStoryIndex);
            }
        } catch (error) {
            currentStoryIndex--;
            showStory(currentStoryIndex);
        }
    }
}

// Add button to stories section
const originalDisplayStories = displayStories;
displayStories = function() {
    originalDisplayStories();
    
    const storiesSection = document.getElementById('storiesSection');
    const existingBtn = document.getElementById('storiesViewerBtn');
    
    if (!existingBtn && storiesSection) {
        const btnDiv = document.createElement('div');
        btnDiv.className = 'stories-action-bar';
        btnDiv.innerHTML = `
            <button id="storiesViewerBtn" class="btn-large" onclick="showStoriesViewer()" style="padding: 15px 30px; font-size: 1.2rem; background: linear-gradient(135deg, #1877f2, #9b59b6); color: white; border: none; border-radius: 50px; cursor: pointer; margin: 20px 0; box-shadow: 0 4px 15px rgba(0,0,0,0.2);">
                <i class="fas fa-play-circle"></i> Show All Stories (Circular Queue Demo)
            </button>
        `;
        storiesSection.insertBefore(btnDiv, storiesSection.firstChild);
    }
};