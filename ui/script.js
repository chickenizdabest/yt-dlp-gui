function updateLinkPlatformOption() {
    console.log("Dang kiem tra link");
    const root = document.documentElement;
    const patterns = {
      youtube: /^(https?:\/\/)?(www\.)?(youtube\.com|youtu\.be)\//i,
      facebook: /^(https?:\/\/)?(www\.)?(facebook\.com|fb\.watch)\//i,
      tiktok: /^(https?:\/\/)?(www\.)?tiktok\.com\//i,
      bilibili: /^(https?:\/\/)?(www\.)?bilibili\.com\//i,
      youtube2: /youtu\.be\/([^?]+)/i
    };
    const url = document.getElementById("link").value.trim();
    if (patterns.youtube.test(url)) {
        root.style.setProperty('--link-platform', '#ff0000');
        root.style.setProperty('--link-platform-muted', '#ff0000');
        document.getElementById("platformIcon").style.clipPath = "inset(0 0 0 0)";
        document.getElementById("gradientBorder").style.opacity = "0";
        document.getElementById("yt").style.opacity = "1";
        document.getElementById("fb").style.opacity = "0";
        document.getElementById("tt").style.opacity = "0";
        return 1;
    }
    else if (patterns.facebook.test(url)) {
        root.style.setProperty('--link-platform', '#17A9FD');
        root.style.setProperty('--link-platform-muted', '#17A9FD');
        document.getElementById("platformIcon").style.clipPath = "inset(0 0 0 0)";
        document.getElementById("gradientBorder").style.opacity = "0";
        document.getElementById("fb").style.opacity = "1";
        document.getElementById("yt").style.opacity = "0";
        document.getElementById("tt").style.opacity = "0";
        return 2;
    }
    else if (patterns.tiktok.test(url)) {
        root.style.setProperty('--link-platform', '#FE2C55');
        root.style.setProperty('--link-platform-muted', '#FE2C55');
        document.getElementById("gradientBorder").style.opacity = "1";
        document.getElementById("platformIcon").style.clipPath = "inset(0 0 0 0)";
        document.getElementById("tt").style.opacity = "1";
        document.getElementById("yt").style.opacity = "0";
        document.getElementById("fb").style.opacity = "0";
        return 3;
    }
    else if (patterns.bilibili.test(url)) {
        return 4;
    }
    else if (patterns.youtube2.test(url)) {
        root.style.setProperty('--link-platform', '#ff0000');
        root.style.setProperty('--link-platform-muted', '#ff0000');
        document.getElementById("platformIcon").style.clipPath = "inset(0 0 0 0)";
        document.getElementById("gradientBorder").style.opacity = "0";
        document.getElementById("yt").style.opacity = "1";
        document.getElementById("fb").style.opacity = "0";
        document.getElementById("tt").style.opacity = "0";
        return 1;
    }
    else {
        root.style.setProperty('--link-platform', 'oklch(0.76 0.1 224)');
        root.style.setProperty('--link-platform-muted', 'oklch(0.4 0 224)');
        document.getElementById("gradientBorder").style.opacity = "0";
        document.getElementById("platformIcon").style.clipPath = "inset(0 0 0 100%)";
        document.getElementById("yt").style.opacity = "0";
        document.getElementById("fb").style.opacity = "0";
        document.getElementById("tt").style.opacity = "0";
        return 0;
    };
}

document.getElementById("link").addEventListener("input", updateLinkPlatformOption);

function loadVideo() {
    const url = document.getElementById("link").value.trim();
    let videoId = "";

    const match1 = url.match(/v=([^&]+)/);
    if (match1) {
        videoId = match1[1];
    }

    const match2 = url.match(/youtu\.be\/([^?]+)/);
    if (match2) {
        videoId = match2[1];
    }

    if (!videoId) {
        alert("Tính năng xem trước chỉ hỗ trợ Video Youtube! Vui lòng kiểm tra lại đường dẫn!");
        return;
    }

    const embedUrl = `https://www.youtube.com/embed/${videoId}`;
    document.getElementById("ytFrame").src = embedUrl;
}

