#include "ds18b20.h"

// Hàm delay micro-giây
void delay_us(uint16_t us) {
    __HAL_TIM_SET_COUNTER(DS18B20_TIM, 0);
    while ((__HAL_TIM_GET_COUNTER(DS18B20_TIM)) < us);
}

// KHỞI TẠO CHẾ ĐỘ OPEN-DRAIN (Chỉ chạy 1 lần)
void DS18B20_Init(void) {
    HAL_TIM_Base_Start(DS18B20_TIM); // Khởi động Timer

    // Nhả đường dây ngay khi khởi động (Xuất mức 1)
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, 1);
}

// Khởi động giao tiếp (Hàm này đã đổi sang int8_t)
int8_t DS18B20_Start(void) {
    int8_t Response = 0;

    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, 0); // Kéo xuống
    delay_us(480);

    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, 1); // Nhả ra
    delay_us(80);

    // Đọc trạng thái chân
    if (!(HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN))) Response = 1;
    else Response = -1;

    delay_us(400);
    return Response;
}

// Gửi 1 Byte
void DS18B20_Write(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        if ((data & (1 << i)) != 0) { // Nếu bit = 1
            HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, 0); // Kéo xuống
            delay_us(1);                                     // Giữ đúng 1us
            HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, 1); // Nhả ra ngay
            delay_us(60);
        } else { // Nếu bit = 0
            HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, 0); // Kéo xuống
            delay_us(60);                                    // Giữ lâu 60us
            HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, 1); // Nhả ra
            delay_us(1);
        }
    }
}

// Đọc 1 Byte
uint8_t DS18B20_Read(void) {
    uint8_t value = 0;
    for (int i = 0; i < 8; i++) {
        HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, 0); // Kéo xuống báo hiệu
        delay_us(2);
        HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, 1); // Nhả ra để cảm biến phản hồi

        /* The DS18B20 read slot closes 15 us after the line is pulled low, so
         * the sample must land before then. delay_us() polls TIM4 and is NOT
         * interrupt-protected, so any ISR firing here stretches it.
         * This used to be delay_us(10) -> sampling at ~12 us, only 3 us of
         * margin. The CAN RX ISR alone is ~4 us and will run every 5 ms once
         * the master streams the current frame at 200 Hz, which would push the
         * sample past the window and corrupt bits.
         * Sampling at ~7 us leaves ~8 us of margin instead. */
        delay_us(5);

        if (HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN)) {
            value |= 1 << i; // Nếu đọc được mức 1
        }
        delay_us(55); // Chờ hết chu kỳ (tong khe >= 60us + 1us hoi phuc)
    }
    return value;
}

// Ra lệnh đo nhiệt độ
void DS18B20_Start_Conversion(void) {
    DS18B20_Start();
    DS18B20_Write(0xCC); // Skip ROM
    DS18B20_Write(0x44); // Convert T
}

// Đọc kết quả
float DS18B20_Read_Temperature(void) {
    uint8_t Temp_LSB, Temp_MSB;
    uint16_t Temp;
    float Temperature;

    if (DS18B20_Start() == -1) return -127.0;

    DS18B20_Write(0xCC); // Skip ROM
    DS18B20_Write(0xBE); // Read Scratchpad

    Temp_LSB = DS18B20_Read();
    Temp_MSB = DS18B20_Read();

    Temp = (Temp_MSB << 8) | Temp_LSB;
    Temperature = (float)Temp / 16.0;

    return Temperature;
}
