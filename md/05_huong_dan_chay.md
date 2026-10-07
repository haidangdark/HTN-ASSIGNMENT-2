# 05 – Hướng dẫn build & chạy

## 1. Yêu cầu

- `gcc` (máy bạn đã có: MSYS2 UCRT64 gcc 14.2 tại `C:\msys64\ucrt64\bin`).
- `mingw32-make` (có sẵn cùng MSYS2) – hoặc dùng `build.bat` nếu không muốn dùng make.
- Nên chạy trong **Windows Terminal / PowerShell / terminal của VS Code** (hỗ trợ màu ANSI).

## 2. Build & chạy test tự động

```powershell
cd D:\HTN_PROJ_2
mingw32-make test
```
hoặc
```powershell
.\build.bat test
```

Kết quả mong đợi (rút gọn):

```
== A. Power-on & LEDs ==
  [TC01 ] Power-on: STANDBY, credit 0, RLED on, BLED off                   PASS
  ...
  [TC64 ] Fault line debounced, real fault handled                         PASS
------------------------------------------------------------------------------
 TOTAL: 64   PASSED: 64   FAILED: 0   (assertions checked: 1378774)
------------------------------------------------------------------------------
>>> ALL TESTS PASSED <<<
```

Lưu báo cáo ra file: `mingw32-make report` → `rpt/test_report.txt`.

## 3. Mô phỏng bằng bàn phím

```powershell
mingw32-make sim
```
hoặc `.\build.bat sim`, hoặc chạy trực tiếp `.\build\interactive_sim.exe`.

### 3.1 Phím điều khiển (bấm là ăn ngay, không cần Enter)

| Phím | Chức năng |
|---|---|
| `1` | Bỏ đồng **10 cent** |
| `2` | Bỏ đồng **20 cent** |
| `3` | Bỏ đồng **50 cent** |
| `4` | Bỏ **xu không hợp lệ** (25 cent) → RLED nhấp nháy 3 s |
| `r` | Nhấn nút **RUN** |
| `p` | Nhấn nút **PAUSE** |
| `s` | Nhấn nút **STOP** (bấm 2 lần trong 2 s để dừng cưỡng bức) |
| `f` | Bật/tắt **lỗi phần cứng** (giả lập mở cửa) |
| `t` / `n` / `m` | Tua nhanh **+10 giây / +1 phút / +10 phút** |
| `+` / `-` | Đổi tốc độ mô phỏng: x1 → x10 → x60 → x600 |
| `q` | Thoát |

### 3.2 Màn hình

```
 +==============================================================+
 |        WASHING MACHINE CONTROLLER  -  CO3053 SIMULATOR       |
 +==============================================================+
   Sim time : 00:01:12.40          Speed : x1

   State    : RUNNING
   RLED     : [   ]  OFF        BLED : [███] BLINK      <- ô màu là LED thật
   Motor    : ON    / spinning
   Credit   : 0 cents  (need 50)
   Timer    : 28:47 remaining  [#.............................]
   STOP     : ARMED - press [s] again within 1.6 s to force stop
   Stats    : completed=0  force-stopped=0   Fault line: ok

   >> Washing... [p] pause, [s] twice to force stop
 ----------------------------------------------------------------
   [1] 10c   [2] 20c   [3] 50c   [4] invalid coin (25c)
   ...
   Event log:
    00:00:01.20  COIN_50      -> OK             STANDBY -> READY
    00:00:02.53  BTN_RUN      -> OK             READY -> RUNNING
    00:01:12.03  BTN_STOP     -> STOP_ARMED     (stay RUNNING)
```

Phím bấm được đưa qua **đúng đường đi của phần cứng thật**: phím → "nút được giữ 80 ms"
→ bộ chống dội → FSM → chân LED. Vì vậy sau khi bấm sẽ có trễ ~30 ms (chống dội) – giống máy thật.

### 3.3 Kịch bản demo gợi ý

