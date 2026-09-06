#include "NimBLEFake.h"
#include "NimBLEHIDDevice.h"

FakeNimBLE Nimble;

// Out-of-class definitions of FakeNimBLE static state (C++11 requires them).
std::string FakeNimBLE::deviceName;
int FakeNimBLE::initCount = 0;
int FakeNimBLE::advertisingStartCount = 0;
std::vector<std::string> FakeNimBLE::advertisedServiceUuids;
bool FakeNimBLE::notifySuccess = true;
int FakeNimBLE::notifyCount = 0;
std::vector<uint8_t> FakeNimBLE::lastNotified;
NimBLEServer FakeNimBLE::serverObj;
NimBLEAdvertising FakeNimBLE::advertisingObj;
NimBLEService FakeNimBLE::servicePool[2];
NimBLECharacteristic FakeNimBLE::charPool[8];
NimBLEConnInfo FakeNimBLE::connInfoObj;
int FakeNimBLE::serviceIndex = 0;
int FakeNimBLE::charIndex = 0;

// ---------------------------------------------------------------------------
// Callback bases: no-op defaults, matching the real NimBLE host behavior.
// ---------------------------------------------------------------------------
void NimBLECharacteristicCallbacks::onRead(NimBLECharacteristic*,
                                           NimBLEConnInfo&) {}
void NimBLECharacteristicCallbacks::onWrite(NimBLECharacteristic*,
                                            NimBLEConnInfo&) {}
void NimBLECharacteristicCallbacks::onStatus(NimBLECharacteristic*,
                                             NimBLEConnInfo&, int) {}
void NimBLECharacteristicCallbacks::onSubscribe(NimBLECharacteristic*,
                                                NimBLEConnInfo&, uint16_t) {}
void NimBLEServerCallbacks::onConnect(NimBLEServer*, NimBLEConnInfo&) {}
void NimBLEServerCallbacks::onDisconnect(NimBLEServer*, NimBLEConnInfo&,
                                         int) {}

// ---------------------------------------------------------------------------
// NimBLEDevice
// ---------------------------------------------------------------------------
bool NimBLEDevice::init(const std::string& deviceName) {
  Nimble.deviceName = deviceName;
  Nimble.initCount++;
  return true;
}

NimBLEServer* NimBLEDevice::createServer() { return &Nimble.serverObj; }

NimBLEServer* NimBLEDevice::getServer() { return &Nimble.serverObj; }

// ---------------------------------------------------------------------------
// NimBLEServer
// ---------------------------------------------------------------------------
NimBLEServer::NimBLEServer() : callbacks_(nullptr), connectedCount_(0),
                               started_(false) {}

NimBLEServer::~NimBLEServer() {}

void NimBLEServer::setCallbacks(NimBLEServerCallbacks* pCallbacks,
                                bool deleteCallbacks) {
  (void)deleteCallbacks;
  callbacks_ = pCallbacks;
}

NimBLEService* NimBLEServer::createService(const NimBLEUUID& uuid) {
  if (Nimble.serviceIndex >= 2) return nullptr;
  NimBLEService* svc = &Nimble.servicePool[Nimble.serviceIndex];
  Nimble.serviceIndex++;
  svc->activate(uuid);
  return svc;
}

bool NimBLEServer::start() {
  started_ = true;
  return true;
}

NimBLEAdvertising* NimBLEServer::getAdvertising() {
  return &Nimble.advertisingObj;
}

void NimBLEServer::deactivate() {
  callbacks_ = nullptr;
  connectedCount_ = 0;
  started_ = false;
}

// ---------------------------------------------------------------------------
// NimBLEService
// ---------------------------------------------------------------------------
NimBLEService::~NimBLEService() {}

NimBLECharacteristic* NimBLEService::createCharacteristic(
    const NimBLEUUID& uuid, uint32_t properties, uint16_t maxLen) {
  if (Nimble.charIndex >= 8) return nullptr;
  NimBLECharacteristic* chr = &Nimble.charPool[Nimble.charIndex];
  Nimble.charIndex++;
  chr->activate(uuid, properties, maxLen);
  return chr;
}

void NimBLEService::activate(const NimBLEUUID& uuid) { uuid_ = uuid; }

void NimBLEService::deactivate() { uuid_ = NimBLEUUID(); }

// ---------------------------------------------------------------------------
// NimBLECharacteristic
// ---------------------------------------------------------------------------
NimBLECharacteristic::NimBLECharacteristic()
    : uuid_(), properties_(0), maxLen_(0), callbacks_(nullptr),
      active_(false) {}

NimBLECharacteristic::NimBLECharacteristic(const NimBLEUUID& uuid,
                                           uint32_t properties,
                                           uint16_t maxLen)
    : uuid_(uuid), properties_(properties), maxLen_(maxLen),
      callbacks_(nullptr), active_(false) {}

NimBLECharacteristic::~NimBLECharacteristic() {}

