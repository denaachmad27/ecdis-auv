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
    cout << "=== FIND VESSEL POSITION ===" << endl;

    // Test Type 5 dengan "ZZZZZZZZZZZZZZZZZZZZ" (20 Z)
    string bitstream;

    // Header Type 5
    bitstream += "000101";  // Type 5 (6)
    bitstream += "00";      // Repeat (2)
    bitstream += "000000011011110001010101101101";  // MMSI (30)
    bitstream += "00";      // AIS Version (2)
    bitstream += "000000000000000000000000000000000000000000000000";  // Callsign (42)

    // VESSEL NAME: 20 Z's
    for (int i = 0; i < 20; ++i) {
        bitstream += "011010";  // Z = 26 = 011010
    }

    cout << "Total bits: " << bitstream.length() << endl;
    cout << "Vessel name starts at bit: 82" << endl;

    string payload = encodeToAISPayload(bitstream);
    cout << "Payload: " << payload << endl;

    // Show characters around vessel name position
    cout << "\nCharacters 15-50 in payload:" << endl;
    for (int i = 15; i < 50 && i < payload.length(); ++i) {
        cout << i << ": '" << payload[i] << "' ";
        if ((i - 15) % 10 == 0) cout << endl;
    }
    cout << endl;

    // Z seharusnya muncul sebagai 'a' (value 26+48=74='J', +8=82='R'? Check mapping)
    cout << "\nZ value mapping test:" << endl;
    cout << "Z=26, +48=74, +8=82 = '" << (char)82 << "'" << endl;

    return 0;
}