#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h> 
#include "hardware/adc.h"
#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "ff.h"
#include "diskio.h"
#include "f_util.h"
#include "hw_config.h"
#include "my_debug.h"
#include "rtc.h"
#include "sd_card.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "font.h"
#include "buzzer.h"

// Definição dos pinos I2C para o MPU6050
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1
#define MPU6050_ADDR 0x68

// Definição dos pinos I2C para o display OLED
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define ENDERECO_DISP 0x3C     

// Definições dos botões
#define BUTTON_A 5      // GPIO 5 para iniciar gravação de 100 amostras
#define BUTTON_B 6      // GPIO 6 para montar SD
#define JOYSTICK_BTN 22 // GPIO 22 para listar arquivos/exibir amostras

// Definição dos pinos do LED RGB
#define LED_RED 13      // GPIO 13 (vermelho)
#define LED_GREEN 11    // GPIO 11 (verde)
#define LED_BLUE 12     // GPIO 12 (azul)

// Definição do buzzer
#define BUZZER_PIN 21

// Nome do arquivo de saída
static char filename[20] = "mpu_data.csv";

// Variáveis para controle de gravação
static bool is_recording = false;
static uint32_t sample_count = 0;
static uint32_t samples_to_record = 100;
static bool sd_mounted = false;

// Estrutura para armazenar os dados do MPU6050
typedef struct {
    float accel[3]; // Aceleração em g (X, Y, Z)
    float gyro[3];  // Giroscópio em graus/s (X, Y, Z)
} MPU6050_Data;

// Variáveis para controle de interrupções
static volatile bool button_a_pressed = false;
static volatile bool button_b_pressed = false;
static volatile bool joystick_pressed = false;
static uint32_t last_time_btn_a = 0;
static uint32_t last_time_btn_b = 0;
static uint32_t last_time_joystick = 0;

// Função para controlar o LED RGB
void set_led_color(bool red, bool green, bool blue) {
    gpio_put(LED_RED, red);    
    gpio_put(LED_GREEN, green);
    gpio_put(LED_BLUE, blue);
}

// Funções para manipulação do SD card
static sd_card_t *sd_get_by_name(const char *const name) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return sd_get_by_num(i);
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

static FATFS *sd_get_fs_by_name(const char *name) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return &sd_get_by_num(i)->fatfs;
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

static void run_ls() {
    char cwdbuf[FF_LFN_BUF] = {0};
    FRESULT fr;
    char const *p_dir;
    
    fr = f_getcwd(cwdbuf, sizeof cwdbuf);
    if (FR_OK != fr) {
        printf("f_getcwd error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    p_dir = cwdbuf;
    
    printf("Listando arquivos em: %s\n", p_dir);
    DIR dj;
    FILINFO fno;
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);
    fr = f_findfirst(&dj, &fno, p_dir, "*");
    if (FR_OK != fr) {
        printf("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    while (fr == FR_OK && fno.fname[0]) {
        const char *pcWritableFile = "arquivo gravável",
                   *pcReadOnlyFile = "arquivo somente leitura",
                   *pcDirectory = "diretório";
        const char *pcAttrib;
        if (fno.fattrib & AM_DIR)
            pcAttrib = pcDirectory;
        else if (fno.fattrib & AM_RDO)
            pcAttrib = pcReadOnlyFile;
        else
            pcAttrib = pcWritableFile;
        printf("%s [%s] [tamanho=%llu]\n", fno.fname, pcAttrib, fno.fsize);

        fr = f_findnext(&dj, &fno);
    }
    f_closedir(&dj);
}

// Função para resetar e inicializar o MPU6050
void mpu6050_reset() {
    uint8_t buf[] = {0x6B, 0x80};
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, buf, 2, false);
    sleep_ms(100);
    
    buf[1] = 0x00;
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, buf, 2, false);
    sleep_ms(10);
}

// Função para ler dados crus do MPU6050
void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    uint8_t buffer[6];
    uint8_t val;

    // Lê aceleração
    val = 0x3B;
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &val, 1, true);
    i2c_read_blocking(I2C_PORT, MPU6050_ADDR, buffer, 6, false);

    for (int i = 0; i < 3; i++) {
        accel[i] = (buffer[i * 2] << 8) | buffer[(i * 2) + 1];
    }

    // Lê giroscópio
    val = 0x43;
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &val, 1, true);
    i2c_read_blocking(I2C_PORT, MPU6050_ADDR, buffer, 6, false);

    for (int i = 0; i < 3; i++) {
        gyro[i] = (buffer[i * 2] << 8) | buffer[(i * 2) + 1];
    }

    // Lê temperatura
    val = 0x41;
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &val, 1, true);
    i2c_read_blocking(I2C_PORT, MPU6050_ADDR, buffer, 2, false);

    *temp = (buffer[0] << 8) | buffer[1];
}

