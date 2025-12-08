#include <iostream>
#include <string>
using namespace std;

// Encode 6-bit string to AIS payload (sesuai AIVDOEncoder.cpp)
string encodeToAISPayload(const string& bitstream) {
    // Calculate needed length (multiple of 6)
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

// Encode vessel name to 6-bit binary
string encodeVesselName(const string& name) {
    string result;
    string padded = name;
    padded.resize(20, ' ');  // Pad to 20 chars with spaces

    for (char ch : padded) {
        int val = 0;

        if (ch == ' ') val = 0;
        else if (ch >= 'A' && ch <= 'Z') val = ch - 'A' + 1;
        else if (ch >= '0' && ch <= '9') val = ch - '0' + 48;
        else val = 0;

        // Convert to 6-bit binary
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
    cout << "=== Simple Type 5 AIS Message ===" << endl;
    cout << "Vessel: CRANE VESTA, MMSI: 123456789" << endl << endl;

    // Build minimal Type 5 bitstream
    string bitstream;

    // Type: 5 (6 bits)
    bitstream += "000101";

    // Repeat: 0 (2 bits)
    bitstream += "00";

    // MMSI: 123456789 (30 bits)
    bitstream += "000000011011110001010101101101";

    // AIS Version: 0 (2 bits)
    bitstream += "00";

    // Skip callsign for simplicity, go straight to vessel name
    // But we need the right bit positions...

    cout << "Creating Type 5 with focus on vessel name..." << endl;

    // Let me try a different approach - create a working Type 5 message
    // with just the essential fields for vessel name

    string minimalType5 =
        "000101"                              // Type 5
        "00"                                  // Repeat
        "000000011011110001010101101101"      // MMSI 123456789
        "00"                                  // AIS Version
        "000000000000000000000000000000000000000000000000"  // Empty callsign (42 bits)
        "000011010010000001001110000101000000010110000101010011010100000001000000000000000000000000000000000000000000000000000000"  // CRANE VESTA (120 bits)
        "00000000"                            // Ship type 0
        "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";  // Rest zero (63 bits)

    cout << "Minimal Type 5 bitstream length: " << minimalType5.length() << endl;

    // Encode to AIS payload
    string payload = encodeToAISPayload(minimalType5);
    cout << "Payload: " << payload << endl;
    cout << "Payload length: " << payload.length() << " characters" << endl;

    // Create NMEA sentences
    if (payload.length() > 61) {
        string payload1 = payload.substr(0, 61);
        string payload2 = payload.substr(61);

        string nmea1 = "!AIVDM,2,1,1,A," + payload1 + ",0";
        nmea1 += "*" + calculateChecksum(nmea1);

        string nmea2 = "!AIVDM,2,2,1,A," + payload2 + ",0";
        nmea2 += "*" + calculateChecksum(nmea2);

        cout << "\n=== NMEA SENTENCES ===" << endl;
        cout << nmea1 << endl;
        cout << nmea2 << endl;
    }

    return 0;
}