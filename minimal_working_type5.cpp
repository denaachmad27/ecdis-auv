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

// Encode vessel name to 6-bit binary
string encodeVesselName(const string& name) {
    string result;
    string padded = name;
    padded.resize(20, ' ');

    for (char ch : padded) {
        int val = 0;
        if (ch == ' ') val = 0;
        else if (ch >= 'A' && ch <= 'Z') val = ch - 'A' + 1;
        else if (ch >= '0' && ch <= '9') val = ch - '0' + 48;
        else val = 0;

        for (int bit = 5; bit >= 0; --bit) {
            result += ((val >> bit) & 1) ? '1' : '0';
        }
    }
    return result;
}

int main() {
    cout << "=== MINIMAL WORKING Type 5 for CRANE VESTA ===" << endl;

    // Build Type 5 dengan field minimal dan fokus di vessel name
    string bitstream;

    // Type 5: 6 bits
    bitstream += "000101";

    // Repeat: 2 bits
    bitstream += "00";

    // MMSI: 30 bits (gunakan MMSI yang sama dulu)
    bitstream += "000000011011110001010101101101";  // 123456789

    // AIS Version: 2 bits
    bitstream += "00";

    // Callsign: 42 bits (7 chars x 6 bits) - kosong
    bitstream += "000000000000000000000000000000000000000000000000";

    // VESSEL NAME: 120 bits (20 chars x 6 bits) - INI FOKUS KITA
    string vesselName = "CRANE VESTA";
    string vesselBinary = encodeVesselName(vesselName);
    cout << "Vessel name: '" << vesselName << "'" << endl;
    cout << "Vessel binary: " << vesselBinary << endl;
    cout << "Vessel binary length: " << vesselBinary.length() << " bits" << endl;

    bitstream += vesselBinary;

    // Total sejauh ini:
    cout << "Bits after vessel name: " << bitstream.length() << endl;

    // Tambah beberapa field penting lainnya minimal
    // Ship Type: 8 bits
    bitstream += "00000000";

    // Padding agar menjadi multiple of 6 untuk NMEA
    while (bitstream.length() % 6 != 0) {
        bitstream += "0";
    }

    cout << "Final bitstream length: " << bitstream.length() << " bits" << endl;

    string payload = encodeToAISPayload(bitstream);
    cout << "Final payload: " << payload << endl;
    cout << "Payload length: " << payload.length() << " characters" << endl;

    // Test NMEA
    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== MINIMAL NMEA SENTENCE ===" << endl;
    cout << nmea << endl;

    return 0;
}