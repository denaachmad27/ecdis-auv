#include <iostream>
#include <string>
using namespace std;

// Copy PERSIS binaryToAIS6Bit dari AIVDOEncoder.cpp
string binaryToAIS6Bit(const string& bitstream) {
    int neededLength = ((bitstream.length() + 5) / 6) * 6;
    string padded = bitstream;

    // Pad with zeros
    while (padded.length() < neededLength) {
        padded += '0';
    }

    string encoded;
    for (int i = 0; i < neededLength; i += 6) {
        string chunk = padded.substr(i, 6);

        // Convert binary to integer
        int value = 0;
        for (int j = 0; j < 6; ++j) {
            value = (value << 1) | (chunk[j] - '0');
        }

        // Apply AIS encoding: value + 48, +8 if > 87
        value += 48;
        if (value > 87) value += 8;

        encoded += (char)value;
    }

    return encoded;
}

// Test Type 5 yang HARUS bekerja - BERDASARKAN SPECIFICATION ASLI
int main() {
    cout << "=== TYPE 5 BERDASARKAN SPECIFICATION ===" << endl;

    // Type 5 structure from ITU-R M.1371 specification
    string bitstream;

    // 1. Message Type: 5 (6 bits)
    bitstream += "000101";

    // 2. Repeat Indicator: 0 (2 bits)
    bitstream += "00";

    // 3. User ID (MMSI): 123456789 (30 bits)
    bitstream += "000000011011110001010101101101";

    // 4. AIS Version: 0 (2 bits)
    bitstream += "00";

    // 5. IMO Number: 0 (30 bits)
    bitstream += "000000000000000000000000000000";

    // 6. Call Sign: 7 chars = 42 bits (kosong)
    bitstream += "000000000000000000000000000000000000000000000000";

    // 7. Vessel Name: 20 chars = 120 bits
    // "TESTVESSELTESTVESSE" - nama yang lebih jelas untuk test
    string vesselName = "TESTVESSELTESTVESSE"; // 20 chars exact
    cout << "Testing vessel name: '" << vesselName << "'" << endl;

    // Encode TESTVESSELTESTVESSE
    for (char ch : vesselName) {
        int val = 0;
        if (ch >= 'A' && ch <= 'Z') val = ch - 'A' + 1;
        else if (ch == ' ') val = 0;
        else if (ch >= '0' && ch <= '9') val = ch - '0' + 48;
        else val = 0;

        // Convert to 6-bit binary
        for (int bit = 5; bit >= 0; --bit) {
            bitstream += ((val >> bit) & 1) ? '1' : '0';
        }
    }

    // 8. Ship Type: 0 (8 bits)
    bitstream += "00000000";

    // 9. Dimension A: 0 (9 bits)
    bitstream += "000000000";

    // 10. Dimension B: 0 (9 bits)
    bitstream += "000000000";

    // 11. Dimension C: 0 (9 bits)
    bitstream += "000000000";

    // 12. Dimension D: 0 (9 bits)
    bitstream += "000000000";

    // 13. Position Reference and ETA: 0 (30 bits)
    bitstream += "000000000000000000000000000000";

    // 14. Destination: 20 chars = 120 bits (kosong)
    bitstream += string(120, '0');

    // 15. DTE: 0 (1 bit)
    bitstream += "0";

    // 16. Spare: 0 (6 bits)
    bitstream += "000000";

    cout << "Total bitstream length: " << bitstream.length() << " bits" << endl;
    cout << "Expected: 424 bits for complete Type 5" << endl;

    if (bitstream.length() != 424) {
        cout << "ERROR: Bitstream length incorrect!" << endl;
    } else {
        cout << "CORRECT: Bitstream length matches specification" << endl;
    }

    string payload = binaryToAIS6Bit(bitstream);
    cout << "Payload: " << payload << endl;
    cout << "Payload length: " << payload.length() << " characters" << endl;

    // Vessel name seharusnya dimulai dari character ke-27 (27-46)
    cout << "Chars 27-46 (vessel name): " << payload.substr(27, 20) << endl;

    return 0;
}