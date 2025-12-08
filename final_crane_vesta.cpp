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
    cout << "=== FINAL CRANE VESTA Type 5 ===" << endl;

    // Build Type 5 untuk CRANE VESTA
    string bitstream;

    // Type 5: 6 bits
    bitstream += "000101";

    // Repeat: 2 bits
    bitstream += "00";

    // MMSI: 30 bits (gunakan MMSI sederhana)
    bitstream += "000000000000000000000001111111";  // MMSI 255 (sederhana)

    // AIS Version: 2 bits
    bitstream += "00";

    // Callsign: 42 bits (7 chars, kosong)
    bitstream += "000000000000000000000000000000000000000000000000";

    // VESSEL NAME: "CRANE VESTA" padded to 20 chars = 120 bits
    // CRANE VESTA         (9 spaces)
    string vesselName = "CRANE VESTA         ";

    // CRANE VESTA dalam AIS 6-bit:
    // C=000011, R=010010, A=000001, N=001110, E=000101, space=000000
    // V=010110, E=000101, S=010011, T=010100, A=000001
    bitstream += "000011010010000001001110000101000000010110000101010011010100000001000000000000000000000000000000000000000000000000000000";

    // Ship Type: 8 bits
    bitstream += "00000000";

    cout << "Total bits: " << bitstream.length() << endl;

    string payload = encodeToAISPayload(bitstream);
    cout << "Payload: " << payload << endl;

    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== FINAL NMEA SENTENCE ===" << endl;
    cout << nmea << endl;

    return 0;
}