document.getElementById("link").addEventListener("keydown", function(event) {
    if (event.key === "Enter") {
        event.preventDefault();
        loadVideo();
    }
});

const pendingCallbacks = new Map();

window.__commandComplete = function(callbackId, exitCode) {
    const callback = pendingCallbacks.get(callbackId);
    if (callback) {
        callback(exitCode);
        pendingCallbacks.delete(callbackId);
    }
};

function appendConsoleBase64(base64Text, type = 'output') {
    // Decode base64
    const text = decodeBase64(base64Text);
    
    const consoleEl = document.getElementById('console');
    const line = document.createElement('div');
    line.className = 'console-line console-' + type;
    line.textContent = text;
    consoleEl.appendChild(line);
    consoleEl.scrollTop = consoleEl.scrollHeight;
}

function decodeBase64(base64) {
    try {
        // Decode base64 to binary string
        const binaryString = atob(base64);
        // Convert binary string to UTF-8
        const bytes = new Uint8Array(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) {
            bytes[i] = binaryString.charCodeAt(i);
        }
        // Decode UTF-8 bytes to string
        return new TextDecoder('utf-8').decode(bytes);
    } catch (e) {
        console.error('Base64 decode error:', e);
        return base64; // Fallback to original if decode fails
    }
}

// Giữ nguyên hàm appendConsole cũ để tương thích
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
    appendConsole('Đã xoá sạch bảng điều khiển.', 'info');
}

async function stopProcess() {
    try {
        await stopCurrentProcess();
    } catch(e) {
        appendConsole('Có lỗi xảy ra khi dừng quá trình: ' + e.toString(), 'error');
    }
}

async function executeCmd() {
    const command = document.getElementById('commandInput').value.trim();
    if (!command) {
        appendConsole('Lỗi: Vui lòng nhập lệnh', 'error');
        return;
    }
    
    const callbackId = 'cmd_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
    
    try {
        await runCommand(command, callbackId);
        
        const exitCode = await new Promise((resolve) => {
            pendingCallbacks.set(callbackId, resolve);
        });
        appendConsole(`Lệnh hoàn thành với exit code: ${exitCode}`, 'info');
        
        if (exitCode === 0) {
            appendConsole('Thành công!', 'info');
        }
        
        return exitCode;
        
    } catch(e) {
        appendConsole('Có lỗi: ' + e.toString(), 'error');
        return -1;
    }
    
}

document.getElementById('commandInput').addEventListener('keypress', function(e) {
  document.addEventListener('keydown', function (e) {
    const isEnter = e.key === 'Enter' || e.keyCode === 13;
    if (isEnter) {
      e.preventDefault();
    }
  });
});

appendConsole('YouTube Downloader - chickenizdabest', 'info');
appendConsole('Nhập lệnh ở bên dưới và nhấn Enter để thực thi.', 'info');

