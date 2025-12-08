#include <iostream>
#include <string>
using namespace std;

// Copy dari working AIS library
string encode6bitString(const string& text, int maxLen) {
    string result;
    string truncated = text.substr(0, maxLen);
    for (char& c : truncated) {
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    }

    for (int i = 0; i < truncated.length(); ++i) {
        char ch = truncated[i];
        int val;
        if (ch == '@' || ch == ' ') val = 0;
        else if (ch >= 'A' && ch <= 'Z') val = ch - 'A' + 1;
        else if (ch >= '0' && ch <= '9') val = ch - '0' + 48;
        else val = 0;

        // Convert ke 6-bit
        string bits;
        for (int bit = 5; bit >= 0; --bit) {
            bits += ((val >> bit) & 1) ? '1' : '0';
        }
        result += bits;
    }

    // Pad
    while (result.length() < maxLen * 6) {
        result += "000000";
    }
    return result;
}

string binaryToAIS6Bit(const string& bitstream) {
    // Pad ke multiple of 6
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

        // AIS 6-bit encoding: value + 48, +8 if > 87
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
    cout << "=== EXACT WORKING Type 5 Implementation ===" << endl;

    // TEST dengan Type 5 example dari internet yang BEKERJA:
    // !AIVDM,1,1,,A,55MvTPO1G<th000845phE@E@0=h8@@T@0@N@,0*1A

    string bitstream;

    // Build Type 5 STEP BY STEP
    // 1. Message Type: 5 (000101)
    bitstream += "000101";

    // 2. Repeat: 0 (00)
    bitstream += "00";

    // 3. MMSI: 123456789
    bitstream += "000000011011110001010101101101";

    // 4. AIS Version: 0 (00)
    bitstream += "00";

    // 5. IMO: 0 (000000000000000000000000000000)
    bitstream += "000000000000000000000000000000";

    // 6. Call Sign: "TEST123" (7 chars)
    string callsign = "TEST123";
    string callsignEncoded = encode6bitString(callsign, 7);
    bitstream += callsignEncoded;

    // 7. Vessel Name: "CRANE VESTA" (20 chars)
    string vesselName = "CRANE VESTA";  // 11 chars, akan di-pad ke 20
    string vesselEncoded = encode6bitString(vesselName, 20);
    bitstream += vesselEncoded;

    cout << "Callsign: " << callsign << endl;
    cout << "Callsign encoded: " << callsignEncoded << endl;
    cout << "Vessel name: " << vesselName << endl;
    cout << "Vessel encoded: " << vesselEncoded << endl;

    // 8. Ship Type: 0
    bitstream += "00000000";

    // 9. Destination: "PORT XYZ" (20 chars)
    string destination = "PORT XYZ";
    string destEncoded = encode6bitString(destination, 20);
    bitstream += destEncoded;

    // 10. DTE: 0
    bitstream += "0";

    // Pad
    while (bitstream.length() % 6 != 0) {
        bitstream += "0";
    }

    cout << "Total bits: " << bitstream.length() << endl;

    string payload = binaryToAIS6Bit(bitstream);
    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== FINAL NMEA ===" << endl;
    cout << nmea << endl;

    return 0;
}