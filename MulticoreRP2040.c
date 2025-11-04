#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "lib/ssd1306.h"

//bibliotecas dos sensores
#include "lib/aht20.h"
#include "lib/bmp280.h"

//dados importantes para o display
#define I2C_PORT i2c1 //Comucação I2C
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
//Sensores
#define I2C_PORT_SENSOR i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA_SENSOR 0                   // 0 ou 2
#define I2C_SCL_SENSOR 1                   // 1 ou 3

//Pinos leds
#define led_red 13
#define led_blue 12
#define led_green 11

volatile uint32_t pressao = 0;
volatile uint32_t umidade = 0;

//enum para permitir diferenciações de dados na fifo
typedef enum{
    AHT20_HUM,
    BMP280_PRES
}sensores;

void init_leds();
void show_display(uint32_t valorPres, sensores valorHum);     //passa como parâmetro os dados dos sensores
void push_fifo_sensor(sensores sensor, uint32_t leitura);
void core1_interrupt_handler();


void core1_entry(){  //recebe dados e mostra no display e aciona leds

    //posso inicializar aqui?
    init_leds();

    //inicializa display
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_t ssd;                                                // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    bool cor = true;

    multicore_fifo_clear_irq();         //Limpa FIFO interrupt

    //configura função de interrupção para ler dados da fifo e exibir no display
    irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_interrupt_handler);
    irq_set_enabled(SIO_IRQ_PROC1,true);

    while (true)
    {
        //verifica se há dados disponível para leitura
        char str_pressao[5]; //para a pressão
        sprintf(str_pressao,"%d",pressao);
        char str_umidade[5]; //para a pressão
        sprintf(str_umidade,"%d",umidade);
        

        //  Atualiza o conteúdo do display com as informações necessárias
        ssd1306_fill(&ssd, !cor);                          // Limpa o display
        ssd1306_rect(&ssd, 3, 1, 122, 61, cor, !cor);      // Desenha um retângulo ok
        ssd1306_line(&ssd, 3, 33, 123, 33, cor);           // Desenha uma linha

        ssd1306_draw_string(&ssd, "Umi.:", 10, 15);  
        ssd1306_draw_string(&ssd, str_umidade, 60, 15); 
        ssd1306_draw_string(&ssd, "%", 100, 15); 

        ssd1306_draw_string(&ssd, "Pres.:", 10, 45); 
        ssd1306_draw_string(&ssd, str_pressao, 60, 45);
        ssd1306_draw_string(&ssd, "kPa", 90, 45);

        ssd1306_send_data(&ssd);
        sleep_ms(700);
    }
    

}

int main()  //main do core 0, realiza a leitura dos sensores
{
    stdio_init_all();

    //inicializa core1
    multicore_launch_core1(core1_entry);

    //Pinos para i2c dos sensores
    i2c_init(I2C_PORT_SENSOR, 400 * 1000);
    gpio_set_function(I2C_SDA_SENSOR, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_SENSOR, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_SENSOR);
    gpio_pull_up(I2C_SCL_SENSOR);

    //inicializa sensores
    //BMP280
    bmp280_init(I2C_PORT_SENSOR);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT_SENSOR, &params);
    //AHT20
    aht20_reset(I2C_PORT_SENSOR);
    aht20_init(I2C_PORT_SENSOR);
    //--------------------------------------------------------
    //Variáveis para os dados dos sensores
    // Estrutura para armazenar os dados do sensor
    AHT20_Data leituraAHT20;
    int32_t temperatura_bmp;
    int32_t pressao_bmp;
    int32_t pressao_final = 0;
    float umidade =0;

    while (true) {
        
        //leitura bmp
        bmp280_read_raw(I2C_PORT_SENSOR, &temperatura_bmp, &pressao_bmp);
        int32_t convert_temperatura_bmp = bmp280_convert_temp(temperatura_bmp, &params);
        pressao_final = ((bmp280_convert_pressure(pressao_bmp, temperatura_bmp, &params))/1000);  //ver necessidade de offset
        while(!multicore_fifo_wready){} //espera liberar espaço para não perder o dado de leitura - deveria?
        push_fifo_sensor(BMP280_PRES, pressao_final);

        //leitura aht20
        if (aht20_read(I2C_PORT_SENSOR, &leituraAHT20)) //tirar essa depois depois que testar
        {
            umidade = leituraAHT20.humidity;
            while(!multicore_fifo_wready){} //espera liberar espaço para não perder o dado de leitura - deveria?
            push_fifo_sensor(AHT20_HUM, (uint32_t) umidade);
        }
        else
        {
            printf("\nErro na leitura do AHT10!\n");
        };

        sleep_ms(1000); //pausa porque os dados não mudam tão rapidamente

    }
}

void init_leds(){

    gpio_init(led_blue);
    gpio_init(led_red);
    gpio_init(led_green);

    gpio_set_dir(led_blue, GPIO_OUT);
    gpio_set_dir(led_red, GPIO_OUT);
    gpio_set_dir(led_green, GPIO_OUT);

    gpio_put(led_red, 0);
    gpio_put(led_red, 0);
    gpio_put(led_red, 0);

}

void push_fifo_sensor(sensores sensor, uint32_t leitura){
    if(sensor == AHT20_HUM){
        multicore_fifo_push_blocking(AHT20_HUM);
        multicore_fifo_push_blocking(leitura);
    }else if(sensor==BMP280_PRES){
        multicore_fifo_push_blocking(BMP280_PRES);
        multicore_fifo_push_blocking(leitura);
    }
};

void core1_interrupt_handler(){

    uint32_t valorpres;
    uint32_t valorhum;

    while(multicore_fifo_rvalid()){
        sensores sensor = multicore_fifo_pop_blocking();
        if(sensor==BMP280_PRES){
            pressao = multicore_fifo_pop_blocking();
            printf("Pressão: %d\n", pressao);
        }else{
            umidade = multicore_fifo_pop_blocking();
            printf("Umidade: %d\n", umidade);
        }
        
    }

    multicore_fifo_clear_irq();

}