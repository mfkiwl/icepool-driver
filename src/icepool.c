#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "icepool.h"

IcepoolContext* icepool_new()
{
    IcepoolContext* ctx = (IcepoolContext*) malloc(sizeof(IcepoolContext));

    if (ctx) {
        icepool_init(ctx);
    }

    return ctx;
}

void icepool_free(IcepoolContext* ctx)
{
    if (ctx) {
        icepool_deinit(ctx);
        free(ctx);
    }
}

void icepool_init(IcepoolContext* ctx)
{
    // Create FTDI context
    ctx->ftdi = ftdi_new();

    // Connect to USB device
    if (ftdi_usb_open(ctx->ftdi, 0x0403, 0x6010) == 0) {
        ctx->ftdi_interface = ICEPOOL_FTDI_FT2232H;
        printf("Connected to an FT2232H (0403:6010).\n");
    }
    else if (ftdi_usb_open(ctx->ftdi, 0x0403, 0x6014) == 0) {
        ctx->ftdi_interface = ICEPOOL_FTDI_FT232H;
        printf("Connected to an FT232H (0403:6014).\n");
    }
    else {
        fprintf(stderr, "Could not find a FT232H (0403:6014) or FT2232H (0403:6010).\n");
        exit(EXIT_FAILURE);
    }

    // From D2XX code:
    //   FT_SetUSBParameters(, 64,64)
    //   FT_SetChars(, 0,0,0,0)
    //   FT_Timeouts(, 1000,1000)
    //   FT_SetLatencyTimer(, 10)
    //   FT_SetFlowControl(, FT_FLOW_NONE, 0, 0)
    //   FT_SetBitMode(, 0,0) // "Reset MPSSE controller"
    //   FT_SetBitMode(, 0xFF,0x2)

    // Select interface
    ftdi_set_interface(ctx->ftdi, INTERFACE_A);

    // Reset
    ftdi_usb_reset(ctx->ftdi);

    // Purge buffers
    ftdi_usb_purge_buffers(ctx->ftdi);

    // Set latency timer
    ftdi_set_latency_timer(ctx->ftdi, 1); // 1 kHz (fastest)

    // Set bit mode to MPSSE
    ftdi_set_bitmode(ctx->ftdi, 0x00, BITMODE_RESET); // Reset
    ftdi_set_bitmode(ctx->ftdi, 0xFF, BITMODE_MPSSE);

    // Set clock divider (6 MHz)
    {
        // Enable
        uint8_t d = 0x8B;
        ftdi_write_data(ctx->ftdi, &d, 1);

        // Set
        uint8_t command[] = {
            0x86,
            0x00,
            0x00,
        };
        
        ftdi_write_data(ctx->ftdi, command, 3);
    }

    // Set up GPIO state
    ctx->gpio_state_lower.data = 0;
    ctx->gpio_state_lower.dir = 0;
    ctx->gpio_state_upper.data = 0;
    ctx->gpio_state_upper.dir = 0;

    // ... sck0
    ctx->gpio_state_lower.dir |= 1 << ICEPOOL_SPI_SCK0_PIN;
    
    // ... sdo0
    ctx->gpio_state_lower.dir |= 1 << ICEPOOL_SPI_SDO0_PIN;

    // ... ~cs0
    ctx->gpio_state_lower.data  |= 1 << ICEPOOL_SPI_CS0_PIN;
    ctx->gpio_state_lower.dir |= 1 << ICEPOOL_SPI_CS0_PIN;

    // ... cdone (input)

    // ... creset_b
    ctx->gpio_state_lower.dir |= 1 << 7;
    ctx->gpio_state_lower.data |= 1 << 7;

    // ... sck1
    ctx->gpio_state_upper.dir |= 1 << ICEPOOL_SPI_SCK1_PIN;

    // ... sdo1
    ctx->gpio_state_upper.dir |= 1 << ICEPOOL_SPI_SDO1_PIN;

    // ... sdi1 (input)
    
    // ... ~cs1
    ctx->gpio_state_upper.data |= 1 << ICEPOOL_SPI_CS1_PIN;
    ctx->gpio_state_upper.dir |= 1 << ICEPOOL_SPI_CS1_PIN;

    // ... ready (input)

    // ... ~rw
    ctx->gpio_state_upper.data |= 1 << 7;
    ctx->gpio_state_upper.dir |= 1 << 7;

    // Update lower GPIO byte
    {
        uint8_t command[] = {
            0x80,
            ctx->gpio_state_lower.data,
            ctx->gpio_state_lower.dir
        };

        ftdi_write_data(ctx->ftdi, command, 3);
    }

    // Update upper GPIO byte
    {
        uint8_t command[] = {
            0x82,
            ctx->gpio_state_upper.data,
            ctx->gpio_state_upper.dir
        };

        ftdi_write_data(ctx->ftdi, command, 3);
    }
}

