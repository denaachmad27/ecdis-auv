#include <iostream>
#include <string>
using namespace std;

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

int main() {
    cout << "=== DEBUG Type 5 Simple ===" << endl;

    // Test dengan "A" saja di vessel name
    string bitstream;

    // Type 5
    bitstream += "000101";  // 6 bits

    // Repeat
    bitstream += "00";      // 2 bits

    // MMSI 123456789
    bitstream += "000000011011110001010101101101";  // 30 bits

    // AIS Version
    bitstream += "00";      // 2 bits

    // Callsign kosong (42 bits)
    bitstream += "000000000000000000000000000000000000000000000000";

    // VESSEL NAME: "A" + 19 spaces (20 chars total)
    // "A" = 1 = 000001, space = 0 = 000000
    bitstream += "000001";  // "A"
    for (int i = 0; i < 19; ++i) {
        bitstream += "000000";  // 19 spaces
    }

    cout << "Total bits: " << bitstream.length() << endl;
    cout << "Vessel name field (bits 82-201): " << bitstream.substr(82, 6) << "..." << endl;

    string payload = encodeToAISPayload(bitstream);
    cout << "Payload: " << payload << endl;
    cout << "Char 27 (should be 'A'): " << payload[27] << endl;

    return 0;
}