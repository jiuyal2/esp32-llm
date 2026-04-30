#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_spiffs.h"
#include "esp_log.h"
#include "llm.h"
#include <driver/usb_serial_jtag.h>
#include <driver/usb_serial_jtag_vfs.h>

static const char *TAG = "MAIN";

// Set to 1 to auto-run a fixed prompt at startup (useful for debugging)
#define AUTO_PROMPT 1

void init_storage(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/data",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return;
    }
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS: total=%d used=%d", total, used);
}

void generate_complete_cb(float tk_s)
{
    printf("[%.2f tok/s]\n", tk_s);
}

void app_main(void)
{
    // Route stdin/stdout through the USB-Serial/JTAG peripheral (what idf.py monitor uses)
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&cfg);
    usb_serial_jtag_vfs_use_driver();

    init_storage();

    Transformer transformer;
    build_transformer(&transformer, NULL);
    int steps = transformer.config.seq_len;

    Tokenizer tokenizer;
    build_tokenizer(&tokenizer, "/data/tok512.bin", transformer.config.vocab_size);

    Sampler sampler;
    build_sampler(&sampler, transformer.config.vocab_size,
                  1.0f, 0.9f, (unsigned int)time(NULL));

    printf("\n\nESP32 TinyLLM  |  seq_len=%d\n", steps);
    printf("Enter a prompt and press Enter. Empty line = free generation.\n");

#if AUTO_PROMPT
    // auto-run a fixed prompt so we can observe model output without typing
    char fixed_prompt[] = "once upon a time";
    printf("\nAuto prompt: %s\n", fixed_prompt);
    generate(&transformer, &tokenizer, &sampler,
             fixed_prompt, steps, generate_complete_cb);
#endif

    char prompt[256];
    while (1) {
        printf("\n> ");
        fflush(stdout);

        // Read directly from USB-JTAG driver (bypasses stdio mutex that starves IDLE).
        // usb_serial_jtag_read_bytes blocks up to the timeout then yields, keeping WDT happy.
        size_t pos = 0;
        while (pos < sizeof(prompt) - 1) {
            uint8_t ch;
            int len = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(100));
            if (len <= 0) continue;
            int c = (int)ch;
            if (c == '\r' || c == '\n') {
                putchar('\n');
                fflush(stdout);
                break;
            }
            if (c == 127 || c == '\b') {
                if (pos > 0) { pos--; printf("\b \b"); fflush(stdout); }
                continue;
            }
            putchar(c);
            fflush(stdout);
            prompt[pos++] = (char)c;
        }
        prompt[pos] = '\0';

        printf("\n");
        generate(&transformer, &tokenizer, &sampler,
                 pos > 0 ? prompt : NULL, steps, generate_complete_cb);
    }
}
