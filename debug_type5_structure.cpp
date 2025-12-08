#include <iostream>
#include <string>
using namespace std;

// Encode ke AIS payload (sesuai AIVDOEncoder.cpp)
string encodeToAISPayload(const string& bitstream) {
    int neededLength = ((bitstream.length() + 5) / 6) * 6;
    string padded = bitstream;
    while (padded.length() < neededLength) {
        padded += '0';
    }

    string encoded;
    for (int i = 0; i < neededLength; i += 6) {
        string chunk = padded.substr(i, 6);
        int value = 0;
        for (int j = 0; j < 6; ++j) {
            value = (value << 1) | (chunk[j] - '0');
        }

        value += 48;
        if (value > 87) value += 8;
        encoded += (char)value;
    }

    return encoded;
}

// Test dengan vessel name sederhana
void testSimpleVesselNames() {
    cout << "=== Testing Simple Vessel Names ===" << endl;

    // Test dengan nama yang sangat sederhana: "A"
    string vesselName = "A";
    cout << "Testing vessel name: '" << vesselName << "'" << endl;

    // Build Type 5 bitstream dengan struktur yang berbeda
    // Mari coba struktur yang paling minimal

    // Type 5 minimal:
    string bitstream =
        "000101"                      // Type 5 (6 bits)
        "00"                          // Repeat (2 bits)
        "000000011011110001010101101101"  // MMSI 123456789 (30 bits)
        "00"                          // AIS Version (2 bits)
        "000000000000000000000000000000000000000000000000"  // Callsign empty (42 bits)
        ;

    // Tambahkan vessel name "A" (1 char = 6 bits)
    string vesselBinary = "000001";  // "A" = 1 dalam AIS 6-bit = 000001
    bitstream += vesselBinary;

    // Padding untuk vessel name field (total 120 bits, kita baru pakai 6)
    bitstream += string(114, '0');

    // Sisanya field lain di-zero semua
    bitstream += string(200, '0');  // Ship type, dimensions, destination, dll

    cout << "Total bits: " << bitstream.length() << endl;

    string payload = encodeToAISPayload(bitstream);
    cout << "Payload: " << payload.substr(0, 20) << "..." << endl;

    // Sekarang coba dengan vessel name "AAAA" untuk pola yang jelas
    cout << "\n=== Testing 'AAAA' ===" << endl;
    vesselName = "AAAA";

    string bitstream2 =
        "000101"                      // Type 5
        "00"                          // Repeat
        "000000011011110001010101101101"  // MMSI
        "00"                          // AIS Version
        "000000000000000000000000000300000000000000000000"  // Callsign empty
        "000001000001000001000001"  // "AAAA" (4 chars)
        ;

    bitstream2 += string(300, '0');  // Rest zero

    string payload2 = encodeToAISPayload(bitstream2);
    cout << "Payload with 'AAAA': " << payload2.substr(0, 30) << "..." << endl;

    // Test dengan pola "ZZZZZZZZZZZZZZZZZZZZ" (20 chars Z)
    cout << "\n=== Testing 20 chars 'Z' ===" << endl;
    string zzzz = string(20, 'Z');

    // Encode 20 Z's: Z = 26 dalam 6-bit = 011010
    string zzzzBinary;
    for (int i = 0; i < 20; ++i) {
        zzzzBinary += "011010";
    }

    string bitstream3 =
        "000101"                      // Type 5
        "00"                          // Repeat
        "000000011011110001010101101101"  // MMSI
        "00"                          // AIS Version
        "000000000000000000000000000300000000000000000000"  // Callsign empty
        + zzzzBinary                   // 20 Z's
        ;

    string payload3 = encodeToAISPayload(bitstream3);
    cout << "Payload with 20 Z's: " << payload3.substr(0, 20) << "..." << endl;
}

int main() {
    testSimpleVesselNames();
    return 0;
}