# 04 â€“ Káº¿ hoáº¡ch & danh sÃ¡ch testcase

- File: [`testbench/test_wm.c`](../testbench/test_wm.c), framework: [`testbench/test_framework.h`](../testbench/test_framework.h)
- Tá»•ng: **64 testcase**, ~1,38 triá»‡u assertion, cháº¡y < 1 giÃ¢y.
- Káº¿t quáº£: **64/64 PASS** (xem `rpt/test_report.txt`).
- Má»i kiá»ƒm tra thá»i gian dÃ¹ng **thÃ´ng sá»‘ tháº­t** (30 phÃºt = 180 000 tick Ã— 10 ms) â€“ mÃ´ phá»ng
  trÃªn PC chá»‰ máº¥t vÃ i mili-giÃ¢y, khÃ´ng cáº§n rÃºt ngáº¯n thá»i gian giáº·t khi test.

## 1. Danh sÃ¡ch testcase

### A. Khá»Ÿi Ä‘á»™ng & LED
| ID | MÃ´ táº£ | YÃªu cáº§u |
|---|---|---|
| TC01 | Báº­t mÃ¡y: STANDBY, tiá»n 0, RLED sÃ¡ng, BLED táº¯t, motor táº¯t | R3 |
| TC02 | RLED sÃ¡ng **Ä‘á»©ng** (khÃ´ng nháº¥p nhÃ¡y) suá»‘t 10 s standby | R3 |

### B. Xu & tiá»n
| ID | MÃ´ táº£ | YÃªu cáº§u |
|---|---|---|
| TC03 | Bá» 10c â†’ tiá»n 10, váº«n STANDBY | R8 |
| TC04 | Bá» 20c â†’ tiá»n 20, váº«n STANDBY | R8 |
| TC05 | Bá» 50c â†’ READY, BLED sÃ¡ng, RLED táº¯t | R5, R7 |
| TC06 | 10+20+20 = Ä‘Ãºng 50c â†’ READY (biÃªn) | R7 |
| TC07 | 40c chÆ°a Ä‘á»§: RUN bá»‹ bá» qua; thÃªm 10c â†’ READY | R7 |
| TC08 | 20+20+20 = 60c â†’ READY | R7 |
| TC09 | Bá» thÃªm xu khi READY â†’ cá»™ng dá»“n | R7 |
| TC10 | Xu sai â†’ ERROR, RLED nháº¥p nhÃ¡y, giá»¯ nguyÃªn tiá»n | R4, R8 |
| TC11 | Lá»—i xu tá»± háº¿t Ä‘Ãºng 3 s (kiá»ƒm tra tá»«ng tick) â†’ STANDBY | D2 |
| TC12 | Lá»—i xu khi Ä‘ang READY â†’ háº¿t lá»—i vá» READY | D2 |
| TC13 | Bá» xu há»£p lá»‡ khi Ä‘ang lá»—i â†’ nháº­n xu, háº¿t lá»—i | D3 |
| TC14 | Xu sai liÃªn tiáº¿p â†’ khá»Ÿi Ä‘á»™ng láº¡i 3 s | D2 |
| TC15 | RLED nháº¥p nhÃ¡y Ä‘Ãºng 500 ms sÃ¡ng / 500 ms táº¯t | R4, D11 |
| TC16 | NÃºt bá»‹ bá» qua khi Ä‘ang bÃ¡o lá»—i; háº¿t lá»—i RUN hoáº¡t Ä‘á»™ng láº¡i | D2 |
| TC17 | Giá»›i háº¡n 990c, xu lÃ m vÆ°á»£t bá»‹ tá»« chá»‘i | D10 |

