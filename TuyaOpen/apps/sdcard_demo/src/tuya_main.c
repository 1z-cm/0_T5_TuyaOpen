#include "tal_api.h"
#include "tkl_fs.h"
#include "tkl_memory.h"
#include "tkl_output.h"
#include "tkl_pinmux.h"
#include <string.h>
#include <stdio.h>

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

#define SDCARD_MOUNT_PATH "/sdcard"
#define SDCARD_TEST_FILE  "/sdcard/test.txt"
#define SDCARD_RANDOM_FILE "/sdcard/random.txt"
#define SDCARD_READ_SIZE  256

#define EXAMPLE_SD_CLK_PIN TUYA_GPIO_NUM_14
#define EXAMPLE_SD_CMD_PIN TUYA_GPIO_NUM_15
#define EXAMPLE_SD_D0_PIN  TUYA_GPIO_NUM_16
#define EXAMPLE_SD_D1_PIN  TUYA_GPIO_NUM_17
#define EXAMPLE_SD_D2_PIN  TUYA_GPIO_NUM_18
#define EXAMPLE_SD_D3_PIN  TUYA_GPIO_NUM_19

static THREAD_HANDLE s_app_thread = NULL;
static THREAD_HANDLE s_sd_thread = NULL;
static char s_write_buf[128] = {0};
static char s_read_buf[128] = {0};

static void __sdcard_force_pinmux(void)
{
    PR_NOTICE("SD stage 0: force SD pinmux to P14~P19");
    PR_NOTICE("force SD pinmux: CLK=P14 CMD=P15 D0=P16 D1=P17 D2=P18 D3=P19");

    tkl_io_pinmux_config(EXAMPLE_SD_CLK_PIN, TUYA_SDIO_HOST_CLK);
    tkl_io_pinmux_config(EXAMPLE_SD_CMD_PIN, TUYA_SDIO_HOST_CMD);
    tkl_io_pinmux_config(EXAMPLE_SD_D0_PIN, TUYA_SDIO_HOST_D0);
    tkl_io_pinmux_config(EXAMPLE_SD_D1_PIN, TUYA_SDIO_HOST_D1);
    tkl_io_pinmux_config(EXAMPLE_SD_D2_PIN, TUYA_SDIO_HOST_D2);
    tkl_io_pinmux_config(EXAMPLE_SD_D3_PIN, TUYA_SDIO_HOST_D3);
}

static void __sdcard_list_root(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_DIR dir = NULL;
    TUYA_FILEINFO info = NULL;
    const char *name = NULL;
    BOOL_T is_dir = FALSE;

    rt = tkl_dir_open(SDCARD_MOUNT_PATH, &dir);
    if (rt != OPRT_OK || dir == NULL) {
        PR_ERR("open %s failed: %d", SDCARD_MOUNT_PATH, rt);
        return;
    }

    PR_NOTICE("list %s begin", SDCARD_MOUNT_PATH);
    while (1) {
        rt = tkl_dir_read(dir, &info);
        if (rt != OPRT_OK) {
            PR_ERR("read dir failed: %d", rt);
            break;
        }

        rt = tkl_dir_name(info, &name);
        if (rt != OPRT_OK || name == NULL) {
            break;
        }

        rt = tkl_dir_is_directory(info, &is_dir);
        if (rt != OPRT_OK) {
            is_dir = FALSE;
        }

        PR_NOTICE("  %s%s", name, is_dir ? "/" : "");
    }

    tkl_dir_close(dir);
    PR_NOTICE("list %s end", SDCARD_MOUNT_PATH);
}

