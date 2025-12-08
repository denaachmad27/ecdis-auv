#include <iostream>
#include <string>
using namespace std;

// Simple NMEA checksum calculation
string calculateChecksum(const string& sentence) {
    unsigned char checksum = 0;
    for (size_t i = 1; i < sentence.length(); ++i) {
        if (sentence[i] == '*') break;
        checksum ^= sentence[i];
    }

    char hex[3];
    sprintf(hex, "%02X", checksum);
    return string(hex);
}

// AIS 6-bit to ASCII mapping (correct)
string sixbitToAIS6Bit(const string& bitstream) {
    string result;
    for (size_t i = 0; i < bitstream.length(); i += 6) {
        if (i + 6 > bitstream.length()) break;

        int value = 0;
        for (int j = 0; j < 6; ++j) {
            value = (value << 1) | (bitstream[i + j] - '0');
        }

        // AIS 6-bit character mapping
        char ch;
        if (value >= 1 && value <= 26) {
            ch = 'A' + value - 1;
        } else if (value >= 30 && value <= 39) {
            ch = '0' + value - 30;
        } else if (value >= 40 && value <= 63) {
            ch = 'A' + value - 27; // Punctuation to letters for simplicity
        } else {
            ch = '@';
        }

        result += ch;
    }
    return result;
}

// Encode vessel name to 6-bit binary
string encodeVesselName(const string& name, int maxLen = 20) {
    string result;
    string paddedName = name;
    paddedName.resize(maxLen, ' '); // Pad with spaces

    for (int i = 0; i < maxLen; ++i) {
        char ch = paddedName[i];
        int value = 0;

        if (ch == ' ') value = 0;
        else if (ch >= 'A' && ch <= 'Z') value = ch - 'A' + 1;
        else if (ch >= '0' && ch <= '9') value = ch - '0' + 48;
        else value = 0;

        // Convert to 6-bit binary
        for (int bit = 5; bit >= 0; --bit) {
            result += ((value >> bit) & 1) ? '1' : '0';
        }
    }
    return result;
}

int main() {
    cout << "=== AIS Type 5 NMEA for CRANE VESTA ===" << endl;

    string vesselName = "CRANE VESTA";
    int mmsi = 123456789;

    // Build complete Type 5 bitstream
    string bitstream;

    // Type 5: 000101 (6 bits)
    bitstream += "000101";

    // Repeat Indicator: 00 (2 bits)
    bitstream += "00";

    // MMSI: 123456789 in binary (30 bits)
    bitstream += "000000011011110001010101101101";

    // AIS Version: 00 (2 bits)
    bitstream += "00";

    // Callsign: empty (42 bits = 7 chars * 6 bits)
    bitstream += string(42, '0');

    // Vessel Name: "CRANE VESTA" padded to 20 chars (120 bits)
    string nameEncoded = encodeVesselName(vesselName, 20);
    bitstream += nameEncoded;

    // Ship Type: 0 (8 bits)
    bitstream += "00000000";

    // Dimension A: 0 (9 bits)
    bitstream += "000000000";

    // Dimension B: 0 (9 bits)
    bitstream += "000000000";

    // Dimension C: 0 (9 bits)
    bitstream += "000000000";

    // Dimension D: 0 (9 bits)
    bitstream += "000000000";

    // Position Reference and ETA: 0 (30 bits)
    bitstream += string(30, '0');

    // Destination: empty (120 bits)
    bitstream += string(120, '0');

    // DTE: 0 (1 bit)
    bitstream += "0";

    // Spare: 0 (6 bits)
    bitstream += "000000";

    cout << "Vessel Name: " << vesselName << endl;
    cout << "Vessel Name Binary: " << nameEncoded << endl;
    cout << "Total Bitstream Length: " << bitstream.length() << " bits" << endl;

    // Convert to 6-bit ASCII payload
    string payload = sixbitToAIS6Bit(bitstream);
    cout << "6-bit Payload: " << payload << endl;
    cout << "Payload Length: " << payload.length() << " characters" << endl;

    // Check if fragmentation needed (max 61 chars for payload in NMEA)
    if (payload.length() > 61) {
        cout << "\n=== FRAGMENTATION NEEDED ===" << endl;
        cout << "Sentence 1 payload: " << payload.substr(0, 61) << endl;
        cout << "Sentence 2 payload: " << payload.substr(61) << endl;

        // Create NMEA sentences
        string nmea1 = "!AIVDM,2,1,1,A," + payload.substr(0, 61) + ",0";
        nmea1 += "*" + calculateChecksum(nmea1);

        string nmea2 = "!AIVDM,2,2,1,A," + payload.substr(61) + ",0";
        nmea2 += "*" + calculateChecksum(nmea2);

        cout << "\n=== NMEA SENTENCES ===" << endl;
        cout << nmea1 << endl;
        cout << nmea2 << endl;
    } else {
        cout << "\n=== SINGLE NMEA SENTENCE ===" << endl;
        string nmea = "!AIVDM,1,1,,A," + payload + ",0";
        nmea += "*" + calculateChecksum(nmea);
        cout << nmea << endl;
    }

    return 0;
}