### C. RUN / báº¯t Ä‘áº§u giáº·t
| ID | MÃ´ táº£ | YÃªu cáº§u |
|---|---|---|
| TC18 | RUN khi 0c â†’ bá» qua | R7 |
| TC19 | 70c + RUN â†’ RUNNING, timer 30:00, **tiá»n = 0 (khÃ´ng tráº£ 20c thá»«a)** | R9, R10 |
| TC20 | BLED nháº¥p nhÃ¡y 1 Hz (Ä‘áº¿m 20 láº§n Ä‘áº£o trong 10 s), RLED táº¯t | R6 |
| TC21 | PAUSE/STOP khi STANDBY/READY â†’ bá» qua, khÃ´ng hoÃ n tiá»n | D9 |
| TC22 | RUN khi Ä‘ang cháº¡y khÃ´ng reset timer | R10 |
| TC23 | Xu bá»‹ tá»« chá»‘i khi RUNNING/PAUSED, xu sai khÃ´ng gÃ¢y ERROR | D6 |

### D. PAUSE / cháº¡y tiáº¿p
| ID | MÃ´ táº£ | YÃªu cáº§u |
|---|---|---|
| TC24 | PAUSE â†’ motor táº¯t, BLED sÃ¡ng Ä‘á»©ng | R11, D7 |
| TC25 | **Timer váº«n Ä‘áº¿m khi pause** (60 s cháº¡y + 5 phÃºt pause) | R11 |
| TC26 | RUN khi PAUSED â†’ cháº¡y tiáº¿p, timer **khÃ´ng** reset | R12 |
| TC27 | PAUSE khi Ä‘ang PAUSED â†’ bá» qua | R11 |
| TC28 | 5 láº§n pause/resume â†’ tá»•ng thá»i gian váº«n Ä‘Ãºng 30 phÃºt | R11â€“R13 |

### E. Timer 30 phÃºt
| ID | MÃ´ táº£ | YÃªu cáº§u |
|---|---|---|
| TC29 | Táº¡i 29:59.99 váº«n RUNNING, táº¡i **Ä‘Ãºng 30:00** â†’ STANDBY (COMPLETED) | R13 |
| TC30 | Timer háº¿t khi Ä‘ang PAUSED â†’ STANDBY | R11, R13 |
| TC31 | Sau khi xong, phá»¥c vá»¥ khÃ¡ch má»›i (tiá»n cÅ© Ä‘Ã£ bá»‹ xoÃ¡) | R3, R10 |

### F. STOP hai láº§n
| ID | MÃ´ táº£ | YÃªu cáº§u |
|---|---|---|
| TC32 | STOP 1 láº§n â†’ váº«n cháº¡y; sau 2 s huá»· tráº¡ng thÃ¡i armed | R14, D5 |
| TC33 | STOPÃ—2 khi RUNNING â†’ STANDBY (FORCE_STOP), timer 0 | R14 |
| TC34 | STOPÃ—2 khi PAUSED â†’ STANDBY | R14 |
| TC35 | STOP láº§n 2 sau 2 s â†’ tÃ­nh lÃ  láº§n 1; láº§n 3 â†’ dá»«ng | D5 |
| TC36 | STOP láº§n 2 á»Ÿ tick cuá»‘i cÃ¹ng cá»§a cá»­a sá»• â†’ váº«n dá»«ng (biÃªn) | D5 |
| TC37 | RUN/PAUSE xen giá»¯a 2 láº§n STOP â†’ chuá»—i bá»‹ huá»· | D5 |
| TC38 | Timer háº¿t khi STOP Ä‘ang armed â†’ COMPLETED (khÃ´ng pháº£i FORCE_STOP) | R13, R14 |

### G. Lá»—i pháº§n cá»©ng
| ID | MÃ´ táº£ | YÃªu cáº§u |
|---|---|---|
| TC39 | FAULT â†’ ERROR, RLED nháº¥p nhÃ¡y, tá»« chá»‘i xu, khÃ´ng tá»± háº¿t | R4, D1 |
| TC40 | FAULT_CLEAR â†’ vá» STANDBY/READY, giá»¯ tiá»n | D1 |
| TC41 | FAULT khi Ä‘ang giáº·t: motor táº¯t ngay, **timer Ä‘Ã³ng bÄƒng**, háº¿t lá»—i â†’ PAUSED, RUN â†’ giáº·t ná»‘t | D4 |
| TC42 | Lá»—i HW ghi Ä‘Ã¨ lá»—i xu (khÃ´ng tá»± háº¿t sau 3 s) | D1 |
| TC43 | FAULT láº·p / FAULT_CLEAR thá»«a bá»‹ bá» qua; FAULT huá»· STOP armed | D1, D5 |

