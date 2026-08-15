# BRIEF DỰ ÁN — BMS XE ĐIỆN VỚI KALMAN SOC (đọc hết trước khi trả lời)

Bạn là trợ lý cho đồ án tốt nghiệp (LVTN) về Battery Management System.
Tài liệu này chứa toàn bộ bối cảnh, quyết định thiết kế, kết quả và việc đang dở.
Giao tiếp với người dùng bằng TIẾNG VIỆT; code/comment/tài liệu viết TIẾNG ANH.

## 1. HỆ THỐNG

- Xe điện 4 bánh hạng nhẹ "Murata" (khái niệm — KHÔNG có xe thật để chạy):
  khối lượng toàn tải 535 kg (bản vẽ thật), cao 1.602 m, rộng 1.414 m,
  dài 2.657 m. Diện tích cản A = 0.85 × 1.414 × 1.602 ≈ 1.93 m².
- Pack: 5 ắc quy chì-axit 12V nối tiếp = 60V. Đã CHỐT model chuẩn:
  **CSB EVX12200** (12V 20Ah VRLA-AGM, "designed for E-mobility",
  IEC 60254-1; datasheet: R0 ≈ 13.5 mΩ, cutoff 10.5V, xả max 230A,
  400 chu kỳ @100% DOD).
- Bench thử: bộ nguồn 400V + tải tạo dòng thật để đo (thay cho xe).
- Master = ESP32 (repo ../BMS_Master): đo dòng pack bằng **INA219 + shunt
  100A/75mV low-side** (I2C 21/22 — mục 6d), điều khiển relay (GPIO26),
  web dashboard (AsyncWebServer + SSE), CAN xe qua MCP2515 (mục 6b),
  ESP-NOW. FreeRTOS. LCD **đã bỏ**.
- Slave = 5 × STM32F103C8T6 (repo này, BMS_Slave): mỗi bình 1 slave,
  CÁCH LY, đo áp bình (ADC + cầu phân áp) + nhiệt (DS18B20), chạy Kalman
  SOC, gửi CAN. Scheduler hợp tác tự viết (delta-list, tick 10ms, dự kiến
  hạ 5ms) — KHÔNG dùng FreeRTOS (RAM 20KB quá chật; Kalman chỉ ~21µs).
- CAN 500kbps. Slave ID 0x103–0x107 (master map idx = id − 0x103,
  TOTAL_PACKS=5). Slave gửi frame: byte0-1 áp×100, byte2-3 dòng×100,
  byte4 SOC, byte5 nhiệt+40, byte6 cờ lỗi; chu kỳ 1000ms.
- **ĐÃ CODE + ĐÃ VERIFY TRÊN PHẦN CỨNG (2026-08-06):** master phát dòng qua
  frame **0x100** (gộp luôn vai trò heartbeat — slave thức khi nhận 0x100),
  chu kỳ **5ms/200Hz**, `DLC=2`. Không còn heartbeat riêng.
  Quy ước payload CHỐT cho **cả hai chiều**: **`int16` bù 2, MSB trước,
  đơn vị A×100** — giống byte 2-3 của frame slave. Master clamp ±320A
  trước khi scale (int16 tối đa ±327.67A, tràn sẽ đảo dấu).
  Slave sleep (WFI) sau 5s không nhận 0x100 — Task_Sleep ĐÃ BẬT trong main.c.
  Filter CAN đã HẸP về đúng 0x100 (2026-08-13): trước đó mask mở (0x0000) nhận
  mọi ID nên frame slave anh em đánh thức WFI → slave không bao giờ ngủ được.
  Verify: giá trị test 12.34A tới slave **chính xác**; Kalman ổn định ở
  innovation 0.34mV (chứng minh cả chuỗi encode→bus→ISR→kho→R0 bù nhiệt→OCV).
- Hiện trạng firmware slave: **KHÔNG CÒN STUB NÀO**.
  - Áp: BMS_ADC.c đọc ADC thật (100 mẫu) + cầu phân áp 33k/9.1k +
    calib 2 điểm. **Chỉ SLAVE_INDEX=0 đã đo** (K=0.971, B=0.470);
    slave 1-4 vẫn K=1/B=0 → PHẢI đo VOM và điền.
  - Nhiệt: DS18B20 đọc thật. Biên timing khe đọc bit đã nới
    (`delay_us(10)`→`5`, lấy mẫu ~7µs thay vì ~12µs trong cửa sổ 15µs) vì
    ISR CAN ~4µs sẽ chen vào khi master phát 200Hz.
  - Dòng: nhận từ 0x100, ISR giải mã rồi `BMS_Data_SetCurrent()`.
  - SOC: Kalman chạy thật (xem mục 2).
  - Biến debug `g_dbg_v_computed` / `g_dbg_adc_avg` cho Live Expressions.
  - Quy trình calib: build K=1/B=0 → Live Expr xem `g_dbg_v_computed` →
    đo VOM 2 mốc → K=(V2-V1)/(Vt2-Vt1), B=V1-K*Vt1 → điền BMS_ADC.h.
    Calib 2 điểm ROBUST cả khi R danh nghĩa lệch R thật.
- **KIẾN TRÚC PHẦN CỨNG (4 khối, thầy chốt — đang refactor):**
  - **Khối THU THẬP (×5, mỗi bình 1 bộ = slave):** STM32F103 + ADuM1201
    (cách ly galvanic 2 kênh cho TXD/RXD của CAN — bắt buộc vì pack nối
    tiếp) + MCP2551 (CAN transceiver) + buck 12V→5V lấy từ CHÍNH bình đó
    (nuôi STM32 + 1 nửa ADuM).
  - **Khối XỬ LÝ (master):** ESP32 + buck 60V→5V (từ pack, nuôi ESP32 +
    MCP2551) + nửa còn lại ADuM của 5 slave + **MCP2515 (CAN controller
    SPI) để gửi lên CAN BUS TRÊN XE**. → có 2 BUS CAN: nội bộ
    (master↔slave qua MCP2551) và xe (qua MCP2515).
  - **Khối ĐÓNG NGẮT:** contactor 200A (dòng chính) + module relay 5V (đã
    tích hợp cách ly + diode coil-relay) đóng/ngắt COIL contactor, ESP32
    điều khiển relay + **điện trở SHUNT để ESP32 đo dòng**. Contactor ở
    cực **DƯƠNG**, shunt ở cực **ÂM** — hai cực khác nhau và đó mới là
    đúng, xem mục 6d.
  - **Mạng:** CAN (thu thập↔xử lý) · Web + ESP-NOW (người dùng↔xử lý).
  - Cân bằng pin (thụ động/chủ động): KHÔNG làm ở đồ án này (khóa sau).
    Logic hiện tại = nếu SOC/áp các bình LỆCH quá nhiều → NGẮT relay
    (bảo vệ, không cân bằng).
- **✅ CẢM BIẾN DÒNG: SHUNT + INA219 — XONG firmware master (2026-08-12).**
  Hall (ACS712/ACS758) **BỊ CẤM** — yêu cầu kỹ thuật của thầy hướng dẫn,
  không phải sở thích. Đã xoá sạch khỏi code master. **Đừng bao giờ đề
  xuất Hall lại cho dự án này.**
  - Chốt **LOW-SIDE ở cực âm pack**, KHÔNG phải high-side như brief cũ ghi.
    Low-side mới cho phép dùng INA219 (common-mode 0–26V); high-side 60V
    thì phải INA240/INA282. Chi tiết + số đo thật: **mục 6d**.
  - Còn nợ phía SLAVE: `simulate.c` vẫn neo `I_NOISE`/`I_BIAS` theo
    datasheet ACS758 (mục 3). **KHÔNG chặn gì** — shunt êm hơn Hall nên
    tuning hiện tại là *bảo thủ*, không sai. Neo lại ở GĐ8.
  - Auto-zero **tụt xuống 🟡**: Hall trôi 0.125A → 0.875A theo nhiệt nên
    auto-zero là bắt buộc; INA219 đo được trôi ~8mA trên 60°C.
- **⚠️ DÒNG GIỜ TỚI ~100A** (4 motor BLDC × 25A, xe Murata 4 bánh), KHÔNG
  phải 50A. Khi quay lại SOC: nâng sim I_CAP 50→100A; nâng KF_MAX_CURRENT
  (60A) — nếu không sẽ CHẶN NHẦM dòng thật 100A. Dòng chạy như nhau qua
  mọi bình nối tiếp (100A/20Ah = 5C — cao, ghi chú cho HPPC/nhiệt).
- Cầu phân áp đo áp: ĐÃ CHỐT R1=33kΩ, R2=9.1kΩ GIỐNG cả 5 slave (tỉ lệ
  0.2162: 14V→3.03V; R_source 7.13kΩ). **ĐÃ nhập code** trong BMS_ADC.h.