window.addEventListener('load', function() {
    var Spinner = function (el) {
        this.svg = el;
        this.timing = .3;
        this.direction = 0;
        this.timelineEnd = 0;
        this.timeline = new TimelineMax({
            paused: true,
            ease: Bounce.easeOut,
            yoyo: true,
            repeat: -1,
            onStart: this.startRepeat.bind(this),
            onRepeat: this.startRepeat.bind(this)
        });

        this.polygon = new Polygon({ el: this.svg.querySelector('#polygon'), parent: this.svg });
        this.checkmark = this.svg.querySelector('#checkmark');

        this.setupTimeline();
    };

    Spinner.prototype.startRepeat = function (el) {
        // Kiểm tra xem có cần dừng không TRƯỚC KHI bắt đầu animation mới
        if (this.timelineEnd) {
            this.timeline.pause();
            this.fadeOut();
            return;
        }
        
        if (this.direction) {
            TweenMax.to(this.polygon.el, this.timing * this.polygon.maxSides * 1.05, { 
                rotation: '10deg', 
                ease: Cubic.easeInOut 
            });
        } else {
            TweenMax.to(this.polygon.el, this.timing * this.polygon.maxSides, { 
                rotation: '370deg', 
                ease: Cubic.easeInOut 
            });
        }

        this.direction = !this.direction;
    };

    Spinner.prototype.applyUpdates = function (tween, sides) {
        var points = [];

        for (var j = 0; j <= sides; j++) {
            points.push(tween.target['x' + j] + ',' + tween.target['y' + j]);
        }
        
        this.polygon.el.setAttribute('points', points.join(' '));
    };

    Spinner.prototype.setupTimeline = function () {
        for (var i = this.polygon.sides - 1; i < this.polygon.maxSides; i++) {
            var finish = this.polygon.setSides(i + 1),
                _sides = i; 
            
            this.timeline.to(this.polygon.points, this.timing, _.extend(finish, {
                ease: Cubic.easeOut,
                onUpdateParams: ["{self}", _sides],
                onUpdate: this.applyUpdates.bind(this)
            }));
        }
    };

    Spinner.prototype.fadeOut = function () {
        TweenMax.to(this.polygon.el, .3, { 
            opacity: 0,
            onComplete: this.showCheckmark.bind(this)
        });
    };

    Spinner.prototype.showCheckmark = function () {
        document.getElementById('spinnerSVG').setAttribute("viewBox", "0 0 100 100");
        var checkmarkLength = this.checkmark.getTotalLength();
        
        this.checkmark.style.strokeDasharray = checkmarkLength;
        this.checkmark.style.strokeDashoffset = checkmarkLength;
        
        TweenMax.to(this.checkmark, 0, { opacity: 1 });
        TweenMax.to(this.checkmark, .5, { 
            strokeDashoffset: 0,
            ease: Power2.easeInOut
        });
        
        TweenMax.fromTo(this.svg, .4, 
            { scale: 0.8 },
            { scale: 1, ease: Back.easeOut }
        );
    };

    Spinner.prototype.finish = function () {
        this.timelineEnd = 1;
    };

    Spinner.prototype.start = function () {
        // Reset tất cả
        this.timelineEnd = 0;
        this.direction = 0;
        
        // Reset SVG opacity
        this.svg.style.opacity = '1';
        TweenMax.set(this.svg, { scale: 1 });
        
        // Reset polygon
        TweenMax.set(this.polygon.el, { opacity: 1, rotation: 0 });
        
        // Reset checkmark
        var checkmarkLength = this.checkmark.getTotalLength();
        TweenMax.set(this.checkmark, { 
            opacity: 0,
            strokeDasharray: checkmarkLength,
            strokeDashoffset: checkmarkLength
        });
        
        // Restart timeline
        this.timeline.restart();
    };

    var Polygon = function (opts) {
        this.el = opts.el;
        this.parent = opts.parent;

        this.setupPolygon();
    };
     
    Polygon.prototype.setupPolygon = function () {
        this.size = this.parent.getBoundingClientRect().width,
        this.center = this.size / 6,
        this.radius = this.center
        
        this.sides = 1,
        this.maxSides = 12,
        this.points = {
            x0: 0, y0: 60, x1: 51, y1: -31, x2: -52, y2: -32, x3: -52,y3: -32
        };
        
        TweenMax.set(this.el, {
            transformOrigin: 'center center',
            x: this.size / 2,
            y: this.size / 2
        });
        
        this.points = this.fillPoints(this.points, this.sides);
    };

    Polygon.prototype.setSides = function (sides) {
        var points = {},
            angle = 2 * Math.PI / sides,
            i = 0;
        
        for (i; i < sides; i++) {
            var _angle = i * angle,
                _radius = this.center + this.radius, 
                x = _radius * Math.sin(_angle),
                y = _radius * Math.cos(_angle); 
            
            points['x' + i] = Math.floor(x);
            points['y' + i] = Math.floor(y);
        }
            
        return this.fillPoints(points, i);
    };

    Polygon.prototype.fillPoints = function (points, i) {
        var x = points['x' + (i - 1)],
            y = points['y' + (i - 1)];
        
        for (i; i < this.maxSides; i++) {
            points['x' + i] = x;
            points['y' + i] = y;
        }
        
        return points;
    };

    var spinner = new Spinner(document.querySelector('#spinnerSVG'));

    document.getElementById('downloadBtn').addEventListener('click', async function() {
        document.getElementById('spinnerSVG').setAttribute("viewBox", "0 0 50 50");
        const btn = this;
        const btnText = document.getElementById('btn-text');
        
        // Bước 1: Fade out text
        await new Promise(resolve => setTimeout(resolve, 150));
        btnText.style.opacity = "0";
        
        // Bước 2: Add class loading
        await new Promise(resolve => setTimeout(resolve, 300));
        btn.classList.add('loading');
        btn.disabled = true;
        
        // Bước 3: Start spinner
        await new Promise(resolve => setTimeout(resolve, 300));
        spinner.start();
        
        // Bước 4: Đợi Download hoàn thành
        const exitCode = await startDownload();
        console.log('Execute completed with exit code:', exitCode);
        
        
        await new Promise(resolve => setTimeout(resolve, 300));
        // Bước 5: Gọi finish
        spinner.finish();
        
        
        // Bước 6: Đợi 3 giây (checkmark hiện)
        await new Promise(resolve => setTimeout(resolve, 3000));
        
        // Bước 7: Fade out spinner
        TweenMax.to(spinner.svg, .5, { 
            opacity: 0,
            scale: 0.8,
            ease: Power2.easeIn
        });
        
        // Bước 8: Reset button
        await new Promise(resolve => setTimeout(resolve, 500));
        btn.classList.remove('loading');
        btn.disabled = false;
        document.getElementById('commandInput').value = '';
        document.getElementById('executeBtn').disabled = false;
        
        // Bước 9: Fade in text
        await new Promise(resolve => setTimeout(resolve, 300));
        btnText.style.opacity = "1";
    });
});

