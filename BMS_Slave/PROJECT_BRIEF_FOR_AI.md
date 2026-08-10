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
- Master = ESP32 (repo ../BMS_Master): đo dòng pack bằng ACS758-050B
  (GPIO32), điều khiển relay (GPIO26), web dashboard (AsyncWebServer +
  SSE), LCD, ESP-NOW. FreeRTOS.
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
    điều khiển relay + **điện trở SHUNT ở dòng chính (60V high-side) để
    ESP32 đo dòng**.
  - **Mạng:** CAN (thu thập↔xử lý) · Web + ESP-NOW (người dùng↔xử lý).
  - Cân bằng pin (thụ động/chủ động): KHÔNG làm ở đồ án này (khóa sau).
    Logic hiện tại = nếu SOC/áp các bình LỆCH quá nhiều → NGẮT relay
    (bảo vệ, không cân bằng).
- **⚠️ CẢM BIẾN DÒNG ĐỔI: ACS758 (Hall) → SHUNT.** Config.h + mô hình
  nhiễu/bias trong sim đang neo theo ACS758 datasheet → PHẢI neo lại theo
  SHUNT + amp khi quay lại tinh chỉnh SOC. Shunt high-side 60V → cần amp
  chịu common-mode 60V (INA240/INA282, KHÔNG dùng INA226 ≤36V). Bias giờ
  đến từ offset của AMP + trôi nhiệt shunt (không phải offset Hall) —
  auto-zero VẪN cần, logic không đổi, chỉ khác nguồn bias.
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
  - Cảm biến (datasheet ACS758-050B): nhiễu I_NOISE=0.15A (từ VNOISE
    10mV/3σ); bias 0.125A (offset 25°C ±5mV — kịch bản "đã auto-zero")
    hoặc 0.875A (offset −40°C ±35mV — "chưa calib"). Nhiễu áp
    V_NOISE=0.05V (bảo thủ — ĐANG treo quyết định hạ về ~0.02V theo
    vật lý ADC).
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
  shunt + contactor) · **GĐ7 master: phát dòng 0x100 5ms ✅** — còn
  auto-zero + SOC_min/max + cảnh báo lệch >5% + gỡ Coulomb cũ ·
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
GĐ5 XONG. A2 (master phát dòng) XONG.

### 🔴 Việc quan trọng nhất còn lại — KHÔNG cần mua gì

**SOC Kalman của slave hiện KHÔNG tới được đâu cả.**
`BMS_Master/src/Task_CAN.cpp::readMessage()` chỉ giải mã `data[0..1]` (áp),
`data[5]` (nhiệt), `data[6]` (cờ lỗi) — **BỎ QUA `data[2..3]` (dòng) và
`data[4]` (SOC)**. Đồng thời `Task_Current.cpp:92-101` vẫn tính SOC bằng
Coulomb cũ rồi ghi `globalPacks[0].soc`. Nên web dashboard đang hiện SOC
Coulomb của master, không phải Kalman của slave.

1. Master giải mã `data[4]` (SOC) + `data[2..3]` (dòng) từ frame slave.
2. Gỡ Coulomb SOC cũ ở `Task_Current.cpp` — hết hai nguồn SOC đánh nhau.
3. Sửa data race: `Task_Current.cpp:101` ghi `globalPacks[0].soc` **KHÔNG
   mutex**, trong khi `System_Get_Snapshot()` đọc CÓ mutex.

### Còn lại theo mức

- 🔴 **Calib `CALIB_K/B` cho slave 1-4** (chỉ SLAVE_INDEX=0 đã đo).
- 🟠 **9b**: under-volt bù `I·R0`. Ngưỡng 10.5V là số LÚC NGHỈ; ở 100A sụt
  `I·R = 1.35V` nên bình đang nghỉ 11.85V sẽ đọc 10.5V dưới tải → **ngắt
  oan ở ~17% SOC**. Sửa: so `V + I·R0`. Đã có dòng nên làm được.
- 🟠 **MCP2515 qua SPI → CAN xe** (chưa có dòng code nào). Cần module.
- 🟡 Gói nhiệt độ: cold boot đọc +85°C (mặc định scratchpad) → kích
  ERROR_OVER_TEMP ở frame đầu; lọc dải hợp lệ (0.0 và 85.0 hiện lọt);
  `uint16_t Temp` → `int16_t` cho nhiệt âm. **Người dùng đã chọn BỎ QUA.**
- 🟡 ADC timeout: mẫu fail không cộng nhưng vẫn chia `NUM_SAMPLES` → áp
  thấp giả. **Người dùng đã chọn BỎ QUA.**
- 🟡 Mạng: `AutoRetransmission=DISABLE` (one-shot, mất frame khi thua
  arbitration — master ID 0x100 < slave nên master luôn thắng); không kiểm
  return `BMS_CAN_Transmit`; filter nhận MỌI ID thay vì chỉ 0x100; thứ tự
  `HAL_CAN_Start` trước `ConfigFilter`. **Đã hoãn.**
- 🟡 Byte 7 frame slave còn trống — chở được `SCH_GetOverrunCount()`.
- 🟡 GĐ8 bench: HPPC (R0/R1/C1 theo SOC), xả tham chiếu, đo variance nhiễu
  ADC thật (thay KF_R_MEAS), xác nhận OCV đầy/cạn.
- 🟡 Bonus: Frama-C (GĐ4), Kalman 3-state ước lượng bias.

### ⚠️ Ghi chú hệ thống, KHÔNG phải lỗi firmware

Datasheet CSB EVX12200 giới hạn **Max Charge Current = 6.00 A**. Regen của
xe (4 motor × 25A) có thể đẩy về **~100A** — gấp **16 lần**. Phải chặn ở
tầng điều khiển motor hoặc chọn bình khác. BMS báo bình thường mà bình vẫn
bị phá.

## 7. QUY ƯỚC & NGUYÊN TẮC LÀM VIỆC VỚI NGƯỜI DÙNG NÀY

- Tiếng Việt khi trao đổi; tiếng Anh trong code/file. Sơ đồ dùng Mermaid.
- TUYỆT ĐỐI không bịa số liệu — mọi tham số phải truy vết về datasheet /
  chuẩn quốc tế / paper có trích dẫn / bản vẽ thật. Người dùng đã bắt lỗi
  "lấp liếm" một lần (giảm bias cho đẹp) — phải phân tích trung thực kể
  cả khi Kalman thua.
- Người dùng muốn HIỂU và TỰ LÀM (đã tự dựng Unity, tự chạy MATLAB):
  giải thích trước, hỏi chốt phương án, KHÔNG code khi chưa được đồng ý.
  Thích bàn kỹ thiết kế trước khi triển khai.
- Trình độ: sinh viên nhúng, nắm cơ bản; giải thích bằng ví von + con số
  cụ thể; hay hỏi "tại sao" sâu nhiều tầng — trả lời thẳng, nhận sai khi
  bị bắt lỗi đúng.
- Kết quả tốt phải kèm giới hạn: cái gì đã chứng minh, cái gì còn chờ
  (HPPC, auto-zero, đo nhiễu thật).

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
- `BMS_Master/src/` — firmware master (Task_Current có Coulomb cũ sẽ gỡ,
  Task_CAN, Task_Logic, WebServer).
- Tham khảo: PDF Thanh Trung (Kalman SOC 403V), bản vẽ "Bố trí chung xe
  Murata.pdf", Excel chuẩn UnitTest_FormalVerification, datasheet CSB
  EVX12200 + ACS758 (đã tải, số đã trích trong tài liệu này).
