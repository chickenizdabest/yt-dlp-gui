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