// Função para obter dados processados do MPU6050
void mpu6050_get_data(MPU6050_Data *data) {
    int16_t raw_accel[3], raw_gyro[3], raw_temp;
    
    // Lê dados crus
    mpu6050_read_raw(raw_accel, raw_gyro, &raw_temp);
    
    // Converte aceleração para g (escala ±2g)
    for (int i = 0; i < 3; i++) {
        data->accel[i] = raw_accel[i] / 16384.0f;
    }
    
    // Converte giroscópio para graus/s (escala ±250°/s)
    for (int i = 0; i < 3; i++) {
        data->gyro[i] = raw_gyro[i] / 131.0f;
    }
}

// Função para criar arquivo CSV com cabeçalho
void create_csv_file() {
    if (!sd_mounted) {
        printf("SD card não montado. Não é possível criar arquivo.\n");
        return;
    }

    FIL file;
    FRESULT res = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        printf("Erro ao criar arquivo %s (%d)\n", filename, res);
        return;
    }
    
    const char *header = "numero_amostra,accel_x,accel_y,accel_z,giro_x,giro_y,giro_z\n";
    UINT bytes_written;
    f_write(&file, header, strlen(header), &bytes_written);
    
    f_close(&file);
    printf("Arquivo %s criado/sobrescrito com sucesso\n", filename);
}

static void run_mount() {
    set_led_color(true, true, false); // Amarelo (vermelho + verde)
    
    const char *arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs) {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        sd_mounted = false;
        set_led_color(false, false, false); // Desliga LED
        return;
    }
    FRESULT fr = f_mount(p_fs, arg1, 1);
    if (FR_OK != fr) {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        sd_mounted = false;
        set_led_color(false, false, false); // Desliga LED
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    myASSERT(pSD);
    pSD->mounted = true;
    sd_mounted = true;
    printf("SD card montado com sucesso\n");
    
    // Cria o arquivo CSV após montar com sucesso
    create_csv_file();
    
    set_led_color(false, true, false); // Verde (sistema pronto)
}

// Função para gravar dados no arquivo CSV
void write_data_to_csv(MPU6050_Data *data) {
    if (!sd_mounted) {
        printf("SD card não montado. Não é possível gravar.\n");
        is_recording = false;
        set_led_color(false, true, false); // Volta para verde
        return;
    }

    set_led_color(true, false, false); // Vermelho (gravação em andamento)
    static absolute_time_t last_beep_time = 0;
        // Toca um beep a cada 1 segundo (500ms ligado, 500ms desligado)
    if (absolute_time_diff_us(last_beep_time, get_absolute_time()) > 1000000) {
        buzzer_beep(BUZZER_PIN, 1000, 500); // Beep de 1kHz por 500ms
        last_beep_time = get_absolute_time();
    }
    FIL file;
    FRESULT res = f_open(&file, filename, FA_WRITE | FA_OPEN_APPEND);
    if (res != FR_OK) {
        printf("Erro ao abrir arquivo para escrita (%d)\n", res);
        is_recording = false;
        set_led_color(false, true, false); // Volta para verde
        buzzer_stop(pwm_gpio_to_slice_num(BUZZER_PIN));
        return;
    }
    
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
             sample_count,
             data->accel[0], data->accel[1], data->accel[2],
             data->gyro[0], data->gyro[1], data->gyro[2]);
    
    UINT bytes_written;
    f_write(&file, buffer, strlen(buffer), &bytes_written);
    f_close(&file);
    
    sample_count++;
}

// Função para ler e exibir o conteúdo do arquivo CSV
void read_csv_file() {
    set_led_color(false, false, true); // Azul (acessando SD)
    
    if (!sd_mounted) {
        printf("SD card não montado. Não é possível ler arquivo.\n");
        set_led_color(false, true, false); // Volta para verde
        return;
    }

    FIL file;
    FRESULT res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        printf("Erro ao abrir arquivo para leitura (%d)\n", res);
        set_led_color(false, true, false); // Volta para verde
        return;
    }
    
    char buffer[128];
    printf("\nConteúdo do arquivo %s:\n", filename);
    
    while (f_gets(buffer, sizeof(buffer), &file) != NULL) {
        printf("%s", buffer);
    }
    
    f_close(&file);
    printf("\nFim do arquivo\n");
    
    set_led_color(false, true, false); // Verde (sistema pronto)
}

// Função para atualizar o display OLED
void update_display(ssd1306_t *ssd, MPU6050_Data *data, bool recording_status, uint32_t samples_recorded) {
    static bool cor = true;
    
    ssd1306_fill(ssd, !cor);
    ssd1306_rect(ssd, 3, 3, 122, 60, cor, !cor);
    ssd1306_line(ssd, 3, 25, 123, 25, cor);
    ssd1306_line(ssd, 3, 37, 123, 37, cor);
    ssd1306_draw_string(ssd, "CEPEDI   TIC37", 8, 6);
    ssd1306_draw_string(ssd, "EMBARCATECH", 20, 16);
    
    if (recording_status) {
        char status_str[20];
        snprintf(status_str, sizeof(status_str), "GRAV: %lu/%d", samples_recorded, samples_to_record);
        ssd1306_draw_string(ssd, status_str, 10, 28);
    } else {
        ssd1306_draw_string(ssd, "PRONTO", 10, 28);
    }
    
    ssd1306_line(ssd, 63, 35, 63, 60, cor);
    
    // Mostra valores de aceleração X e Y
    char accel_str[16];
    snprintf(accel_str, sizeof(accel_str), "X:%.1f", data->accel[0]);
    ssd1306_draw_string(ssd, accel_str, 14, 41);
    
    snprintf(accel_str, sizeof(accel_str), "Y:%.1f", data->accel[1]);
    ssd1306_draw_string(ssd, accel_str, 14, 52);
    
    // Mostra valores de giroscópio X e Y
    char gyro_str[16];
    snprintf(gyro_str, sizeof(gyro_str), "X:%.1f", data->gyro[0]);
    ssd1306_draw_string(ssd, gyro_str, 73, 41);
    
    snprintf(gyro_str, sizeof(gyro_str), "Y:%.1f", data->gyro[1]);
    ssd1306_draw_string(ssd, gyro_str, 73, 52);
    
    ssd1306_send_data(ssd);
}