static void __sdcard_random_rw_test(void)
{
    TUYA_FILE file_hdl = NULL;
    uint32_t write_len = 0;
    uint32_t ret_len = 0;
    uint32_t read_len = 0;
    int random_value = tal_system_get_random(0xFFFFFFFF);

    memset(s_write_buf, 0, sizeof(s_write_buf));
    memset(s_read_buf, 0, sizeof(s_read_buf));

    snprintf(s_write_buf, sizeof(s_write_buf), "random value: %d", random_value);

    file_hdl = tkl_fopen(SDCARD_RANDOM_FILE, "w");
    if (file_hdl == NULL) {
        PR_ERR("open file %s failed", SDCARD_RANDOM_FILE);
        return;
    }

    write_len = strlen(s_write_buf);
    PR_NOTICE("Write file content: %s", s_write_buf);
    ret_len = tkl_fwrite(s_write_buf, write_len, file_hdl);
    if (ret_len != write_len) {
        PR_ERR("write file %s failed: %u/%u", SDCARD_RANDOM_FILE, ret_len, write_len);
        tkl_fclose(file_hdl);
        return;
    }

    tkl_fclose(file_hdl);
    file_hdl = NULL;

    file_hdl = tkl_fopen(SDCARD_RANDOM_FILE, "r");
    if (file_hdl == NULL) {
        PR_ERR("open file %s for read failed", SDCARD_RANDOM_FILE);
        return;
    }

    read_len = tkl_fread(s_read_buf, sizeof(s_read_buf), file_hdl);
    if ((int)read_len <= 0) {
        PR_ERR("read file %s failed: %u", SDCARD_RANDOM_FILE, read_len);
        tkl_fclose(file_hdl);
        return;
    }

    if (strncmp(s_write_buf, s_read_buf, read_len) != 0) {
        PR_ERR("---> fail: compare random file failed");
    } else {
        PR_NOTICE("---> success: compare random file success");
    }

    tkl_fclose(file_hdl);
}

static void __sdcard_read_test_file(void)
{
    char *buf = NULL;
    TUYA_FILE file = NULL;
    INT_T read_len = 0;

    file = tkl_fopen(SDCARD_TEST_FILE, "r");
    if (file == NULL) {
        PR_NOTICE("optional file not found: %s", SDCARD_TEST_FILE);
        PR_NOTICE("create %s on the SD root if you want fixed-content read test", SDCARD_TEST_FILE);
        return;
    }

    buf = tkl_system_malloc(SDCARD_READ_SIZE + 1);
    if (buf == NULL) {
        PR_ERR("alloc read buffer failed");
        tkl_fclose(file);
        return;
    }

    memset(buf, 0, SDCARD_READ_SIZE + 1);
    read_len = tkl_fread(buf, SDCARD_READ_SIZE, file);
    if (read_len < 0) {
        PR_ERR("read test file failed: %d", read_len);
    } else {
        buf[read_len] = '\0';
        PR_NOTICE("read %s ok, len=%d", SDCARD_TEST_FILE, read_len);
        PR_NOTICE("file content begin");
        PR_NOTICE("%s", buf);
        PR_NOTICE("file content end");
    }

    tkl_fclose(file);
    tkl_system_free(buf);
}

static void __sdcard_demo_thread(void *arg)
{
    OPERATE_RET rt = OPRT_OK;

    (void)arg;

    __sdcard_force_pinmux();

    PR_NOTICE("SD stage 1: mount %s", SDCARD_MOUNT_PATH);
    rt = tkl_fs_mount(SDCARD_MOUNT_PATH, DEV_SDCARD);
    if (rt != OPRT_OK) {
        PR_ERR("mount %s failed: %d", SDCARD_MOUNT_PATH, rt);
        goto exit;
    }

    PR_NOTICE("SD stage 2: random write/read/compare");
    __sdcard_random_rw_test();

    PR_NOTICE("SD stage 3: list root directory");
    __sdcard_list_root();

    PR_NOTICE("SD stage 4: read optional fixed file %s", SDCARD_TEST_FILE);
    __sdcard_read_test_file();

    PR_NOTICE("SD card demo finished");

exit:
    while (1) {
        tal_system_sleep(1000);
    }
}

void user_main(void)
{
    OPERATE_RET ret = OPRT_OK;
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 4096,
        .priority = THREAD_PRIO_2,
        .thrdname = "sdcard_demo",
    };

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("SDIO pins: P14 CLK, P15 CMD, P16 D0, P17 D1, P18 D2, P19 D3");

    ret = tal_thread_create_and_start(&s_sd_thread, NULL, NULL, __sdcard_demo_thread, NULL, &thread_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("create SD demo thread failed: %d", ret);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
}
#else
static void __app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(s_app_thread);
    s_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 4096,
        .priority = THREAD_PRIO_1,
        .thrdname = "tuya_app_main",
    };

    tal_thread_create_and_start(&s_app_thread, NULL, NULL, __app_thread, NULL, &thread_cfg);
}
#endif
