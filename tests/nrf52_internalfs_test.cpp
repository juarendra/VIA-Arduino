#include "VIA_nRF52_InternalFS.h"
#include <InternalFileSystem.h>
#include <assert.h>
#include <stdio.h>

Adafruit_LittleFS_Namespace::LittleFS InternalFS;
bool g_fake_fs_write_fail = false;

extern "C" uint32_t crc32_compute(uint8_t const * p_data, uint32_t size, uint32_t const * p_crc) {
    uint32_t crc;
    crc = (p_crc == NULL) ? 0xFFFFFFFF : *p_crc;
    for (uint32_t i = 0; i < size; i++) {
        crc = crc ^ p_data[i];
        for (uint32_t j = 8; j > 0; j--) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return crc;
}

void test_storage() {
    InternalFS.format();
    g_fake_fs_write_fail = false;

    uint8_t staging[4096] = {};
    via::nrf52::InternalFSStorage storage(staging, sizeof(staging));
    assert(storage.begin());
    assert(storage.capacity() == sizeof(staging));

    // Initially staging should be 0xFF
    for (size_t i = 0; i < sizeof(staging); ++i) {
        assert(staging[i] == 0xFF);
    }

    uint8_t test_data[] = { 1, 2, 3, 4 };
    assert(storage.write(10, test_data, sizeof(test_data)));
    assert(storage.commit());

    // Restart adapter
    uint8_t staging2[4096] = {};
    via::nrf52::InternalFSStorage storage2(staging2, sizeof(staging2));
    assert(storage2.begin());

    uint8_t read_data[4];
    assert(storage2.read(10, read_data, 4));
    assert(read_data[0] == 1 && read_data[1] == 2 && read_data[2] == 3 && read_data[3] == 4);

    // Test A/B alternation
    assert(InternalFS.files_.size() == 1); // Only A written
    assert(InternalFS.files_.count("/via_a.dat") == 1);
    
    assert(storage2.write(20, test_data, sizeof(test_data)));
    assert(storage2.commit());
    assert(InternalFS.files_.size() == 2); // Both A and B written
    
    // Simulate write failure on next commit
    g_fake_fs_write_fail = true;
    assert(!storage2.commit()); // Should fail
    g_fake_fs_write_fail = false;

    // Erase
    assert(storage2.erase());
    assert(InternalFS.files_.size() == 0);
    assert(staging2[10] == 0xFF);
}

int main() {
    test_storage();
    printf("PASS\n");
    return 0;
}