async function callFolder() {
    let folderPath = document.getElementById('directory').value;
    if (!folderPath || folderPath.trim() === '') {
        console.error('Invalid folder path');
        return;
    }
    
    // Trim whitespace
    folderPath = folderPath.trim();
    
    console.log('Opening folder:', folderPath);
    
    try {
        const result = await window.openFolder(folderPath);
        console.log('Result:', result);
    } catch (error) {
        console.error('Error opening folder:', error);
        alert('Cannot open folder: ' + folderPath + '\nError: ' + error);
    }
}
async function pickFolder() {
    try {
        const data = await selectFolder();
        
        console.log('Data:', data);
        console.log('Data type:', typeof data);
        
        if (data.success) {            
            console.log('Selected folder:', data.path);
            console.log('Original path:', data.originalPath);
            document.getElementById('directory').value = data.path;
        } else {
            alert('Không thể chọn thư mục. Vui lòng thử lại.');
            document.getElementById('directory').value = "C:/YTDownloader";
        }
    } catch (error) {
        console.error('Error:', error);
        alert('Lỗi: ' + error.message);
    }
}

function mergeCommand(url, directory, platform, mode) {
    if (platform === 1) {
        trimmedUrl = trimmingUrl(url);
        if (mode === 1) {
            return "C:/YTDownloader/yt-dlp.exe -f bv*[ext=mp4]+ba[ext=m4a]/b[ext=mp4]/bv*+ba/b --merge-output-format mp4 --no-playlist --newline --concurrent-fragments 5 --cookies cookies.txt " + url + " -P " + directory;
        }
        else if (mode === 2) {
            return "C:/YTDownloader/yt-dlp.exe --extract-audio --audio-format mp3 --audio-quality 0 --embed-thumbnail --embed-metadata --no-playlist --newline --concurrent-fragments 5 --cookies cookies.txt " + url + " -P " + directory;
        }
        else {
            return "echo [X.X]: Invalid mode for YouTube";
        }
    }
    else if (platform === 2) {
        if (mode === 1) {
            return "C:/YTDownloader/yt-dlp.exe -f best[ext=mp4]/best --merge-output-format mp4 --no-playlist --newline --concurrent-fragments 5 --cookies cookies.txt --add-metadata " + url + " -P " + directory;
        }
        else if (mode === 2) {
            return "C:/YTDownloader/yt-dlp.exe --extract-audio --audio-format mp3 --audio-quality 0 --embed-thumbnail --embed-metadata --no-playlist --newline --cookies cookies.txt " + url + " -P " + directory;
        }
        else {
            return "echo [X.X]: Invalid mode for Facebook";
        }
    }
    else if (platform === 3) {
        if (mode === 1) {
            return "C:/YTDownloader/yt-dlp.exe -f best[ext=mp4]/best --merge-output-format mp4 --no-playlist --newline --cookies cookies.txt --add-metadata " + url + " -P " + directory;
        }
        else if (mode === 2) {
            return "C:/YTDownloader/yt-dlp.exe --extract-audio --audio-format mp3 --audio-quality 0 --embed-thumbnail --embed-metadata --no-playlist --newline --cookies cookies.txt " + url + " -P " + directory;
        }
        else {
            return "echo [X.X]: Invalid mode for TikTok";
        }       
    }
    else if (platform === 4) {
        if (mode === 1) {
            return "C:/YTDownloader/yt-dlp.exe -f bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best --merge-output-format mp4 --no-playlist --newline --concurrent-fragments 8 --cookies cookies.txt --add-metadata --write-subs --sub-langs \"zh-Hans,en\" --embed-subs " + url + " -P " + directory;
        }
        else if (mode === 2) {
            return "C:/YTDownloader/yt-dlp.exe --extract-audio --audio-format mp3 --audio-quality 0 --embed-thumbnail --embed-metadata --no-playlist --newline --concurrent-fragments 5 --cookies cookies.txt " + url + " -P " + directory;
        }
        else {
            return "echo [X.X]: Invalid mode for Bilibili";
        }
    }
    else {
        if (mode === 1) {
            return "C:/YTDownloader/yt-dlp.exe -f bv*[ext=mp4]+ba[ext=m4a]/best[ext=mp4]/best --merge-output-format mp4 --no-playlist --newline --cookies cookies.txt --add-metadata " + url + " -P " + directory;
        }
        else if (mode === 2) {
            return "C:/YTDownloader/yt-dlp.exe --extract-audio --audio-format mp3 --audio-quality 0 --embed-thumbnail --embed-metadata --no-playlist --newline --cookies cookies.txt " + url + " -P " + directory;
        }
        else {
            return "echo [X.X]: Invalid mode for Unknown Platform";
        }
    }
}

