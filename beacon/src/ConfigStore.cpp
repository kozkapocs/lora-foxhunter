#include "ConfigStore.h"

#include <string.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

// Magic marker stored at the start of the config file.
// Change this value whenever the BeaconConfig layout changes so that
// stale files are automatically ignored after a firmware update.
static const uint32_t CONFIG_MAGIC = 0x464F5801U;  // 'FOX' + version 1
static const char *   CONFIG_PATH  = "/foxhunter.cfg";

void config_init_defaults(BeaconConfig &cfg) {
    memset(&cfg, 0, sizeof(cfg));
    cfg.freq         = 869.45f;
    cfg.bw           = 125.0f;
    cfg.tx_count     = 10;
    cfg.tx_interval_s = 1;
    cfg.period_s     = 20;
    cfg.sf           = 9;
    cfg.cr           = 5;
    // system_id is left as empty string, fox_num stays 0 → invalid config
}

bool config_load(BeaconConfig &cfg) {
    File f(InternalFS);
    if (!f.open(CONFIG_PATH, FILE_O_READ)) {
        return false;
    }

    uint32_t magic = 0;
    if (f.read(&magic, sizeof(magic)) != sizeof(magic) || magic != CONFIG_MAGIC) {
        f.close();
        return false;
    }

    BeaconConfig tmp;
    int32_t n = f.read(&tmp, sizeof(tmp));
    f.close();

    if (n != (int32_t)sizeof(tmp)) {
        return false;
    }

    cfg = tmp;
    return true;
}

bool config_save(const BeaconConfig &cfg) {
    // Remove old file first so we always write a fresh copy.
    InternalFS.remove(CONFIG_PATH);

    File f(InternalFS);
    if (!f.open(CONFIG_PATH, FILE_O_WRITE)) {
        return false;
    }

    f.write(reinterpret_cast<const uint8_t *>(&CONFIG_MAGIC), sizeof(CONFIG_MAGIC));
    f.write(reinterpret_cast<const uint8_t *>(&cfg), sizeof(cfg));
    f.close();
    return true;
}

bool config_is_valid(const BeaconConfig &cfg) {
    // system_id must have at least one printable character
    if (cfg.system_id[0] == '\0') {
        return false;
    }
    // fox_num must be non-zero
    if (cfg.fox_num == 0) {
        return false;
    }
    return true;
}