void NimBLECharacteristic::setCallbacks(
    NimBLECharacteristicCallbacks* pCallbacks) {
  callbacks_ = pCallbacks;
}

bool NimBLECharacteristic::notify(const uint8_t* data, size_t length,
                                  uint16_t connHandle) const {
  (void)connHandle;
  if (!Nimble.notifySuccess) return false;
  Nimble.notifyCount++;
  Nimble.lastNotified.assign(data, data + length);
  return true;
}

void NimBLECharacteristic::activate(const NimBLEUUID& uuid,
                                    uint32_t properties, uint16_t maxLen) {
  uuid_ = uuid;
  properties_ = properties;
  maxLen_ = maxLen;
  callbacks_ = nullptr;
  static_cast<NimBLEAttValue&>(*this).setValue(nullptr, 0);
  active_ = true;
}

void NimBLECharacteristic::deactivate() {
  activate(NimBLEUUID(), 0, 0);
  active_ = false;
}

// ---------------------------------------------------------------------------
// NimBLEAdvertising
// ---------------------------------------------------------------------------
NimBLEAdvertising::~NimBLEAdvertising() {}

bool NimBLEAdvertising::start(uint32_t duration, const NimBLEAddress* dirAddr) {
  (void)duration;
  (void)dirAddr;
  Nimble.advertisingStartCount++;
  return true;
}

bool NimBLEAdvertising::addServiceUUID(const NimBLEUUID& serviceUUID) {
  Nimble.advertisedServiceUuids.push_back(serviceUUID.str());
  return true;
}

bool NimBLEAdvertising::setName(const std::string& name) {
  name_ = name;
  return true;
}

// ---------------------------------------------------------------------------
// FakeNimBLE
// ---------------------------------------------------------------------------
void FakeNimBLE::reset() {
  deviceName.clear();
  initCount = 0;
  advertisingStartCount = 0;
  advertisedServiceUuids.clear();
  notifySuccess = true;
  notifyCount = 0;
  lastNotified.clear();
  serviceIndex = 0;
  charIndex = 0;
  serverObj.deactivate();
  advertisingObj.serviceUuids_.clear();
  advertisingObj.name_.clear();
  for (int i = 0; i < 2; ++i) servicePool[i].deactivate();
  for (int i = 0; i < 8; ++i) charPool[i].deactivate();
}

NimBLECharacteristic* FakeNimBLE::findChar(const char* uuidSuffix) {
  for (int i = 0; i < 8; ++i) {
    NimBLECharacteristic* chr = &charPool[i];
    if (chr->active() &&
        chr->uuid().str().find(uuidSuffix) != std::string::npos) {
      return chr;
    }
  }
  return nullptr;
}

NimBLECharacteristic* FakeNimBLE::createHidInputReport() {
  if (charIndex >= 8) return nullptr;
  NimBLECharacteristic* chr = &charPool[charIndex];
  charIndex++;
  chr->activate(NimBLEUUID("0x2A4D"), NIMBLE_PROPERTY::NOTIFY, 9);
  return chr;
}

bool FakeNimBLE::dispatchWrite(const uint8_t* data, size_t len) {
  NimBLECharacteristic* chr = findChar("FF61");
  if (!chr || !chr->getCallbacks()) return false;
  chr->setValue(data, len);
  chr->getCallbacks()->onWrite(chr, Nimble.connInfoObj);
  return true;
}

void FakeNimBLE::dispatchSubscribe(uint16_t subValue) {
  NimBLECharacteristic* chr = findChar("FF61");
  if (!chr || !chr->getCallbacks()) return;
  chr->getCallbacks()->onSubscribe(chr, Nimble.connInfoObj, subValue);
}

void FakeNimBLE::connect() {
  if (Nimble.serverObj.getConnectedCount() < 255) {
    Nimble.serverObj.connectedCount_++;
  }
  if (Nimble.serverObj.getCallbacks()) {
    Nimble.serverObj.getCallbacks()->onConnect(&Nimble.serverObj,
                                               Nimble.connInfoObj);
  }
}

void FakeNimBLE::disconnect() {
  Nimble.serverObj.connectedCount_ = 0;
  if (Nimble.serverObj.getCallbacks()) {
    Nimble.serverObj.getCallbacks()->onDisconnect(&Nimble.serverObj,
                                                   Nimble.connInfoObj, 0x13);
  }
}

// ---------------------------------------------------------------------------
// NimBLEHIDDevice (fake)
// ---------------------------------------------------------------------------
void NimBLEHIDDevice::setReportMap(uint8_t* map, uint16_t size) {
  reportMap_.assign(map, map + size);
}

bool NimBLEHIDDevice::setManufacturer(const std::string& name) {
  (void)name;
  return true;
}

NimBLECharacteristic* NimBLEHIDDevice::getInputReport(uint8_t reportId) {
  if (inputReport_ && inputReportId_ == reportId) return inputReport_;
  inputReport_ = FakeNimBLE::createHidInputReport();
  inputReportId_ = reportId;
  return inputReport_;
}
