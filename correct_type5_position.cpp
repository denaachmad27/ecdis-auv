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
    cout << "=== CORRECT Type 5 Structure ===" << endl;
    cout << "Vessel Name: CRANE VESTA" << endl << endl;

    string vesselName = "CRANE VESTA";
    string vesselBinary = encodeVesselName(vesselName);
    cout << "Vessel binary: " << vesselBinary << endl;

    // CORRECT Type 5 bitstream structure
    string bitstream;

    // Type 5: 6 bits
    bitstream += "000101";

    // Repeat: 2 bits
    bitstream += "00";

    // MMSI: 30 bits
    bitstream += "000000011011110001010101101101";

    // AIS Version: 2 bits
    bitstream += "00";

    // Callsign: 42 bits (7 chars) - must be included!
    bitstream += "000000000000000000000000000000000000000000000000";  // Empty callsign

    // VESSEL NAME: 120 bits (20 chars) - THIS IS FIELD 5
    bitstream += vesselBinary;

    // Ship Type: 8 bits
    bitstream += "00000000";

    // Dimensions: 36 bits
    bitstream += "000000000000000000000000000000000000";

    // Position reference & ETA: 30 bits
    bitstream += "000000000000000000000000000000";

    // Destination: 120 bits
    bitstream += "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

    // DTE: 1 bit
    bitstream += "0";

    // Spare: 6 bits
    bitstream += "000000";

    cout << "Total bitstream length: " << bitstream.length() << " bits (should be 424)" << endl;

    // Show vessel name position in bitstream
    cout << "\nField positions:" << endl;
    cout << "  0-  5: Type (6 bits)" << endl;
    cout << "  6-  7: Repeat (2 bits)" << endl;
    cout << "  8-37: MMSI (30 bits)" << endl;
    cout << " 38-39: AIS Version (2 bits)" << endl;
    cout << " 40-81: Callsign (42 bits)" << endl;
    cout << " 82-201: VESSEL NAME (120 bits) <--- OUR FIELD!" << endl;
    cout << " 82-201 binary: " << bitstream.substr(82, 120) << endl;

    string payload = encodeToAISPayload(bitstream);
    cout << "\nPayload: " << payload << endl;
    cout << "Payload length: " << payload.length() << " characters" << endl;

    // Show vessel name in payload
    cout << "Characters 27-46 in payload (vessel name): " << payload.substr(27, 20) << endl;

    // Create NMEA
    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== CORRECT NMEA SENTENCE ===" << endl;
    cout << nmea << endl;

    cout << "\n=== TEST INSTRUCTIONS ===" << endl;
    cout << "Expected result in decoder:" << endl;
    cout << "- Type: 5" << endl;
    cout << "- MMSI: 123456789" << endl;
    cout << "- Vessel Name: CRANE VESTA" << endl;
    cout << "- Ship Type: 0" << endl;

    return 0;
}