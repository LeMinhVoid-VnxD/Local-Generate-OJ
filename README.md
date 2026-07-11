# 🚀 Local AI Online Judge (OJ)

Đây là một hệ thống chấm bài lập trình (Online Judge) chạy local, hỗ trợ tự động sinh đề và testcase bằng AI, chấm điểm theo subtask.

## 🛠️ Yêu cầu cài đặt
- **Trình biên dịch C++:** Cần cài đặt **MSYS2** (với bộ dịch `g++` ucrt64) để biên dịch mã nguồn của server và các bài nộp của người dùng.
- **Windows API:** Sử dụng WinSock2 và WinHTTP (được tích hợp sẵn trong Windows SDK).

## 🚀 Hướng dẫn khởi chạy

### 1. Biên dịch Server
Nếu bạn thay đổi mã nguồn trong `oj_server.cpp`, hãy biên dịch lại bằng lệnh sau:
```bash
g++ -std=c++17 -O2 -o oj_server.exe oj_server.cpp -lws2_32 -lwinhttp -static
```

### 2. Chạy Server
Mở terminal tại thư mục root và chạy file thực thi:
```bash
./oj_server.exe 8080 static problems
```
- `8080`: Cổng chạy web server (có thể thay đổi).
- `static`: Thư mục chứa frontend.
- `problems`: Thư mục chứa dữ liệu bài tập.

Sau khi chạy, truy cập: **`http://localhost:8080`** trên trình duyệt.

## 📂 Cấu trúc thư mục
- `/static`: Chứa giao diện HTML/CSS/JS của hệ thống.
- `/problems`: Chứa các bài tập dưới dạng file `.json`.
- `/submissions`: Thư mục tạm lưu mã nguồn và file thực thi khi chấm bài.

## 📝 Cách thêm bài tập mới

Tạo một file `.json` trong thư mục `/problems` (ví dụ: `my_problem.json`). Cấu trúc mẫu:

```json
{
  "title": "Tên Bài Tập",
  "category": "math",
  "difficulty": "easy",
  "time_limit": 1.0,
  "memory_limit": 256,
  "description": "Mô tả bài tập chi tiết...",
  "input_format": "Định dạng đầu vào...",
  "output_format": "Định dạng đầu ra...",
  "constraints": "Ràng buộc (ví dụ: N <= 10^5)...",
  "subtasks": [
    { "id": 1, "points": 30, "constraints": "N <= 100" },
    { "id": 2, "points": 70, "constraints": "N <= 10^5" }
  ],
  "samples": [
    {
      "input": "5\\n1 2 3 4 5",
      "output": "15",
      "explanation": "Giải thích mẫu..."
    }
  ],
  "test_cases": [
    { "input": "5\\n1 2 3 4 5", "output": "15", "subtask": 1 },
    { "input": "10\\n...", "output": "...", "subtask": 2 }
  ]
}
```

**Lưu ý:**
- Dùng `\\n` cho xuống dòng trong chuỗi JSON.
- Mỗi `test_case` phải gắn với một `subtask` tương ứng.

## 🤖 Sinh đề & Testcase bằng AI

Hệ thống hỗ trợ tự động hóa việc tạo bài tập thông qua API của các LLM (OpenAI, Claude) hoặc sinh test bằng các client AI với tính năm inject như OpenCode,Antigravity,Claude,...

### Cơ chế hoạt động (AI Inject)
Khi lệnh sinh đề được gửi đi, server thực hiện quy trình sau:
1. **Prompting:** Gửi yêu cầu chi tiết cho AI để tạo bài tập định dạng JSON chuẩn (bao gồm tiêu đề, mô tả, samples và testcases).
2. **Parsing:** Server tiếp nhận JSON, kiểm tra tính hợp lệ và gán một ID ngẫu nhiên.
3. **Injection:** Ghi trực tiếp nội dung vào file hệ thống tại `/problems/{id}.json`.
4. **Live Update:** Bài tập mới xuất hiện ngay lập tức trên giao diện mà không cần restart server.

**Mẹo:** Bạn có thể tùy chỉnh `system_prompt` trong file `oj_server.cpp` để yêu cầu AI tạo testcase "chặt" hơn hoặc tập trung vào các trường hợp biên.

## ⚙️ API Endpoints (Cho nhà phát triển)

| Endpoint | Method | Mô tả |
|----------|--------|--------|
| `/api/problems` | `GET` | Lấy danh sách tất cả bài tập |
| `/api/problem/{id}` | `GET` | Lấy chi tiết một bài tập cụ thể |
| `/api/judge` | `POST` | Nộp bài chấm điểm (`problem_id`, `code`, `language`) |
| `/api/generate` | `POST` | Gọi AI sinh đề và testcase (`apiKey`, `provider`, `model`) |
| `/api/reload` | `POST` | Reload lại toàn bộ dữ liệu bài tập từ folder |

## ⚖️ Cơ chế chấm điểm
- **CE (Compile Error):** Lỗi biên dịch.
- **WA (Wrong Answer):** Kết quả sai so với testcase.
- **TLE (Time Limit Exceeded):** Vượt quá thời gian cho phép.
- **RE (Runtime Error):** Lỗi thực thi (crash).
- **AC (Accepted):** Kết quả đúng.

**Điểm số:** Một subtask chỉ được tính điểm nếu **tất cả** các testcase thuộc subtask đó đều đạt **AC**.

--- Theo dõi các bản cập nhật tại ---
Ytb: https://www.youtube.com/@LeMinhVoid
Github: https://github.com/LeMinhVoid-VnxD
