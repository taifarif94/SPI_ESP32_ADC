# include <stdio .h>
# include <stdlib .h>
# include <string .h>
# include " freertos / FreeRTOS .h"
# include " freertos / task .h"
# include " esp_system .h"
# include " driver / spi_master .h"
# include " driver / gpio .h"
# include " esp_log .h"
# include " esp_err .h"
# include <sys/ unistd .h>
# include <sys/ stat .h>
# include " esp_vfs_fat .h"
# include " driver / sdspi_host .h"
# include " sdmmc_cmd .h"
# include " sdkconfig .h"
# include " driver / sdmmc_host .h"
static const char * TAG1 = "ADC";
// There are two HOSTs available , the VSPI host is used
// due to the flexibility of the pins .
# define HOST VSPI_HOST
// The MISO pin is defined , which gets the data from the slave .
# define PIN_NUM_MISO 19
// MOSI pin is not used as no command bits are
// required by the ADC .
# define PIN_NUM_MOSI -1
# define PIN_NUM_CLK 18
// The CS Pin is manually defined so it is declared
// here as not in use .
# define PIN_NUM_CS -1
// The clock is defined a max of 80 mega Hertz .
# define CLOCK 80 * 1000 * 1000
# define MOUNT_POINT "/ sdcard "
// Pin for the CS signal defined .
# define GPIO_OUTPUT_IO_0 17
// Pin for the Pushbutton defined .
# define GPIO_INPUT_IO_0 22
// CS signal pin masked ( register position ).
# define GPIO_OUTPUT_PIN_SEL (1 ULL << GPIO_OUTPUT_IO_0 )
// Push button pin masked ( register position ).
# define GPIO_INPUT_PIN_SEL (1 ULL << GPIO_INPUT_IO_0 )
void app_main ( void )
{
// variable to store and check the state of the pin
uint8_t pin = 1;
// variable to check if button already pressed
// and to store the state
uint8_t test = 1;
// Code for the GPIO pins taken from [13] and
// edited as per the requirement .
//A GPIO configuration structure for setting the
// Input pin for the button .
gpio_config_t io_conf1 = {};
// bit mask of the pins ,
io_conf1 . pin_bit_mask = GPIO_INPUT_PIN_SEL ;
// set as input mode
io_conf1 . mode = GPIO_MODE_INPUT ;
// enable pull - down mode
io_conf1 . pull_down_en = 1;
// disable pull -up mode
io_conf1 . pull_up_en = 0;
// interrupt disable
io_conf1 . intr_type = GPIO_INTR_DISABLE ;
// configure GPIO with the given settings
gpio_config (& io_conf1 );
gpio_config_t io_conf ;
// disable interrupt
io_conf . intr_type = GPIO_INTR_DISABLE ;
// set as output mode
io_conf . mode = GPIO_MODE_OUTPUT ;
// bit mask of the pins that you want to set ,e.g. GPIO15
io_conf . pin_bit_mask = GPIO_OUTPUT_PIN_SEL ;
// disable pull - down mode
io_conf . pull_down_en = 0;
// disable pull -up mode
io_conf . pull_up_en = 0;
// configure GPIO with the given settings
gpio_config (& io_conf );
// error variables created to store and evaluate errors .
esp_err_t ret;
esp_err_t retone ;
// variable to store the data from the slave device .
uint32_t rxdata = 0;
// array to store value of each transaction .
uint32_t ADC_Value [300] = {0};
// configuration for the SD card .
// Code for SD card taken from [12].
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
# ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
. format_if_mount_failed = true ,
# else
. format_if_mount_failed = false ,
# endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
. max_files = 5,
. allocation_unit_size = 16 * 1024};
sdmmc_card_t * card ;
const char mount_point [] = MOUNT_POINT ;
ESP_LOGI (TAG1 , " Initializing SD card ");
ESP_LOGI (TAG1 , " Using SDMMC peripheral ");
sdmmc_host_t host = SDMMC_HOST_DEFAULT ();
sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT ();
// To use 1- line SD mode , uncomment the following line :
slot_config . width = 1;
// The following lines are internally pulled up
// to satisfy the requirements of the SD card interface .
// CMD , needed in 4- and 1- line modes
gpio_set_pull_mode (15 , GPIO_PULLUP_ONLY );
// D0 , needed in 4- and 1- line modes
gpio_set_pull_mode (2, GPIO_PULLUP_ONLY );
// D1 , needed in 4- line mode only
gpio_set_pull_mode (4, GPIO_PULLUP_ONLY );
// D2 , needed in 4- line mode only
gpio_set_pull_mode (12 , GPIO_PULLUP_ONLY );
// D3 , needed in 4- and 1- line modes
gpio_set_pull_mode (13 , GPIO_PULLUP_ONLY );
// The following lines check for the SD card error :
retone = esp_vfs_fat_sdmmc_mount ( mount_point , &host ,
& slot_config , & mount_config , & card );
if ( retone != ESP_OK )
{
if ( retone == ESP_FAIL )
{
ESP_LOGE (TAG1 , " Failed to mount filesystem . "
"If you want the card to be formatted ,
set the EXAMPLE_FORMAT_IF_MOUNT_FAILED
menuconfig option .");
}
else
{
ESP_LOGE (TAG1 , " Failed to initialize the card (%s). "
" Make sure SD card lines have pull -up
resistors in place .",
esp_err_to_name ( retone ));
}
return ;
}
// Prints the SD card information .
sdmmc_card_print_info ( stdout , card );
ESP_LOGI (TAG1 , " Opening file ");
// A new file is created .
FILE *f = fopen ( MOUNT_POINT "/ hello . txt ", "w");
if (f == NULL )
{
ESP_LOGE (TAG1 , " Failed to open file for writing ");
return ;
}
// Input the first line of the text file
fprintf (f, " Hello %s!\n", card -> cid . name );
fclose (f);
ESP_LOGI (TAG1 , " File written ");
// spi device handle
spi_device_handle_t spi ;
// bus configuration
spi_bus_config_t bus_cfg = {
. mosi_io_num = PIN_NUM_MOSI ,
. miso_io_num = PIN_NUM_MISO ,
. sclk_io_num = PIN_NUM_CLK ,
. quadwp_io_num = -1,
. quadhd_io_num = -1,
. max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE ,
};
// device configuration
spi_device_interface_config_t sensor = {
. command_bits = 0,
. address_bits = 0,
. dummy_bits = 0,
. mode = 0, // SPI mode 0 is used with CPOL =0 and CPHA =0.
. duty_cycle_pos = 128 ,
. cs_ena_pretrans = 0, // 0 not used
. cs_ena_posttrans = 0, // 0 not used
. flags = 0, // 0 not used
. queue_size = 1,
. pre_cb = NULL ,
. post_cb = NULL ,
. clock_speed_hz = CLOCK ,
. input_delay_ns = 0,
. spics_io_num = PIN_NUM_CS };
// spi transaction struct
spi_transaction_t transaction = {
. flags = 0,
. cmd = 0,
. addr = 0,
. length = 16,
. rxlength = 16,
. user = NULL ,
. tx_buffer = NULL ,
. rx_buffer = & rxdata
};
// initialize the bus with DMA disabled . 0 means DMA disabled .
ret = spi_bus_initialize (HOST , & bus_cfg , 0);
// Check for error
ESP_ERROR_CHECK (ret );
// add the ADC as a device .
ret = spi_bus_add_device (HOST , & sensor , & spi );
ret = spi_device_acquire_bus (spi , portMAX_DELAY );
while (1)
{
if ( test == 1)
{
// check if button pressed
pin = gpio_get_level ( GPIO_NUM_22 );
if ( pin == 0)
{
test = 2;
// debounce delay pf 300 miliseconds
vTaskDelay (300 / portTICK_PERIOD_MS );
}
}
if ( test == 2)
{
for ( int i = 0; i < 300; i++)
{
gpio_set_level ( GPIO_OUTPUT_IO_0 , 0);
spi_device_polling_transmit (spi , & transaction );
ADC_Value [i] = rxdata ;
gpio_set_level ( GPIO_OUTPUT_IO_0 , 1);
}
for ( int j = 0; j < 300; j++)
{
// reads from the lower and upper nibble
// to combine it into a 16 bit integer
ADC_Value [j] = SPI_SWAP_DATA_RX ( ADC_Value [j], 16);
// shifts the bits one time to the right to get
// rid of one trailing zero as required by the ADC
ADC_Value [j] = ADC_Value [j] >> 1;
// gets rid of the leading leading zero
// as required by the ADC
ADC_Value [j] = ADC_Value [j] & 0 x3FFF ;
f = fopen ( MOUNT_POINT "/ hello . txt ", "a");
fprintf (f, "%d\n", ADC_Value [j ]);
fclose (f);
}
// state changed so it checks for the push - button again
test = 1;
vTaskDelay (1000 / portTICK_PERIOD_MS );
}
}
esp_vfs_fat_sdcard_unmount ( mount_point , card );
ESP_LOGI (TAG1 , " Card unmounted ");
spi_device_release_bus ( spi );
}