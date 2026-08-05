const SERVICE = "0000ff60-0000-1000-8000-00805f9b34fb";
const FF61 = "0000ff61-0000-1000-8000-00805f9b34fb";
const output = document.querySelector("#log");
const log = (message) => { output.textContent += `${message}\n`; };
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function write(characteristic, length) {
  const packet = new Uint8Array(length);
  packet[0] = 0x01;
  try {
    await characteristic.writeValueWithResponse(packet);
    log(`${length}-byte write accepted by GATT`);
  } catch (error) {
    log(`${length}-byte write rejected: ${error.name}`);
  }
}

document.querySelector("#run").addEventListener("click", async () => {
  output.textContent = "";
  try {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{services: [SERVICE]}],
    });
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE);
    const ff61 = await service.getCharacteristic(FF61);

    await write(ff61, 31);
    await write(ff61, 33);
    await write(ff61, 32);

    let validResponse = false;
    for (let attempt = 0; attempt < 20; attempt += 1) {
      await sleep(50);
      const response = await ff61.readValue();
      if (response.byteLength === 32 && response.getUint8(2) === 0x0d) {
        validResponse = true;
        break;
      }
    }
    if (!validResponse) throw new Error("valid VIA protocol response missing");
    log("PASS: valid 32-byte traffic works after invalid writes");
  } catch (error) {
    log(`FAIL: ${error.message}`);
  }
});
