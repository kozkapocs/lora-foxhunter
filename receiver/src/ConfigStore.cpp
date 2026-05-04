#include "ConfigStore.h"

#include <string.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

// Magic marker stored at the start of the config file.
// Change this value whenever the ReceiverConfig layout changes so that
// stale files are automatically ignored after a firmware update.
static const uint32_t CONFIG_MAGIC = 0x464F5804U;  // 'FOX' + version 4
static const char *   CONFIG_PATH  = "/foxhunter_rx.cfg";

void config_init_defaults(ReceiverConfig &cfg) {
    memset(&cfg, 0, sizeof(cfg));
    cfg.freq = 869.45f;
    cfg.bw   = 125.0f;
    cfg.sf   = 9;
    cfg.cr   = 5;
    cfg.beep_duration_ms = 100;
    cfg.beep_freq_min = 500;
    cfg.beep_freq_max = 2500;
    // system_id is left as empty string → invalid config
}

bool config_load(ReceiverConfig &cfg) {
    File f(InternalFS);
    if (!f.open(CONFIG_PATH, FILE_O_READ)) {
        return false;
    }

    uint32_t magic = 0;
    if (f.read(&magic, sizeof(magic)) != sizeof(magic) || magic != CONFIG_MAGIC) {
        f.close();
        return false;
    }

    ReceiverConfig tmp;
    int32_t n = f.read(&tmp, sizeof(tmp));
    f.close();

    if (n != (int32_t)sizeof(tmp)) {
        return false;
    }

    cfg = tmp;
    return true;
}

bool config_save(const ReceiverConfig &cfg) {
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

bool config_is_valid(const ReceiverConfig &cfg) {
    return cfg.system_id[0] != '\0';
}