| # | Thao tác | Kết quả mong đợi |
|---|---|---|
| 1 | `2` `2` | Tiền 40c, vẫn STANDBY (RLED sáng). Bấm `r` → không có gì xảy ra |
| 2 | `1` | Tiền 50c → READY, BLED sáng, RLED tắt |
| 3 | `r` | RUNNING, BLED nhấp nháy, motor quay, tiền về 0, timer 30:00 |
| 4 | `p` | PAUSED, motor dừng, BLED sáng đứng, **timer vẫn chạy** |
| 5 | `n` | Tua 1 phút → timer giảm 1 phút dù đang pause |
| 6 | `r` | Chạy tiếp, timer không reset |
| 7 | `s` | STOP "ARMED", máy vẫn chạy; chờ > 2 s → tự huỷ armed |
| 8 | `s` `s` (nhanh) | Dừng cưỡng bức → STANDBY |
| 9 | `3` `r` rồi `m` `m` `m` | Tua 30 phút → tự kết thúc → STANDBY |
| 10 | `4` | Xu sai → ERROR, RLED nhấp nháy 3 s rồi tự về STANDBY |
| 11 | `3` `r` `f` … `f` | Lỗi HW khi đang giặt → motor tắt, timer đứng; tắt lỗi → PAUSED |

### 3.4 Chế độ từng bước (step mode)

Nếu terminal không hỗ trợ màn hình động, hoặc muốn chạy kịch bản có sẵn:

```powershell
.\build\interactive_sim.exe --plain
```
Gõ lệnh rồi Enter (gõ nhiều lệnh một lúc được, vd `33r`). Mỗi lệnh tiến 200 ms thời gian
mô phỏng; `i` in trạng thái; `h` trợ giúp.

Chạy kịch bản tự động bằng pipe:

```powershell
"3 r p i n i r s s i" | .\build\interactive_sim.exe
```

Ví dụ output:

```
  00:00:00.00  COIN_50      -> OK             STANDBY -> READY
  00:00:00.23  BTN_RUN      -> OK             READY -> RUNNING
  00:00:00.43  BTN_PAUSE    -> OK             RUNNING -> PAUSED
  ---- [00:00:00.60] state=PAUSED  RLED=OFF   BLED=ON    motor=OFF credit=0c timer=30:00
  00:00:00.60  [fast forward +1 min]
  ---- [00:01:00.60] state=PAUSED  RLED=OFF   BLED=ON    motor=OFF credit=0c timer=29:00
  00:01:00.63  BTN_RUN      -> OK             PAUSED -> RUNNING
  00:01:00.83  BTN_STOP     -> STOP_ARMED     (stay RUNNING)
  00:01:01.03  BTN_STOP     -> OK             RUNNING -> STANDBY
```

## 4. Build thủ công (không dùng make/bat)

```powershell
mkdir build
gcc -std=c99 -Wall -Wextra -Isrc src/wm_fsm.c src/wm_debounce.c src/wm_app.c testbench/test_wm.c -o build/test_wm.exe
gcc -std=c99 -Wall -Wextra -Isrc src/wm_fsm.c src/wm_debounce.c src/wm_app.c testbench/interactive_sim.c -o build/interactive_sim.exe
```

Demo với thời gian giặt ngắn (1 phút) thay vì 30 phút:

```powershell
gcc -std=c99 -Isrc -DWM_WASH_DURATION_MS=60000UL src/wm_fsm.c src/wm_debounce.c src/wm_app.c testbench/interactive_sim.c -o build/sim_1min.exe
.\build\sim_1min.exe
```

> [!NOTE]
> Không biên dịch `test_wm.c` với `-DWM_WASH_DURATION_MS=...` khác 30 phút: TC19 kiểm tra
> rằng timer bằng đúng 1800 giây theo đề bài.

## 5. Lỗi thường gặp

| Lỗi | Cách xử lý |
|---|---|
| `mingw32-make` không tìm thấy | Dùng `.\build.bat test` / `.\build.bat sim` |
| Màn hình hiện ký tự lạ `←[32m` | Terminal cũ không hỗ trợ ANSI → dùng Windows Terminal, hoặc `--plain` |
| Bấm phím không phản hồi | Click vào cửa sổ terminal để nó có focus |
| Chạy trên Linux/macOS | `make test`, `make sim` (mã nguồn simulator hỗ trợ cả POSIX termios) |