// Handler de interrupção
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    
    if (gpio == BUTTON_A && (current_time - last_time_btn_a > 300000)) {
        last_time_btn_a = current_time;
        button_a_pressed = true;
    }
    else if (gpio == BUTTON_B && (current_time - last_time_btn_b > 300000)) {
        last_time_btn_b = current_time;
        button_b_pressed = true;
    }
    else if (gpio == JOYSTICK_BTN && (current_time - last_time_joystick > 300000)) {
        last_time_joystick = current_time;
        joystick_pressed = true;
    }
}

int main() {
    stdio_init_all();
    sleep_ms(5000); // Espera para inicialização do serial
    
    // Inicializa o LED RGB
    gpio_init(LED_RED);
    gpio_init(LED_GREEN);
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    set_led_color(false, false, false); // Inicia com LED desligado

    // Inicializa o buzzer
    buzzer_init(BUZZER_PIN);
     uint slice_num_buzzer = pwm_gpio_to_slice_num(BUZZER_PIN);  

    // Inicializa o display OLED
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO_DISP, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializa o MPU6050
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    mpu6050_reset();

    // Configura botões com interrupções
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    
    gpio_init(JOYSTICK_BTN);
    gpio_set_dir(JOYSTICK_BTN, GPIO_IN);
    gpio_pull_up(JOYSTICK_BTN);

    gpio_set_irq_enabled(JOYSTICK_BTN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_A, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true); 
    gpio_set_irq_callback(&gpio_irq_handler);
    irq_set_enabled(IO_IRQ_BANK0, true);

    printf("Sistema inicializado. Aguardando comandos...\n");
    printf("1. Pressione Botão B (GPIO 6) para montar o cartão SD\n");
    printf("2. Pressione Botão A (GPIO 5) para iniciar gravação de 100 amostras\n");
    printf("3. Pressione Botão do Joystick (GPIO 22) para exibir amostras gravadas\n");
    
    MPU6050_Data sensor_data;
    memset(&sensor_data, 0, sizeof(sensor_data));
    
    absolute_time_t last_sample_time = get_absolute_time();
    const uint32_t sample_interval_ms = 100; // Intervalo entre amostras (100ms = 10Hz)
    
    while (true) {
        // Atualiza dados do MPU6050
        mpu6050_get_data(&sensor_data);
        
        // Tratamento dos botões (agora no loop principal)
        if (button_b_pressed) {
            button_b_pressed = false;
            printf("\nMontando cartão SD...\n");
            run_mount();
        }
        
        if (button_a_pressed && !is_recording) {
            button_a_pressed = false;
            if (!sd_mounted) {
                printf("\nERRO: SD card não montado. Monte o SD antes de gravar.\n");
            } else {
                is_recording = true;
                sample_count = 0;
                printf("\nIniciando gravação de %d amostras...\n", samples_to_record);
                create_csv_file();
            }
        }
        
        if (joystick_pressed) {
            joystick_pressed = false;
            printf("\nExibindo amostras gravadas...\n");
            read_csv_file();
        }

        // Grava dados se estiver no modo de gravação e passou o intervalo
        if (is_recording && absolute_time_diff_us(last_sample_time, get_absolute_time()) >= sample_interval_ms * 1000) {
            write_data_to_csv(&sensor_data);
            last_sample_time = get_absolute_time();
            
            printf("Amostra %lu: Accel(%.2f, %.2f, %.2f) Giro(%.1f, %.1f, %.1f)\n",
                   sample_count,
                   sensor_data.accel[0], sensor_data.accel[1], sensor_data.accel[2],
                   sensor_data.gyro[0], sensor_data.gyro[1], sensor_data.gyro[2]);
            
            // Verifica se já gravou as 100 amostras
            if (sample_count >= samples_to_record) {
                is_recording = false;
                set_led_color(false, true, false); // Volta para verde
                printf("\nGravação concluída. %d amostras gravadas.\n", samples_to_record);
            }
        }
        
        // Atualiza display
        update_display(&ssd, &sensor_data, is_recording, sample_count);
        
        sleep_ms(10); // Pequena pausa para reduzir carga da CPU
    }
    
    return 0;
}