- **BẢN ĐỒ CHÂN STM32F103C8T6 (slave) — đọc từ firmware:**
  - Tín hiệu: PB1 = đo áp bình (ADC1_IN9, cầu phân áp) · PB7 = DS18B20
    nhiệt (1-Wire, open-drain, dùng TIM4 làm delay µs) · PB8 = CAN_RX ·
    PB9 = CAN_TX (CAN1 remap) · PC13 = LED báo (nháy khi gửi CAN / sáng
    khi sleep).
  - Hệ thống: PA13/PA14 = SWDIO/SWCLK (nạp+debug, JTAG đã tắt → giải
    phóng PB3/PB4/PA15) · PD0/PD1 = thạch anh HSE.
  - **Debug timing (chỉ khi `DBG_TIMING` bật trong `Debug_Pins.h`):**
    PA0=SysTick(1ms) · PA1=nhịp SCH(5ms) · PA2=Task_SOC · PA3=Task_Voltage ·
    PA4=Task_CAN · PA5=Task_Temp. Comment `#define DBG_TIMING` là trả lại
    6 chân này cho hệ (zero overhead).
  - Nội (không ra chân): SysTick = tick scheduler (1ms → 5ms gọi
    SCH_Update) · TIM4 = delay µs cho DS18B20 · TIM2 = timer 1ms IRQ bật
    nhưng USER code TRỐNG → nghi mã chết legacy CubeMX, nên rà & gỡ (nhóm
    lỗi #2). ADC1 IRQ phục vụ đọc ADC.
  - Slave KHÔNG có chân relay (relay do MASTER điều khiển) và KHÔNG đo
    dòng (nhận qua CAN). Còn nhiều chân trống cho GĐ sau.

## 2. GIẢI THUẬT SOC (trọng tâm đồ án)

- Kalman 2 trạng thái x = [SOC, V_RC], mô hình 1-RC. Ký hiệu: I > 0 = xả.
- OCV(SOC) tuyến tính: OCV = 11.63 + 1.26·SOC (V) — endpoint AGM
  (bảng SOC-voltage AGM, nguồn footprinthero.com; sẽ xác nhận bằng đo
  pin thật). Q = 20Ah = 72000 C.
- Tham số: R0 = 13.5 mΩ (datasheet CSB). R1 = 13.5 mΩ, C1 = 1850 F
  (τ≈25s) — suy theo tỷ lệ paper IEEE Hu et al. (R1≈R0); vẫn là
  placeholder chờ HPPC đo thật (GĐ8).
- **MULTIRATE** (quyết định quan trọng): `SOC_Kalman_Predict(I, dt)` chạy
  5ms theo dòng; `SOC_Kalman_Update(V, I, T)` CHỈ chạy khi có mẫu áp MỚI
  (50ms). Không bao giờ hiệu chỉnh bằng áp cũ (từng là lỗi thiết kế,
  đã sửa). Trên STM32 dùng cờ `voltage_fresh` — **ĐÃ CODE** trong kho
  (`BMS_Data_TakeVoltageFresh()` đọc-và-xoá; cờ tự bật trong `SetVoltage`).
- **CẢ HAI nửa chạy trong CÙNG task `Task_SOC_Run`**, main context. Lý do:
  scheduler hợp tác run-to-completion nên không ai cắt ngang được `kf`.
  Nếu để Predict trong ISR CAN thì nó sẽ phá ma trận P giữa lúc Update
  đang ghi → P mất tính xác định dương → filter phân kỳ / NaN.
- **`dt` ĐO THẬT bằng `HAL_GetTick()`, không lấy theo chu kỳ đăng ký.**
  Trước đây `SCH_Update()` chỉ set `RunMe` cho node ĐẦU → tối đa 1 task
  dispatch/tick; tổng cầu 222/giây > cung 200/giây (SOC 5ms xin mỗi tick)
  nên MỌI task chậm ~11%. **ĐÃ SỬA (2026-08-08, xem mục 5b): nhả nhiều
  task/nhịp + ưu tiên** → SOC về đúng 5.000ms, 0 skip (LA xác nhận). `dt`
  ĐO THẬT vẫn giữ vì vẫn còn 1 nguồn trễ hợp lệ: đọc DS18B20 blocking ~5ms
  (giới hạn 1-wire) đẩy nhịp kế; nhờ SOC ưu tiên cao nhất chạy TRƯỚC Temp
  nên SOC lấy update xong rồi Temp mới block → không sai số. Tổng các `dt`
  luôn bằng đúng thời gian thực trôi (không lệ thuộc jitter).
- Timeout dòng: không nhận 0x100 trong `CURRENT_TIMEOUT_MS = 100` →
  ép `current = 0` (ghi cả vào kho để byte 2-3 frame nhất quán). 20 frame
  liên tiếp mới kích; sai số nếu tích phân thêm 100ms @100A = 0.014%.
- Init: `SOC_Kalman_Init()` gọi ở lần đầu có cờ áp TƯƠI (không gọi trong
  `main()` vì lúc đó áp còn 0.0 → đảo OCV ra SOC = 0%).
- Nhiệt độ: R0 hiệu chỉnh tuyến tính R0·(1+0.01·(25−T)) (Mức B).
- 4 lớp phòng vệ nhiễu:
  1. Sanity: |I|>60A hoặc NaN → giữ giá trị dòng trước.
  2. Validator vật lý — **CHỈ CHIỀU XẢ** (I > +10A): dòng lớn mà áp không
     sụt (sag < 0.3×I·R0) → dòng nói dối → ép I=0. KHÔNG áp cho regen
     (I<0) vì V_RC chưa tan làm phép so OCV sai → từng gây gai SOC mỗi
     lần phanh (BUG đã tìm ra nhờ soi đồ thị, đã sửa + test hồi quy).
  3. Innovation gating: |innov| > 3√S → bỏ update. **Cài dưới dạng SO BÌNH
     PHƯƠNG** `innov² > 3²·S` (tương đương vì hai vế không âm và S>0 luôn,
     do KF_R_MEAS>0). Mục đích: không kéo libm vào firmware — đo được là
     **tiết kiệm 308 byte flash + 392 byte RAM** trên F103, và bỏ một lời
     gọi softfloat mỗi Update. RAM là tài nguyên khan (20KB).
  4. Rn + K<1 làm mượt nhiễu áp.
- Bias cảm biến dòng là VIỆC CỦA MASTER (auto-zero lúc nghỉ —
  đã cam kết kiến trúc, CHƯA code). Slave nhận dòng sạch → 2-state đủ,
  không cần 3-state ước lượng bias.
- **Đo được trên phần cứng về độ nhạy bias** (dùng giá trị test cố định):
  bias dòng **+12.34A** gây lệch SOC **+28%** (36%→64%). Cơ chế: filter
  suy "đang rút 12A mà áp vẫn 12.08V ⇒ OCV thật = 12.08 + I·R0 + V_RC
  = 12.44V ⇒ bình đầy hơn". Đúng vật lý, sai vì dòng là giả. Số liệu tốt
  cho báo cáo. Validator #4 KHÔNG bắt được vì filter đã hội tụ tới trạng
  thái tự-nhất-quán (nó bắt dòng nói dối TỨC THỜI, không bắt bias HẰNG).
- **Tương quan P[0][1]: dòng HẰNG không tách được 2 trạng thái.** Đo thật:
  ρ = 0.972 khi I=0, **tăng** lên 0.987 khi I=12.34A hằng. Vì với I hằng
  thì cả soc và v_rc vẫn chỉ ảnh hưởng qua một con số là điện áp. Chỉ dòng
  BIẾN THIÊN mới tách (soc theo tích phân, v_rc theo động học RC). ⇒ phép
  kiểm ρ cần chu trình lái, không phải tải đứng yên.
- Triết lý trình bày (trung thực): giá trị của KF là ĐỘ BỀN (hội tụ từ
  init sai, sai số bị chặn, chịu bias/nhiễu/pin chai) — KHÔNG phải "luôn
  chính xác hơn CC". CC lý tưởng (init đúng + dòng sạch) đạt 0.47%;
  nhưng thực tế không bao giờ lý tưởng. Sai số sàn của KF do độ chính
  xác tham số mô hình quyết định (cần HPPC).

## 3. KIỂM THỬ & MÔ PHỎNG (đã làm, PC-only, không cần phần cứng)

- Vị trí: `BMS_Slave/UnitTest/`. Cấu trúc:
  - ⚠️ **`SOC_Kalman.c|.h` ĐÃ DỜI sang `BMS_Slave/Core/`** (Inc/ và Src/).
    Chỉ còn **MỘT bản duy nhất** — `run.sh` trỏ `-I ../Core/Inc` và
    `../Core/Src/SOC_Kalman.c`. Nghĩa là **bộ test biên dịch đúng file mà
    firmware nạp lên chip**. TUYỆT ĐỐI không tạo bản copy thứ hai: sửa
    tham số sau HPPC ở một bản sẽ làm test kiểm một filter khác với filter
    đang chạy.
  - `test/test_SOC_Kalman.c` — Unity, **20 test**, coverage **100% line
    + 100% branch + 100% calls** (gcov). Test dùng macro KF_OCV_*
    (không hardcode áp). Vẫn PASS sau khi dời file và sau khi đổi cổng
    gating sang so bình phương.
  - `sim/simulate.c` — mô phỏng: ECE-15 → động lực xe → dòng → pin ảo
    (plant) → 3 bộ ước lượng (CC / Hybrid / KF) → CSV 9 cột:
    time,speed_kmh,i_true,i_meas,v_meas,true_soc,cc_soc,hybrid_soc,kf_soc
  - `run.sh` — `bash run.sh` (test), `bash run.sh cov` (coverage).
  - Toolchain: Git Bash + MinGW gcc 6.3 + gcov. MATLAB R2024a chỉ để vẽ
    (readmatrix + plot; Add-On Explorer BỊ CHẶN license — không cài thêm
    được drive cycle/toolbox).
- Test theo chuẩn môn "Lập trình nhúng trên ô tô": 10 nhóm lỗi (BVA,
  robustness NaN/spike, fault injection từng lớp phòng vệ, back-to-back
  Coulomb, hội tụ init sai, stress 10k vòng, ép kiểu uint8, test hồi quy
  bug regen). File chuẩn: Excel UnitTest_FormalVerification (2 sheet:
  10 nhóm lỗi + ACSL/Frama-C).
- Mô phỏng theo chuẩn/datasheet (KHÔNG bịa số — nguyên tắc cứng của
  người dùng):
  - Chu trình: **ECE-15 (UNECE R83)**, 18 phân đoạn, 195s, max 50 km/h
    (hợp xe 60V chậm). Nguồn: github.com/dabo248/nedc (udc.csv).
    Lưu ý: chu trình chuẩn = đường BẰNG (dynamometer), không có dốc.
  - Động lực xe: F = Crr·m·g + ½ρ·Cd·A·v² + m·a (+ m·g·sinθ nếu dốc);
    P_elec = P/0.85 (kéo) hoặc P×0.6 (regen); I = P/60V, cap ±50A.
    m=535kg (bản vẽ), Cd=0.4, Crr=0.015 (điển hình, có trích).
  - Cảm biến — ⚠️ **CÒN NEO THEO ACS758, cảm biến đã bị loại.**
    `simulate.c` hiện: `I_NOISE=0.15A` (từ VNOISE 10mV/3σ của
    ACS758-050B); `I_BIAS=0.125A` (offset 25°C ±5mV, "đã auto-zero") hoặc
    `0.875A` (offset −40°C ±35mV, "chưa calib"). Nhiễu áp `V_NOISE=0.02V`.
    - **KHÔNG chặn gì**, và lệch theo chiều AN TOÀN: shunt + INA219 đo
      thật êm hơn Hall ở cả hai mặt (offset 0.023A vs 0.125–0.875A;
      nhiễu ~0.036A vs 0.15A). Bộ lọc đang giả định nhiễu NHIỀU hơn thực
      tế ⇒ không phân kỳ, chỉ hơi chậm.
    - Khi neo lại (GĐ8, sau khi master xong): thay 2 con số theo mục 6d,
      và **thêm MỘT SỐ HẠNG MỚI** — Hall trôi **cộng** (offset zero theo
      nhiệt), shunt trôi **nhân** (TCR ảnh hưởng gain, tỉ lệ theo dòng).
      Đổi giá trị `I_BIAS` là chưa đủ, phải đổi cả *dạng* sai số.
    - `KF_Q_SOC`/`KF_Q_VRC`/`KF_R_MEAS` được tune bằng CHÍNH bộ quét
      Monte-Carlo này ⇒ neo lại số thì phải chạy lại quét.
  - Plant lệch filter ~2% (giả lập sai số HPPC) + Q_TRUE_FACTOR=0.90
    (pin chai 18Ah) + WRONG_INIT (init 75% khi thật 100%) — các núm
    kịch bản trong simulate.c.
- **CẤU HÌNH CHỐT (đã đóng băng sau 7 bước tinh chỉnh)**:
  - Q/R tune bằng quét ("đo R, tune Q"): KF_R_MEAS=4e-4 (σ≈20mV, khớp
    chuỗi ADC 12-bit + trung bình 100 mẫu + dự phòng), KF_Q_SOC=1e-7,
    V_NOISE sim = 0.02V (mức phần cứng tốt).
  - Clamp MỀM: state nội bộ được vượt [0,1] ±0.01 (hết "dính 100%");
    SOC chính thức qua `SOC_Kalman_GetSOC()` kẹp cứng [0,1] — firmware
    PHẢI dùng GetSOC(), không đọc kf->soc trực tiếp. Init kẹp cứng.
  - R1=13.5mΩ (=R0, tỷ lệ IEEE Hu et al.), C1=1850F (τ≈25s).
  - 20 unit test, 100% line + 100% branch; test #17 = 1 chu kỳ ECE-15
    thật + plant có V_RC.
- **MA TRẬN CUỐI** (RMSE % CC / Hybrid / KF):
  - ECE-15: baseline .47/4.1/**.44** · init-sai 25.5/4.1/**.43** ·
    chai 3.8/4.0/**.39** · chưa-calib 2.6/4.0/**1.32** ·
    thực-tế 21.8/4.0/**.37** · đối-chứng(plant=filter,no-noise)
    0.00/4.0/**0.01** (chứng minh cùng code bám dính như đồ án tham khảo
    khi đề "sạch").
  - DST: baseline .38/2.2/**.28** · init-sai 25.3/2.2/**.27** ·
    chai 1.5/2.1/**.27** · chưa-calib 2.4/2.2/**1.55** ·
    thực-tế 23.7/2.1/**.26** → filter KHÔNG học tủ 1 chu trình.
  - Sai số điện áp KF: 0.14–0.18% << mốc 0.5% của đồ án tham khảo.
  - Monte Carlo 30 seed (sim/monte_carlo.sh): ece_baseline KF
    0.448±0.030, ece_realistic 0.392±0.044, dst_realistic 0.283±0.055
    (mean±std) → std nhỏ, không ăn may seed.
  - File: sim/res_{0..4,5-doichung}.csv (ECE-15), sim/res_dst_*.csv (DST),
    sim/mc_results.txt (Monte Carlo thô). CSV 10 cột (thêm v_pred).
- Bug đã tìm & sửa qua kiểm thử (câu chuyện tốt cho báo cáo): validator
  giết nhầm dòng regen (gai SOC mỗi lần phanh) → validator chỉ áp chiều
  xả; Init phải kẹp cứng; plant test cũ thiếu V_RC. Mỗi bug → 1 test.

## 4. SO VỚI ĐỒ ÁN THAM KHẢO (Thanh Trung — file PDF)

- Anh Trung: Li-ion 403V, Simulink thuần (KF = khối, có khối Delay z⁻¹),
  bảng tra 2D R0/R1/C1(SOC,T), chu trình US06, SSE: KF 13.13 vs CC
  3071.73, sai số áp <0.5%, có chương Range. KHÔNG có: firmware C, unit
  test, coverage, phân tích cảm biến datasheet, baseline Hybrid, ma trận
  cô lập yếu tố.
- Ta HƠN: code C **thật đã nạp chip và verify** (SOC khớp lý thuyết, dư
  innovation 0.34mV), 20 test + 100% coverage **kiểm đúng file firmware
  nạp**, ma trận cô lập 5 kịch bản, 2 baseline (CC + Hybrid), mọi số truy
  vết datasheet/chuẩn, câu chuyện tìm-bug-sửa-regression thật, multirate
  đúng nguyên lý, **giao thức CAN 2 chiều chạy thật giữa 2 MCU khác họ**,
  và số đo thực nghiệm về độ nhạy bias (+12.34A → +28% SOC) cùng tương
  quan P chứng minh cần dòng BIẾN THIÊN mới tách được 2 trạng thái.
- Ta THUA (đã vá phần lớn): mô hình pin đơn giản hơn (tuyến tính + hằng
  số vs bảng 2D — chờ HPPC GĐ8); R1/C1 giờ theo tỷ lệ IEEE (không còn
  tự chế); đã có 2 chu trình (ECE-15 + DST) + Monte Carlo 30 seed +
  metric sai số áp (0.14-0.18% < mốc 0.5% của họ) + kịch bản đối chứng.
  Còn thiếu duy nhất: chương Range (ngoài phạm vi).
- Xe 60V khác 403V một điểm vật lý quan trọng: bias dòng làm lệch KF
  qua số hạng I·R0 tỉ lệ lớn hơn ở hệ áp thấp — nhưng với R0 thật
  13.5mΩ, lệch chỉ ~0.9%/1A bias (đã tính).

## 5. LỘ TRÌNH (GĐ = giai đoạn)

- GĐ0 môi trường test ✅ · GĐ1 module Kalman ✅ · GĐ2 unit test ✅ ·
  GĐ3 mô phỏng + ma trận ✅ · GĐ4 Frama-C (bonus, chưa) ·
  **GĐ5 tích hợp firmware STM32 ✅ (2026-08-06)** — Task_SOC.c chạy thật,
  nhận dòng CAN, tick 5ms, verify trên chip · GĐ6 bench test (CHỜ mua
  shunt + contactor) · **GĐ7 master ✅ gần xong (2026-08-11)** — phát dòng
  0x100 5ms ✅ · gỡ Coulomb cũ ✅ · SOC lấy từ slave ✅ · SOC hệ thống =
  min ✅ · CAN xe qua MCP2515 ✅; CÒN auto-zero + cảnh báo lệch SOC ·
  GĐ8 pin thật: HPPC đo R0/R1/C1(SOC,T) + xả tham chiếu làm ground
  truth + đo nhiễu ADC thật.
- Người dùng KHÔNG có xe → chiến lược: dồn bằng chứng vào mô phỏng theo
  chuẩn + bench 400V; GĐ5-8 vẫn khả thi không cần xe.

## 5b. KIẾN TRÚC FIRMWARE SLAVE (refactor xong 2026-08-05/06)

Đã làm 6 bước đóng gói + 1 bước sửa scheduler. Nguyên tắc CHỐT:

- **`.h` KHÔNG chứa `extern <biến>`.** Trạng thái đo nằm trong
  `Shared_Data.c` dạng `static volatile`, ghi qua `BMS_Data_Set*()`
  (mỗi đại lượng MỘT chủ), đọc qua `BMS_Data_GetSnapshot()` trả bản copy
  **theo giá trị**. `myBMS` và `lastHeartbeatTick` đã XOÁ hẳn.
  Test âm đã chứng minh: `extern` từ file khác → **lỗi lúc LINK**.
- **KHÔNG dùng mutex.** Scheduler hợp tác run-to-completion ⇒ task-với-task
  không thể chồng nhau ⇒ không có race. Mutex ở đây còn **deadlock** vì
  không có context switch để chủ mutex chạy tiếp. Chỗ duy nhất cần bảo vệ
  là **task↔ISR**, dùng critical section `__get_PRIMASK`/`__set_PRIMASK`
  (KHÔNG dùng `__enable_irq()` trần — phá lồng ngắt).
- **Kho chỉ GIỮ, không TÍNH.** ISR CAN tự scale `raw * 0.01f` rồi mới gọi
  `BMS_Data_SetCurrent()`. Dùng nhân thay chia (softfloat mul rẻ hơn div
  ~2.5×) để ISR nhẹ.
- **Handle HAL thuộc driver.** `main()` nộp một lần: `BMS_ADC_Init(&hadc1)`,
  `BMS_CAN_Init(&hcan)`. Tầng Task không còn kiểu HAL nào.
  `Task_CAN.c`: **0 lời gọi `HAL_`**. LED qua `BSP_Led.c` (che cả cực tính
  active-low của PC13).
- **`Board_Config.h`**: `SLAVE_INDEX` (0..4) là **DÒNG DUY NHẤT** phải sửa
  khi nạp board khác. `CAN_SLAVE_ID`, `CALIB_K/B`, `start_delay` đều suy ra.
  Có `#error` chặn index ngoài dải.
- **Scheduler** (`SCH_TICK_MS = 5`): đã sửa 7 lỗi — `static volatile` cho
  `SCH_tasks_G`/`Head_Index`; chặn `PERIOD_MS % SCH_TICK_MS != 0` (chia
  nguyên từng biến `PERIOD_MS=5` thành `Period=0` → Dispatch coi là one-shot
  rồi XOÁ); `RunMe -= 1` vào critical section cùng lúc pop Head; nạp lại
  lịch TRƯỚC khi chạy thân task; kiểm biên trước khi index; PRIMASK
  save/restore; `SCH_GetOverrunCount()` đếm trượt deadline (đo được **0**).
- **Scheduler nâng cấp thời gian thực (2026-08-08, xác minh bằng Logic
  Analyzer):**
  1. **Nhả nhiều task/nhịp:** `SCH_Update` khi head `Delay==0` đi dọc chuỗi
     bật `RunMe` cho CẢ nhóm cùng deadline (node kế `Delay==0`), dừng ở
     `Delay>0`. Vòng `while(1)` gọi Dispatch liên tục nên nhả hết trong cùng
     nhịp 5ms (SOC 46µs + Voltage 2.3ms = 2.4ms < 5ms). Xoá nghẽn 1-task/tick.
  2. **Ưu tiên khi trùng deadline:** thêm field `Priority` (số LỚN = chạy
     trước) vào `sTask`; `SCH_Add_Task(..., Priority)` chèn xét ưu tiên (case
     2 + case 3, so `==` deadline rồi so prio); Dispatch re-arm GIỮ prio.
     `main.c`: SOC=3, Voltage=2, CAN=1, Temp=0 → SOC luôn là head, chạy
     TRƯỚC cả Voltage/Temp blocking.
  - **Kết quả LA (50s, sample cao):** SysTick 1.000ms, nhịp SCH 5.000ms
    (std ~0); SOC **5.000ms, 0 skip, gap max 5.02ms** (trước fix: 5.55ms,
    503 skip, gap 15ms); Voltage 50.000, CAN/Temp 1000.01ms; **overlap=0**;
    tại nhịp trùng SOC chạy trước Voltage/Temp ~36µs. Vụ DS18B20 chặn SOC
    tự hết (SOC ưu tiên cao hơn Temp). CSV ở `Logic Analyzer/digital.csv`.
  - **Cách đo:** `Core/Inc/Debug_Pins.h` — mỗi task nhá 1 chân PA (PA0..PA5,
    HAL_GPIO_WritePin), bọc `#ifdef DBG_TIMING` (comment 1 dòng là tắt sạch,
    zero overhead). Bắt bằng Saleae Logic 2.
- **Ngưỡng bảo vệ** (`Board_Config.h`, dải AGM 12V phổ thông):
  over-volt **15.00V** (cũ 12.7 thấp hơn cả mức sạc 14.4 → báo lỗi suốt
  lúc sạc), under-volt **10.50V** (=1.75 VPC), over-temp **50.0°C**
  (datasheet giới hạn xả 50°C; cũ 60 nằm ngoài dải).
- Biến xem trong Live Expressions: `s_voltage_v`, `s_temp_c`, `s_current_a`,
  `s_soc_pct`, `s_faults_volt`, `s_faults_temp`, `s_overrun_count`,
  `'Task_SOC.c'::s_kf`, `'BMS_CAN.c'::s_last_rx_tick`. Là `static` nên gõ
  `'file.c'::ten` nếu CubeIDE không giải được.

## 6. TRẠNG THÁI & VIỆC KẾ TIẾP

Mô phỏng ĐÓNG BĂNG (kết quả mục 3, không đổi trừ khi có số đo thật GĐ8).
GĐ5 XONG. Phần master của GĐ7 gần xong (2026-08-11).

### ✅ Vòng dữ liệu ĐÃ KHÉP KÍN (2026-08-11)

```
Master đo dòng → 0x100 (5 ms) → Slave nhận → Kalman SOC
                                                  ↓
Web / CAN xe / Terminal / ESP-NOW ← System_MinSoc ← 0x103 byte4
```

Việc từng là "quan trọng nhất còn lại" — master bỏ qua byte 4 của frame slave
và tự chạy Coulomb counter — **đã xong**:
- `readMessage()` giải mã `data[4]` → `globalPacks[i].soc` (SOC RIÊNG từng bình).
  `data[2..3]` (dòng) **cố ý KHÔNG giải mã**: đó là số master tự gửi đi ở 0x100,
  đọc lại chỉ tạo hai nguồn sự thật cho cùng một con số.
- Coulomb counter cũ ở `Task_Current.cpp` **đã gỡ** (115 → 63 dòng). Cùng lúc
  mất luôn data race: nó ghi `globalPacks[0].soc` NGOÀI mutex mà
  `System_Get_Snapshot()` đọc field đó CÓ mutex.
- `Task_Current` giờ một việc: đo dòng. Bỏ khối chờ CAN lúc khởi động nên đo
  dòng **từ boot** — slave cần dòng sớm.

### ✅ SOC hệ thống = MIN, và `−1` khi thiếu dữ liệu

`globalPacks[i].soc` giờ nghĩa là "SOC của bình i", nên 4 chỗ đang đọc
`globalPacks[0].soc` như thể là SOC hệ thống đều phải đổi. `System_MinSoc()`:
- Trả **min** của các bình: pack NỐI TIẾP nên bình yếu nhất quyết định giới hạn
  xả, lấy bình 0 làm đại diện chỉ đúng do tình cờ.
- Trả **`−1`** khi CÓ bất kỳ bình offline: không thể biết bình mất tích có phải
  bình yếu nhất hay không, nên báo "không biết" thay vì đoán lạc quan.
- `−1` cho hành vi fail-safe **miễn phí**: nhánh `sysSoc >= RECOVERY_SOC` thành
  false ⇒ **relay không tự đóng lại khi còn bình chưa giám sát được**.

Terminal in `--`, web in `-- %` và `-- Ah`, CAN xe gửi `0xFF`.

### ✅ Ba hàm THUẦN của kho (nhấc lên khi có nhiều người gọi)

Quy tắc: tính trong task; **chỉ nhấc lên `System_*` khi xuất hiện người gọi thứ
hai**. Không nhấc trước.

| Hàm | Vì sao lên | Ghi chú |
|---|---|---|
| `System_MinSoc(snaps)` | 4 người gọi | trả `−1` khi thiếu bình |
| `System_TotalVoltage(snaps)` | 4 chỗ tự cộng, sắp thành 5 | bỏ qua bình offline |
| `System_GetPackCount()` | móc treo | hôm nay = `TOTAL_PACKS` |

Cả ba **không khoá mutex** — nhận sẵn snapshot người gọi đã lấy.

`Task_LCD` **đã xoá** (master không dùng LCD nữa) ⇒ giải phóng GPIO 21/22 (I2C).
`LCD_TIMEOUT` đổi tên **`SLAVE_TIMEOUT_MS`** — nó chưa bao giờ liên quan LCD, đó
là timeout CAN và `System_Data.cpp` mới là chỗ dùng chính.

### ✅ Tách task + đóng gói cờ (master, 2026-08-13)

- **`Task_Logic` tách đôi:** `Task_Ingest` (event-driven, block `xQueueReceive`
  → `System_Update_Pack`) lo NHẬP kho; `Task_Logic` giờ CHỈ giám sát an toàn,
  quét mỗi `PROTECTION_PERIOD_MS`=10ms bằng `vTaskDelay` (bỏ kiểu ăn ké timeout
  queue). Producer/consumer của kho tách rời; mất slave vẫn phát hiện qua
  timestamp trong kho, độc lập queue. Lý do KHÔNG "đọc liên tục": busy-loop đốt
  100% CPU + không nhanh hơn tốc độ nguồn (dòng đổi 5ms).
- **Cờ web đóng gói:** `webForceRelayOff`/`webSlavesActive` bỏ `extern`, chuyển
  `static` trong System_Data.cpp + `System_Set/Get_RelayOverride` /
  `System_Set/Get_SlavesActive` (bool atomic 32-bit → không mutex). Đúng luật
  "không extern biến trong .h" (như mục 5b bên slave).
- **KHÔNG chuyển slave sang FreeRTOS** (đã cân nhắc kỹ): master DÙNG FreeRTOS
  (ESP32), slave GIỮ cooperative (F103). Tải CPU slave ~6% (rảnh 94%);
  cooperative + ưu tiên không chiếm quyền; ISR do NVIC quản (RTOS không cứu
  "bão ISR"); FreeRTOS chỉ hơn ở latency (~µs preempt vs chờ task xong ~5ms) mà
  tốn RAM 20KB + mutex khắp nơi. Giới hạn thật của slave = latency xấu nhất
  ~5ms do DS18B20 blocking (1-wire) — cần thì làm non-blocking, không thay RTOS.

### Còn lại theo mức

- ✅ ~~Hằng số dòng điện neo SAI cảm biến~~ — **XONG 2026-08-12.** Hall đã xoá
  sạch, `Config.h` §6 giờ là INA219 + shunt với **số đo thật** (mục 6d).
- 🔴 **`CURRENT_SIGN` chưa xác nhận bằng dòng thật** — không có tải trên bàn.
  Dấu đặt theo cách đấu dây. **PHẢI kiểm lần chạy xe đầu tiên**: SOC phải
  GIẢM, byte 5 của `0x200` phải = 2 (DISCHARGE), dòng trên web phải DƯƠNG.
  Sai dấu làm `(packI > MAX_DISCHARGE_CURRENT)` luôn sai ⇒ **bảo vệ quá dòng
  tắt hoàn toàn**. Bù lại lỗi rất ồn ào (SOC tăng khi đang chạy) nên không
  trốn được lâu — nhưng phải nằm trong checklist, không để trôi.
- 🔴 **Calib `CALIB_K/B` cho slave 1-4** (chỉ SLAVE_INDEX=0 đã đo).
- 🟠 **9b**: under-volt bù `I·R0`. Ngưỡng 10.5V là số LÚC NGHỈ; ở 100A sụt
  `I·R = 1.35V` nên bình đang nghỉ 11.85V sẽ đọc 10.5V dưới tải → **ngắt
  oan ở ~17% SOC**. Sửa: so `V + I·R0`. Đã có dòng nên làm được.
- 🟡 **Auto-zero cảm biến dòng lúc nghỉ** — *tụt từ 🟠 xuống 🟡.* Hall trôi
  offset 0.125A → 0.875A theo nhiệt nên auto-zero là bắt buộc; INA219 đo được
  trôi ~8mA trên 60°C, và `CURRENT_ZERO_MV` đã đo tĩnh. Vẫn nên có để bù nhiệt
  điện động mối hàn khi shunt nóng lên, nhưng không còn chặn gì.
- 🟠 **Lọc RC ở chân INA219** (10Ω mỗi nhánh + 100nF vi sai) + xoắn đôi dây
  sense. Bench đo được ngoại lai ±150µV (−4.9σ/+6.2σ trên 45k mẫu) **trên bàn
  yên tĩnh** ⇒ với 4 bộ BLDC băm 100A thì đây là bắt buộc, không phải khuyên.
- 🟠 **Shunt không cách ly như Hall.** GND của ESP32 giờ nằm trong đường công
  suất. Hai đường nối tắt qua shunt còn thật: **cáp USB lúc debug** (rút cáp
  động lực, hoặc laptop chạy pin) và **chassis**. Module MCP2515 chỉ có H/L,
  không có chân GND ⇒ CAN xe KHÔNG tạo đường nối tắt.
- 🟠 **EEPROM/NVS cho `activePackCount`** — xem mục 9. Không lưu thì tính năng
  "web nhập số pack" reset về 5 mỗi lần mất điện.
- 🟠 **Web nhập số pack** — móc `System_GetPackCount()` đã sẵn, chỉ cần đổi thân
  hàm + setter + UI.
- 🟡 **`Task_Terminal` và `Task_EspNow` đang bị comment** trong `main.cpp` — có
  code, không chạy. Giữ lại vì không chiếm chân nào. Quyết bật hay bỏ.
- 🟡 Gói nhiệt độ: cold boot đọc +85°C (mặc định scratchpad) → kích
  ERROR_OVER_TEMP ở frame đầu; lọc dải hợp lệ (0.0 và 85.0 hiện lọt);
  `uint16_t Temp` → `int16_t` cho nhiệt âm. **Người dùng đã chọn BỎ QUA.**
- 🟡 ADC timeout: mẫu fail không cộng nhưng vẫn chia `NUM_SAMPLES` → áp
  thấp giả. **Người dùng đã chọn BỎ QUA.**
- 🟡 Mạng: `AutoRetransmission=DISABLE` (one-shot, mất frame khi thua
  arbitration — master ID 0x100 < slave nên master luôn thắng); không kiểm
  return `BMS_CAN_Transmit`; thứ tự `HAL_CAN_Start` trước `ConfigFilter`.
  **Đã hoãn.** — ✅ Filter nhận MỌI ID → **ĐÃ SỬA 2026-08-13**: mask hẹp về đúng
  `0x100` (`FilterIdHigh=0x100<<5`, `FilterMaskIdHigh=0x7FF<<5`), nên frame slave
  anh em không sinh ngắt RX đánh thức WFI → slave mới ngủ được.
- 🟡 Byte 7 frame slave còn trống — chở được `SCH_GetOverrunCount()`.
- 🟡 GĐ8 bench: HPPC (R0/R1/C1 theo SOC), xả tham chiếu, đo variance nhiễu
  ADC thật (thay KF_R_MEAS), xác nhận OCV đầy/cạn.
- 🟡 Bonus: Frama-C (GĐ4), Kalman 3-state ước lượng bias.

### ⚠️ Ghi chú hệ thống, KHÔNG phải lỗi firmware

Datasheet CSB EVX12200 giới hạn **Max Charge Current = 6.00 A**. Regen của
xe (4 motor × 25A) có thể đẩy về **~100A** — gấp **16 lần**. Phải chặn ở
tầng điều khiển motor hoặc chọn bình khác. BMS báo bình thường mà bình vẫn
bị phá. Nghĩa là **với bình này, regen gần như không dùng được**: 6A ở 60V =
360W trong khi 4 motor có thể trả về ~6kW.

Cách ĐÚNG để chặn: gửi frame "giới hạn dòng sạc cho phép" lên CAN xe để xe tự
giới hạn — **chưa làm** vì chưa có xe nào đọc.

## 6b. CAN XE — MCP2515 qua SPI (xong firmware 2026-08-11)

**Vì sao cần chip ngoài:** ESP32 chỉ có **MỘT** bộ TWAI và nó đã cõng bus nội bộ
master↔slave. Bus xe là mạng thứ hai ⇒ bắt buộc controller CAN thứ hai.

### Giao thức CHỐT (tự định nghĩa, đổi ID lại khi có xe thật)

Quy ước **giống bus nội bộ**, không tạo cái thứ hai: **MSB trước · `int16` bù 2 ·
dòng > 0 = XẢ**. Cả hai frame mang **đúng tập dữ liệu web dashboard đang hiện**,
nên hai đường ra kiểm chéo nhau được bằng mắt.

**`0x200` BMS_STATUS · 100 ms · DLC 8**

| Byte | Nội dung | Kiểu | Scale |
|---|---|---|---|
| 0-1 | Tổng áp bình ONLINE | `uint16` | 0.01 V |
| 2-3 | Dòng pack | `int16` | 0.01 A, >0 = xả, clamp ±320 |
| 4 | SOC hệ thống | `uint8` | 1 %, **`0xFF` = không xác định** |
| 5 | Trạng thái | `uint8` | 0=init 1=idle 2=xả 3=sạc 4=fault |
| 6 | Cờ lỗi | `uint8` | bitmask ↓ |
| 7 | **Bộ đếm sống** | `uint8` | tăng 1 mỗi frame, quay vòng |

Byte 6: `01` quá áp · `02` sụt áp · `04` quá nhiệt *(ba bit này lấy NGUYÊN từ byte
status của slave — slave chỉ dùng bit 0-2 nên bit 3 trở lên an toàn cho master)* ·
`08` quá dòng · `10` mất slave · `20` relay đang ngắt · `40` **người vận hành tự
ngắt** *(để xe phân biệt "lỗi" với "người tắt" — hai phản ứng khác nhau)* ·
`80` lỗi nội bộ.

**`0x201` BMS_PACK · 100 ms · DLC 8 · GHÉP KÊNH** (5 bình ⇒ quét đủ 500 ms)

| Byte | Nội dung | Ghi chú |
|---|---|---|
| 0 | Chỉ số bình | bộ chọn kênh, xoay theo `System_GetPackCount()` |
| 1-2 | Áp bình | `uint16` 0.01 V |
| 3 | **SOC bình** | Kalman của chính slave đó · `0xFF` nếu offline |
| 4 | Nhiệt bình | `int8` °C · **`0x80`** nếu offline |
| 5 | Cờ lỗi bình | = byte 6 frame slave |
| 6 | Online | 0/1 |
| 7 | **Số bình đang giám sát** | frame TỰ MÔ TẢ, xe không hardcode 5 |

Tải bus: 2 frame × 10 Hz × 222 µs = **0.44%** của 500 kbps.

**Ba chi tiết tồn tại để BẮT LỖI, không phải trang trí:**
1. `0xFF` / `0x80` = "không biết". Gửi số TRÔNG hợp lý lúc không có dữ liệu nguy
   hiểm hơn là báo thẳng.
2. Bộ đếm sống: frame vẫn tới mà bộ đếm **đứng** ⇒ BMS treo. Timeout frame
   **không** bắt được ca này.
3. Byte 7 của `0x201` khớp luôn tính năng "web nhập số pack".

### BẢN ĐỒ CHÂN ESP32 (master) — sau khi bỏ LCD

Nguyên tắc bố trí: **mỗi task một cụm chân liền nhau**.

| Chân | Dùng | Task |
|---|---|---|
| `16` TX · `17` RX | TWAI — bus **nội bộ** ↔ slave | Task_CAN |
| `5` CS · `18` SCK · `19` MISO · `23` MOSI | SPI — bus **xe** qua MCP2515 | Task_VehicleCAN |
| `22` | `INT` của MCP2515 — **khai chỗ, CHƯA nối** | Task_VehicleCAN |
| `26` | Relay tổng | Task_Logic |
| `32` | ADC dòng — **phải là ADC1**, ADC2 không dùng được khi WiFi bật | Task_Current |
| `21` · `4` · `25` · `27` · `33-35` | **trống** | |

Dùng **chân VSPI mặc định** nên KHÔNG phải gọi `SPI.begin()` với chân tuỳ chọn —
bớt một chỗ sai, và khớp mọi ví dụ của thư viện.

`INT` đặt ở **22** (vừa giải phóng từ I2C) chứ không phải GPIO 4: trên header
DevKit v1, chân 4 nằm ngay dưới 16/17 của TWAI ⇒ chân của **hai bus CAN khác
nhau** sẽ nằm xen kẽ, dễ cắm lẫn khi có 5 slave + 1 bus xe cùng lúc. Ở 22 thì
toàn bộ cụm CAN xe nằm gọn trong vùng `5…23`, TWAI ở ngay dưới.

### Module HW-184 — ba cái bẫy

| Bẫy | Chi tiết |
|---|---|
| **Thạch anh 8 MHz** (X1 in `8.000`) | Phải khai `MCP_8MHZ`. Khai `MCP_16MHZ` như phần lớn ví dụ ⇒ baudrate còn **một nửa** ⇒ bus IM LẶNG HOÀN TOÀN, không lỗi nào báo. Giá trị kỳ vọng trên SPI: `02 2A 00` · `02 29 D1` · `02 28 81` (= `MCP_8MHz_500kBPS_CFG1/2/3`) |
| **Mức logic** | TJA1050 là transceiver **5V** (cần ≥4.75V), ESP32 là 3.3V, module dùng **một chân VCC chung**. Cấp 5V ⇒ `SO` xuất 5V vào ESP32 (vượt absolute max) VÀ ESP32 xuất 3.3V < `VIH = 3.5V` của MCP2515 ⇒ **vấn đề hai chiều**. Cách: level shifter, hoặc thay `SN65HVD230`, hoặc cấp 3.3V (ngoài spec transceiver, dây ngắn trên bàn thường vẫn chạy) |
| **Jumper `J1` 120 Ω** | Module có sẵn điện trở kết cuối. Bus phải đúng **2** cái ở **hai đầu**. Nằm giữa bus đã terminate ⇒ **THÁO J1**. Áp cả cho bus nội bộ: 5 slave + master phải đúng 2 cái, không phải mỗi board một cái |

### ⚠️ Logic analyzer KHÔNG đọc được CAN_H/CAN_L

CAN là tín hiệu **vi sai**: recessive `H≈L≈2.5V`, dominant `H≈3.5V / L≈1.5V`.
Ngưỡng LA ~1.5V ⇒ cắm CAN_H luôn đọc HIGH, cắm CAN_L nhảy loạn. Cần **oscilloscope**
để xem sóng, mà scope cũng không decode ra frame.

**Cách test đúng, không cần thêm thiết bị:** nối `H`/`L` của MCP2515 vào **bus nội
bộ** (cũng 500 kbps) rồi soi **GPIO 17** (chân RX của TWAI) — đó là logic đơn, LA
decode được. Thấy `0x200`/`0x201` ở đó ⇒ MCP2515 **thực sự đẩy được tín hiệu ra
CAN_H/CAN_L**.

An toàn khi nối chung bus: master nhận `0x200` → `idx = 0x200 − 0x103 = 253` →
bounds check chặn; slave chỉ xét `StdId == 0x100` → bỏ qua. Không ai bị nhiễu.

Chỉ cần **RX**, không cần TX: transceiver dội TX lên RX (cơ chế bit-monitoring)
nên RX thấy toàn bộ traffic trên bus.

### Chưa làm, cố ý

- **Nhận frame từ xe** — `PIN_VCAN_INT` (GPIO 22) khai chỗ nhưng **không nối,
  không dùng**. Chưa biết xe có gửi gì.
- **Frame giới hạn dòng sạc/xả** — chưa ai đọc.
- Bit "lệch SOC" — xe tự tính được từ `0x201`, không cần bit riêng.

## 6c. LƯU DỮ LIỆU QUA MẤT ĐIỆN (chưa làm)

**Cả hai chip đều KHÔNG có EEPROM thật.** STM32F103C8T6 chỉ có Flash 64KB (muốn
dùng phải giả lập, ST có AN2594). ESP32 cũng không — `EEPROM.h` của Arduino là
giả lập trên partition Flash; API đúng là **`Preferences`** (NVS), tự lo wear
levelling.

| Ưu tiên | Dùng làm gì | Chip | Ghi chú |
|---|---|---|---|
| 1 | **`activePackCount`** | ESP32 `Preferences` | Đã là yêu cầu; không lưu thì reset về 5 mỗi lần mất điện |
| 2 | **SOC lúc tắt máy** | STM32 | Init hiện đảo OCV từ mẫu áp đầu — nhưng ngay sau khi xe chạy thì áp CHƯA NGHỈ (V_RC chưa tan) ⇒ OCV sai. Đây là ứng dụng kinh điển của BMS |
| 3 | `SLAVE_INDEX` + `CALIB_K/B` | STM32 | ⇒ **MỘT binary cho cả 5 board**. Hiện 5 file `.elf` khác nhau = rủi ro nạp lẫn, mà biểu hiện chỉ là áp lệch vài chục mV |
| 4 | Log lỗi cuối | STM32 | Relay ngắt rồi mất điện ⇒ hiện không còn dấu vết |
| 5 | Ah tích luỹ → **SOH** | STM32 | Hướng mở rộng: datasheet 400 chu kỳ @100% DOD |

**Ba bẫy:**
1. **Ghi Flash trên STM32 chặn CPU 20-40 ms.** Scheduler vừa đo được 5.000 ms /
   0 skip — một lần xoá trang là **bỏ 4-8 nhịp**. KHÔNG được ghi trong lúc chạy
   bình thường.
2. **Số lần ghi.** F103 chịu ~10.000 lần xoá/trang. Xoá+ghi cùng địa chỉ mỗi lần
   SOC đổi 1% ⇒ 100 lần/chu kỳ ⇒ **Flash chết sau 100 chu kỳ**, mà pin rated
   400. Cách đúng: **luân phiên trong trang** — trang 1KB / 8 byte = 128 ô ⇒
   `128 × 10.000 = 1.28 triệu` lần ghi ⇒ 12.800 chu kỳ, gấp 32 lần tuổi thọ pin.
   ESP32 `Preferences` tự làm; STM32 phải tự viết.
3. **Mất điện giữa lúc ghi** ⇒ dữ liệu rác. Cần 2 bản + checksum. Với SOC thì số
   rác đó đi thẳng vào Kalman.

## 6d. CẢM BIẾN DÒNG — INA219 + SHUNT (xong firmware 2026-08-12)

**Hall (ACS712/ACS758) BỊ CẤM** — yêu cầu kỹ thuật của thầy hướng dẫn. Đã xoá
sạch khỏi code. **Đừng đề xuất Hall lại.** Brief cũ ghi "shunt high-side 60V →
INA240/INA282" — **câu đó đã bị thay**, xem dưới.

### Vì sao LOW-SIDE

Common-mode của INA219 chỉ **0–26V**. Pack 60V (72–75V lúc sạc) từ high-side sẽ
phá chip. Đặt shunt ở cực âm giữ common-mode gần 0 ⇒ INA219 dùng được.

Giới hạn phải ghi trong báo cáo: chiều **regen** đẩy một ngõ vào xuống **−56mV ở
75A** — ngoài dải danh định (cận dưới 0V) nhưng trong absolute max −0.3V. GND của
ESP32 bám ở đầu `IN−` (phía cực âm pack) nên **chiều xả — chiều dòng lớn — nằm
trong dải hợp lệ**. Chỉ INA228/INA238 (−0.3…85V) mới hết hẳn vấn đề này.

### Contactor cực DƯƠNG, shunt cực ÂM — khác cực mới đúng

Chassis bond vào cực âm. Contactor ở cực âm thì khi "tắt", **mọi dây dương trong
xe vẫn còn +60V so với chassis** và contactor không cản được đường sự cố đó. Ở
cực dương thì mở ra là hạ nguồn chết hẳn.

Lợi ích kèm theo: contactor ở cực dương giữ cho **mốc đo của INA219 không bao giờ
bị ngắt**, bất kể contactor đóng hay mở. Nếu cả hai ở cực âm thì mở contactor là
cắt luôn đường mass của phép đo.

Contactor cơ điện có cuộn hút cách ly với tiếp điểm ⇒ lý lẽ "low-side dễ lái hơn"
KHÔNG áp dụng (chỉ đúng với MOSFET).

🔴 **Đường sạc phải nằm TRONG vòng đo.** Cực âm bộ sạc nối vào **điểm mass sao**
(phía chassis của shunt), KHÔNG nối thẳng cực âm pack — nối thẳng thì dòng sạc đi
tắt qua shunt, BMS không thấy gì, 5 slave nhận `I = 0` suốt lúc sạc. Cùng lý do:
mọi thứ tiêu thụ điện từ pack lấy mass ở **điểm sao**. Chỉ 2 thứ được bám vào cực
âm pack: **shunt** và **GND của INA219/ESP32**.

Còn thiếu: **precharge** (tụ DC-link của 4 bộ BLDC ⇒ dòng vào đỉnh hàn dính tiếp
điểm) và **cầu chì HRC** ở cực dương, trước contactor.

### Số ĐO THẬT — bench `Code/BENCH_INA219` (2026-08-12)

Shunt **100A / 75mV = 0.75mΩ, class 0.5**. Module INA219 đã **gỡ `R100`** (0.1Ω
trên bo — để lại thì nó song song 2 dây sense và rút 0.75A qua dây dành cho µV).

| Thông số | Đo được | Ở 100A | Ảnh hưởng SOC |
|---|---|---|---|
| **Thang đo** (PGA /8) | 320mV *(kẹp đúng 320.000, sd=0)* | **±427A** | dư **277A** trên trip 150A |
| **Offset** (đã hàn shunt) | **−17.5µV** | −23.3mA | **0.117 %/h** trên 20Ah |
| Offset datasheet worst (85°C) | ±100µV | ±133mA | 0.665 %/h |
| Gain INA219 datasheet | ±1% | ±1.0A | tỉ lệ dòng |
| Class shunt | ±0.5% | ±0.5A | tỉ lệ dòng |
| **Gain tổng worst case** | **±1.5%** | ±1.5A | 1.5% Coulomb |
| Nhiễu 1 mẫu | 27.2µV | 36.3mA | → 12.8mA sau lọc 8 mẫu |
| Bước lượng tử | 10µV | 13.3mA | bỏ qua |
| Chu kỳ lấy mẫu | 3.0ms | | vừa nhịp CAN 5ms |

Hai con **tốt hơn ước lượng ban đầu**: trôi SOC 0.158→**0.117 %/h**, độ phân giải
107→**13.3mA** (bước 10µV giữ nguyên cả ở PGA /8, mịn gấp 8 lần giả định "12-bit
trên toàn thang").

### Ba điều bench dạy được mà tra datasheet không ra

1. **Thang đo phải ĐO, không tin tài liệu.** Cấp 3.27V vi sai → số kẹp ở đúng
   `320.000` với `sd = 0` ⇒ PGA /8 xác nhận. Nếu nó hoá ra /2 (107A) thì ngưỡng
   trip 150A **không bao giờ đạt tới** và bảo vệ quá dòng chết âm thầm.
2. **Gain chỉ đo được TRƯỚC khi hàn shunt.** Sau đó không còn cách nào tách sai
   số shunt khỏi sai số gain của chip. Đo bằng cầu phân áp 2 trở đã biết → khớp
   vôn kế trong 1%.
3. **I2C hỏng KHÔNG chỉ trả về 0.** Rút dây SDA lúc đang chạy: một lần đọc hỏng
   trả về **−168.21mV = −224A** và **đi qua được**, vì probe và read là HAI giao
   dịch I2C riêng. Range check vô dụng (thanh ghi 16-bit ⇒ rác nằm trong ±327mV).
   ⇒ `Task_Current` dùng **trung bình CẮT BIÊN** (8 mẫu, bỏ min+max, lấy 6 mẫu
   giữa): loại mẫu lẻ mà không cần biết vì sao nó xấu — dùng chung cho rác I2C và
   gai EMI từ BLDC.

Hai lần "lỗi gain" trước đó đều là **lỗi mạch thử, không phải chip**: `R100` thật
≈0.103Ω (+3%), và cầu phân áp trở kháng cao bị sụt vì ngõ vào INA219 hút ~340µA
(−9%). **Hai sai số NGƯỢC DẤU chính là bằng chứng nó chưa bao giờ là gain** — lỗi
gain là hệ số nhân cố định, phải cùng dấu ở cả hai phép thử. Không áp dụng cho
mạch thật: shunt 0.75mΩ có trở kháng nguồn gần bằng 0.

### Mất cảm biến ⇒ NGẮT RELAY

`getShuntVoltage_mV()` trả `0.0` **cả khi dòng thật bằng 0 lẫn khi bus chết**, và
thư viện không báo lỗi gì. Một dây SDA tuột sẽ nạp `0 A` cho 5 bộ Kalman VÀ so
`0 > 150A` ⇒ **hai thất bại, cả hai im lặng**.

`Task_Current` probe địa chỉ mỗi vòng bằng `Wire.beginTransmission/endTransmission`
(thuần Wire API, không cần biết thanh ghi). 20 lần không ACK liên tiếp ⇒ cờ
`currentSensorFault` (trong `System_Data`, khuôn 1-ghi/N-đọc như `systemLocked`,
khởi tạo **TRUE** để fail-safe) ⇒ `Task_Logic` ngắt relay + `faultReason`, và
`Task_VehicleCAN` bật cờ `0x80` + byte 2-3 = `VCAN_CURRENT_INVALID` + state FAULT.

INA219 **không có chân ALE** nên đường mềm này là lớp DUY NHẤT. INA226 có `ALE`
(trip quá dòng bằng phần cứng, sống sót cả khi I2C chết) — nhưng thang cố định
±81.92mV ⇒ chỉ **109A** trên shunt này, dưới cả đỉnh tăng tốc 100A. Đó là lý do
chọn INA219: **nó có PGA**, INA226 thì không.

## 7. QUY ƯỚC & NGUYÊN TẮC LÀM VIỆC VỚI NGƯỜI DÙNG NÀY

- Tiếng Việt khi trao đổi; tiếng Anh trong code/file. Sơ đồ dùng Mermaid.
- TUYỆT ĐỐI không bịa số liệu — mọi tham số phải truy vết về datasheet /
  chuẩn quốc tế / paper có trích dẫn / bản vẽ thật. Người dùng đã bắt lỗi
  "lấp liếm" một lần (giảm bias cho đẹp) — phải phân tích trung thực kể
  cả khi Kalman thua.
- Người dùng muốn HIỂU và TỰ LÀM (đã tự dựng Unity, tự chạy MATLAB, tự
  nâng cấp scheduler real-time + đo bằng Logic Analyzer):
  giải thích trước, hỏi chốt phương án, KHÔNG code khi chưa được đồng ý.
  Thích bàn kỹ thiết kế trước khi triển khai. Thường yêu cầu "liệt kê chi
  tiết công việc và code, tôi duyệt mới làm".
- **Người dùng bắt lỗi thiết kế thừa rất đúng.** Đã hai lần chỉ ra AI thêm
  tính năng suy đoán: (1) dựng 3 frame CAN xe cho tình huống chưa tồn tại,
  (2) đòi thêm field vào kho trong khi task tự suy ra được từ snapshot.
  **Nguyên tắc anh chốt:** thêm chức năng = thêm MỘT task, task snapshot rồi
  tự đóng gói; kho chỉ GIỮ, không tính; chỉ nhấc hàm lên `System_*` khi có
  người gọi thứ hai. Đừng nhấc trước, đừng dựng frame chưa ai đọc.
- **Ràng buộc phần cứng từ THẦY HƯỚNG DẪN là chốt, không bàn lại.** Ví dụ đang
  hiệu lực: **shunt, không Hall**. AI đã tốn nhiều lượt đề xuất INA226 rồi
  INA228/238 trong khi việc cần làm là dùng cái đang có. Khi người dùng nói
  "đó là yêu cầu của thầy" ⇒ ngừng so sánh linh kiện, làm cho chạy.
- **Đừng đưa 5 phương án — chọn 1 rồi nói lý do.** Người dùng đã nói thẳng
  "Quá phức tạp, tôi thấy rất rối" và "tại sao tôi thấy quá nhiều vấn đề bạn
  liệt kê vậy". Cách chữa đã hiệu quả: **tách danh sách thành nhóm** (bug có
  sẵn / hệ quả của thay đổi / không liên quan) và chỉ ra **cái nào chặn**.
- **Đo, đừng suy.** Bài học đắt nhất của session cảm biến dòng: AI suy thang
  đo từ độ phân giải (sai), suy sai số gain từ tính toán trở (sai 2 lần vì
  mạch thử). Cả ba lần đều được giải quyết bằng MỘT phép đo. Với thông số
  liên quan an toàn thì đo là bắt buộc, không tra tài liệu thư viện.
- Trình độ: sinh viên nhúng, nắm cơ bản; giải thích bằng ví von + con số
  cụ thể; hay hỏi "tại sao" sâu nhiều tầng — trả lời thẳng, nhận sai khi
  bị bắt lỗi đúng. Hỏi lại định nghĩa cơ bản ("PGA là gì") **không phải** vì
  không theo được — mà vì AI đã dùng thuật ngữ chưa giải thích. Giải thích
  bằng ví von quen (thang đo đồng hồ vạn năng, cân điện tử bước 0.1 kg).
- Kết quả tốt phải kèm giới hạn: cái gì đã chứng minh, cái gì còn chờ
  (HPPC, `CURRENT_SIGN`, đo nhiễu thật trên xe).

## 8. FILE QUAN TRỌNG

- `BMS_Slave/Core/Src/SOC_Kalman.c` + `Core/Inc/SOC_Kalman.h` — module
  Kalman (bản chốt, **MỘT bản duy nhất**, unit test trỏ vào đây).
- `BMS_Slave/Core/Src/Task_SOC.c` + `Core/Inc/Task_SOC.h` — task chạy Kalman.
- `BMS_Slave/Core/Inc/Board_Config.h` — `SLAVE_INDEX` + CAN id + ngưỡng.
- `BMS_Slave/Core/Inc/Shared_Data.h` + `Src/Shared_Data.c` — KHO dữ liệu
  (tên file là di sản; nó KHÔNG còn là global store, xem mục 5b).
- `BMS_Slave/UnitTest/test/test_SOC_Kalman.c` — 20 test.
- `BMS_Slave/UnitTest/sim/simulate.c` — mô phỏng ECE-15/DST (công tắc
  DRIVE_CYCLE) + plant + 3 estimator; seed Monte Carlo qua argv.
- `BMS_Slave/UnitTest/sim/monte_carlo.sh` — 30 seed × 3 cấu hình.
- `BMS_Slave/Core/Src/` — firmware slave hiện tại (Scheduler, Task_*, BMS_CAN...).
- `BMS_Slave/Core/Inc/Debug_Pins.h` — chân nhá timing cho Logic Analyzer
  (PA0..PA5, `#ifdef DBG_TIMING`); `Logic Analyzer/digital.csv` = capture mới nhất.
- `BMS_Master/src/System_Data.cpp` + `include/System_Data.h` — kho của master:
  `globalPacks[]` + mutex + `System_Get_Snapshot()`, và 3 hàm THUẦN
  `System_MinSoc` / `System_TotalVoltage` / `System_GetPackCount`.
- `BMS_Master/src/Task_VehicleCAN.cpp` + `include/Task_VehicleCAN.h` — CAN xe
  qua MCP2515 (2 frame `0x200`/`0x201`, xem mục 6b).
- `BMS_Master/src/Task_CAN.cpp` — bus nội bộ: phát `0x100` chở dòng 5 ms,
  nhận frame slave (giải mã áp / **SOC byte 4** / nhiệt / cờ lỗi).
- `BMS_Master/src/Task_Current.cpp` + `include/Task_Current.h` — CHỈ đo dòng
  (Coulomb cũ đã gỡ). INA219 qua I2C, lọc **cắt biên** 8 mẫu × 3 ms, phát hiện
  mất I2C → `currentSensorFault`. Xem mục 6d.
- `BENCH_INA219/` — **project PlatformIO ĐỘC LẬP** (ESP32 rời) đã kiểm chứng cảm
  biến trước khi sửa master. Sinh ra mọi con số trong mục 6d. Giữ lại: nó là nơi
  duy nhất đo được thang đo và gain một cách độc lập.
- `BMS_Master/src/Task_Ingest.cpp` + `include/Task_Ingest.h` — NHẬP frame CAN
  từ `canQueue` vào kho (event-driven, block `xQueueReceive`). Tách khỏi Task_Logic.
- `BMS_Master/src/Task_Logic.cpp` — CHỈ bảo vệ + relay + hysteresis, quét mỗi
  `PROTECTION_PERIOD_MS`=10ms (`vTaskDelay`). `currentSensorFault` → ngắt relay
  ngay. Cờ web đọc qua `System_Get_RelayOverride()`.
- `BMS_Master/src/Task_WebServer.cpp` + `include/Web_HTML.h` — dashboard;
  JSON có SOC RIÊNG từng bình, xử lý `soc = −1` thành `--`.
- `BMS_Master/include/Config.h` — mục 6 = INA219 + shunt (**số đo thật** từ
  bench, kèm lý do low-side); mục 8 = chân/ID/chu kỳ CAN xe.
- **Bản đồ chân ESP32 (sau 2026-08-12):** `16/17` TWAI nội bộ · `21/22` **I2C →
  INA219** · `5` MCP2515 CS · `4` MCP2515 INT *(dời từ 22 vì GPIO 22 là SCL mặc
  định và INA219 dùng thật)* · `18/19/23` VSPI · `26` relay · `32` **đã giải
  phóng** (của ACS758 cũ).
- Đã XOÁ: `Task_LCD.*` (master không dùng LCD), `BATTERY_CAPACITY_AH`,
  `VOLTAGE_SYS_*` (chết theo Coulomb counter), `PIN_CURRENT_SENSOR` +
  `ACS758_SENSITIVITY` / `ACS758_ZERO_VOLTAGE` / `ACS758_ZERO_CURRENT`
  (chết theo cảm biến Hall — thầy yêu cầu shunt).
- Tham khảo: PDF Thanh Trung (Kalman SOC 403V), bản vẽ "Bố trí chung xe
  Murata.pdf", Excel chuẩn UnitTest_FormalVerification, datasheet CSB
  EVX12200 + INA219 (SBOS448) — số đã trích trong tài liệu này. Datasheet
  ACS758 **không còn dùng**, chỉ còn là mốc so sánh cho mục 3.
