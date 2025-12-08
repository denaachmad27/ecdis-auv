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
    cout << "=== CORRECTED Type 5 for CRANE VESTA ===" << endl;

    // Build Type 5 bitstream dengan positioning yang BENAR
    string bitstream;

    // Type 5: 000101 (6 bits)
    bitstream += "000101";

    // Repeat: 00 (2 bits)
    bitstream += "00";

    // MMSI: 000000000000000000000001111111 (30 bits) = 255
    bitstream += "000000000000000000000001111111";

    // AIS Version: 00 (2 bits)
    bitstream += "00";

    // Callsign: 42 bits = 7 chars, kita kosongkan semua
    bitstream += "000000000000000000000000000000000000000000000000";

    // VESSEL NAME: 120 bits = 20 chars
    // CRANE VESTA (11 chars) + 9 spaces
    // C=3=000011, R=18=010010, A=1=000001, N=14=001110, E=5=000101, space=0=000000
    // V=22=010110, E=5=000101, S=19=010011, T=20=010100, A=1=000001
    string vesselNameBinary =
        "000011010010000001001110000101000000"  // CRANE[space]
        "010110000101010011010100000001000000"  // VESTA[space]
        "000000000000000000000000000000000000000000000000000000"; // 7 more spaces

    bitstream += vesselNameBinary;

    // Ship Type: 8 bits
    bitstream += "00000000";

    // Tambah field lain untuk membuat Type 5 lengkap
    bitstream += "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

    cout << "Total bits: " << bitstream.length() << endl;
    cout << "Vessel name starts at bit: 82" << endl;
    cout << "Vessel name bits: " << bitstream.substr(82, 120) << endl;

    string payload = encodeToAISPayload(bitstream);
    cout << "Payload: " << payload << endl;
    cout << "Vessel name in payload (chars 27-46): " << payload.substr(27, 20) << endl;

    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== CORRECTED NMEA SENTENCE ===" << endl;
    cout << nmea << endl;

    return 0;
}