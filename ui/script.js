function loadVideo() {
    const url = document.getElementById("ytLink").value.trim();

    // Biến để chứa ID video
    let videoId = "";

    // --- Cách 1: Link dạng "https://www.youtube.com/watch?v=XXXX"
    const match1 = url.match(/v=([^&]+)/);
    if (match1) {
        videoId = match1[1];
    }

    // --- Cách 2: Link dạng "https://youtu.be/XXXX"
    const match2 = url.match(/youtu\.be\/([^?]+)/);
    if (match2) {
        videoId = match2[1];
    }

    // Nếu không tìm thấy ID hợp lệ
    if (!videoId) {
        alert("Tính năng xem trước chỉ hỗ trợ Video Youtube! Vui lòng kiểm tra lại đường dẫn!");
        return;
    }

    // Tạo link embed
    const embedUrl = `https://www.youtube.com/embed/${videoId}`;

    // Gán vào iframe
    document.getElementById("ytFrame").src = embedUrl;
}

document.getElementById("ytLink").addEventListener("keydown", function(event) {
    if (event.key === "Enter") {
        event.preventDefault(); // Ngăn form submit mặc định
        loadVideo();            // Gọi hàm loadVideo
    }
});


// Console ảo
function appendConsole(text, type = 'output') {
    const consoleEl = document.getElementById('console');
    const line = document.createElement('div');
    line.className = 'console-line console-' + type;
    line.textContent = text;
    consoleEl.appendChild(line);
    consoleEl.scrollTop = consoleEl.scrollHeight;
}

function clearConsole() {
    document.getElementById('console').innerHTML = '';
    appendConsole('Console cleared.', 'info');
}

async function stopProcess() {
    try {
        await stopCurrentProcess();
    } catch(e) {
        appendConsole('Error stopping process: ' + e.toString(), 'error');
    }
}

async function executeCmd() {
    const command = document.getElementById('commandInput').value.trim();
    
    if (!command) {
        appendConsole('Error: Please enter a command', 'error');
        return;
    }
    
    appendConsole('DEBUG: Attempting to call runCommand...', 'info');
    appendConsole('DEBUG: runCommand type = ' + typeof runCommand, 'info');
    
    try {
        const result = await runCommand(command);
        appendConsole('DEBUG: Backend returned: ' + JSON.stringify(result), 'info');
    } catch(e) {
        appendConsole('Error calling backend: ' + e.toString(), 'error');
        appendConsole('Stack: ' + e.stack, 'error');
    }
}

// Enter key support
document.getElementById('commandInput').addEventListener('keypress', function(e) {
    if (e.key === 'Enter') executeCmd();
});

// Welcome message
appendConsole('YouTube Downloader - chickenizdabest', 'info');
appendConsole('Nhập lệnh ở bên dưới và nhấn Enter để thực thi.', 'info');