### H. Báº£ng chuyá»ƒn tráº¡ng thÃ¡i Ä‘áº§y Ä‘á»§ (54 cáº·p)
| ID | MÃ´ táº£ |
|---|---|
| TC44 | HÃ ng STANDBY Ã— 9 sá»± kiá»‡n |
| TC45 | HÃ ng READY Ã— 9 sá»± kiá»‡n |
| TC46 | HÃ ng RUNNING Ã— 9 sá»± kiá»‡n |
| TC47 | HÃ ng PAUSED Ã— 9 sá»± kiá»‡n |
| TC48 | HÃ ng ERROR (xu sai) Ã— 9 sá»± kiá»‡n |
| TC49 | HÃ ng ERROR (lá»—i HW) Ã— 9 sá»± kiá»‡n |

So khá»›p vá»›i báº£ng trong [02_thiet_ke_fsm.md](02_thiet_ke_fsm.md#3-báº£ng-chuyá»ƒn-tráº¡ng-thÃ¡i-Ä‘áº§y-Ä‘á»§-6--9--54-cáº·p):
**má»i cáº·p (tráº¡ng thÃ¡i, sá»± kiá»‡n) Ä‘á»u Ä‘Æ°á»£c kiá»ƒm tra** cáº£ tráº¡ng thÃ¡i má»›i láº«n káº¿t quáº£ tráº£ vá».

### I. Äá»™ bá»n
| ID | MÃ´ táº£ |
|---|---|
| TC50 | Con trá» `NULL`, sá»± kiá»‡n ngoÃ i pháº¡m vi, biáº¿n `state` bá»‹ há»ng â†’ reset an toÃ n |
| TC51 | CÃ¡c hÃ m tráº£ tÃªn (dÃ¹ng cho log) |
| TC52 | **Kiá»ƒm thá»­ ngáº«u nhiÃªn** 200 000 bÆ°á»›c (seed cá»‘ Ä‘á»‹nh â†’ láº·p láº¡i Ä‘Æ°á»£c): kiá»ƒm tra 10 báº¥t biáº¿n an toÃ n sau má»—i bÆ°á»›c, Ä‘áº£m báº£o Ä‘i qua má»i tráº¡ng thÃ¡i, cÃ³ cáº£ máº» hoÃ n thÃ nh vÃ  máº» bá»‹ dá»«ng cÆ°á»¡ng bá»©c |

### J. Chá»‘ng dá»™i phÃ­m
| ID | MÃ´ táº£ |
|---|---|
| TC53 | Nháº­n nháº¥n sau Ä‘Ãºng N = 3 máº«u á»•n Ä‘á»‹nh |
| TC54 | Xung nhiá»…u ngáº¯n hÆ¡n N máº«u bá»‹ loáº¡i (100 láº§n) |
| TC55 | Giá»¯ nÃºt 10 s â†’ Ä‘Ãºng 1 láº§n nháº¥n vÃ  1 láº§n nháº£ (khÃ´ng auto-repeat) |

### K. ToÃ n há»‡ thá»‘ng (HAL giáº£ + debounce + FSM)
| ID | MÃ´ táº£ |
|---|---|
| TC56 | `wm_app_init` ghi tráº¡ng thÃ¡i chÃ¢n ban Ä‘áº§u, tá»« chá»‘i HAL thiáº¿u hÃ m |
| TC57 | Ãnh xáº¡ giÃ¡ trá»‹ bá»™ nháº­n xu â†’ sá»± kiá»‡n (5c, 25c, 100c lÃ  xu sai) |
| TC58 | Chu trÃ¬nh Ä‘áº§y Ä‘á»§: bá» xu â†’ nháº¥n RUN â†’ chÃ¢n BLED Ä‘áº£o â†’ 30 phÃºt â†’ STANDBY |
| TC59 | NÃºt RUN bá»‹ dá»™i (chuá»—i 0/1 thá»±c táº¿) â†’ Ä‘Ãºng 1 sá»± kiá»‡n RUN |
| TC60 | Xung nhiá»…u 10 ms trÃªn nÃºt â†’ bá»‹ bá» qua |
| TC61 | Nháº¥n nÃºt STOP váº­t lÃ½ 2 láº§n â†’ dá»«ng giáº·t |
| TC62 | NÃºt PAUSE / RUN váº­t lÃ½ |
| TC63 | Xu 25c â†’ chÃ¢n RLED nháº¥p nhÃ¡y rá»“i tá»± phá»¥c há»“i |
| TC64 | ÄÆ°á»ng lá»—i Ä‘Æ°á»£c chá»‘ng dá»™i; lá»—i tháº­t xá»­ lÃ½ Ä‘Ãºng, háº¿t lá»—i â†’ PAUSED |

## 2. Ma tráº­n truy váº¿t yÃªu cáº§u â†’ testcase

| YÃªu cáº§u | Testcase |
|---|---|
| R1 3 nÃºt | TC21, TC24â€“TC27, TC32â€“TC37, TC59â€“TC62 |
| R2 2 LED | TC01, TC05, TC15, TC20, TC24, TC52 |
| R3 RLED sÃ¡ng khi standby | TC01, TC02, TC11, TC29, TC31 |
| R4 RLED nháº¥p nhÃ¡y khi lá»—i | TC10, TC15, TC39, TC63 |
| R5 BLED sÃ¡ng khi sáºµn sÃ ng | TC05, TC24 |
| R6 BLED nháº¥p nhÃ¡y khi giáº·t | TC19, TC20, TC58 |
| R7 â‰¥ 50 cent | TC05â€“TC09, TC18, TC45 |
| R8 chá»‰ nháº­n 10/20/50 | TC03â€“TC05, TC10, TC57, TC63 |
| R9 RUN khi Ä‘á»§ tiá»n | TC19, TC45 |
| R10 timer 30', xoÃ¡ tiá»n | TC19, TC22, TC31 |
| R11 PAUSE, timer váº«n Ä‘áº¿m | TC24, TC25, TC28, TC30 |
| R12 RUN Ä‘á»ƒ cháº¡y láº¡i | TC26, TC28, TC62 |
| R13 tá»± káº¿t thÃºc sau 30' | TC28â€“TC30, TC38, TC58 |
| R14 STOP hai láº§n | TC32â€“TC38, TC61 |

## 3. ÄÃ¡nh giÃ¡ cháº¥t lÆ°á»£ng bá»™ test (mutation testing)

Äá»ƒ chá»©ng minh bá»™ test thá»±c sá»± báº¯t Ä‘Æ°á»£c lá»—i, cá»‘ Ã½ cÃ i 6 lá»—i vÃ o mÃ£ nguá»“n
rá»“i cháº¡y láº¡i bá»™ test. **Cáº£ 6 lá»—i Ä‘á»u bá»‹ phÃ¡t hiá»‡n:**

| Lá»—i cÃ i vÃ o | Sá»‘ testcase FAIL |
|---|---|
| M1 â€“ KhÃ´ng xoÃ¡ tiá»n khi RUN | 16 |
| M2 â€“ So sÃ¡nh `>` thay vÃ¬ `>=` 50 cent | 38 |
| M3 â€“ Timer Ä‘á»©ng yÃªn khi PAUSED | 3 |
| M4 â€“ Báº¥m STOP má»™t láº§n Ä‘Ã£ dá»«ng | 11 |
| M5 â€“ RUN sau PAUSE reset timer vá» 30' | 3 |
| M6 â€“ Táº¯t chá»‘ng dá»™i phÃ­m | 5 |

## 4. Cháº¡y má»™t pháº§n

```powershell
.\build\test_wm.exe            # táº¥t cáº£
.\build\test_wm.exe TC41       # chá»‰ TC41
.\build\test_wm.exe TC4        # TC4, TC40..TC49
.\build\test_wm.exe --no-color > rpt\test_report.txt
```