void icepool_deinit(IcepoolContext* ctx)
{
    // Reset device
    ftdi_disable_bitbang(ctx->ftdi);

    // Close FTDI connection
    ftdi_usb_close(ctx->ftdi);

    // Free FTDI context
    ftdi_free(ctx->ftdi);
    
    // Zero out context
    memset(ctx, 0, sizeof(IcepoolContext));
}

void icepool_spi_assert_shared(IcepoolContext* ctx)
{
    icepool_gpio_set_bit_lower(ctx, ICEPOOL_SPI_SCK0_PIN, 0);
    icepool_gpio_set_bit_lower(ctx, ICEPOOL_SPI_CS0_PIN, 0);
}

void icepool_spi_deassert_shared(IcepoolContext* ctx)
{
    icepool_gpio_set_bit_lower(ctx, ICEPOOL_SPI_SCK0_PIN, 0);
    icepool_gpio_set_bit_lower(ctx, ICEPOOL_SPI_CS0_PIN, 1);
}

void icepool_spi_assert_daisy(IcepoolContext* ctx)
{
    icepool_gpio_set_bit_upper(ctx, ICEPOOL_SPI_SCK1_PIN, 0);
    icepool_gpio_set_bit_upper(ctx, ICEPOOL_SPI_CS1_PIN, 0);
}

void icepool_spi_deassert_daisy(IcepoolContext* ctx)
{
    icepool_gpio_set_bit_upper(ctx, ICEPOOL_SPI_SCK1_PIN, 0);
    icepool_gpio_set_bit_upper(ctx, ICEPOOL_SPI_CS1_PIN, 1);
}

void icepool_spi_write_shared(IcepoolContext* ctx, uint8_t data[], size_t data_length)
{
    // TODO
}

void icepool_spi_read_daisy(IcepoolContext* ctx, uint8_t data[], size_t data_length)
{
    
    // TODO
}

void icepool_spi_write_daisy(IcepoolContext* ctx, uint8_t data[], size_t data_length)
{
    // TODO
}

void icepool_spi_exchange_daisy(IcepoolContext* ctx, uint8_t data_out[], uint8_t data_in[], size_t data_length)
{
    // Mode 0,0 / msb-first
    for (size_t n = 0; n < data_length; n++)
    {
        for (size_t i = 0; i < 8; i++)
        {
            // SDO1 out
            icepool_gpio_set_bit_upper(ctx, ICEPOOL_SPI_SDO1_PIN, (data_out[n] >> (7-i)) & 0x01);

            // Wait half cycle
            // TODO usleep

            // SCK1 up
            icepool_gpio_set_bit_upper(ctx, ICEPOOL_SPI_SCK1_PIN, 1);
            
            // Sample SDI1
            data_in[n] <<= 1;
            data_in[n] |= icepool_gpio_get_bit_upper(ctx, ICEPOOL_SPI_SDI1_PIN);

            // Wait half cycle
            // TODO usleep

            // SCK1 down
            icepool_gpio_set_bit_upper(ctx, ICEPOOL_SPI_SCK1_PIN, 0);
        }
    }
}

void icepool_gpio_set_bit_lower(IcepoolContext* ctx, uint8_t pin, bool value)
{
    if (value) {
        // Set bit
        ctx->gpio_state_lower.data |= (1 << pin);
    }
    else {
        // Clear bit
        ctx->gpio_state_lower.data &= ~(1 << pin);
    }

    uint8_t command[] = {
        0x80,
        ctx->gpio_state_lower.data,
        ctx->gpio_state_lower.dir
    };

    ftdi_write_data(ctx->ftdi, command, 3);
}

void icepool_gpio_set_bit_upper(IcepoolContext* ctx, uint8_t pin, bool value)
{
    // Set bit
    ctx->gpio_state_upper.data |= 1 << pin;
    
    if (!value) {
        // Clear bit
        ctx->gpio_state_upper.data ^= 1 << pin;
    }

    uint8_t command[] = {
        0x82,
        ctx->gpio_state_upper.data,
        ctx->gpio_state_upper.dir
    };

    ftdi_write_data(ctx->ftdi, command, 3);
}

uint8_t icepool_gpio_get_bit_lower(IcepoolContext* ctx, uint8_t pin)
{
    uint8_t command[] = {
        0x81
    };
    
    uint8_t result = 0;

    ftdi_write_data(ctx->ftdi, command, 1);
    
    // FIXME infinite loop?
    while(ftdi_read_data(ctx->ftdi, &result, 1) == 0);

    return (result >> pin) & 1;
}

uint8_t icepool_gpio_get_bit_upper(IcepoolContext* ctx, uint8_t pin)
{
    uint8_t command[] = {
        0x83
    };

    uint8_t result = 0;

    ftdi_write_data(ctx->ftdi, command, 1);
    
    // FIXME infinite loop?
    while(ftdi_read_data(ctx->ftdi, &result, 1) == 0);

    return (result >> pin) & 1;
}