#include <iostream>
#include <string>
using namespace std;

// AIS 6-bit character encoding (ITU-R M.1371 specification)
string encode6bitString(const string& text, int maxLen) {
    string result;
    string truncated = text.substr(0, maxLen);
    for (char& c : truncated) {
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    }

    for (int i = 0; i < truncated.length(); ++i) {
        char ch = truncated[i];
        int val;

        if (ch == '@' || ch == ' ')
            val = 0;
        else if (ch >= 'A' && ch <= 'Z')
            val = ch - 'A' + 1;
        else if (ch >= '0' && ch <= '9')
            val = ch - '0' + 48;
        else
            val = 0;

        for (int bit = 5; bit >= 0; --bit) {
            result += ((val >> bit) & 1) ? '1' : '0';
        }
    }

    while (result.length() < maxLen * 6) {
        result += "000000";
    }

    return result;
}

string binaryToAIS6Bit(const string& bitstream) {
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
    cout << "=== CORRECT Type 5 with Vessel Name at BIT 143 ===" << endl;
    cout << "Based on ITU-R M.1371 specification" << endl << endl;

    // AIS Type 5 structure (ITU-R M.1371)
    string bitstream;

    // 0-5: Message Type (6 bits) = 5
    bitstream += "000101";

    // 6-7: Repeat Indicator (2 bits) = 0
    bitstream += "00";

    // 8-37: User ID (MMSI) (30 bits) = 123456789
    bitstream += "000000011011110001010101101101";

    // 38-39: AIS Version (2 bits) = 0
    bitstream += "00";

    // 40-69: IMO Number (30 bits) = 0 (not used)
    bitstream += "000000000000000000000000000000";

    // 70-111: Call Sign (42 bits = 7 chars) = EMPTY
    string callsignEncoded = encode6bitString("", 7);
    cout << "Call sign field (bits 70-111): " << callsignEncoded.length() << " bits" << endl;
    bitstream += callsignEncoded;

    // 112-142: Ship Type and other fields (31 bits)
    bitstream += "00000000";  // Ship Type (8 bits)
    bitstream += "0000000000000000000000";  // Spare + other fields (23 bits)

    // 143-263: VESSEL NAME (120 bits = 20 chars) - THIS IS THE KEY!
    string vesselName = "CRANE VESTA";  // 11 chars + 9 spaces padding
    string vesselNameEncoded = encode6bitString(vesselName, 20);
    cout << "Vessel name: '" << vesselName << "'" << endl;
    cout << "Vessel name field (bits 143-263): " << vesselNameEncoded.length() << " bits" << endl;
    cout << "Starts at bit: " << bitstream.length() << " (should be 143)" << endl;
    bitstream += vesselNameEncoded;

    // 264-292: More fields (29 bits)
    bitstream += "00000000000000000000000000000";

    // 293-423: Destination (120 bits = 20 chars) = EMPTY
    string destEncoded = encode6bitString("", 20);
    bitstream += destEncoded;

    // 424: DTE (1 bit) = 0
    bitstream += "0";

    // Add padding if needed
    while (bitstream.length() % 6 != 0) {
        bitstream += "0";
    }

    cout << "Total bitstream length: " << bitstream.length() << " bits" << endl;
    cout << "Expected: 424 bits (or multiple of 6)" << endl;

    string payload = binaryToAIS6Bit(bitstream);
    cout << "Payload: " << payload << endl;
    cout << "Payload length: " << payload.length() << " characters" << endl;

    // Calculate vessel name position in payload
    int vesselNameStartChar = 143 / 6;  // Bit 143 = character 23 (0-indexed)
    cout << "Vessel name starts at character: " << vesselNameStartChar << endl;
    cout << "Vessel name in payload: " << payload.substr(vesselNameStartChar, 20) << endl;

    string nmea = "!AIVDM,1,1,,A," + payload + ",0";
    nmea += "*" + calculateChecksum(nmea);

    cout << "\n=== CORRECTED NMEA SENTENCE ===" << endl;
    cout << nmea << endl;

    return 0;
}