function trimmingUrl(url) {
    let videoId = "";

    const match1 = url.match(/v=([^&]+)/);
    if (match1) {
        videoId = match1[1];
    }

    const match2 = url.match(/youtu\.be\/([^?]+)/);
    if (match2) {
        videoId = match2[1];
    }

    if (!videoId) {
        alert("Tính năng xem trước chỉ hỗ trợ Video Youtube! Vui lòng kiểm tra lại đường dẫn!");
        return;
    }

    const trimmedUrl = `https://www.youtube.com/watch?v=${videoId}`;
    return trimmedUrl;
}



async function startDownload() {
    document.getElementById('executeBtn').disabled = true;
    const url = document.getElementById('link').value.trim();
    const directory = document.getElementById('directory').value.trim();
    const platform = updateLinkPlatformOption();
    const mode = parseInt(document.querySelector('input[name="downloadMode"]:checked').value);
    if (!url) {
        alert('Lỗi: Vui lòng nhập đường dẫn video.');
        return;
    }
    else if (!mode || (mode !== 1 && mode !== 2)) {
        alert('Lỗi: Vui lòng chọn chế độ tải xuống!');
        return;
    }
    else {
        await new Promise(resolve => setTimeout(resolve, 1000));
        const command = mergeCommand(url, directory, platform, mode);
        document.getElementById('commandInput').value = command;
        return await executeCmd();
    }

}