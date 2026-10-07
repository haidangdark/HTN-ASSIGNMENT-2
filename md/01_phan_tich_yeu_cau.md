# 01 – Phân tích yêu cầu

## 1. Yêu cầu trích từ đề bài

| Mã | Yêu cầu |
|---|---|
| R1 | Máy có 3 nút: **STOP, RUN, PAUSE** |
| R2 | 2 LED: **RLED** (đỏ) và **BLED** (xanh) |
| R3 | RLED **sáng** khi máy ở standby (sẵn sàng phục vụ) |
| R4 | RLED **nhấp nháy** khi máy có lỗi |
| R5 | BLED **sáng** khi máy sẵn sàng chạy |
| R6 | BLED **nhấp nháy** khi máy đang giặt |
| R7 | Chỉ chạy được khi đã bỏ vào **≥ 50 cent** |
| R8 | Chỉ nhận đồng **10, 20, 50 cent** |
| R9 | Đủ tiền (BLED sáng) + bấm **RUN** → máy chạy |
| R10 | Khi chạy: bật **timer 30 phút**, **xoá tiền, không trả lại tiền thừa** |
| R11 | Đang chạy bấm **PAUSE** → tạm dừng, **timer vẫn đếm** |
| R12 | Đang pause bấm **RUN** → chạy tiếp |
| R13 | Tự kết thúc sau **30 phút** |
| R14 | Bấm **STOP hai lần** → dừng cưỡng bức |

## 2. Những điểm đề bài chưa nói rõ & lựa chọn của nhóm

Đề bài là đặc tả ngắn nên có nhiều chỗ mơ hồ. Mỗi lựa chọn dưới đây đều được
cấu hình được (trong `wm_config.h`) và đều có testcase kiểm chứng.

| # | Câu hỏi | Lựa chọn | Lý do |
|---|---|---|---|
| D1 | "Lỗi" (R4) là lỗi gì? | 2 loại: **xu không hợp lệ** (tạm thời) và **lỗi phần cứng** (cửa mở, mất nước… – đến khi hết lỗi) | R8 nói máy chỉ nhận 3 loại xu → xu lạ là lỗi tự nhiên nhất. Lỗi phần cứng để hệ thống có ý nghĩa thực tế |
| D2 | Lỗi xu kéo dài bao lâu? | RLED nhấp nháy **3 giây**, sau đó tự về STANDBY/READY; tiền đã bỏ **được giữ** | Người dùng thấy cảnh báo nhưng không mất tiền |
| D3 | Bỏ xu hợp lệ trong lúc đang báo lỗi xu? | Nhận xu, **kết thúc lỗi ngay** | Người dùng đã sửa sai, không bắt chờ |
| D4 | Lỗi phần cứng khi đang giặt? | Tắt motor ngay, **timer đóng băng**; hết lỗi → về **PAUSED** (phải bấm RUN mới chạy lại) | An toàn (không tự quay khi vừa đóng cửa) và công bằng cho khách |
| D5 | "STOP hai lần" trong bao lâu? | Lần 2 phải trong **2 giây** sau lần 1; nếu bấm RUN/PAUSE xen giữa thì chuỗi bị huỷ | Tránh dừng nhầm khi lỡ tay bấm STOP một lần cách đó rất lâu |
| D6 | Bỏ xu khi đang giặt/pause? | **Từ chối** (trả xu), không báo lỗi | Đề nói tiền bị xoá khi RUN; khe xu khoá khi máy chạy để khách không mất tiền oan |
| D7 | LED khi PAUSED? | RLED tắt, **BLED sáng đứng** | BLED sáng = "sẵn sàng chạy" (R5): đúng nghĩa, vì bấm RUN là chạy lại |
| D8 | RLED khi READY? | **Tắt** | Mỗi thời điểm chỉ 1 LED hoạt động → người dùng dễ đọc trạng thái |
| D9 | STOP/PAUSE khi chưa chạy? | Bỏ qua, **không hoàn tiền** | Đề không có chức năng hoàn tiền |
| D10 | Giới hạn tiền? | Tối đa **990 cent**, xu làm vượt bị từ chối | Tránh tràn biến, không "nuốt" tiền khách |
| D11 | Tốc độ nhấp nháy? | 1 Hz (500 ms sáng / 500 ms tắt) | Phổ biến, dễ nhìn |
| D12 | Chống dội phím? | Phải ổn định **3 mẫu liên tiếp (30 ms)** | Nút cơ khí dội 5–20 ms |

## 3. Yêu cầu phi chức năng (hệ thống nhúng)

- Không dùng `malloc`, không dùng số thực, không có vòng lặp chờ (blocking).
- Mọi hàm chạy trong thời gian hữu hạn, cố định → gọi được từ ngắt timer.
- Logic tách khỏi phần cứng → test được trên PC, port được lên mọi MCU.
- Trạng thái lỗi bộ nhớ (biến `state` sai) → reset về trạng thái an toàn.
