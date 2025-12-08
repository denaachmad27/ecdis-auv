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
    cout << "=== WORKING Type 5 for CRANE VESTA ===" << endl;

    string vesselName = "CRANE VESTA";
    cout << "Vessel name: '" << vesselName << "'" << endl;

    // Build Type 5 bitstream
    string bitstream;

    // Type 5 (6 bits)
    bitstream += "000101";

    // Repeat (2 bits)
    bitstream += "00";

    // MMSI: 123456789 (30 bits)
    bitstream += "000000011011110001010101101101";

    // AIS Version (2 bits)
    bitstream += "00";

    // Callsign: empty (42 bits)
    bitstream += "000000000000000000000000000000000000000000000000";

    // VESSEL NAME: "CRANE VESTA" + padding (120 bits)
    // CRANE VESTA         (11 chars + 9 spaces)
    // C=000011, R=010010, A=000001, N=001110, E=000101, space=000000
    // V=010110, E=000101, S=010011, T=010100, A=000001
    string vesselBinary =
        "000011010010000001001110000101000000"  // CRANE[space]
        "010110000101010011010100000001000000"  // VESTA[space]
        "000000000000000000000000000000000000000000000000000000"; // 7 more spaces

    bitstream += vesselBinary;

    cout << "Total bits: " << bitstream.length() << endl;

    string payload = encodeToAISPayload(bitstream);
    cout << "Payload: " << payload << endl;
    cout << "Vessel name in payload (chars 15-34): " << payload.substr(15, 20) << endl;

    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== WORKING NMEA SENTENCE ===" << endl;
    cout << nmea << endl;

    return 0;
}