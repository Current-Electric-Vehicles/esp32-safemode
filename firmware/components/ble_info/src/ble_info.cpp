#include "ble_info.h"

#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <cstdio>
#include <cstring>

namespace safemode
{
namespace ble
{

static constexpr const char* kTag = "ble_info";
static constexpr size_t kMaxNameLength = 29;
static constexpr size_t kMaxJsonLength = 256;

// Service UUID:        5afe0000-2026-4d3e-b9c1-7fa8c4d6e8a1
// Info characteristic: 5afe0001-2026-4d3e-b9c1-7fa8c4d6e8a1
// (BLE_UUID128_INIT takes bytes in reverse order)
static const ble_uuid128_t kServiceUuid =
    BLE_UUID128_INIT(0xa1, 0xe8, 0xd6, 0xc4, 0xa8, 0x7f, 0xc1, 0xb9,
                     0x3e, 0x4d, 0x26, 0x20, 0x00, 0x00, 0xfe, 0x5a);

static const ble_uuid128_t kInfoCharUuid =
    BLE_UUID128_INIT(0xa1, 0xe8, 0xd6, 0xc4, 0xa8, 0x7f, 0xc1, 0xb9,
                     0x3e, 0x4d, 0x26, 0x20, 0x01, 0x00, 0xfe, 0x5a);

// Cached info — the JSON payload is rebuilt once at start.
static Info sInfo;
static char sJsonBuf[kMaxJsonLength];
static size_t sJsonLen = 0;
static std::string sDeviceName;

static uint16_t sInfoCharHandle = 0;

static int gapEventHandler(struct ble_gap_event* event, void* arg);
static int gattAccessHandler(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt* ctxt, void* arg);

static struct ble_gatt_chr_def sCharacteristics[] = {
    {
        .uuid = &kInfoCharUuid.u,
        .access_cb = gattAccessHandler,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = &sInfoCharHandle,
        .cpfd = nullptr,
    },
    {
        .uuid = nullptr,
        .access_cb = nullptr,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = 0,
        .min_key_size = 0,
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
};

static const struct ble_gatt_svc_def sServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &kServiceUuid.u,
        .includes = nullptr,
        .characteristics = sCharacteristics,
    },
    {
        .type = 0,
        .uuid = nullptr,
        .includes = nullptr,
        .characteristics = nullptr,
    },
};

static void buildJson()
{
    sJsonLen = std::snprintf(sJsonBuf, sizeof(sJsonBuf),
        "{\"ssid\":\"%s\",\"password\":\"%s\",\"ip\":\"%s\",\"version\":\"%s\"}",
        sInfo.ssid.c_str(),
        sInfo.password.c_str(),
        sInfo.ipAddress.c_str(),
        sInfo.firmwareVersion.c_str());
    if (sJsonLen >= sizeof(sJsonBuf))
    {
        sJsonLen = sizeof(sJsonBuf) - 1;
        sJsonBuf[sJsonLen] = '\0';
    }
}

static void startAdvertising()
{
    struct ble_gap_adv_params advParams = {};
    advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
    advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;

    struct ble_hs_adv_fields advFields = {};
    advFields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    advFields.uuids128 = const_cast<ble_uuid128_t*>(&kServiceUuid);
    advFields.num_uuids128 = 1;
    advFields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&advFields);
    if (rc != 0)
    {
        ESP_LOGE(kTag, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    struct ble_hs_adv_fields rspFields = {};
    rspFields.name = reinterpret_cast<const uint8_t*>(sDeviceName.c_str());
    rspFields.name_len = sDeviceName.length();
    rspFields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rspFields);
    if (rc != 0)
    {
        ESP_LOGE(kTag, "ble_gap_adv_rsp_set_fields failed: %d", rc);
        return;
    }

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER,
                           &advParams, gapEventHandler, nullptr);
    if (rc != 0 && rc != BLE_HS_EALREADY)
    {
        ESP_LOGE(kTag, "ble_gap_adv_start failed: %d", rc);
        return;
    }

    ESP_LOGI(kTag, "BLE info service advertising (name=%s)", sDeviceName.c_str());
}

static int gapEventHandler(struct ble_gap_event* event, void* /*arg*/)
{
    switch (event->type)
    {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(kTag, "Client connected (status=%d)", event->connect.status);
            if (event->connect.status != 0)
            {
                startAdvertising();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(kTag, "Client disconnected (reason=%d)", event->disconnect.reason);
            startAdvertising();
            break;
        default:
            break;
    }
    return 0;
}

static int gattAccessHandler(uint16_t /*conn_handle*/, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt* ctxt, void* /*arg*/)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR && attr_handle == sInfoCharHandle)
    {
        int rc = os_mbuf_append(ctxt->om, sJsonBuf, sJsonLen);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static void onSync()
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0)
    {
        ESP_LOGE(kTag, "ble_hs_util_ensure_addr failed: %d", rc);
        return;
    }
    uint8_t addrType;
    rc = ble_hs_id_infer_auto(0, &addrType);
    if (rc != 0)
    {
        ESP_LOGE(kTag, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }
    startAdvertising();
}

static void hostTask(void* /*param*/)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t startInfoBroadcast(const Info& info)
{
    sInfo = info;
    sDeviceName = info.deviceName.substr(0, kMaxNameLength);
    buildJson();

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ble_hs_cfg.sync_cb = onSync;
    ble_hs_cfg.reset_cb = [](int reason) {
        ESP_LOGW(kTag, "NimBLE host reset (reason=%d)", reason);
    };
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;

    ble_svc_gap_device_name_set(sDeviceName.c_str());
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(sServices);
    if (rc != 0)
    {
        ESP_LOGE(kTag, "ble_gatts_count_cfg failed: %d", rc);
        return ESP_FAIL;
    }
    rc = ble_gatts_add_svcs(sServices);
    if (rc != 0)
    {
        ESP_LOGE(kTag, "ble_gatts_add_svcs failed: %d", rc);
        return ESP_FAIL;
    }

    nimble_port_freertos_init(hostTask);

    ESP_LOGI(kTag, "BLE info service initialized — info=%s", sJsonBuf);
    return ESP_OK;
}

esp_err_t stopInfoBroadcast()
{
    int rc = nimble_port_stop();
    if (rc != 0)
    {
        ESP_LOGW(kTag, "nimble_port_stop failed: %d", rc);
    }
    rc = nimble_port_deinit();
    if (rc != 0)
    {
        ESP_LOGW(kTag, "nimble_port_deinit failed: %d", rc);
    }
    return ESP_OK;
}

}  // namespace ble
}  // namespace safemode
