# Bộ điều khiển máy giặt – CO3053 Assignment

Tài liệu của project, đọc theo thứ tự:

| File | Nội dung |
|---|---|
| [01_phan_tich_yeu_cau.md](01_phan_tich_yeu_cau.md) | Phân tích đề bài, các điểm đề bài chưa nói rõ và lựa chọn thiết kế |
| [02_thiet_ke_fsm.md](02_thiet_ke_fsm.md) | Sơ đồ máy trạng thái (FSM), bảng chuyển trạng thái, bảng LED |
| [03_kien_truc_ma_nguon.md](03_kien_truc_ma_nguon.md) | Kiến trúc phân lớp, vai trò từng file, cách đưa lên vi điều khiển thật |
| [04_testcase.md](04_testcase.md) | Kế hoạch kiểm thử: 64 testcase, ánh xạ yêu cầu ↔ testcase |
| [05_huong_dan_chay.md](05_huong_dan_chay.md) | Cách build, chạy test, chạy mô phỏng bằng bàn phím |
| [06_bao_cao_fsm_pseudocode.md](06_bao_cao_fsm_pseudocode.md) | **Báo cáo FSM và Mã giả (Pseudocode) đầy đủ cho thuyết minh / báo cáo** |

## Cấu trúc thư mục

```
HTN_PROJ_2/
├── src/                      # Mã nguồn bộ điều khiển (chạy được trên MCU)
│   ├── wm_config.h           # Thông số: tick 10 ms, 30 phút, 50 cent...
│   ├── wm_fsm.h / .c         # Máy trạng thái - logic thuần, không đụng phần cứng
│   ├── wm_debounce.h / .c    # Chống dội phím
│   ├── wm_hal.h              # Giao diện phần cứng (con trỏ hàm)
│   └── wm_app.h / .c         # Lớp ứng dụng: HAL -> debounce -> FSM -> HAL
├── testbench/
│   ├── test_framework.h      # Framework test tự viết (không cần thư viện ngoài)
│   ├── test_wm.c             # 64 testcase tự động
│   └── interactive_sim.c     # Mô phỏng tương tác: bấm phím để điều khiển máy giặt
├── md/                       # Tài liệu (thư mục này)
├── rpt/test_report.txt       # Báo cáo kết quả test (sinh bởi `make report`)
├── Makefile                  # mingw32-make test | sim | report | clean
└── build.bat                 # Build không cần make: build.bat test | sim
```

## Chạy nhanh

```powershell
cd D:\HTN_PROJ_2
mingw32-make test      # chạy 64 testcase
mingw32-make sim       # mở mô phỏng, bấm 1/2/3 để bỏ xu, r = RUN ...
```

Kết quả hiện tại: **64/64 testcase PASS**, build sạch với
`-Wall -Wextra -Wpedantic -Wshadow -Wconversion` (0 warning).
