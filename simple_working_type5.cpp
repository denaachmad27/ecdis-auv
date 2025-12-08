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

string calculateChecksum(const string& sentence) {
    unsigned char checksum = 0;
    for (size_t i = 1; i < sentence.length(); ++i) {
        if (sentence[i] == '*') break;
        checksum ^= (unsigned char)sentence[i];
    }

    char hex[3];
    sprintf(hex, "%02X", checksum);
    return string(hex);
}

int main() {
    cout << "=== SIMPLE WORKING Type 5 ===" << endl;

    // Type 5 dengan vessel name "CRANE" (5 char saja untuk test)
    string bitstream;

    // Type 5
    bitstream += "000101";

    // Repeat
    bitstream += "00";

    // MMSI 123456789
    bitstream += "000000011011110001010101101101";

    // AIS Version
    bitstream += "00";

    // Callsign: "TEST" (4 chars = 24 bits)
    bitstream += "010100000101010011010100000000";  // "TEST" + padding

    // VESSEL NAME: "CRANE" (5 chars = 30 bits)
    // C=3, R=18, A=1, N=14, E=5
    // C=000011, R=010010, A=000001, N=001110, E=000101
    bitstream += "000011010010000001001110000101";

    cout << "Bitstream: " << bitstream << endl;
    cout << "Length: " << bitstream.length() << " bits" << endl;

    string payload = encodeToAISPayload(bitstream);
    cout << "Payload: " << payload << endl;

    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== SIMPLE NMEA ===" << endl;
    cout << nmea << endl;

    cout << "\nExpected decode:" << endl;
    cout << "- Callsign: TEST" << endl;
    cout << "- Vessel: CRANE" << endl;

    return 0;
}