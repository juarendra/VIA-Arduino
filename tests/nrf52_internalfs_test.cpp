#include "VIA_nRF52_InternalFS.h"
#include <InternalFileSystem.h>
#include <assert.h>
#include <stdio.h>

Adafruit_LittleFS_Namespace::LittleFS InternalFS;
bool g_fake_fs_begin_ok = true;
bool g_fake_fs_write_fail = false;
bool g_fake_fs_corrupt_after_write = false;

void test_crc32_record() {
    InternalFS.format();
    g_fake_fs_write_fail = false;
    uint8_t staging[9] = {0};
    via::nrf52::InternalFSStorage storage(staging, sizeof(staging));
    assert(storage.begin());
    const uint8_t test_data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    assert(storage.write(0, test_data, sizeof(test_data)));
    assert(storage.commit());

    struct Header { uint32_t magic, generation, length, crc; } header = {};
    const std::vector<uint8_t>& record = InternalFS.files_.at("/via_a.dat");
    assert(record.size() == sizeof(header) + sizeof(staging));
    memcpy(&header, record.data(), sizeof(header));
    assert(header.magic == 0x56494146);
    assert(header.generation == 1);
    assert(header.length == sizeof(staging));
    assert(header.crc == 0x340BC6D9); // Known CRC32 (polynomial 0xEDB88320) for "123456789" with initial 0xFFFFFFFF
}

void test_storage() {
    InternalFS.format();
    g_fake_fs_write_fail = false;

    uint8_t staging[4096] = {};
    via::nrf52::InternalFSStorage storage(staging, sizeof(staging));
    assert(storage.begin());
    assert(storage.capacity() == sizeof(staging));

    // Initially staging should be 0x00
    for (size_t i = 0; i < sizeof(staging); ++i) {
        assert(staging[i] == 0x00);
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
    assert(staging2[10] == 0x00);
    
    // Bounds testing
    uint8_t dummy = 0;
    assert(!storage2.read(sizeof(staging2) + 1, &dummy, 1));
    assert(!storage2.read(sizeof(staging2), &dummy, 1));
    assert(!storage2.read(sizeof(staging2) - 1, &dummy, 2));
    assert(!storage2.write(sizeof(staging2) + 1, &dummy, 1));
    assert(!storage2.write(sizeof(staging2), &dummy, 1));
    assert(!storage2.write(sizeof(staging2) - 1, &dummy, 2));

    // Corrupt-newest fallback & generation wraparound testing
    uint8_t genWrapStaging[4096] = {};
    via::nrf52::InternalFSStorage genWrapStorage(genWrapStaging, sizeof(genWrapStaging));
    assert(genWrapStorage.begin());

    // Write file A with generation = 0xFFFFFFFF
    {
        Adafruit_LittleFS_Namespace::File fA = InternalFS.open("/via_a.dat", Adafruit_LittleFS_Namespace::FILE_O_WRITE);
        struct { uint32_t magic, gen, len, crc; } hdrA = { 0x56494146, 0xFFFFFFFF, sizeof(genWrapStaging), 0 };
        hdrA.crc = 0x38E3FFEE; // CRC32 state for 4096 zero bytes
        fA.write((const uint8_t*)&hdrA, sizeof(hdrA));
        fA.write(genWrapStaging, sizeof(genWrapStaging));
        fA.close();
    }
    
    // Write file B with generation = 0 (wrap around), but make it corrupted (wrong CRC)
    {
        Adafruit_LittleFS_Namespace::File fB = InternalFS.open("/via_b.dat", Adafruit_LittleFS_Namespace::FILE_O_WRITE);
        struct { uint32_t magic, gen, len, crc; } hdrB = { 0x56494146, 0, sizeof(genWrapStaging), 0xBADBAD };
        fB.write((const uint8_t*)&hdrB, sizeof(hdrB));
        fB.write(genWrapStaging, sizeof(genWrapStaging));
        fB.close();
    }
    
    // Test begin() again to trigger selection
    assert(genWrapStorage.begin());
    
    // The active generation should fallback to 0xFFFFFFFF since B is corrupted.
    // The next commit should wrap to 0.
    assert(genWrapStorage.commit());
    
    // Let's verify file B is written correctly now and has generation 0
    {
        Adafruit_LittleFS_Namespace::File fB = InternalFS.open("/via_b.dat", Adafruit_LittleFS_Namespace::FILE_O_READ);
        struct { uint32_t magic, gen, len, crc; } hdrB;
        fB.read((uint8_t*)&hdrB, sizeof(hdrB));
        assert(hdrB.gen == 0);
        fB.close();
    }
}

void test_mount_failure() {
    uint8_t staging[32] = {};
    g_fake_fs_begin_ok = false;
    via::nrf52::InternalFSStorage storage(staging, sizeof(staging));
    assert(!storage.begin());
    g_fake_fs_begin_ok = true;
}

void test_corrupt_write_preserves_previous_slot() {
    InternalFS.format();
    uint8_t staging[32] = {};
    via::nrf52::InternalFSStorage storage(staging, sizeof(staging));
    assert(storage.begin());
    staging[0] = 0x11;
    assert(storage.commit());

    staging[0] = 0x22;
    g_fake_fs_corrupt_after_write = true;
    assert(!storage.commit());

    uint8_t recovered[32] = {};
    via::nrf52::InternalFSStorage restarted(recovered, sizeof(recovered));
    assert(restarted.begin());
    assert(recovered[0] == 0x11);
}

int main() {
    test_crc32_record();
    test_mount_failure();
    test_corrupt_write_preserves_previous_slot();
    test_storage();
    printf("PASS\n");
    return 0;
}
